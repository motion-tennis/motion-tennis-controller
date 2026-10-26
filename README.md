# Motion Tennis Controller

The handheld component of **Motion Tennis**, a doubles tennis demonstrator for four people in two fixed teams. Each battery-powered wireless controller belongs to one player/avatar, sends identified movement samples to the Raspberry Pi 3 Model B, and provides visible and haptic feedback after physics processing.

**Start contributing:** read [repository orientation](docs/orientation.md), [staged workflow](docs/workflow.md) and [AGENTS.md](AGENTS.md), then check the canonical [project status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md). The [shared task/handoff workflow](https://github.com/motion-tennis/documentation/blob/main/docs/contribution-workflow.md) keeps each human or AI contribution within a named, bounded slice; the specifications below are not an instruction to implement all future stages at once.

The [S0 contract review and first S1 handoff](docs/s0-contract-review.md) record this component's producer/consumer expectations, required fixtures and remaining evidence.

## Status and reference hardware

| Status | Meaning |
| --- | --- |
| **Implemented** | Documentation and an MIT license only; no firmware, board configuration, build/flash setup or tests exist |
| **Agreed design** | ESP32 handhelds using MPU-6050 over I²C and Zephyr for the first prototype, with LED and vibration motor; wireless battery operation; unchanged continuous 200 Hz stream plus an independent, traceable hardware threshold marker |
| **Proposed reference** | ESP32-S3-DevKitC-1-N8R8 board, bounded UDP/Protobuf messaging and hardware-limited feedback |
| **Open** | Exact ESP32 board/module and MPU-6050 breakout, validated wiring/pins, battery/power circuit, motor/driver selection, Zephyr/HAL versions, motion-handler/early-timestamp integration and measured timing |

MPU-6050/I²C and Zephyr are selected for the first prototype; the exact breakout and ESP32 board remain to be verified. This is a design decision, not an inspected inventory or a working driver integration. The [IMU assessment](https://github.com/motion-tennis/documentation/blob/main/docs/imu-selection.md) and [ADR 0008](https://github.com/motion-tennis/documentation/blob/main/docs/adr/0008-mpu6050-zephyr-prototype.md) record the remaining marker/timing gates and later alternatives. BMI270/SPI and ESP-IDF are historical choices superseded by the MPU-6050/Zephyr selection first recorded in profile `webcam-pi3-wireless-v2` and retained in `webcam-pi3-doubles-v3`. The v0.3 contract retains the controller UDP stream, rate and expiry while extending the separate match-state interface. The controllers are not Raspberry Pis. Use the [master plan](https://github.com/motion-tennis/documentation/blob/main/docs/master-plan.md), [hardware specification](https://github.com/motion-tennis/documentation/blob/main/docs/hardware.md), and [reference profile](https://github.com/motion-tennis/documentation/blob/main/spec/reference-profile.json) before implementation.

## Responsibilities and boundaries

Each regular IMU sampling boundary starts a separate measurement transaction. Record controller-local `t0`, boot/session identity, request sequence and sample ID before dispatching bounded work. Preserve original sample timing and any sensor counter separately; sensor conversion/filtering may precede the data-ready interrupt. The additional threshold IRQ creates a diagnostic marker, not a gameplay transaction.

Configure and verify the MPU-6050 filter and sample divider together for nominal 200 Hz; with a 1 kHz base, divider four gives a 5 ms interval. Read back the selected mode and measure cadence under load rather than inferring 200 Hz from the divider alone. Its single INT output combines enabled data-ready/motion sources, so an edge must be attributed before it becomes a regular sample boundary or a threshold marker.

Read and validate the sample outside unnecessary interrupt work, send specific force and angular rate to physics, and keep a pending record until a matching application result or local terminal failure. Physics owns pose association, fusion, automatic avatar movement, ball/serve calculations and scoring. Firmware does not own match state or infer long-term racket position by double-integrating acceleration.

A valid `feedback_result` completes the application measurement; a network or receipt acknowledgement does not. Record complete-datagram receive time as candidate `tr`, retaining it only after validation. Duplicate, wrong-session or expired responses cannot trigger a second or stale cue. Preserve explicit errors for missing tracking, stale/invalid input, overload and communication failure; none becomes an ordinary miss.

## Player assignment and gestures

Follow the [assignment contract](https://github.com/motion-tennis/documentation/blob/main/docs/player-assignment.md): the proposed slots are `1=A1`, `2=A2`, `3=B1`, `4=B2`, with one confirmed controller/person/avatar per slot. The operator selects team, available slot and hand, confirms the person identified by guided motion, then physics installs the versioned `Association`. Require the matching `command_id` acceptance and resume barrier; re-pairing retires old pending work/cues and occurs only while paused or in the lobby. Teams remain fixed through virtual end/front-back changes. Normal doubles requires all four slots ready; one-to-three-controller runs are integration tests without implicit AI players.

In the [agreed gameplay](https://github.com/motion-tennis/documentation/blob/main/docs/gameplay.md), people remain in calibrated physical stations and control body-relative arm/racket gestures. The Pi moves avatar roots automatically. The assigned server lifts the controller to request a virtual toss, then attempts an overhead swing; Pi recognition uses ordinary pose/IMU history. All controllers retain the continuous 200 Hz stream through serving and between points, and the additional IRQ remains diagnostic. The proposed v0.3 contract defines full match/roster/score state on the separate Pi/game boundary while retaining controller sample and feedback semantics; use the shared version/hash baseline before runtime integration. Assignment, gesture recognition and firmware remain unimplemented.

## Connections and reference limits

The proposed [`0.3-proposed` protocol](https://github.com/motion-tennis/documentation/blob/main/docs/protocol.md) and [schema](https://github.com/motion-tennis/documentation/blob/main/spec/motion.proto) define the exact fields. All components must agree on schema/profile hashes.

| Direction | Interface | Component |
| --- | --- | --- |
| Local input | MPU-6050 accel/gyro samples over I²C, verified regular sampling boundary, additional hardware motion event and source timing | One shared INT output for data-ready/motion; exact breakout, I²C pins and validated GPIO mapping remain open |
| Output / input | `controller_input` / matching `feedback_result`, one UDP/Protobuf envelope per datagram, at most 1,200 bytes | Pi UDP 41000 and the negotiated controller reply endpoint |
| Control | Registration, clock exchange, pairing, configuration and readiness over TCP | Pi TCP 41040 |
| Local output | Bounded LED/motor commands and electrical markers | Feedback driver and measurement test points |
| Diagnostics | Buffered transaction/stage records and battery/error counters | Shared `debug-units` tooling |

Retain 2.4 GHz Wi-Fi. Baseline A uses an external access point with wired Pi and laptop; planned variant B uses the Pi's onboard hotspot with the laptop still wired. The [A/B comparison](https://github.com/motion-tennis/documentation/blob/main/docs/networking.md#external-ap-versus-pi-hotspot-comparison) keeps controller traffic, protocol, deadlines and marker behavior unchanged. Register the selected deployment endpoint before each run and verify Windows debug reachability. USB is a stationary diagnostic reference, not the four-player transport; ESP-NOW is deferred.

The active-play power policy disables Wi-Fi modem sleep and leaves Bluetooth unused. Pin and verify the corresponding Zephyr ESP32 driver configuration, then measure receive latency and battery cost; do not treat an ESP-IDF-specific application call as a Zephyr API.

The reference profile uses 200 transactions/s per controller, a 16-entry local pending table and a 50 ms local expiry. Inputs are sent once without live application retransmission; missing data is counted. The Pi may batch transactions for transport and calculation, but every regular sample retains its own ID and response. Proposed p99 targets are 30 ms for `tr - t0` and 40 ms for eligible electrical feedback `t1 - t0`. These are unverified targets, not hard real-time guarantees.

## Additional threshold marker

Follow the [marker contract](https://github.com/motion-tennis/documentation/blob/main/docs/imu-trigger-markers.md): timestamp a configurable hardware acceleration event as `t_trigger` on the ESP32 clock, assign a session-scoped trigger ID, and export a bounded diagnostic record asynchronously. Keep the gameplay rate, payloads, sample ordering, priority and expiry unchanged. A trigger is a swing candidate; physics remains responsible for swing/contact decisions.

Explicitly link the marker to the first regular sample captured at or after it, retaining the full transaction key and association; unresolved capture timing remains unresolved. The planned debug viewer follows pose selection, required ball query/response, decision, commit and matching feedback. Keep per-sample RTT separate from trigger-to-linked-response and optional semantic cue timing.

MPU-6050 has one shared INT pin; no second marker wire is assumed. The reviewed upstream Zephyr driver provides data-ready callbacks, while motion setup/handling and early shared-edge timestamp capture require extension. Validate detector semantics and threshold/duration settings on the actual part, including status clearing, latching, overlapping sources and rearm. A delayed Zephyr callback or ambiguous shared-line status must not be reported as an exact threshold IRQ edge. Continue regular acquisition without marker-triggered sends, mode changes or priority changes. Software thresholding would be a different boundary and cannot silently replace the hardware marker. This firmware and viewer/auditor support are not implemented.

## Pi reference clock mapping

The [agreed clock design](https://github.com/motion-tennis/documentation/blob/main/docs/coordinates-and-time.md#pi-reference-clock-mapping) maps each controller's native monotonic clock into Pi reference time with offset and drift correction, `T_pi = a * t_source + b`. Retain raw timestamps and source clock/session identity; never step the ESP32 clock. Respond to Pi-coordinated four-timestamp `ClockExchange` over the existing TCP control connection, recording complete-frame receipt and the declared response-send boundary before deferred work where possible. These are software boundaries, not radio transmission timestamps.

Runtime timing decisions and debug traces use the same Pi-owned mapping version, uncertainty and source/reference epochs. A controller restart invalidates its mapping; a Pi reference restart invalidates all mappings. Unknown, expired or insufficiently accurate mappings prevent cross-device freshness approval and timing comparisons, while the continuous 200 Hz stream, sample/failure accounting and same-controller RTT remain available. Mapping firmware and its control/debug integration are not implemented; this decision changes neither gameplay payloads nor sampling behavior.

## Feedback and electrical measurement

Every measurement receives an outcome or a counted local failure; it does **not** generate a motor pulse. Emit a haptic cue once per semantic hit or completed missed swing, using the result's cue identity and eligibility flag. Ordinary no-collision samples may return `miss` with no electrical cue. The status LED reflects state; it is not toggled for all 200 samples/s.

For an eligible cue, `t1` is actual LED or motor-drive activation, not a call that merely queues output. Record separate output times where needed. Non-eligible samples have no `t1` and remain visible in reporting. Suppress expired cues, cap drive intensity/duration locally, turn outputs off during reset/fault recovery, and use distinct fault indications. Exact motor behavior requires the chosen electrical hardware to be characterized.

All `t0`, `tr` and `t1` differences use the same ESP32 clock; no inter-device synchronization is needed for local RTT. The full path includes Pi queueing, the mandatory game ball-state query/response, calculation and proposed commit acknowledgement. Camera acquisition preceding `t0`, actual vibration onset and display photons are separate measurements. See [controller behavior](https://github.com/motion-tennis/documentation/blob/main/docs/controller.md) and [timing](https://github.com/motion-tennis/documentation/blob/main/docs/timing.md).

## Prerequisites and setup

Git and a Markdown viewer are sufficient for current documentation work. The selected prototype firmware requires pinned and verified Zephyr/toolchain/HAL revisions, an ESP32 board configuration, MPU-6050 I²C/INT integration, feedback/power design and generated bounded codecs. Zephyr integration must cover the actual 200 Hz mode, motion-handler extension and early timestamp hooks, not only an IMU binding. No firmware installation or flashing command is available yet.

The verified repository remote can be cloned with:

```sh
git clone git@github.com:motion-tennis/motion-tennis-controller.git
cd motion-tennis-controller
```

Configured GitHub access is required. Planned implementation modules are board/sensor initialization, interrupt timestamp capture, sample validation, protocol/control handling, bounded pending transactions, cue scheduling, battery monitoring and buffered tracing. Add actual source paths, dependency locks, configuration and build/flash/run instructions with implementation. Keep credentials outside Git.

## Planned smoke test and current checks

These are stage-scoped acceptance cases, not a separate implementation sequence or release of future work. Use the [local workflow](docs/workflow.md) and canonical status to select one authorized slice:

- **S1, local slice:** verify MPU-6050 I²C/filter/divider configuration, 200 Hz cadence, source IDs/timestamps and safe bounded output defaults using local fixtures and available hardware.
- **S2, feasibility:** use one battery controller and explicitly labeled transport/pose/game stubs for isolated session and radio probes. Add the scoped Zephyr motion-handler/timestamp extension and threshold markers; verify IRQ delay, status clearing/latching, repeated/haptic triggers and overlaps without changing the stream. Validate clock drift/uncertainty/asymmetry and restart behavior with the actual Pi and controller; stub-only evidence cannot pass that gate.
- **S3, real diagnostic path:** with real tracking, trace a sample and linked marker through pose selection, the required ball query/response, accepted atomic update and matching feedback. Verify `t0`, `tr` and eligible `t1` with GPIO/electrical traces, including miss, tracking failure, packet loss, expired/duplicate response, reboot and queue-overflow cases.
- **S4, doubles:** under the reviewed match-contract extension, test all four confirmed bindings and body-relative racket/overhead-serve gestures while Pi moves avatars automatically. Keep every regular sample and avoid marker-driven sends/decisions. Use the shared [integration matrix](https://github.com/motion-tennis/documentation/blob/main/docs/testing.md); S5 owns the complete matched measurement campaign.

No firmware, hardware or wireless test has run. The current repository-local documentation check is:

```sh
git diff --check
```

The central [development guide](https://github.com/motion-tennis/documentation/blob/main/docs/development.md) describes the cross-repository checker. The [debug environment](https://github.com/motion-tennis/documentation/blob/main/docs/debugging.md) defines trace correlation, packet evidence and timing reports; offline synthetic checks are not hardware evidence.

## Layout, limitations, and contribution

```text
README.md            Component reference profile and implementation status
AGENTS.md            Contributor instructions and scope boundaries
docs/orientation.md  Role, reading order, actual evidence and checks
docs/workflow.md     Local deliverables mapped to shared stage gates
LICENSE              Existing MIT license
```

Hardware wiring, battery runtime, safe actuation limits, interrupt/sample timing, radio behavior and four-player capacity remain unverified. Follow the [open-decision register](https://github.com/motion-tennis/documentation/blob/main/docs/roadmap.md).

All project edits must be in English, including code, identifiers, comments, docstrings, examples and diagrams. New C/C++ comments use `//`. Keep shared contracts and related READMEs consistent, preserve implementation-status labels and report actual checks.

Related repositories: [documentation](https://github.com/motion-tennis/documentation), [camera tracking](https://github.com/motion-tennis/motion-tennis-camera-tracking), [physics](https://github.com/motion-tennis/motion-tennis-physics-engine), [game engine](https://github.com/motion-tennis/motion-tennis-game-engine), and [debug-units](https://github.com/motion-tennis/debug-units).

Licensed under the [MIT License](LICENSE), copyright 2026 Viktor Hofer. Preserve this attribution.
