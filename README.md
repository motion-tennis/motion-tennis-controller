# Motion Tennis Controller

The handheld component of **Motion Tennis**, a multiplayer tennis demonstrator for up to four players. Four battery-powered wireless controllers measure movement, send identified samples to the Raspberry Pi 3 Model B, and provide visible and haptic feedback after physics processing.

## Status and reference hardware

| Status | Meaning |
| --- | --- |
| **Implemented** | Documentation and an MIT license only; no firmware, board configuration, build/flash setup or tests exist |
| **Agreed design** | ESP32 handhelds with IMU, LED and vibration motor; all four controllers must operate wirelessly from batteries |
| **Proposed reference** | ESP32-S3-DevKitC-1-N8R8, ESP-IDF C/C++, BMI270 over SPI at 200 Hz, bounded UDP/Protobuf messaging and hardware-limited feedback |
| **Open** | Actual purchased parts, validated wiring/pins, battery/power circuit, motor/driver selection, dependency versions and measured timing |

The reference board and IMU are recommendations, not an inventory claim. The controllers are not Raspberry Pis. Use the [master plan](https://github.com/motion-tennis/documentation/blob/main/docs/master-plan.md), [hardware specification](https://github.com/motion-tennis/documentation/blob/main/docs/hardware.md), and [reference profile](https://github.com/motion-tennis/documentation/blob/main/spec/reference-profile.json) before implementation.

## Responsibilities and boundaries

Each IMU interrupt starts a separate measurement transaction. Record controller-local `t0`, boot/session identity, request sequence and sample ID before dispatching bounded work. Preserve original sample timing and any sensor counter separately; sensor conversion/filtering may precede the interrupt.

Read and validate the sample outside unnecessary interrupt work, send specific force and angular rate to physics, and keep a pending record until a matching application result or local terminal failure. Physics owns pose association, fusion and hit calculation. Firmware does not own ball state or infer long-term racket position by double-integrating acceleration.

A valid `feedback_result` completes the application measurement; a network or receipt acknowledgement does not. Record complete-datagram receive time as candidate `tr`, retaining it only after validation. Duplicate, wrong-session or expired responses cannot trigger a second or stale cue. Preserve explicit errors for missing tracking, stale/invalid input, overload and communication failure; none becomes an ordinary miss.

## Connections and reference limits

The proposed [`0.2-proposed` protocol](https://github.com/motion-tennis/documentation/blob/main/docs/protocol.md) and [schema](https://github.com/motion-tennis/documentation/blob/main/spec/motion.proto) define the exact fields. All components must agree on schema/profile hashes.

| Direction | Interface | Component |
| --- | --- | --- |
| Local input | BMI270 SPI samples, data-ready interrupt and sensor timing | IMU |
| Output / input | `controller_input` / matching `feedback_result`, one UDP/Protobuf envelope per datagram, at most 1,200 bytes | Pi UDP 41000 and the negotiated controller reply endpoint |
| Control | Registration, clock exchange, pairing, configuration and readiness over TCP | Pi TCP 41040 |
| Local output | Bounded LED/motor commands and electrical markers | Feedback driver and measurement test points |
| Diagnostics | Buffered transaction/stage records and battery/error counters | Shared `debug-units` tooling |

Use a dedicated 2.4 GHz access point; the Pi and Windows laptop use wired LAN links. USB is a stationary diagnostic reference, not the four-player transport. See [networking](https://github.com/motion-tennis/documentation/blob/main/docs/networking.md).

The reference profile uses 200 transactions/s per controller, a 16-entry local pending table and a 50 ms local expiry. Inputs are sent once without live application retransmission; missing data is counted. The Pi may batch transactions for transport and calculation, but every original interrupt retains its own ID and response. Proposed p99 targets are 30 ms for `tr - t0` and 40 ms for eligible electrical feedback `t1 - t0`. These are unverified targets, not hard real-time guarantees.

## Feedback and electrical measurement

Every measurement receives an outcome or a counted local failure; it does **not** generate a motor pulse. Emit a haptic cue once per semantic hit or completed missed swing, using the result's cue identity and eligibility flag. Ordinary no-collision samples may return `miss` with no electrical cue. The status LED reflects state; it is not toggled for all 200 samples/s.

For an eligible cue, `t1` is actual LED or motor-drive activation, not a call that merely queues output. Record separate output times where needed. Non-eligible samples have no `t1` and remain visible in reporting. Suppress expired cues, cap drive intensity/duration locally, turn outputs off during reset/fault recovery, and use distinct fault indications. Exact motor behavior requires the chosen electrical hardware to be characterized.

All `t0`, `tr` and `t1` differences use the same ESP32 clock; no inter-device synchronization is needed for local RTT. The full path includes Pi queueing, the mandatory game ball-state query/response, calculation and proposed commit acknowledgement. Camera acquisition preceding `t0`, actual vibration onset and display photons are separate measurements. See [controller behavior](https://github.com/motion-tennis/documentation/blob/main/docs/controller.md) and [timing](https://github.com/motion-tennis/documentation/blob/main/docs/timing.md).

## Prerequisites and setup

Git and a Markdown viewer are sufficient for current documentation work. Future firmware requires the selected ESP-IDF/toolchain version, verified board/IMU/driver/power design and generated bounded codecs. No firmware installation or flashing command is available yet.

The verified repository remote can be cloned with:

```sh
git clone git@github.com:motion-tennis/motion-tennis-controller.git
cd motion-tennis-controller
```

Configured GitHub access is required. Planned implementation modules are board/sensor initialization, interrupt timestamp capture, sample validation, protocol/control handling, bounded pending transactions, cue scheduling, battery monitoring and buffered tracing. Add actual source paths, dependency locks, configuration and build/flash/run instructions with implementation. Keep credentials outside Git.

## Planned smoke test and current checks

First test one battery-powered controller against simulated pose/game services: trace an interrupt through the required ball query, accepted update and matching response; verify `t0`, `tr` and eligible `t1` with GPIO/electrical traces. Repeat for a miss, tracking failure, packet loss, expired/duplicate response, reboot and queue overflow. Then run all four controllers with real tracking and the [integration matrix](https://github.com/motion-tennis/documentation/blob/main/docs/testing.md).

No firmware, hardware or wireless test has run. The current repository-local documentation check is:

```sh
git diff --check
```

The central [development guide](https://github.com/motion-tennis/documentation/blob/main/docs/development.md) describes the cross-repository checker. The [debug environment](https://github.com/motion-tennis/documentation/blob/main/docs/debugging.md) defines trace correlation, packet evidence and timing reports; offline synthetic checks are not hardware evidence.

## Layout, limitations, and contribution

```text
README.md  Component reference profile and implementation status
LICENSE    Existing MIT license
```

Hardware wiring, battery runtime, safe actuation limits, interrupt/sample timing, radio behavior and four-player capacity remain unverified. Follow the [open-decision register](https://github.com/motion-tennis/documentation/blob/main/docs/roadmap.md).

All project edits must be in English, including code, identifiers, comments, docstrings, examples and diagrams. New C/C++ comments use `//`. Keep shared contracts and related READMEs consistent, preserve implementation-status labels and report actual checks.

Related repositories: [documentation](https://github.com/motion-tennis/documentation), [camera tracking](https://github.com/motion-tennis/motion-tennis-camera-tracking), [physics](https://github.com/motion-tennis/motion-tennis-physics-engine), [game engine](https://github.com/motion-tennis/motion-tennis-game-engine), and [debug-units](https://github.com/motion-tennis/debug-units).

Licensed under the [MIT License](LICENSE), copyright 2026 Viktor Hofer. Preserve this attribution.
