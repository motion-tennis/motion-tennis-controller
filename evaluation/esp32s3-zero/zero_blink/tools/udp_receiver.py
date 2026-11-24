#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Test receiver for controller_input messages (motion.v03.Envelope over UDP).

Decodes every datagram with the project's motion.proto, checks canonical
encoding, missing, late and duplicate samples, and prints one line per message.
A status line pinned to the top of the terminal shows receive statistics over
a sliding time window.

Requirements: pip install protobuf grpcio-tools
Usage:        python udp_receiver.py [--port 41000] [--window 10] [--raw]
"""

import argparse
import collections
import importlib
import math
import os
import pathlib
import shutil
import signal
import socket
import statistics
import subprocess
import sys
import tempfile
import time

PROTO = pathlib.Path(__file__).resolve().parent.parent / "proto" / "motion.proto"
STATUS_REFRESH_S = 0.2


def load_proto_module():
    """Generates motion_pb2 into a temporary directory and imports it."""
    out_dir = pathlib.Path(tempfile.mkdtemp(prefix="motion_pb2_"))
    args = [f"-I{PROTO.parent}", f"--python_out={out_dir}", str(PROTO)]
    try:
        from grpc_tools import protoc  # bundled protoc, also works on Windows
        if protoc.main(["protoc"] + args) != 0:
            sys.exit("protoc (grpc_tools) failed")
    except ImportError:
        subprocess.run(["protoc"] + args, check=True)
    sys.path.insert(0, str(out_dir))
    return importlib.import_module("motion_pb2")


def enable_ansi():
    """Enables ANSI escape sequences on Windows consoles; returns False if unsupported."""
    if os.name != "nt":
        return True
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_uint32()
        if not kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            return False
        # ENABLE_VIRTUAL_TERMINAL_PROCESSING
        return bool(kernel32.SetConsoleMode(handle, mode.value | 0x0004))
    except (AttributeError, OSError):
        return False


class StatusLine:
    """Keeps one line pinned at the top; normal output scrolls below it."""

    def __init__(self, enabled):
        self.enabled = enabled and sys.stdout.isatty() and enable_ansi()
        self.rows = 0
        if self.enabled:
            self._setup()

    def _setup(self):
        self.rows = shutil.get_terminal_size().lines
        # Clear screen, scroll region rows 2..N, cursor to the last row.
        sys.stdout.write(f"\033[2J\033[2;{self.rows}r\033[{self.rows};1H")
        sys.stdout.flush()

    def show(self, text):
        if not self.enabled:
            print(text, flush=True)
            return
        if shutil.get_terminal_size().lines != self.rows:
            self._setup()  # terminal resized
        width = shutil.get_terminal_size().columns
        text = text[: width - 1]
        # Save cursor, write row 1 in reverse video, restore cursor.
        sys.stdout.write(f"\0337\033[1;1H\033[2K\033[7m{text}\033[0m\0338")
        sys.stdout.flush()

    def close(self):
        if self.enabled:
            sys.stdout.write(f"\033[r\033[{self.rows};1H\n")
            sys.stdout.flush()


class Stats:
    """Statistics for one controller session over a sliding time window."""

    def __init__(self, window_s):
        self.window_s = window_s
        self.window = collections.deque()  # (arrival_s, t0_us, sample_id, size)
        self.recent_ids = collections.deque(maxlen=1024)
        self.recent_set = set()
        self.highest_sample = None
        self.total = 0
        self.lost = 0
        self.duplicates = 0
        self.reordered = 0
        self.saturated = 0
        self.not_canonical = 0
        self.started = time.monotonic()

    def add(self, arrival_s, t0_us, sample_id, size, saturated, canonical):
        """Records one message; returns a note for the packet line or None."""
        self.total += 1
        self.saturated += saturated
        self.not_canonical += not canonical
        if sample_id in self.recent_set:
            self.duplicates += 1
            return f"duplicate sample {sample_id}"
        if len(self.recent_ids) == self.recent_ids.maxlen:
            self.recent_set.discard(self.recent_ids[0])
        self.recent_ids.append(sample_id)
        self.recent_set.add(sample_id)
        note = None
        if self.highest_sample is None:
            self.highest_sample = sample_id
        elif sample_id > self.highest_sample:
            missing = sample_id - self.highest_sample - 1
            if missing:
                self.lost += missing
                note = (f"missing {missing}: {self.highest_sample + 1}"
                        f"{'..' + str(sample_id - 1) if missing > 1 else ''}")
            self.highest_sample = sample_id
        else:
            # Late arrival of a sample previously counted as lost.
            self.reordered += 1
            self.lost = max(0, self.lost - 1)
            note = f"late sample {sample_id} (reordered)"
        self.window.append((arrival_s, t0_us, sample_id, size))
        self._trim(arrival_s)
        return note

    def _trim(self, now_s):
        while self.window and now_s - self.window[0][0] > self.window_s:
            self.window.popleft()

    def summary(self, now_s):
        self._trim(now_s)
        w = list(self.window)
        if len(w) < 3:
            return "waiting for data ..."
        ordered = sorted(w, key=lambda r: r[2])
        arrivals = [r[0] for r in w]
        recv_dt_ms = [(b - a) * 1e3 for a, b in zip(arrivals, arrivals[1:])]
        t0_dt_ms = [(b[1] - a[1]) / 1e3 / max(1, b[2] - a[2])
                    for a, b in zip(ordered, ordered[1:])]
        span_s = arrivals[-1] - arrivals[0]
        rate = (len(w) - 1) / span_s if span_s > 0 else float("nan")
        sender_rate = 1e3 / statistics.fmean(t0_dt_ms) if t0_dt_ms else float("nan")

        # Relative one-way delay: arrival minus t0 on unsynchronised clocks.
        # Its spread is delivery jitter; its minimum is an arbitrary offset.
        delay_ms = [r[0] * 1e3 - r[1] / 1e3 for r in w]
        base = min(delay_ms)
        extra_ms = sorted(d - base for d in delay_ms)
        p99 = extra_ms[min(len(extra_ms) - 1, math.ceil(0.99 * len(extra_ms)) - 1)]

        expected = ordered[-1][2] - ordered[0][2] + 1
        window_loss = expected - len(w)
        silence_s = now_s - arrivals[-1]

        parts = [
            f"{rate:5.1f} Hz (sender {sender_rate:5.1f})",
            f"dt {statistics.fmean(recv_dt_ms):6.1f}±{statistics.pstdev(recv_dt_ms):5.1f} ms"
            f" [{min(recv_dt_ms):.1f}..{max(recv_dt_ms):.1f}]",
            f"jitter {statistics.pstdev(delay_ms):4.1f} ms p99 {p99:5.1f}",
            f"loss {window_loss}/{expected} ({100 * window_loss / expected:.1f}%)",
            f"total {self.total} lost {self.lost} dup {self.duplicates} ooo {self.reordered}",
            f"{w[-1][3]} B",
        ]
        if self.saturated:
            parts.append(f"SAT {self.saturated}")
        if self.not_canonical:
            parts.append(f"NONCANON {self.not_canonical}")
        if silence_s > 1.0:
            parts.insert(0, f"NO DATA {silence_s:.1f} s")
        return " | ".join(parts)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--port", type=int, default=41000)
    parser.add_argument("--window", type=float, default=10.0,
                        help="statistics window in seconds (default 10)")
    parser.add_argument("--raw", action="store_true", help="print the full decoded message")
    parser.add_argument("--no-status-line", action="store_true",
                        help="print statistics as normal lines every 2 s instead")
    args = parser.parse_args()

    pb = load_proto_module()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", args.port))
    sock.settimeout(STATUS_REFRESH_S)

    # Terminate like Ctrl+C so the terminal scroll region is always restored.
    signal.signal(signal.SIGTERM, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt))
    status = StatusLine(not args.no_status_line)
    print(f"Listening on UDP {args.port}, statistics window {args.window:g} s ...", flush=True)

    stats = {}  # (sender_id, session) -> Stats
    current = None
    decode_errors = 0
    next_status = 0.0
    status_period = STATUS_REFRESH_S if status.enabled else 2.0

    try:
        while True:
            try:
                data, (host, _port) = sock.recvfrom(2048)
                arrival = time.monotonic()
            except socket.timeout:
                data = None

            if data is not None:
                env = pb.Envelope()
                try:
                    env.ParseFromString(data)
                except Exception as exc:  # noqa: BLE001 - report any decode error
                    decode_errors += 1
                    print(f"{host}: {len(data)} B, decode error: {exc}", flush=True)
                    data = None

            if data is not None and env.WhichOneof("body") != "controller_input":
                print(f"{host}: unexpected body {env.WhichOneof('body')}", flush=True)
                data = None

            if data is not None:
                canonical = env.SerializeToString(deterministic=True) == data
                ci = env.controller_input
                key = (env.sender_id, env.sender_session_id)
                notes = []
                if key not in stats:
                    notes.append(f"new session {env.sender_session_id.hex()[:8]}")
                    stats[key] = Stats(args.window)
                current = key
                note = stats[key].add(arrival, ci.t0_us, ci.sample_id, len(data),
                                      ci.sensor_saturated, canonical)
                if note:
                    notes.append(note)
                if env.message_seq != ci.sample_id:
                    # This firmware sends exactly one message per sample.
                    notes.append(f"message_seq {env.message_seq} != sample_id")
                if not canonical:
                    notes.append("NOT CANONICAL")
                if (env.major, env.minor) != (0, 3):
                    notes.append(f"version {env.major}.{env.minor}")

                a, g = ci.specific_force_m_s2, ci.angular_velocity_rad_s
                print(f"{host} ctrl={env.sender_id} seq={env.message_seq} sample={ci.sample_id} "
                      f"{len(data)} B | a {a.x:6.2f} {a.y:6.2f} {a.z:6.2f} | "
                      f"g {g.x:5.2f} {g.y:5.2f} {g.z:5.2f}"
                      f"{' SAT' if ci.sensor_saturated else ''}"
                      f"{' | ' + ', '.join(notes) if notes else ''}", flush=True)
                if args.raw:
                    print(env, flush=True)

            now = time.monotonic()
            if now >= next_status:
                next_status = now + status_period
                if current is None:
                    text = f"UDP {args.port}: no data yet"
                else:
                    sessions = len(stats)
                    text = (f"ctrl {current[0]} [{current[1].hex()[:8]}]"
                            f"{f' ({sessions} sessions)' if sessions > 1 else ''} | "
                            + stats[current].summary(now))
                if decode_errors:
                    text += f" | decode errors {decode_errors}"
                status.show(text)
    except KeyboardInterrupt:
        pass
    finally:
        status.close()


if __name__ == "__main__":
    main()
