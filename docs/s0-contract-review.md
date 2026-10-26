# S0 controller contract review and S1 handoff

[Repository home](../README.md) · [Local workflow](workflow.md) · [Central status](https://github.com/motion-tennis/documentation/blob/main/docs/project-status.md) · [Shared contract](https://github.com/motion-tennis/documentation/blob/main/docs/protocol.md)

## Review scope and evidence

Task: S0 component-side contract review and bounded S1 handoff, 2026-10-26. The starting shared revision is `documentation@95c2592`; this review accompanies the S0 schema/profile revision identified by the exact hashes in the shared evidence. The [S0 baseline](https://github.com/motion-tennis/documentation/blob/main/docs/s0-baseline.md) pins the final contract hashes and codec/size results. The candidate is `motion.v03`, protocol `0.3-proposed`, profile `webcam-pi3-doubles-v3`; [ADR 0011](https://github.com/motion-tennis/documentation/blob/main/docs/adr/0011-full-match-contract-and-state-stream.md) records the full-match state channel. This document records an AI technical review, not a human stage decision or an implemented component. The central ledger alone releases S1.

Allowed work here is documentation only. Shared schema, codec fixtures and resource checks belong in `documentation`; runtime integration and hardware measurements retain their later stage gates. Proposed numerical targets remain unmeasured. No later-stage exception is claimed.

## Producer and consumer expectations

- Produce one `controller_input` for every regular 200 Hz boundary with the complete controller/session/request key, sample ID, original `t0`/capture time, association revision and calibration. Team and match configuration travel through setup, not new labels on each sample.
- Accept only feedback for the original pending transaction and its still-installed association/calibration. A gameplay result follows the Pi's required state query and accepted atomic match commit; receipt acknowledgements are not results. Duplicate, expired or old-session results never actuate twice.
- Install associations while suspended, echo the exact command and association in the installation acknowledgement, and wait for the correlated resume barrier. Rebinding retires old pending work and queued cues. A repeated numerical controller/person ID after reboot does not restore an old binding.
- Preserve independent diagnostic markers with controller-session-scoped `trigger_id`, raw `t_trigger` and explicit sample links. Shared data-ready/motion INT ambiguity stays visible. Markers change no send cadence, priority, expiry or hit decision.
- Preserve original clocks and Pi-issued map provenance in diagnostics. Same-controller response and electrical intervals remain local-clock differences; unavailable cross-device maps do not invent zero uncertainty.

## Required contract cases

| Case | Expected result and handoff evidence |
| --- | --- |
| Four slots and eight admitted inputs | Stable A1/A2/B1/B2 bindings; at most two entries/controller/batch; one result identity per admitted input, independent of role/end changes |
| Old-session, stale-association or late feedback | Reject and record suppression; retain the original terminal accounting; no output edge |
| Duplicate valid feedback | One retained completion and at most one eligible electrical activation |
| Lost/stale install acknowledgement | Slot remains suspended; idempotent retry cannot activate a newer or different association |
| Marker around overlapping shared INT sources | Preserve source-attribution uncertainty and unresolved link; no extra gameplay transaction |
| Maximum fields and pending capacity | Generated-codec bounds plus the configured 16 pending transactions; overflow has an explicit outcome rather than unbounded allocation |

Codec fixtures establish the message boundary only. Exact ESP32 board, MPU-6050 breakout, wiring, Zephyr/HAL revisions and measured sampling/IRQ/output behavior still need evidence.

## First bounded S1 task

**Outcome:** a local Zephyr/MPU-6050 acquisition slice with regular sample identities/timestamps and safe default-off outputs. Allow only the board configuration, acquisition module, focused tests and real build instructions needed by that slice. Do not add WLAN, match processing, threshold-driver extensions or integrated feedback.

**Dependencies:** an accepted shared contract/profile, the actual board/breakout/pin selection, pinned Zephyr/toolchain/HAL and an explicit output safety configuration. If a device is unavailable, fixture-backed tests remain synthetic and do not pass the device checks.

**Acceptance:** reproducible build; configured DLPF/divider values and readback on available hardware; recorded sampling cadence, missing/overflow accounting and session/sample IDs; safe startup/error output behavior; at least one acquisition failure case. Save revisions, environment and raw evidence. Deliver to the Pi input adapter and debug conformance owners without claiming S2 WLAN/marker feasibility.

## Review disposition

The controller boundary is compatible with a full-match extension when the shared version/hash negotiation, fixed resource bounds and lifecycle cases above are enforced. Final codec/size evidence and any contract-blocking findings are recorded centrally. No firmware, generated runtime codec, board test or measured timing is supplied by this review.
