# Stadia BLE: pre-PR feature and validation plan

This is a plan, not a record of hardware passes. No tests, console writes, configuration
changes, pairing attempts or PR publication are authorized merely by this checklist.
Use `STADIA-FW23-GATT-FINDINGS.md` for artifact-specific evidence and chronology.

## What “remembered” must mean

Normal MissionControl usage promises pairing once and reconnecting when a controller is
powered on. The current experimental BLE path does not yet demonstrate that experience:

- `ble_hid_host.cpp::SaveRememberedDevice` persists ONLY the last Bluetooth address, and only
  with `[ble] auto_connect=true`. That option defaults false and is not enabled in this bench
  configuration. It does not save a mutually usable bond.
- Background auto-connect scans for that address advertising in pairing/sync mode. It is not
  a demonstrated normal-power-button reconnect implementation.
- `Connect()` currently calls `btdrvRemoveBond(address)` before a new direct connection, then
  requests fresh pairing. We must understand this deliberate stale-bond workaround before
  designing a persistent-bond path; do not just remove it from the working baseline.
- A working legacy Just Works connection does NOT establish whether either peer retains its
  bond. Existing blanket documentation about Stadia requiring Secure Connections for bond
  retention is unproved. Verify behavior without reading, dumping or extracting key material.
- Only one remembered BLE address/active BLE controller is currently represented by the host.
  This must not be advertised as general multi-controller BLE support.

**Scope decision:** recommend reliable ordinary reconnect as the intended user experience.
If it cannot be delivered safely, an initial PR must explicitly propose default-off,
experimental, fresh-pairing-only support and obtain maintainer agreement on that limitation.
Do not call address remembering “persistent bonding.” Even that narrower scope still needs
reliable repeat fresh pairing and safe coexistence/power transitions.

## Evidence presently available

- OBSERVED: initial fresh pairing, live Report Map/HID Information reads, notification enable,
  Stadia identification, virtual presentation, L1+R1 slot claiming and most controls work.
- OBSERVED: operator now confirms full re-pairing from controller pairing mode works. This
  does not establish normal power-on reconnect, persistent bonds, or repeatability across sleep.
- IMPLEMENTED/TESTED OFFLINE: central Options/Menu bit correction and tester late sampling.
  Complete post-fix physical button verification remains a checklist item. Production cache
  extraction now has behavioral and50,000-event old-policy equivalence coverage; duplicate
  accumulation and same-ID stale-callback limitations are characterized, not fixed.
- OPEN: subjective stick delay. An initial diagnostic run had no saved timing summary because
  of sleep-path suppression. A later awake-only run captured53 input-work samples, all<=500us,
  no FIFO write failures and zero sampled pre-write occupancy. No injection backlog was observed
  in that sample; radio-to-screen latency and intermittent earlier lag remain unmeasured. Menu
  slowdown during reboot initiation was separately reported and went away after boot.
- OPEN: a trace shows module sleep-handling transitions followed by a bonding-stage failure;
  operator is only approximately70% certain they powered off just the controller. Do not
  classify that as a proven awake-only reconnect failure or blame operator/controller.
- UNAVAILABLE LOCALLY: Xbox BLE hardware. Portable fixtures are not Xbox hardware regression proof.

Status below is unchecked until exact evidence is attached. Every hardware attempt requires
operator readiness. Stop on the first failed setup; never blindly repeat directed connection
attempts or disable/enable BLE around sleep.

## P0: connection, ownership and lifecycle

- [ ] C01 Fresh pairing: clean boot, already-running system, Change Grip/Order, correct target,
  and a controlled failure/cancellation. Measure link-to-usable time, not just accepted IPC.
- [ ] C02 Ordinary reconnect: pair once, controller off/on without sync mode; work at HOME
  and in a game without reopening Change Grip/Order. Distinguish directed reconnect from a
  new pairing. Investigate bond persistence using behavior/non-secret metadata only.
- [ ] C03 Persist across a console reboot and full power cycle. Document any controller-side
  bond replacement after pairing with a different host; user-requested re-pairing must recover.
- [ ] C04 Repeat fresh pairing and same-boot off/on without reboot, duplicate slots or stuck
  held inputs. Establish a bounded target of five successful cycles, stopping at any failure.
- [ ] C05 Optional auto-connect: enabled/disabled behavior, last-address persistence after reboot,
  pairing-mode reconnect outside Change Grip/Order, and no connection to an unrelated peer.
  Missing/truncated/unwritable remembered-state file must be safe and not cause boot loops.
- [ ] C06 Sleep/wake while connected, disconnected, scanning, pairing and awaiting a GATT read.
  No spontaneous wake, no radio access while unavailable, no sleep-time BLE reset, no need
  for a reboot to restore GATT. Separate actual controller power-off from console power changes.
- [ ] C07 Shutdown/reboot and cancellation finish safely; distinguish slow reboot initiation
  from active-input lag. Exercise pending work and first-failure latch without deadlock.
- [ ] C08 Out-of-range, controller battery exhaustion and unexpected link loss: neutralize or
  retire held inputs, detach exactly once and recover by the supported connection workflow.
- [ ] C09 Slot/menu lifecycle: initial L1+R1, reopening Change Grip/Order, withdrawing/reclaiming,
  player assignment/reassignment, disconnect indication, no ghost or duplicate controllers.
- [ ] C10 Bound every setup request and preserve one outstanding read. Test late/error/malformed
  completion, disconnect/radio-off/shutdown, and stale callbacks after a new link/connection-ID
  reuse. Where firmware cannot identify an old completion, fail safely rather than assume ownership.

## P0: input and Switch integration

- [ ] I01 All face buttons by physical Pro Controller position: Stadia A→Switch B, B→A,
  X→Y, Y→X. Verify press, hold and release, not only a brief highlight.
- [ ] I02 D-pad cardinals, diagonals, neutral and opposite-direction transitions.
- [ ] I03 L1/R1, both stick clicks and both triggers; verify default and supported configured
  trigger thresholds, release threshold and simultaneous input. Switch triggers are digital.
- [ ] I04 Left three dots/Options→Minus, right three lines/Menu→Plus; Home and Capture,
  short/held behavior, system-reserved actions and built-in Minus+D-pad alternatives.
  Assistant/media inputs must not accidentally generate unrelated Switch buttons.
- [ ] I05 Both sticks: correct axes/inversion, all directions, diagonals, full range, centering,
  slow movement and rapid reversals. Check drift/deadzones/calibration and held-state cleanup.
- [ ] I06 Multi-button chords, both sticks/triggers together and sustained holds; no phantom
  input, dropped releases or periodic neutral replays overwriting real input after attachment.
- [ ] I07 Native button remapping, persistence/restart, supported stick calibration and controller
  settings; exercise production controller publication, not just a tester model of the mapping.
- [ ] I08 Real games plus HOME and a non-game system menu. Test applet/homebrew versus title
  contexts and entering/leaving HOME. Input Lab alone cannot certify system or gameplay latency.
- [ ] I09 Switch virtual-controller subcommands/report modes and initialization keep working;
  commands do not leak unsupported output to the physical controller or break subsequent input.

## P0: performance and coexistence

- [ ] P01 Compare the same system/game/display with MissionControl disabled; enabled with BLE
  disabled; BLE enabled with Stadia off; Stadia connected. Use a regular controller as control.
  Separate buttons from sticks and first movement from normal menu autorepeat timing.
- [ ] P02 Capture current RAM histogram summaries during an awake-only controller disconnect:
  slot-read IPC, delivery-lock wait, translation/injection, listener work, notification-return
  intervals, FIFO occupancy and write failures. Queue statistics include all HID producers.
  Gaps include idle; these do not measure radio arrival, pre-wake scheduling delay or FIFO age.
- [ ] P03 Establish an agreed user-visible latency budget before evaluating changes. Use repeated
  press-to-visible trials (e.g. phone video) and median/tail latency versus a regular controller.
  Histogram percentile bounds must not be presented as precise end-to-end latency.
- [ ] P04 No unexplained sustained menu/game lag, increasing backlog or repeated dropped writes.
  Compare logging on/off and final production build without bench instrumentation. Investigate
  listener priority and FIFO behavior only as hypotheses until measured; change one factor at a time.
- [ ] P05 Long session (target60minutes), idle then resume, continuous motion and rapid input:
  stable memory/CPU, no slot/client leaks, freezes, unexpected disconnects or log growth explosion.
- [ ] P06 Joy-Cons and at least one ordinary classic-Bluetooth controller: connect before/after
  Stadia, simultaneous play, input isolation, pairing another controller and reconnect after sleep.
  Check startup and configured recovery radio cycles do not leave other controllers unusable.
- [ ] P07 Mixed wired/handheld and wireless input, Wi-Fi enabled and ordinary SD activity.
  Keep diagnostic FTP/SD activity out of latency trials. Confirm BLE-off is a true behavioral no-op.
- [ ] P08 Obtain external Xbox BLE regression evidence for shared-host changes, including its
  15/16-byte report variants and L+R. No local hardware means this remains explicitly unvalidated.

## P0: automated safety and regression gates

- [ ] A01 Run `bash tools/test-stadia.sh` on the final proposed snapshot; retain ASan/UBSan,
  malformed-input/fuzz, concurrency, fixture framing and output-allowlist coverage.
- [ ] A02 Add behavioral tests around the PRODUCTION GATT discovery/cache implementation:
  prepare before pairing; fragments received during pairing; duplicate/replayed batches;
  overflow/incomplete discovery; pre-preparation/foreign/stale events; same-address reconnect;
  recycled connection IDs and generation boundaries. The extracted production algorithm now
  has portable fragmentation/filter/reset/bounds/overflow/rediscovery and duplicate-characterization
  tests. These do not prove firmware event ownership, deduplication or generation-safe callbacks.
- [ ] A03 Test the complete in-flight read lifecycle across timeout/disconnect/radio-off/shutdown,
  then a new same-connection/UUID request and an old callback. Helper tests do not prove integration.
- [ ] A04 Behaviorally test production mapping/publication/remapping, including the corrected
  central buttons, neutralization and isolation from stale/unbound endpoints.
- [ ] A05 Unknown devices/layouts fail closed. Strengthen identification/review legacy fallback:
  current `LegacyPolicy` can assume Xbox for advertisements without HID service, and report ID
  plus size alone is not a unique Stadia layout identity. Test unknown descriptors with coincident
  sizes, names and appearances. Preserve the upstream identification/safety contract explicitly.
- [ ] A06 Event withholding and ownership: no unexpected connection/registration/filter event
  reaches btm; no duplicate event-slot readers; no extra registration, unowned disconnect,
  invented permissions or guessed authentication arguments. Test relevant firmware gates.
- [ ] A07 Compiled defaults and missing/malformed config keep BLE/logging/radio-cycle/auto-connect
  off, with no diagnostic files or radio operations. Tests of the deployment script alone do
  not establish actual module defaults. Remove or clearly separate obsolete bench-only settings.
- [ ] A08 Pinned Switch target build and warnings review; compare NPDM/capabilities/service ACLs
  with baseline; verify no unexplained privilege, session-limit, firmware-setting or dependency change.

## Supported versus optional features: state the truth

- [ ] F01 Input and connection features above are either passed or explicitly outside an agreed
  initial experimental scope. Requiring fresh pairing every time must be prominent, not hidden.
- [ ] F02 Battery/charging: validate genuine readings before claiming support. If unavailable,
  document the fallback and avoid presenting an invented reading as real telemetry.
- [ ] F03 Rumble/LEDs: unsupported output report0x05 stays UNWRITTEN. Rumble is not a mandatory
  deliverable for this input-only PR. Correct existing documentation claiming Stadia has no
  rumble motors: unsupported transport output is not absence of hardware. Google's
  [controller troubleshooting](https://support.google.com/stadia/answer/9584135?hl=en-CA)
  describes the controller vibrating; this does not establish Bluetooth output semantics.
- [ ] F04 Motion, NFC/IR, audio/headset, assistant button, USB operation and wake-from-sleep:
  separately state unsupported/not-in-scope unless verified and deliberately implemented.
  Do not implement speculative output or promise native-controller feature parity.
- [ ] F05 One active/one remembered BLE controller versus multiple BLE controllers, address
  changes and controller variants: document the actual scope. Multiple BLE pads require a
  separate design/testing commitment, not a claim inferred from general Switch slot support.

## PR packaging and evidence

- [ ] R01 Agree the base/scope: Stadia adapter on the upstream BLE host versus importing the
  entire experimental host. Identify prerequisite PR/commits and review shared-host changes separately.
- [ ] R02 Minimal understandable diff, upstream conventions/licensing/attribution, complete new
  source/test files and fixtures included. Untracked working files are not a reproducible PR.
- [ ] R03 Separate diagnostic counters, Input Lab, deployment tools and historical investigation
  material from the production change unless explicitly useful to upstream. Final clean build
  must be tested; a diagnostic binary passing does not certify a different release binary.
- [ ] R04 Correct current README/config claims about reconnect/bonding, rumble hardware and
  descriptor-only identification. Consolidate contradictory/superseded bench instructions;
  publish one current setup/test/stop/recovery procedure and supported HOS/Atmosphere versions.
- [ ] R05 Record only actually tested firmware (currently HOS23.0.0) and obtain any claimed
  additional firmware/controller coverage. Attach a table of pass/open/unsupported with build
  IDs, configuration, exact steps and privacy-safe results; no keys or raw private database dumps.
- [ ] R06 Package only intended files; preserve user config. Verify clean installation, upgrade,
  disabling BLE, disabling the whole module and restoring the previous module/config. Durable
  backups precede destructive writes and installed artifacts require complete readback verification.
- [ ] R07 No commit, push, upstream contact or PR creation until separately requested.

## Recommended order and stop rules

1. Define reconnect scope and establish ordinary controller-only power-off versus console
   power-state behavior. Capture one successful awake-only timing session before tuning anything.
2. Resolve/reproduce bonding/reconnect and sleep/wake; add production-cache/read-lifecycle tests.
3. Complete the physical input/remapping and measured latency checklist, then coexistence/soak.
4. Produce the minimal production patch, correct documentation, build/test that exact snapshot,
   obtain missing Xbox/shared-host regression review, and assemble evidence for the proposed PR.

Plan repeated cycles only after one scenario succeeds. On the first failure, preserve the log
and stop. Do not turn validation into blind pairing retries or repeated global radio resets.
A credible experimental PR may document non-goals, but unresolved crashes, unsafe event routing,
stuck input, unexplained system-wide lag or regressions with BLE disabled are merge blockers.
