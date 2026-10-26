# Controller contributor instructions

These instructions apply to this repository. They support both human and AI contributors; a description of future functionality is not authorization to implement it all.

## Establish the task boundary

1. Read [orientation](docs/orientation.md), [local workflow](docs/workflow.md), the canonical [project status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md), and the shared [contribution workflow and task template](https://github.com/motion-tennis/documentation/blob/main/docs/contribution-workflow.md).
2. Inspect Git status and relevant existing files; preserve other contributors' changes. State the task's stage ID, bounded deliverable, allowed paths, acceptance evidence and deferred work using the shared template before implementation. The [shared stage gates](https://github.com/motion-tennis/documentation/blob/main/docs/project-stages.md) govern progression; this repository does not maintain a competing active-stage flag.
3. Work only on the authorized slice. Do not automatically advance stages, implement dependent future features, add a general framework, or turn an isolated sensor task into the full controller runtime. A completed slice does not complete its whole stage.
4. An explicit human request for a named later-stage slice is a bounded exception: record its scope and missing prerequisite evidence in the handoff without advancing the global stage or authorizing adjacent work. Routine fixes needed for an already authorized slice do not require repeated approval.
5. If canonical status cannot be read, use a pinned, verified status/revision supplied with the task. Do not infer runtime authorization from a roadmap or an old README; record the missing baseline and any explicit human exception rather than inventing stage progress.

S0 permits small explicitly tasked contract-validation tools and generated-codec fixtures for compatibility, encoded-size and resource proof. Keep those validation artifacts separate from firmware/runtime codec integration, drivers and live services, which require their own later-stage task. A contract fixture does not authorize a controller runtime.

## Preserve the controller contract

- Write all repository content, code, identifiers, comments and examples in English. Use `//` for new C/C++ comments. Preserve licenses and attribution.
- The first prototype uses MPU-6050 over I²C and Zephyr. Exact board/breakout, pins and verified dependency versions remain explicit evidence gates; the proposed ESP32-S3 board is not an inspected inventory.
- Preserve continuous 200 Hz sampling/transmission and each sample's controller/session/request identity. A hardware threshold IRQ is additional diagnostic evidence, not an extra transaction, send trigger, replacement `t0`, hit decision or new priority.
- MPU-6050 has one shared INT. Do not invent a second interrupt output, sensor timestamps or working Zephyr motion support. Keep source attribution, early timestamps, filter/divider readback and unresolved overlap explicit.
- Pi physics owns fusion, avatar movement, ball/serve/rule calculations and scoring. Preserve the required Pi-to-game ball-state request/response and accepted-commit feedback semantics; controller firmware must not replace them with local decisions.
- Preserve original clocks, same-controller timing, bounded outputs/queues, expiry, duplicate handling, and explicit failures. The Pi owns clock maps; never silently step the ESP32 clock or relabel late callbacks as IRQ edges.
- Use the central protocol/profile/marker/assignment specifications; coordinate a versioned contract change rather than silently changing payloads or numerical limits. The proposed v0.3 full-match schema is a shared contract, not implemented controller/runtime interoperability; use its pinned profile and fixtures.

## Validate and hand off

Report implemented, agreed, proposed and open work separately. Run only checks that actually exist and apply to the slice; do not invent build/flash commands or hardware results. For documentation, run `git diff --check`, inspect new untracked Markdown for whitespace/final newlines, and run the central documentation checker when that checkout is available as described in [orientation](docs/orientation.md#available-checks).

Finish with the stage/slice, changed files, commands and evidence, unresolved risks/dependencies, any later-stage exception, and the next bounded task using the shared handoff template. Do not continue into that next task automatically. Do not commit, push or publish without authorization.
