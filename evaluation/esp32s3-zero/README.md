# ESP32-S3-Zero evaluation

[Repository home](../../README.md) · [Workflow](../../docs/workflow.md) · [Zephyr evaluation setup](https://github.com/motion-tennis/documentation/blob/main/docs/zephyr-eval-setup.md)

**Evaluation and test code only.** This directory is not the controller firmware, does not implement the controller contract and provides no evidence for a stage gate. It records what was tried on a Waveshare ESP32-S3-Zero so that parts can be reviewed and taken over into a clean firmware later. Delete or archive it once that firmware exists.

**Scope as an explicit exception:** the active shared stage is S1. Besides a local MPU-6050 capture path, this code also contains Wi-Fi and UDP/Protobuf transport, which belongs to S2. It was added on the requirements engineer's explicit request for evaluation; it does not advance the stage or authorize adjacent work. The ESP32-S3-Zero is not a board decision: the proposed reference board remains the ESP32-S3-DevKitC-1-N8R8.

## Contents

| Directory | Content | License |
| --- | --- | --- |
| [`board/`](board) | Zephyr board definition `esp32s3_zero` (board root for `BOARD_ROOT`), derived with zephyr-utilities from the upstream `waveshare/esp32s3_matrix` board and adapted (WS2812 on GPIO21, single LED) | Apache-2.0, see [`board/LICENSE`](board/LICENSE) and the original file headers; provenance in `zephyr-board-origin.json` |
| [`zero_blink/`](zero_blink) | Test application: status LED, MPU-6050 or simulated IMU thread, nanopb encoding of `controller_input`, Wi-Fi station and UDP sender, plus the Python test receiver `tools/udp_receiver.py`. See its [README](zero_blink/README.md) | MIT; `proto/` holds copies of the shared contract files |
| [`vscode/`](vscode) | VS Code configuration template for a WSL west workspace. See its [README](vscode/README.md) | MIT |

## Verified so far

| Item | State |
| --- | --- |
| Board definition, minimal application | Built, flashed and running on one module (onboard LED) |
| Toolchain | Zephyr v4.4.0, SDK 1.0.1, Ubuntu 26.04 under WSL 2; details in the [evaluation setup](https://github.com/motion-tennis/documentation/blob/main/docs/zephyr-eval-setup.md) |
| VS Code build/flash/monitor | Confirmed by the user |
| Protobuf encoding | Built; one message checked on a host as canonical (133 bytes) |
| Wi-Fi, UDP delivery, real MPU-6050, 200 Hz | **Not verified on the device** |

## Known deviations from the controller contract

These are test simplifications to be resolved in the clean firmware, not contract changes:

- Timer-driven polling instead of the MPU-6050 data-ready interrupt; `t0` is the timer wake-up and the DLPF/divider are not read back.
- No `hello`, association, calibration or clock exchange; `run_id`, `association_version`, `calibration_id` and `battery_millivolts` are placeholders.
- No pending table, `feedback_result` handling, local expiry or failure records.
- Zephyr's nanopb module (1.0.0-dev) instead of the nanopb 0.4.9.1 pinned by the shared contract checks.
- No threshold marker, haptic output or battery measurement.

## Workspace layout

The code expects a Zephyr v4.4.0 west workspace in WSL as described in the evaluation setup, with this repository cloned inside it:

```text
~/zephyrproject/
├── .venv/  .west/  zephyr/  modules/
├── motion-tennis-controller/          this repository
│   └── evaluation/esp32s3-zero/{board,zero_blink,vscode}
└── build/zero_blink/                  build output (outside the repository)
```

Clone it inside WSL rather than building from a Windows or cloud-synchronised folder (`/mnt/c`, `/mnt/d`), which is slow and would synchronise build output.
