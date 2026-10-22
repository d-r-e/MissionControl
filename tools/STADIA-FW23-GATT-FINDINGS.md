# Firmware 23 GATT completion investigation

## OPERATOR DISABLED MISSIONCONTROL

Operator requests normal play because of input lag. MissionControl auto-start was reversibly
DISABLED over explicitly authorized FTP: only `flags/boot2.flag` under title
`010000000000bd00` was renamed to `boot2.flag.disabled-20261022T172914Z`.
Absence of the active flag and disabled-file readback were verified. Module and INI are
unchanged; other modules untouched. Original module, INI and flag were durably backed up
before modification in `disable-missioncontrol-20261022T172914Z/` under the bench cache.
A MANUAL REBOOT is still required to unload the currently running module. No remote reboot.
Do not re-enable, deploy experiments or run radio tests without a new operator request.
Latency research is deferred; no cause has been proved. Earlier installed-build notes below
are historical and do not mean MissionControl remains enabled for the next boot.

## Current installed update: corrected central buttons, input-delay investigation

Operator authorized both corrections and explicitly prioritized investigating joystick lag next.
Installed module240087bytes, SHA-256
`9a63fdfa72cbd7541f7fb0cbc4d3ce464c15c08b6c5febdc5798440c584db507`.
Input Lab1.0.1, SHA-256
`91757feb2de243ec6ab982cd46caa07a5eeef9e822c5870d488093c22341a7f5`.
Both full readbacks matched; INI unchanged; all three originals durably backed up before writes:
`receipt-menu-fix-20261022T171930Z/` in the bench cache. Full portable suite and pinned
module/NRO builds passed; NPDM unchanged. No remote reboot/launch/pairing command.

The physical left-three-dots button activated Plus in the tester. This was a production
bit-name reversal, not incorrect tester placement: Options is0x40 (Back/Minus), Menu is0x20
(Start/Plus), matching [SDL's Stadia parser](https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_stadia.c).
The initial timed-capture menu/options labels were corrected with explicit provenance;
raw captured bytes are unchanged. The normal semantic Plus/Minus controller mapping is retained.

The latest read-only log `ble-input-check-20261022T170959Z.log` (15856bytes,
SHA-256`fd8d876b7e60032f5d027cf19f17b5966dabe0ebc0fb207b7f4948c7e7eb46ec`)
also confirms a second successful pairing/presentation in the same boot, with the existing
configured recovery radio cycle between them. This is not persistent-bond or sleep/wake proof.

Normal input notifications are not SD-logged, and this log shows no report-error flood.
The tester did sample input before a potentially blocking framebuffer dequeue;1.0.1 moves
sampling after that wait. This removes one avoidable UI delay, not a demonstrated explanation
for all subjective stick lag. Actual transport/update cadence, processing and display latency
remain the immediate investigation priority. Do not speculate by changing radio parameters.

## Successful operator bench and read-only log retrieval

The operator confirms connection, L1+R1 slot claiming and working controls. Read-only FTP
retrieval saved `ble-success-20261022T165519Z.log`:8552bytes,
SHA-256 `1f452f878cd297493323e006c86ed0037cbdbe693e955019a05e5d8e2d2032d7`.
The installed module still matches `11812f27...`; the258-byte INI is unchanged.
`success-baseline-20261022T165519Z.json` records this baseline in the bench cache.
No console write, reboot, pairing request or key-file access accompanied retrieval.

Trace observations:
- Link established106.942s; bond requested106.961s, before explicit GATT/MTU work.
- All27 cache entries arrived during the bond phase and were retained.
- Core lifecycle event111.921s: about4.96s after the bond request, unlike the immediate
  event in the failing run. It remains a lifecycle observation, not independent encryption telemetry.
- Report Map read113.627s completed successfully113.773s:182bytes, result0.
- HID Information read succeeded with4bytes; CCCD enable completed with result0.
- Parsed input report0x03 is10bytes; output0x05 is4bytes but remains unsupported/unwritten.
- Stadia presented114.178s, about7.24s after link establishment.
- No sleep transition is logged before pairing in this boot. Earlier failed boots had one;
  therefore this run does not isolate setup ordering from every other lifecycle difference.

The log later records disconnect reason0x0008 at186.620s, about72.44s after presentation,
then the existing configured recovery radio cycle. The reason is a connection timeout;
it does not distinguish intentional controller power-off from an unexpected link loss.
Ask the operator whether they switched the controller off before FTP; do not count this
as validated reconnect behavior or silently conclude the controller remained stable indefinitely.
The deployed bundle's successful setup/input is confirmed, not a uniquely proven SMP root cause.

## Previous checkpoint: security-first candidate installed

Operator-authorized module-only FTP deployment completed; no remote reboot or pairing command.
Candidate `ble-security-first-405d18d-dirty-11812f27.nsp`:240083bytes,
SHA-256 `11812f279b936c06dc9e4d20d8cdb3399db52478cf2f0cd383208d85b191c58f`.
Full portable suite and pinned Switch build passed; NPDM unchanged; independent focused review
found no definite blocker. Actual connection benefit remains unverified.

Both original module and INI were durably backed up and checked before writing. Full module
readback is byte-identical; INI unchanged. Receipt under the bench cache:
`receipt-security-first-20261022T165024Z/receipt.json`. Host clock supplied that timestamp;
the existing cache directory remains `~/.cache/missioncontrol/ble-bench-20261021/`.
Source/test/build provenance is in `ble-security-first-405d18d-dirty-11812f27-provenance/`.

Next: operator reboot with Stadia off, keep console awake, pair once from Change Grip/Order,
wait60seconds after white even if it returns to pairing, then switch Stadia off and report.
Use L1+R1 (shoulders) only if the controller reaches presentation; they cannot bypass setup.
Earlier sections below retain the investigation chronology and superseded installed states.

## Scope and current outcome

Offline inspection only. No FTP operation, console write, radio command, reboot, or pairing attempt was performed during this investigation. The bounded diagnostic currently installed remains unchanged. The failure is **not yet explained**.

Target: the already-approved Bluetooth driver Program NCA ExeFS `main` export, decompressed as `btdrv-loadimage-0x80000000.bin` (1,444,096 bytes; SHA-256 `4819eb58c248543cf2a1d991a8245997ae60e1726acb8ef97ffdb3f44c4e218a`). Ghidra 12.1.4 via REA, explicit raw ARM64 mapping at `0x80000000`; selected original bytes independently disassembled with Capstone. Names below are analysis addresses, not recovered symbols. Static paths do not prove runtime execution. Raw pointer-table values are image-relative and require relocation; writable runtime slots are not recovered state.

Source baseline: `405d18d6314973044c258770ab4139e182925ea9` plus the existing uncommitted bounded GATT diagnostic. The initial offline trace made no sysmodule edits. A subsequent passive-ownership diagnostic has now been implemented locally; validation and deployment status are tracked separately.

## Observations

### Existing bench log

`ble-gatt-followup-20261021T200914Z.log` records:

- LE-HID event registration accepted at 37.383 seconds.
- HID service `0x1812`, Report Map `0x2a4b` at handle `0x0048`, and 27 total cached attributes.
- A core lifecycle event during the bond request; this is not proof of encryption.
- HID data-path registration accepted at 59.071 seconds.
- Report Map read IPC accepted at 59.081 seconds.
- At 69.102 seconds, no observed matching completion, radio on, and no recorded disconnection.
- Setup stops; the following disconnect has reason `0x0016` after the host abandons setup. This does not demonstrate that security negotiation disconnected the link.

The trace does not establish successful ATT completion, encryption, or a valid Report Map. It also does not establish that another process consumed an event.

### Firmware queue and slot behavior

- `0x8002b590` initializes named channels including `BLE_CORE` and `BLE_HID`, and starts the message handler at `0x8002bcd0`.
- `0x8000aba0` registers UUID/owner records; `0x8000ad70` looks them up. `0x8000aa30` compares UUIDs by their 2-, 4-, or 16-byte representation. Registration entry points at `0x80008e30`, `0x80008e90`, and `0x80008ef0` select owners 4, 5, and 3 respectively. Exact IPC-to-entry binding must remain separate from these observed functions.
- At `0x80017e40`–`0x80017e64`, a callback path looks up ownership using the UUID at `x21 + 0xc`. On successful lookup it emits internal event `0x22`, size `0x24c`, through `0x8002bb20`. The enclosing function was not recognized by Ghidra; its complete callback classification and all callers remain unestablished.
- `0x8002bb20` accepts owners 2–5 and selects their queue through the pointer table at `0x8015aef8`. It also checks queue state and capacity; it can refuse emission. Static inspection does not establish that those conditions caused this bench failure.
- That table's original file values are `0x1a1720`, `0x1a3e70`, `0x1a65c0`, and `0x1a8d10`. Owner 5 therefore selects the queue referenced by the handler as `0x801a8d10` after image-base relocation.
- In `0x8002bcd0`, the `0x801a8d10` queue's internal event `0x22` becomes public event **8**, with a `0x24c`-byte payload in the slot at `0x801b0d08`; pending size is at `0x801b0d00`, type at `0x801b1108`. This agrees with libnx `BtdrvBleEventType_ClientNotify = 8`. The managed queue also has a `0x22` → 8 conversion.
- Reader `0x8002ba10` locks this slot, reads pending size, clears it, copies type/payload when nonempty, unlocks, and notifies the handler. The next reader cannot retrieve that same slot contents. The wrapper at `0x80014d90` calls this reader and returns success; success alone is not proof that a nonempty slot was copied. The recognized direct-caller graph is incomplete.

These observations support the existing single-consumer ownership concern. They do **not** show a firmware-23 event-number or payload-size change requiring an ABI patch.

REA record for the shared emitter: `ev_f51e0921f41958c84ea512da0198075e6fba650889c280318b6a605f0a0e5ee2`. The handler decompilation and selected original-byte disassembly were also inspected; incomplete procedure recognition limits call-graph conclusions.

### Source ownership workaround

`BtdrvMitmService::ShouldMitm()` selects HID and BTM clients. Command 97 (`RegisterBleHidEvent`) is intercepted from firmware 5.1.0 onward. When BLE is enabled, it supplies an unsignalled placeholder instead of the real event. MissionControl's own service client is not selected by that predicate.

Configuration is loaded by `init::Startup()` before `Main()` launches the modules. There is no source evidence here that BLE is enabled only after initial system registration. Nevertheless, neither source inspection nor successful registration by MissionControl proves that the system actually received the placeholder on this boot.

The placeholder path currently has no ownership telemetry. Logging only inside that path would be insufficient: a call before BLE log initialization would be silently omitted. Command 96 (`GetLeHidEventInfo`) is currently forwarded without visibility. A client polling that command would not be stopped by withholding its event handle.

## Remaining hypotheses and limits

1. Security negotiation or the underlying read never produces the expected completion.
2. A completion is routed or queued differently, or fails an internal admission condition.
3. Another client consumes the public single slot, or the host listener does not receive/process its wake.

The existing evidence cannot choose among these. Numeric `0x2a671` remains unmapped here; do not call it insufficient authentication based only on local naming. Do not change authentication arguments, UUID representation, event withholding, or radio behavior on that basis.

## Passive ownership diagnostic (subsequently installed and tested)

Use metadata-only counters, active only with experimental BLE enabled:

- Count command-97 placeholder registrations, separated by HID/BTM client. Capture before logging starts, without SD writes in the IPC callback. Disabled BLE/logging records nothing; forwarded registrations on that disabled path are intentionally not counted.
- Observe command-96 calls from those clients while preserving the existing forwarding behavior; count calls only, never add another slot read or copy payloads.
- Count host-listener start/registration, wakes, read returns, and event types; include a snapshot at the first GATT timeout. For a completion, log only validated connection/result/type/UUID length/value-size metadata, not report payloads or key material.
- Preserve one outstanding GATT request, the first-failure latch, current authentication arguments, and existing event-withholding rules. Do not add retries, radio cycles, or Stadia output writes.

This can test whether placeholder interception occurred, whether system clients still requested the slot, and whether the listener woke. Calls alone would not prove that a client stole this specific completion; zero observed reads would not prove successful security. If ownership is excluded, further security evidence will still be needed.

The local implementation passes `bash tools/test-stadia.sh` (including atomic-counter tests and five source contracts), `bash tools/build-switch.sh`, and independent read-only source review. Source contracts do not demonstrate runtime IPC delivery. The subsequent approved installation and operator test are recorded below. Its counters are unsigned 32-bit boot totals (wrapping modulo 2^32), and do not observe clients outside the existing HID/BTM selection. Deployment and a single bounded operator test require separate readiness confirmation, verified module+INI backups, and readback. For now: keep Stadia off; do not reboot or retry pairing.

## Subsequent approved installation and operator test

Installed artifact: `ble-ownership-405d18d-dirty-144d06f5.nsp`, 239,757 bytes,
SHA-256 `144d06f5acfb9dc68e5897e80bc2950f3f6a05cbc79d6c355e2ac71158baef55`.
Main/NPDM only; NPDM identical to the prior installed diagnostic.
Receipt: `receipt-ownership-20261021T203554Z/receipt.json` under the bench cache.
The operator explicitly approved installation; both originals were durably backed up before the
module-only write, full readback matched, and INI remained unchanged. No reboot or radio command
was sent remotely.

After the operator reported the same white-blink/return-to-pairing symptom, read-only FTP retrieval
saved `ble-ownership-20261021T204107Z.log` (6,961 bytes;
SHA-256 `15ceb88fd8d7f0bfc3c63928ec2f618e0716502506e7c5037126912e8fca4d4a`).
All cache paths are under `~/.cache/missioncontrol/ble-bench-20261021/`.

At read-begin (112.538 s) and read-incomplete (122.587 s), counters were unchanged:

- HID placeholder registrations: **1**; BTM placeholder registrations: **0**.
- Observed HID and BTM slot-read calls: **0** each.
- Listener starts: **1**, registered: **1**, failed registration: **0**, wakes: **0**.
- Listener read returns/failures and managed notification count: **0**.

The Report Map request at 112.564 s was accepted. At 122.617 s the timeout still reported radio on
and no disconnection. Setup latched off; the disconnect at 122.639 s followed abandonment of setup
(reason `0x0016`). Therefore the observed return to pairing is consistent with our bounded teardown;
it is not, by itself, evidence of a controller-side security failure.

These observations weaken the specific HID/BTM competing-reader hypothesis for this run and show
that the placeholder interception occurred. They do not exclude other clients, routing/admission
failure, a stale event registration, or security negotiation that never produces a completion.

**Comparison caveat:** this boot records sleep at 33.589 s, radio unavailable at 38.695 s, listener
registration at 38.701 s, and radio available again at 84.995 s, before the connection. It is not a
clean no-sleep first-attempt sample. Earlier failed logs did not require this sequence, so sleep
cannot be declared the common cause. Next offline work should inspect event-registration lifetime
across system radio lifecycle alongside security/read callback generation. Do not introduce radio
cycling or request another pairing attempt merely to erase this caveat.

## Further offline checks and next bounded diagnostic

A read-only source comparison against the saved upstream host found no concrete client-registration,
ordering, UUID/instance, read-ABI, or listener-attachment mismatch. This excludes no firmware-internal
prerequisite. Review record: agent `6e8e3edb-7065-421` (retained local output); no console operations.

Selected native facts from the same approved image:
- At `0x8002ba00`, the event accessor selects global event object `0x801b0cc8` and branches through
  `0x800393d0` to `0x8003a080`; the latter reads its handle field at offset `0x14`. It does not
  create a fresh event on registration. This does not establish every lifetime path across sleep.
- The initializer `0x8002ea90` constructs that event. Incomplete direct/data reference recognition
  prevents treating the recognized reference set as an exhaustive lifecycle audit.
- The result table at `0x80119614` maps raw status `0x34` (52) to `0x2a671`, through lookup
  `0x80017f70`. This strengthens the numeric mapping only: the semantic name of raw52 is still
  unverified. Do not infer insufficient authentication from the existing local result name.
- The lower request wrapper at `0x80047cc0` returns the received16-bit status on successful transport.
  Its assembly is important: decompilation alone elided the response's effect. Transport acceptance
  and ATT/security completion remain distinct.

New evidence supporting an observation-only change: the existing read deadline is10seconds, whereas
Bluetooth specifies30-second SMP and ATT timeout rules. See the primary-source references in
`STADIA-BENCH-TEST.md`. Those timers may start/reset during the procedure; they are not proof of an
actual security stall or a global30-second setup deadline for BSA1.6.

The next local patch raises **only the existing single-read observation window to35seconds**,
retains all disconnect/radio-off/shutdown guards and the first-failure latch, and logs `wait_ms=35000`.
Portable tests cover the real deadline helper at10/30/34.999/35seconds and acceptance of a delayed
answer on the same outstanding request. It adds no read, retry, authentication argument, event-handle
replacement, radio command, or output write. Full portable tests and the pinned Switch build pass.
The small delta received direct source review; the expired previous independent review session could
not be resumed, so no independent review of this delta is claimed. It is not installed or a confirmed
fix. After explicit operator approval it was installed with fresh verified backups and byte-identical
module readback; INI was unchanged. Artifact: `ble-window35-405d18d-dirty-ce02c05f.nsp`,239714bytes,
SHA-256 `ce02c05f2496f761ef4ead6ff57694d9f9472c182e6a2f96027685132a7fea7f`.
Receipt: `receipt-window35-20261021T210254Z/receipt.json` under the bench cache. No reboot/radio/pairing
command was sent remotely. The controlled operator test must keep the console awake and allow60seconds
after the logo becomes white, rather than abort at the earlier20-second instruction.

## Extended-window operator result: a delayed error, not a lost completion

Read-only FTP retrieval saved `ble-window35-20261021T210735Z.log`,6779bytes,
SHA-256 `734c0ad889737cfe5857b148187e5d820e8afc77a9f7fc3fdb289aae5c5f4a43`.

The Report Map request was accepted at63.623s with `wait_ms=35000`. At90.830s the
LE-HID listener received its completion; the correlated read at90.839s had
**result26, size0, valid0**. Thus this build did not hit its35s deadline: it stopped on a
real failure response roughly27.2s after the request, about30.1s after connection.
The subsequent disconnect at90.870s was again the host's bounded teardown (reason0x0016).

Event registration/routing and listener delivery demonstrably worked for this response.
The earlier10s timeout obscured the delayed outcome; it was not proof the completion could
never arrive. The run again logged sleep/radio-off before listener registration and wake,
so it cannot establish a clean no-sleep sequence, but that sequence did not prevent delivery.

Native original-byte inspection of converter `0x80019690`:
- It dispatches16-bit input statuses up to0x12e through byte table `0x801194e0`.
- Entry0x85 targets `0x80019898`, which returns0x1a (26).
- By contrast, raw0x89 targets the separate block returning9.
- These are driver callback results, not HCI reason0x1a or Horizon Result0x1a.
- Public Broadcom-derived [Android4.4 gatt_api.h](https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/android-4.4_r1/stack/include/gatt_api.h)
  names0x85 `GATT_ERROR` and0x89 `GATT_AUTH_FAIL`. This is corroborating status-family evidence,
  not proof of the exact Nintendo stack source or of why the generic error occurred.

**Current conclusion:** the blocker is a delayed failed GATT read with no map data.
The timing suggests a lower-layer procedure/timer, but neither timing nor the generic
status proves an SMP/authentication failure. No additional pairing attempt is requested.

The operator's L+R question also exposed a separate source bug: Stadia's parsed L1/R1
already maps to SwitchL/R, but the parked-controller reattachment predicate still reads
Xbox byte13. A10-byte Stadia payload can never satisfy it. This affects later controller-slot
reattachment, not the current pre-presentation Report Map failure. The local correction passes
portable tests and the pinned Switch build.

## Candidate: start security before explicit GATT/MTU setup

The operator now explicitly requests an implemented potential fix and direct verified deployment,
with notification only when ready for a manual reboot. This is authorization to deploy a bounded
candidate, not a finding that the root cause has been proved.

Independent public-source research found a concrete ordering difference in a Stadia-capable host:
[Bluepad32, pinned commit80d6314e46f871719fbb4b721a2cc02227c4f369](https://github.com/ricardoquesada/bluepad32/blob/80d6314e46f871719fbb4b721a2cc02227c4f369/src/components/bluepad32/bt/uni_bt_le.c#L676-L837)
starts pairing at LE link completion (790–794), then starts device-information work only after
successful pairing/re-encryption (676–755). The same file (992–997) records Stadia working with
legacy Just Works on another stack. Thus Stadia does not universally require Secure Connections;
this does not establish Nintendo BSA compatibility.

The Xbox-derived port instead requested explicit discovery/MTU work before initiating the bond.
The candidate moves the existing, single CreateBond request ahead of that work. It first prepares
a fresh passive attribute cache, so automatic discovery records drained during Pair are retained
for this connection and stale records from the preceding link are not reused. Existing GATT
authentication arguments, MTU value, event withholding,35s single-read bound and first-failure
latch remain unchanged. It includes the independently tested L+R correction.

**Limits:** this is a sequencing/race candidate, not a demonstrated fix. Nintendo's automatic
discovery can still occur independently. The port still lacks an exposed, independently verified
encryption-complete event: its lifecycle heuristic is not equivalent to Bluepad32's security-success
gate. Successful authenticated Report Map retrieval and device-reported identification remain
mandatory before presentation. No unauthenticated fallback, guessed decoder, bond reset beyond
the existing request, output0x05 write, additional retry, or recovery radio cycle is added.

Native inspection also found preferred-parameter event emission in callback0x8002fc60 and core
connection synthesis in0x8002da50; these do not prove a missing application response or justify
inventing radio parameters. Public pairing captures were not downloaded or inspected, because
their HCI/SMP contents may include key material. No private build or upstream contact was used.
