# BLE diagnostic regressions — staged instrumentation

## Latest occurrence: scan-only sidebar/power-menu slowdown (0.6.14)

After an actually verified fresh full reboot (scan/lease/worker all0, format0),
user-approved --inspect-control passed nativeUUID128 discovery and learned
format2. Complete-HID16 control then returned no managed candidate, despite3
driver HID advertisements and3 appended BTM-copy aliases. Scoped filter cleanup
was balanced1/1, active0 failed0 and scan_errors0. The batch stopped before the
incomplete-list stage. Connection lease, worker, cancel and connection-event
counters stayed0 throughout.

User reported mainHOME responsive but HOME sidebar and power menu slow. Read-only
0.6.15 status confirmed the same scan counters and unused connection lease.
Therefore the owned pending-cancel path is **not required** for every reported
slowdown. Shared native event consumption, scan teardown and transformed BTM
copies remain candidates, not proven causes. Existing128 plus appended128 AD
records may affect BTM parsing, but exact21.2.0 parsing is unverified; no confirmed
multi-entry-list bug exists (the29byte record payload rejects32byte lists).

0.6.16 read-only service timing returned MCcmd6=275us, MCcmd7=472us,
BTMserviceopen=358us, BTMGetState=281us/state6 Initialized. Its scan counters and
format had reset to0; intervening reboot/service reset and current symptoms were
not yet confirmed. Do not present these times as a demonstrated symptomatic
capture. User subsequently reported nRF noticing a new connection: preserve
log/time and identify whether it is a current native/other-client link. Our
read-only request list is not proof that no background connection/discovery
occurred. No unowned disconnect, retry or radio reset is justified.

## Diagnostic-only instrumentation and transport correction

First0.6.17/module deployment used MCcmd11 protocol1,224byte inline POD.
After user reboot, read-only baseline returned0xe801(kernelInvalidCombination)
for cmd11; no event snapshot was received. Pinned SF framing encodes264bytes
(8HIPC+16padding+16CMIF+224data), exceeding256byteTLS. Initial tests missed that
full envelope; this is a diagnostic bug, not evidence for a Bluetooth cause.

Local0.6.18/protocol2 correction uses exact224byte OutMapAliasBuffer; a40byte
reply envelope carries no inline status. Bounds/canary/roundtrip tests and
SDK-framing/source guards added; host suites and both ARM builds pass. Candidate
module181763bytes SHA531f07c6d9db8f51b1ff2532e0ba3d104130cb3552d85106893998c140cf131d
is now deployed and readback/protections verified; user activation reboot pending.
Prior181940byteSHA58b68 retained host+SD. See HANDOFF24 and archive
event-buffer-fix-20261019T203742Z. New transport still
requires real hardware validation; no new radio test until diagnostic works.

Metrics, unchanged by transport fix:
Core/HID/BLE phase (read/lock, processing, consumer-wait, idle), current phase age,
last event type, dispatch/read/wait-return counts, last/max completed wait, and
counts at the **existing** command65 hook (BTM/other and passthrough attempts).
Attempts are NOT completed connections or ownership. Consumers of cached/fake
events may read independently; counts are not guaranteed one-to-one.

Instrumentation is lock-free with bounded atomic snapshots; phase instability
is reported rather than hidden. No event wait/signal/native call is added,
removed or reordered. No raw request tuple, address, key, ARUID, handle or payload
is retained. Existing cmd6/7 ABIs unchanged. No new MITM command IDs.
--event-progress samples12 snapshots over about11seconds, pumping PLUS between
calls. Synchronous MC IPC itself is not timeout-bounded. --service-latency now
also compares progress before/after BTMquery. No artificial ack/timeouts/drop,
cleanup inference, callback routing change, connect retry or settings mutation.

## Earlier cancellation-associated incident (2026-10-19, probe 0.6.12)

One explicitly approved Android pending-cancellation trial captured one managed
BTM native command65 and issued command66 with the captured tuple. Cancel IPC
returned success, but no connected/disconnected terminal callback appeared in
the10second observation window. The ARUID-qualified managed handle list was empty.
The operator then reported system sluggishness/unresponsiveness and difficulty
reaching the reboot menu. After reboot the operator confirmed normal menus.
**The cause is not established; reboot recovery is not a safety validation.**

No rollback was performed or approved. At that earlier checkpoint the deployed
module was180610bytes,
SHA256 `19e46802c51dd2b2582532d12910ffa7c2c18fa745a620c111e574466b6d5e06`,
with the213byte INI unchanged. Live diagnostics were paused at that checkpoint;
later staged approvals and findings are described above and in HANDOFF.md.
See `HANDOFF.md` sections14 onward and the anonymous archive
`~/.cache/missioncontrol/stadia-backups/android-pending-cancel-unconfirmed-0.6.12-20261019T190310Z/`.

## Offline findings — structural evidence, NOT root-cause claims

1. **Mixed ownership layers:** `ConnectionProbe::Run` calls managed
   `btdevConnectToGattServer` (libnx forwards to BTMU cmd18 with ARUID/PID).
   `bluetooth_connection_probe.cpp::Worker` later calls raw driver command66.
   The captured tuple constrains the raw command, but there is no implemented
   managed cancellation/retirement handshake with BTM's pending state.
   Native IPC acceptance and absent managed connection handles do not close
   this architectural gap.
2. **Coupled event handling:** `bluetooth_events.cpp::EventHandlerThreadFunc`
   handles core, HID and BLE events on the same thread. BLE `HandleEvent`
   forwards to BTM then waits without a timeout for `g_data_read_event`.
   A blocked BTM consumer can therefore stall the shared event thread. This
   forwarding design predates the new connection test; we did not observe its
   runtime wait state, so it does not prove why menus were sluggish. Do not
   remove the wait, synthesize an acknowledgement or drop events speculatively.
3. **Synchronous lock/dispatch exposure:** one Btdrv MITM dispatch thread runs
   `ConnectGattServerProbe`, holding the operation mutex across native forwarding.
   The worker holds that same mutex across command66 IPC. This can couple service
   progress if either IPC blocks. However, command66 had already returned success
   in the captured trial; no sustained deadlock was demonstrated. The worker
   sleeps50ms each iteration, and its state mutex is not held across command66.
   There is no evidence sufficient to call this a CPU busy-loop or lock inversion.
4. **Owner teardown still unresolved:** cleanup reports failure after10seconds,
   then the NRO can close `mc` and call `btdevExit`/`btmuExit` despite retaining
   abstract lease state in the module. Closing the managed session is NOT
   documented cancellation. What native BTM does on this teardown is unverified.
5. **No callback guarantee in the inspected SDK:** libnx's command66 wrapper
   sends the tuple and returns an IPC result; it does not document a completion
   event. `ClientConnection` documents connected/disconnected statuses, not a
   pending-cancel completion contract. Waiting longer or treating IPC success
   as terminal would merely hide the unresolved contract.
6. **Public documentation caveat:** Switchbrew's `ConnectGattServer` section
   says the input direct flag/ARUID are unused internally and BSA direct=1.
   That description is not independently validated against our exact21.2.0
   executable. Capturing IPC fields alone must not be equated with verified
   lower-stack tuple semantics. No speculative flag variants will be tested.
   Reference: https://switchbrew.org/wiki/Bluetooth_Driver_services#ConnectGattServer
   and #CancelConnectGattServer. The public page documents no cancel callback
   guarantee. A separate BTM page-answer retrieval failed; it supplied no extra
   evidence and is not cited as if read.

## Current containment and operator authorization

User now explicitly permits purposeful scans/connections/helpful tests and
reboots, asks autonomous work with input only when needed, and offered FTP for a
diagnostic module update. Do not replay mixed-layer raw cancellation merely to
make progress; its native/managed retirement contract remains unknown.

Local0.6.16+ launcher blocks ALL radio-changing modes before preflight/upload;
Probe independently blocks scan initialization and ConnectionProbe independently
blocks all connection initialization/arm. Read-only status/service/event progress
remain permitted. Gates are temporary diagnostic discipline, not a claim the
user forbade tests. Only lift a relevant gate deliberately for a reviewed,
separable experiment after instrumentation/baseline. **These local gates do not
change the running module or guard older NROs.**

Module was reviewed/built/tested and deployed MODULE ONLY over user-enabled FTP.
Live SD module181940bytes SHA256
`58b68e85e08793bbba64f825093386c4d78b3e6c4027326aaea8eaa6d7590933`;
original module backed up host+SD, live readback and all93IPS filename/hash sets,
INI/boot/mitm/toolbox/template metadata verified before/after unchanged. Archive
`event-progress-module-20261019T201407Z/` under backup root; see HANDOFF22.
No rollback occurred. User activated this first diagnostic module by reboot;
its inline cmd11 then failed. Corrected protocol2 module has now been deployed
and all protections verified again; user activation reboot and actual buffered
cmd11 validation remain pending. See diagnostic transport correction above.
Ask user to activate by normal CFW EMUMMC reboot only after preserving the nRF
log. Verify read-only baseline before a single-variable test; no bundles that
hide the first failure. Host tests cannot certify native scheduling.

Prior code-export failures remain no permission to steal loader sessions, read
memory/NAND, alter Bluetooth globals or broaden exports. Exact native pending
retirement and callback semantics still need public-code/fixed-binary evidence;
no fabricated terminal state or longer wait can replace that contract.

---

# Historical experimental 0.4.0 runtime regression — distinct incident

The operator reported extremely slow/unresponsive power and connection side
menus, followed by boot trouble after the 0.4.0 deployment. Restoring standard
HATS MissionControl and disabling both experimental flags recovered full menu
responsiveness. No further console experiment is authorized by this recovery.

## Verified recovery

- Standard HATS `exefs.nsp`: 195041 bytes,
  SHA256 `2724d6e9cf82611591dcd6ef713fda24f8f44ea7e250cc5b868813d79aa199fb`.
- Experimental 0.4.0: 175929 bytes,
  SHA256 `7ac68257fa3b507e35d56f60900721996d3a55925c5467516faba419cb672fb6`.
- Both embedded build names identify base commit `71bbe2d` (stable has eight
  hash digits, local build seven); a different displayed version alone is not
  evidence of a different source base.
- All 93 installed IPS files and remaining baseline metadata/boot flag were
  hash-verified unchanged during restoration. No NAND or Bluetooth IPC operation
  was used for recovery. The operator confirmed responsiveness after restarting.
- The experimental binary/config and prior scan log were retained in verified
  local and SD backups before restoration.

## What is and is not known

Rollback establishes strong evidence against the experimental installation, not
which change caused the problem. The 0.4.0 probe was not sent by the assistant
before the complaint. Whether an earlier experimental build had similar latency
still needs operator confirmation. Do not claim the advertisement bridge itself,
its 128-byte status ABI, or stack overflow is the root cause without evidence.

Host ASan/UBSan tests and both pinned-toolchain builds passed; they do not model
Switch IPC scheduling, service forwarding, controller coexistence or boot safety.
The Btdrv dispatch thread has a 4096-byte stack. Disassembly shows a 400-byte
`BridgeBtmEvent` frame and a 48-byte caller frame; this alone does not establish
an overflow. Direct-call analysis omits indirect dispatch ancestors.

## Next work: offline only

Audit normal/default-off forwarding, global lifecycle interception, lock and event
acknowledgment order, and complete dispatch stack usage. Keep baseline/version
comparisons distinct from proof of causality. Any candidate fix needs a concrete
explanation and review before proposing hardware deployment. Do not request raw
fatal memory dumps: they can contain private Bluetooth/security data. Do not
resume GATT/pairing work or re-upload the current generated experimental binary.
