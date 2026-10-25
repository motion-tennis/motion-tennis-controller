# Controller repository orientation

[Repository home](../README.md) · [Local workflow](workflow.md) · [Contributor instructions](../AGENTS.md) · [Canonical project status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md)

## Purpose and present evidence

This repository is the firmware boundary for four wireless battery handhelds. Each device measures one player's movement, sends identified samples to the Pi and applies validated LED/haptic feedback. The chosen first prototype is MPU-6050 over I²C with Zephyr; ESP32-S3-DevKitC-1-N8R8 is a proposed board variant. Exact parts, wiring, dependency versions and measured behavior remain unresolved.

**Implemented here:** README, contributor/orientation/workflow documents and MIT license. There is no firmware, board configuration, generated codec, build/flash command or local test suite. The adjacent `debug-units` repository has an offline synthetic trace auditor; that is not controller firmware or hardware evidence. **Agreed design** and **proposed implementation** remain distinct throughout the [component README](../README.md).

Read the canonical [project status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md) to find the currently authorized global stage. The [shared stage gates](https://github.com/motion-tennis/documentation/blob/main/docs/project-stages.md) and [local workflow](workflow.md) describe possible deliverables, not permission to start later ones. Keep the active-stage decision in the central record rather than copying it here.

## Place in the system

| Boundary | This repository owns | Other owner / shared contract |
| --- | --- | --- |
| IMU input | I²C acquisition, verified regular sample boundary, local sample/session IDs, shared-INT source attribution and bounded marker capture | Sensor behavior must be characterized on the selected part; [hardware](https://github.com/motion-tennis/documentation/blob/main/docs/hardware.md) and [IMU decision](https://github.com/motion-tennis/documentation/blob/main/docs/imu-selection.md) |
| Gameplay output | Continuous identified `controller_input`, local pending/result accounting | Pi combines camera and IMU inputs and calculates the match; [protocol](https://github.com/motion-tennis/documentation/blob/main/docs/protocol.md) |
| Control input/output | Session registration, association installation acknowledgement, readiness, raw clock-exchange timestamps | Pi owns player associations and clock maps; [assignment](https://github.com/motion-tennis/documentation/blob/main/docs/player-assignment.md) and [clock mapping](https://github.com/motion-tennis/documentation/blob/main/docs/coordinates-and-time.md#pi-reference-clock-mapping) |
| Feedback input/output | Validate matching `feedback_result`, apply eligible bounded output, record electrical boundaries and suppression | Hit/miss follows the required ball query/response and accepted commit; game service stores the committed state |
| Diagnostics | Buffered original sample/IRQ/receive/output records and visible losses | `debug-units` owns collection and analysis; [marker contract](https://github.com/motion-tennis/documentation/blob/main/docs/imu-trigger-markers.md) |

The normal controller stream remains 200 Hz, or one nominal transaction every 5 ms, including between points. The threshold IRQ adds a trace marker without changing that stream. A 200 Hz result stream is not a 200 Hz vibration stream. Firmware must not calculate hits, infer long-term racket position by integrating acceleration, assign teams from discovery order, or maintain another match state.

The Pi remains the sole calculator and clock-map authority. Windows tracking runs continuously; Windows game storage answers the mandatory state query and commits results. A controller's local `tr - t0` and eligible `t1 - t0` do not require cross-device synchronization. Original timestamps and failure outcomes survive diagnostic export.

## Reading order for a new task

1. Read [project status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md), [shared stages](https://github.com/motion-tennis/documentation/blob/main/docs/project-stages.md), and the [contribution workflow/task template](https://github.com/motion-tennis/documentation/blob/main/docs/contribution-workflow.md); record the stage, scope and acceptance evidence.
2. Read [local workflow](workflow.md) and the relevant parts of the [README](../README.md), not every future feature as an implementation checklist.
3. Review the central [controller design](https://github.com/motion-tennis/documentation/blob/main/docs/controller.md), [protocol](https://github.com/motion-tennis/documentation/blob/main/docs/protocol.md), [reference profile](https://github.com/motion-tennis/documentation/blob/main/spec/reference-profile.json), and the exact hardware/marker/timing contracts touched by the slice.
4. Check current files and Git status. Record cross-repository dependencies and use the central handoff format so the next human or AI contributor can continue without guessing.

## Available checks

From this repository, the existing local check for tracked changes is:

```sh
git diff --check
```

Also inspect new untracked Markdown for trailing whitespace and a final newline. When the central documentation checkout is available, run its existing checker **from that checkout**:

```sh
python3 scripts/check_docs.py
```

That checker examines available sibling documentation and links; it does not compile firmware, test the MPU-6050, validate Zephyr compatibility or measure latency. Missing sibling checkouts are not verified remote content. No additional build, flash or test command exists here yet. Add reproducible commands only when their implementation arrives in an authorized slice.

## Small S0 tasks to propose

Use the shared task template to select one bounded documentation/contract task, for example:

- Review the controller fields needed by the future versioned doubles/roster contract and document compatibility/maximum-size fixture cases with the other owners.
- When explicitly tasked, add a small generated-codec contract fixture or validation tool proving those field/size/resource assumptions; this is not firmware or runtime codec integration.
- Specify known-answer cases for session/association retirement, sample expiry and shared data-ready/motion INT ambiguity without implementing a driver.
- Record the hardware and Zephyr evidence still needed for a future minimal acquisition slice, including filter/divider readback and safe output defaults.

These are S0 contract/planning suggestions, not automatic follow-on tasks. Small contract-validation artifacts require their own explicit scope; firmware, runtime codec integration, live networking, marker-driver extensions and gameplay integration remain governed by their later named stages and task authorization.
