# Stadia BLE probe — foreground diagnostics

> **Legacy scan/leased-cancel modes remain paused.** Dedicated managed,
> discovery-request and passive-trace modes have separate gates; their presence
> is not permission to run them. See the current [handoff](../../HANDOFF.md)
> before any hardware action. Sidebar/power-menu latency was also observed with
> a read-only probe and cleared on PLUS exit; its cause remains unproven.

## Stage-specific native code export (0.6.33)

The existing `code` / `code-fs` operations remain limited to `/btdrv` and `/btm`,
`/main` only, read-only, at most16MiB. No automatic route fallback, new app,
permissions, key access, raw storage or Bluetooth operation. Availability of either
native code-filesystem route is still unproven after the prior failures.

The loader route now owns one normally acquired `fsp-ldr` session and separates
service acquisition, domain conversion, PID registration and code-filesystem open.
Requests match pinned libnx `fsldr.c` for HOS20+: zero placeholder + send PID on
cmd2; 16-byte attributes/storage/program request and map-alias verification output
on cmd0. The parent session stays alive through all domain-child file operations;
cleanup order is file, filesystem, parent. The `/main` argument is now a zero-
padded full `FS_MAX_PATH` array: libnx advertises all769bytes without padding a
caller string. A new full-range ASan mock first reproduced the old six-byte
literal over-read, then passed with the buffer fix. Whether that bug explains a
prior native failure is unknown without stage evidence. Normal-FS mode identifies
NCM/database/key/content/storage/path and filesystem-open failures without changing
mount parameters or trying another route.

Failure replies extend the existing32-byte detail field without changing request,
header size or success framing: raw IPC at0, `MCEX` at4, version1 at8, route at12,
stage at16, reserved zero bytes20..31. Only bounded numeric stages are returned;
no paths, peer/program identifiers, signatures, keys or executable data. Old raw-
only failures remain readable as stage-unknown. Host `CodeExportError` exposes
status/IPC/route/stage and rejects malformed detail. No firmware/HCI interpretation
is assigned to those raw values.

Cancellation is checked before opening and during both passes. Before the success
header, failures receive one diagnostic response. After success, interrupted reads
or sends terminate the stream rather than inserting an error frame into NSO bytes.
The host checks the complete body hash before creating an exclusive output file.

Failure-injection tests exercise every fallible loader/NCM/file stage, identity and
path validation, parent lifetime and exactly-once cleanup, size bounds, bad magic,
short reads, cancellation, reply/send failure and second-pass read failure. Fake-
socket tests cover every stage/route, legacy and malformed replies, truncated or
mutated streams and absence of partial output. Host mocks do not certify native
service availability. A runtime trial diagnoses the access blocker; it is not a
promise that export will succeed. No module update is needed for this NRO.

## Shared native discovery field policy (0.6.32; offline only)

The Stadia `FreshTarget` predicate still required native `device_type`1/2 after
0.6.30 fixed only Android control validation. It now validates the same native u8
field width as the proven control path; `FreshNativeControl` delegates to that
common predicate while retaining its exact-address/known-UUID query requirement.
Protocol, exact-target matching, freshness, successful scan status, address type,
record count, ownership and scoped scan cleanup gates are unchanged.

A regression first failed on native type0, then passed with the shared fix. Tests
cover type0/1/2/99/255, reject256, and exercise both actual Stadia match/connect
gates plus stale, wrong-target, error, cancellation and teardown failures. This
is probe-side predicate use only; no new driver operation or module deployment
is needed. It does NOT explain the direct callback failure, and the earlier
Stadia native HID scans had zero advertisements: do not relabel those negatives
as observed targets or request an unchanged hardware retry on this basis.

## Settled Stadia / native driver return (0.6.31)

Critical-path test, not another discovery/helper app. `--trace-managed-stadia`
now requires the exact NativeReturn module ID and trace protocol2 (108-byte
map-alias status; Begin32B unchanged). After SDK initialization and empty owned
connection list, wait12seconds **without app-issued scan/GATT/connect actions**,
then require release/fresh second A. This compares the successful control's
preamble delay; it is NOT a documented readiness requirement or discovery proof.
Both Mac and Android Bluetooth must be OFF; Stadia orange-flashing at the gates.

The native observer records actual command65 IPC result separately from managed
acceptance and raw callback result, plus first/last target/client-correlated
callback delay from native submission. Only the one private-ticketed unreserved
request uses the existing exact16-byte MITM forward wrapper to capture its return;
all other unreserved requests retain generic forwarding, legacy guards unchanged.
Exactly one downstream forward per path; no new native action or cancellation.
Zero/foreign/late tickets cannot update/tick the trace; duplicate return or clock
regression fails closed. Local retire never means native cleanup.

Only unique ARUID-owned Stadia handle AND agreeing native connected observation
with successful driver IPC permits a bounded managed-cache HID catalogue in the
same attempt. It reports counts/range-owner consistency, Report Map/Report and
2908/CCCD presence; no IDs, arbitrary UUIDs or values. No characteristic reads,
subscriptions, pairing or proven callback-instance mapping. Android control
trace remains cache-free. Owned cleanup/quarantine rules stay independent.

Tests include actual inert helper with fresh-A/held-A/timeout/abort, driver return
before/after callback, foreign/expired/duplicate return, relative timings and
strict consumer acceptance. Neither models nor timing alone prove radio history
or the namespace/cause of prior0x1a. Another native trace needs a fresh boot.

## Native control byte-field policy (0.6.30)

A live qualified control advertisement had fresh sequence, exact address/UUID
match, successful native scan status, address_type1 and three AD records, but
raw device_type0. The old classified1/2 assumption rejected it, despite the SDK's nominal
0=BR/EDR annotation. The observed native BLE scan/link contradicts using that
classification as a gate here. NRO-only `FreshNativeControl` validates its u8
transport width instead of relying on that annotation. It still requires a valid **qualified** query, exact
native match, fresh sequence, event6/result0/status2, valid address-type range
and bounded nonempty records. No metadata is forged or written back to radio.
The generic historical classified-discovery helper is unchanged.

Positive native-type0 fixtures and negative checks for every remaining gate are
covered, along with real-helper selection/confirmation and unchanged-target
rejection models. This is not a working managed connection or HOGP claim.

## Native cache gate diagnostics (0.6.29)

NRO-only addition, same a72 module and unchanged connection gate. When an SDK
candidate lacks qualified freshness, report scalar cache sequence/event/status,
address-match counts, same-snapshot counts and metadata with validity/source
labels. Additional unqualified and reversed-order comparisons are **read-only MC
queries**: they never change the selected address, original query, freshness or
radio arguments. No addresses, native IDs, names or payloads are printed.

The model includes working release/fresh-A confirmation and a valid-metadata
Ready control; rejected metadata must return NoTarget before confirmation. A
forced-freshness mutation is rejected by this regression. This is not SDK proof.

## Canonical control continuation and event reuse (0.6.28)

Probe-only update; uses the same a72/scan6/cache3 module. A scan event is acquired
once per SDK session, reused across scan phases and closed once on every return.
The earlier0.6.27 run stopped its little-endian scan cleanly, then failed a second
event acquisition0x6159 before any canonical scan or connection request.

`--trace-managed-control-canonical` runs ONLY the previously unreached canonical
UUID128 scan (12seconds), with the same loaded-code, fresh exact native-control,
unique RAM candidate and cleanup gates. A second fresh A is still mandatory for
one managed connection. It never falls back to or repeats the little-endian scan.
The default `--trace-managed-control` retains both phases with corrected event
ownership. SDK-model regressions cover one acquisition/two phases, canonical-only
selection, all-path closure and stop/error guards; these are not hardware proof.

## Android managed positive control (introduced in 0.6.27; matching module required)

`--trace-managed-control` is a dedicated diagnostic, not an override for paused
legacy modes. Requires scan protocol6, cache protocol3 and the exact reviewed
module build ID pinned in this probe. The probe verifies actual loaded metadata,
clean state and all required capabilities **before Bluetooth initialization**.

Setup: **Stadia OFF, Mac Bluetooth OFF, Android ON**, one connectable nRF Connect
advertiser with `1812` and `9f6dca74-69b7-4a35-9f01-9ab841ab5832` in primary data,
empty scan response. Do not manually connect, pair or accept pairing dialogs.
The launcher checks Mac state and never reads the saved Stadia target in this
mode. No advertiser addresses or payloads are printed or persisted.

1. Fresh A: automatic loaded-code/readiness gate, then a native UUID128 scan for
   at most12seconds. If there is no candidate, stop and verify exact tracked
   add/remove1/1 before trying the alternate UUID byte order for another12seconds.
   These are scoped documented UUID128 conditions, forwarded unchanged; no
   aliases, scan-mode guesses or global filter clears.
2. SDK scan rows supply only their documented address field into RAM. Require
   one nonrotating candidate over the whole window and a fresh exact-address
   native-cache match containing the known control UUID128. Missing evidence,
   malformed/full batches, multiple peers or uncertain cleanup forbid connect.
3. Only after verified scan stop, release A then press fresh A for **one** shared
   managed connection with the passive observer. No GATT cache/value access,
   subscription, output write, lease arm or raw cancellation occurs.
4. Reuse the same current-ARUID managed-list ownership checks and one-disconnect
   cleanup as the Stadia trace. Pending/unconfirmed state still holds for reboot;
   observer retirement is never link cleanup. No candidate means no connect;
   a bounded negative is inconclusive.

A successful control establishes this managed path for a fresh native-discovered
Android peer, not Stadia compatibility. Discovery/cache state, peer stack and
registration settling time differ from the earlier direct Stadia trial.

## Passive connection tracing (introduced in 0.6.26)

`--trace-managed-stadia` is a new opt-in **connection-only** mode. It requires
commands 14–16 in the matching module; older modules fail capability checking
before `btdevInitialize` or a connect request. The first live trial captured a matching nonzero-result callback but no managed
link; see the current handoff. Do not run this mode as part of a build or offline
test.

For a separately approved future run, both **Mac and Android Bluetooth must be
off**. Android is now bonded to Stadia and could reconnect automatically. The
launcher verifies Mac state; Android state is an operator prerequisite. A fresh
physical A authorizes one standard managed direct connection to the saved target.
There is no scan, armed lease, cancellation worker, app GATT-cache read, explicit
pairing, output write or subscription. Native background setup/security is not
excluded.

The observer binds one target/app ARUID to the actual native client and direct
flag, then records matching connection-event scalars. A conflicting request,
client/connection-ID mismatch or clock regression makes observation ambiguous.
It is one-shot per boot, with at most 45 seconds of collection; event/status calls
check expiry. Token-zero capability reads expose only availability/consumption,
not prior results. Private bindings are cleared when observation ends.

- Cmd14: 32-byte Begin request; target, ARUID and token remain private.
- Cmd15: token input and an exact **88-byte mapalias** status, never an inline
  reply. Status contains no addresses, tokens, ARUIDs or native/client IDs.
- Cmd16: retire observation locally. This does **not** cancel, disconnect or
  establish native cleanup.

A unique target handle in this ARUID's managed list and an unambiguous successful
native callback are separate evidence. Observer output never authorizes adopting
or disconnecting a native ID. Managed cleanup rechecks the owned handle, submits
one SDK disconnect, and requires change notification plus handle absence.
Pending/unconfirmed connection cleanup still holds the session for a reboot,
regardless of observer retirement. Raw callback results are not decoded as HCI
errors or proof of bonding/encryption.

## Dedicated Stadia connection comparison (0.6.22; new module required)

`--managed-stadia` performs one direct standard managed connection to the private
saved target. `--discover-stadia` first requests a native scoped complete HID UUID16
scan through a separate exact private marker. It does NOT use the HID alias bridge.
Flags05 is an AD type, NOT a documented filter condition: the v21 approach failed
and is retired. Requires the new module's MCcmd12 cached-scan protocol2 (also
certifies corrected native-only filter support); P1 modules fail closed
BEFORE any scan. The host launcher requires Mac BluetoothOFF and injects the
saved target inside the container without printing it or persisting it in Docker
command metadata. Fresh physical A authorizes the chosen mode.

```sh
bash tools/netload-stadia.sh SWITCH_IP --discover-stadia
```

Discovery observes at most30seconds. Non-consuming MCcmd12 compares the current
native BLE event with the exact saved target; exposes no address/name/payload.
A fresh target sequence+valid native device/address metadata is mandatory. Cache
replacement can miss a target (inconclusive negative), not justify assuming it.
Then stop once and verify exact private filter add/remove1/1, active0/failed0,
no scan errors and unchanged bridge counters. No fresh target => NO CONNECT;
uncertain cleanup => hold session and request normal fullCFWEMUMMC reboot.

Only successful evidence permits the SAME one standard managed connection as
the direct mode, no address-type injection or global radio/filter writes. Unique
ARUID-owned target in the managed list gates cached HID metadata queries; no
pairing API, value writes or subscriptions yet. Revalidate owned handle before
one managed disconnect. Unconfirmed connect/cleanup holds the SDK session and
requests reboot; PLUS cannot claim pending retirement. No raw cancellation.

Host tests and ARM builds pass. The new filter is byte-identical to a previously
accepted native UUID16 condition, but exact-target discovery and the corrected
comparison are NOT yet hardware validated. Baseline/post-stop counters are logged
even if scan start fails. No target observation => no connect; no blind retry. See `STADIA-CONNECTION-FINDINGS.md` (in tools/) and HANDOFF
for candidate/deployment state and raw native error0x1a's unresolved namespace.

## Read-only diagnostics (0.6.20+)

```sh
bash tools/netload-stadia.sh SWITCH_IP --status-only
bash tools/netload-stadia.sh SWITCH_IP --service-latency
bash tools/netload-stadia.sh SWITCH_IP --event-progress
```

Each netloader upload needs a fresh waiting Y session and a fresh physical A.
No advertiser or Mac radio preflight is needed. These modes install nothing.
Status-only reads MC state; service-latency also opens the documented BTM
service and queries GetState(cmd0), with BEGIN and elapsed time around each
synchronous request. No adapter initialization, scan, connect, cancel, pairing
or application GATT command is issued; native/foreign background activity is
not excluded. A blocked service call itself has no guaranteed timeout.

Event progress requires matching **diagnostic module cmd11 protocol3**, with
an exact224byte OutMapAliasBuffer. Version1's inline payload encoded264bytes,
exceeding the256byte TLS buffer and failed0xe801; do not reuse that transport.
Protocol3 retains the same size and adds the last global native connection
result/status/reason. The buffered reply has a40byte envelope. Errors print module/description and
are NOT assumed to mean a missing command; no fallback is attempted. It samples12
identity-free MC snapshots about one second apart, pumping PLUS between calls.
Core/HID/BLE lanes show read/lock, processing, consumer-wait or idle; phase age,
last type, dispatch/read/wait counts and last/max completed wait durations.
Connection-hook attempts count the existing command65 path, even unarmed; zero
is not proof of no other-client/lower-stack activity. No tuple/address/ARUID/
handle/payload/key is retained. Reads can be cached/fake, so counts need not pair
one-to-one; aggregate counters are individually atomic, not a transaction.
Unstable phase snapshots invalidate age/type correlation. The instrumented
module preserves all existing native calls, signals and waits in order; no
fake acknowledgements, dropped callbacks, timeout fixes or extra MITM IDs.

Candidate module deployment is MODULE ONLY, separately authorized over FTP,
with original module backups, exact INI unchanged and93 protected IPS filename/
hash sets plus boot/mitm/toolbox/template metadata verified. See HANDOFF for
candidate hashes and whether deployment actually occurred.

## Scan-only inspection (0.6.14)

With separate operator approval, `--inspect-control` reads status, then runs
native diagnostic UUID128 calibration and complete/incomplete HID16 scoped
controls in sequence. Android advertises both diagnostic UUID128 and HID16 in
primary data throughout; Mac Bluetooth must be OFF. No address selection or
stored-target use, lease arm, connect, pending cancel, pairing or application
GATT operation occurs. One fresh A authorizes the batch; PLUS aborts. Stop on
first failed discovery, cleanup, evidence or idle-state check.

Before/between stages the lease/worker/events must be entirely unused, scan
filters balanced, and fresh HID evidence available where required. The0.6.13
hardware upload succeeded after hbmenu restart but the idle guard aborted before
scanning. A later full reboot was verified clean, then0.6.14 passed nativeUUID128
but failed managed completeHID16 delivery (driverHID3/copied3, cleanup1/1/0/0).
The incomplete stage did not run. User then reported menu slowdown.0.6.15
HID-only comparison mode was prepared but never executed; all scan paths are
currently paused. Closing/restarting hbmenu does not reset a sysmodule lease.

This is a **standalone, experimental Switch homebrew diagnostic**, not a working
BLE controller driver. It does not replace/install MissionControl. Keep your
working SD installation backed up. The input mapping in `mc_mitm` is a separate
piece of the implementation.

## One-shot owned connection diagnostic

This is an **NRO-only** mode requiring the updated experimental module's
**connection protocol 1** (mc commands 7–10); older scan-only builds do not
support it. The NRO installs nothing and changes no sysmodule setting. It is still **not** controller support: it performs **no** pairing,
GATT read/write, notification, rumble, key access or virtual-HID attachment. It
only exercises the module-side owned connection lease that the later GATT work
depends on. These are application-request limits, NOT proof of no native
background discovery; Android reported automatic service discovery during an
earlier managed connection, and a later connection report is not yet correlated.

```sh
# Mac Bluetooth must be OFF. Stadia unplugged and disconnected from the Mac.
bash tools/netload-stadia.sh 192.168.1.146 --connect
```

The guarded launcher injects the configured target address; the NRO also accepts
`--target ADDRESS --connect` or a bare `--connect` that reads the SD address
file. The probe requires the module config flag
`experimental_ble_connection_probe=true` and a clean lease state; it refuses to
run otherwise and sends no Bluetooth request.

The NRO arms the lease with its own applet resource user id and the reserved
target, issues the platform managed connect **once**, then polls the leased
status (mc command 7). On connection, completion, timeout or PLUS it requests a
stop (mc command 9). When the lease owns a known connection the module
deliberately does not disconnect by a cached native id: the NRO resolves the
owned handle from the connection list, disconnects once, and reports the result
back (mc command 10). The NRO never guesses a handle, never disconnects by a
stale id, and never retries a failed disconnect. Polling/confirmation waits are
bounded; synchronous platform IPC calls themselves have no added timeout and
may block. Every decode fails closed.

Recorded outcomes:

- `LEASE RETIRED` — the owned lease reached a clean terminal state.
- `CONNECT NOT CONFIRMED` — no owned connection within the bound; no blind retry.
- `CONNECTION NOT CAPTURED` — the reserved native tuple was never observed, so
  the attempt is **not** treated as an owned connection.
- `MANAGED DISCONNECT UNRESOLVED` — no matching owned handle; a failure is
  reported rather than disconnecting something else.
- `LEASE QUARANTINED` / `LEASE CLEANUP UNCONFIRMED` — a normal restart is
  required before another trial; do not blindly retry.

Only one owner/request is allowed per boot. No device address, applet resource
user id, encryption key or pairing credential is logged. Host tests cover the
byte-layout handle resolution, lease classification and token rules; the NRO and
the ARM64 module both build clean. Real-hardware behaviour is **not** yet
validated.

## RAM-only Android control selection (probe 0.6.11)

Obtain explicit approval for one Android connection-only test; advertisement
permission alone is not connection permission. Keep Stadia off, Mac Bluetooth
off, and only the intended Android advertiser running:

- Legacy, connectable, discoverable if offered.
- Primary advertising data contains UUID16 `1812` and UUID128
  `9f6dca74-69b7-4a35-9f01-9ab841ab5832`; Scan Response empty.
- No application GATT server/characteristics, pairing or scanner-side Connect.

```sh
bash tools/netload-stadia.sh 192.168.1.146 --connect-control
```

The launcher never injects the stored Stadia address for this mode. A first
fresh A authorizes a native UUID128 scoped scan for **30 seconds**. All result
addresses must be one valid, stable address; multiple peers, rotation, API
errors and full/truncated result batches refuse selection. Diagnostic counters
contain only batch/count/issue flags, not addresses or advertising payloads.
The five-second window in 0.6.10 failed selection; earlier successful controls
needed more than five seconds, so 0.6.11 restores the established 30-second scan
budget without changing UUID bytes or scan settings.

Scanning must stop successfully before a second fresh A authorizes the one
managed connection. Selection stays in RAM only: no address file read/write or
host export. Existing connections to the target in the NRO's ARUID-qualified
list cause refusal rather than adoption. PLUS aborts; confirmation times out in
30 seconds. The same lease cleanup rules above apply; a pending cancel is not
proven completion. An explicit CreateBond guard is not proof that every native
security path is blocked. Stop Android advertising at the end of the session.

## Pending-cancellation control (probe 0.6.12, not hardware-validated)

**Obtain separate approval for one cancellation trial before launching.** This
is not an automatic repeat of the completed Android connection test. No module
or INI update is needed if connection protocol1 is enabled and the lease is
idle/unarmed. Keep Stadia off and Mac Bluetooth off. No pairing or application
GATT attribute operation is requested; native automatic discovery/security
behavior is not fully characterized.

```sh
# Only after approval and the operator confirms advertiser/netloader readiness:
bash tools/netload-stadia.sh 192.168.1.146 --cancel-control
```

1. Start the same Android primary diagnostic UUID128 + HID16 advertiser.
2. First fresh A authorizes the native scoped 30-second scan.
3. The probe requires one stable candidate and successful scan stop.
4. At the second prompt, **STOP Android advertising** (leave phone Bluetooth
   on). Only then release and press fresh A within 30 seconds. PLUS or timeout
   aborts without connecting. The probe cannot independently verify the phone's
   advertising toggle; this gate relies on operator confirmation.
5. The NRO arms its own ARUID lease, issues one managed connection request and
   immediately requests lease stop after that IPC returns. It does not poll or
   deliberately wait for connection first. Only the module may cancel a captured,
   accepted native pending tuple; if already connected it requests the proven
   managed-disconnect path instead. No invented address or own client registration.
6. Bounded cleanup watches for terminal events and late connections. A final
   ARUID-qualified handle query is diagnostic only: handle absence does not prove
   BTM has cleared every pending request.

Outcomes are deliberately distinct:
- **PENDING CANCELLATION NOT EXERCISED**: no captured cancel request (for example,
  BTM rejected before native capture or connection won the race). Do not claim a
  cancellation pass, even if managed disconnect was clean.
- **NATIVE TERMINAL OBSERVED AFTER CANCEL**: one cancel request, matching terminal,
  no connected event or managed handle, no reported action failure. This records
  observed completion, not proof of cancellation causality/all BTM bookkeeping.
- **LATE CONNECTION AFTER CANCEL CLEANED**: terminal and handle absence confirmed
  after one owned managed disconnect; separate evidence from cancellation alone.
- **CANCELLATION CLEANUP UNCONFIRMED**: no sufficient terminal/ownership evidence,
  API/action failure, ambiguity or remaining handle. Stop testing and reboot
  normally into CFW (EMUMMC); never blindly retry or globally reset Bluetooth.

The module keeps one-shot ownership, even if there is no terminal callback.
The NRO's stop request is sent once, including when the next status read fails.
Closing the NRO/service is NOT treated as proof of cancellation. Offline suites
cover the evidence classifier (including counter mismatches, ambiguous state,
errors and residual handles), buffer bounds and native cancel/late-success lease
races. They do not establish real BTM cancellation semantics. If native command65
or a managed IPC blocks, the caller cannot guarantee the requested stop runs
immediately. No test-mode launch or connection is authorized by building it.

## Actual Android advertisement evidence

The operator's nRF Connect screenshots confirm **LE-only, legacy** advertising,
HID UUID `1812` in AD type **0x02** (incomplete 16-bit list), flags `0x05`, gamepad
appearance, and TX power -8 dBm. A sanitized summary—not the screenshots, device
address, name or complete RAW data—is in
`tests/fixtures/stadia_advertisement_summary.txt`.

The displayed records total **35 bytes** including AD length octets; the HID
record starts at combined offset 31. This exceeds a single legacy PDU's 31-byte
data limit and indicates combined packet data. HID residing in scan response is
strongly suggested, but the actual PDU boundary/order must not be claimed proven
from nRF's merged view alone. A passive scan or controller filtering before scan
response could explain the contrast with the primary-HID Mac control.

The incomplete-list condition therefore matches the observed AD type, yet the
valid real-controller trial still produced zero driver results. Do not add more
byte-order variants or enable/reset global scanning speculatively. The primary
Switchbrew filter table documents UUID16/32/128 and manufacturer conditions, not
local-name conditions 0x08/0x09 or prefix semantics; do not send undocumented name
filters based only on their generic Bluetooth AD type numbers. Further work must
establish safe scan-response handling or another verified, scoped transport path.

## Guarded SD access (0.6.4 / protocol 5)

The persistent console now supports SD operations without switching to FTP:

```sh
python3 tools/stadia-console.py file list /config/MissionControl
python3 tools/stadia-console.py file hash /config/MissionControl/missioncontrol.ini
python3 tools/stadia-console.py file get /config/MissionControl/missioncontrol.ini /tmp/mc-ini-copy.ini
# New files: creation-only by default. Existing files require their exact SHA256.
python3 tools/stadia-console.py file put /tmp/candidate.ini /config/MissionControl/remote-staging/candidate.ini
python3 tools/stadia-console.py file put /tmp/edited.ini /config/MissionControl/missioncontrol.ini --expected CURRENT_SHA256
```

This is **not unrestricted full access**. SD only; no NAND, key/credential files,
hidden paths, address-target file or path traversal. Read access is restricted to
known development-file extensions. Writes are allowed only to the live
`/config/MissionControl/missioncontrol.ini` and flat files under
`/config/MissionControl/remote-staging/`. IPS and boot flags cannot be written,
even in staging. Active modules, patches and the probe executable cannot be
replaced by this API; deployment/self-update is a separate future capability.

Each request requires an authorized, idle foreground session. The Switch opens a
second **outbound** connection to the same Mac on port 28772; it never listens on
a Switch port. Control uses the same user-only Mac socket directory, with a
separate `file.sock`. Binary payloads are never mixed into diagnostic logs.
Downloads are SHA256-verified before saving and never overwrite existing Mac
files. Listings and operation receipts are saved under the protected Mac cache;
CLI output contains metadata, not file contents.

Transfers are limited to 4 MiB, listings to 512 entries / 64 KiB, paths to 240 ASCII
characters, and live INI writes to 64 KiB. Capacity errors never silently truncate.
Writes use exclusive staging, source/disk hash verification, an immediate repeat
of the old-file hash check and a retained backup before activation. Two FAT file
renames are **recoverable, not a power-loss-atomic swap**. A failed/uncertain write
must be reconciled with hash/list/get, never blindly retried. There is no delete
command. No radio operation is performed by a file request.

Host tests cover traversal/protected paths, fake-socket framing and real SHA256
checks, plus filesystem transactions with an SDK shim (its fake digest tests
control flow, not cryptography). Real hardware validation also passed get/list/
hash and staging-only create/edit/read-back/retained-backup checks. Wrong-hash
writes, staged IPS writes and secret reads were refused. The live INI and module
hashes, and all 93 baseline MissionControl IPS hashes, remained unchanged.
Artifacts: `~/.cache/missioncontrol/stadia-console/artifacts/`.

## Persistent foreground console (0.6.3 / protocol 5)

Start the Mac broker **instead of** nxlink's log-only `-s` listener:

```sh
python3 tools/stadia-console.py serve 192.168.1.146 --upload
# After pressing fresh A once on Switch, from another terminal:
python3 tools/stadia-console.py send status
```

The broker saves no secrets and accepts the Switch stream only from the supplied
private LAN address. Control is through a Mac user-only Unix socket (directory
0700, socket 0600), not an HTTP endpoint. Use a trusted LAN, not the Internet.
There is **no inbound Switch server**, shell or arbitrary command execution.
Logs stream to broker stdout; redirect it to a protected local log for archiving.
The NRO's existing SD log remains a fallback, but normal tests need no FTP.

Commands: `status`, `control` (native UUID128), `hid-complete`, `hid-incomplete`,
`real-complete`, `real-incomplete`, `cancel`, `quit`. Scans still have their bounded
timeout and exact existing private-filter scope. Real-controller commands require
Mac Bluetooth OFF; synthetic controls require ON. Status/cancel/quit need no radio
preflight; the broker never toggles a radio. Busy/unknown commands are rejected.
Each scan's destructor completes scoped cleanup before returning to REMOTE IDLE;
unconfirmed cleanup blocks additional scans. PLUS exits locally. Host loss or
network backpressure ends the session and runs cleanup instead of blocking it.
Queued is **not** an operation-complete acknowledgment: inspect REMOTE IDLE/status.

This app must remain open in the foreground. Closing it or rebooting ends access;
it installs no system module or boot service. **SD file commands are not implemented in 0.6.3; use 0.6.4 for the guarded
interface described above.** The command/parser and broker/privacy checks are part of
`bash tools/test-stadia.sh`. `_DEFAULT_SOURCE` exposes the pinned BSD socket
flags in the strict C++ build; no socket constants or firmware offsets are guessed.

## Paired complete/incomplete filter control (0.6.2 / protocol 5)

This is an **NRO-only** update, compatible with the already installed 0.6 module.
No sysmodule/config/IPS replacement or restart is required. Keep the same Mac
`stadia-ble-control --hid16-only` advertiser running for both passes:

```sh
bash tools/netload-stadia.sh 192.168.1.146 --control-hid16-lists
```

Mac Bluetooth must be ON (the guarded launcher checks, never toggles it). Stadia
is not needed. Native UUID128 calibration must already have established a valid
AD format since the last restart; otherwise the probe refuses to scan. Both
experimental scan/bridge flags must already be enabled.

Pass 1 uses the existing complete-list BE marker (condition 0x03). Only if it
succeeds and scoped filter cleanup is confirmed does pass 2 use the existing
incomplete-list BE marker (condition 0x02). Neither pass tries another byte order.
A candidate counts only with fresh private filter-add, driver HID and bridge
counters, unchanged validated format and no new failures; retained BTM candidates
alone do not qualify. PLUS cancels, and the fresh-A start gate remains in place.

This compares filter-condition behaviour against the same synthetic advertiser.
It does **not** control macOS's on-air AD type/PDU placement, establish real
Stadia discovery, prove active scanning or authorize a GATT connection. A failure
of pass 2 is not by itself proof that condition 0x02 is unsupported. Record both
before/after snapshots rather than interpreting either result in isolation.

### Validated paired result (2026-10-19)

Both passes succeeded with the same Mac HID16-only advertiser:

- Complete condition 0x03: `added/removed 2 -> 3`, fresh driver HID/copy `0 -> 1`.
- Incomplete condition 0x02: `added/removed 3 -> 4`, fresh driver HID/copy `1 -> 2`.
- Both ended `active=0 failed=0`, learned format remained 2.

Archive: `~/.cache/missioncontrol/stadia-backups/paired-hid-lists-0.6.2-20261019T131429Z/`.
Read-only FTP verified all **93 baseline MissionControl IPS** across hid/btm/
bluetooth patch groups, boot flags, installed 0.6 module and INI hashes unchanged.
Dot-prefixed auxiliary files and other patch groups were not modified; they are
not covered by the historical 93-file integrity claim. The Mac advertiser stopped
its own advertisement at its deadline. No connection or GATT operation occurred.

This confirms both filter-condition paths work with the control, while the real
Stadia trials remain negative. It does not prove scan-response delivery or locate
the firmware stage responsible for the difference.

## Probe-only operational update (0.6.1 / protocol 5)

No sysmodule change or restart is needed for this probe update. It requires a
fresh A press after observing A released, logs actual/expected handshake versions,
and explicitly labels cancellation rather than reporting negative discovery.
Use `bash tools/netload-stadia.sh SWITCH_IP MODE` for launches: real-controller
modes require the Mac radio confirmed **off**, controls require **on**, and
`--status-only` does not inspect/change the radio. Failed/unknown preflight exits
before Docker or nxlink. Fake-tool tests verify that abort behavior, including
malformed state data and profiler failures; no device identities are printed.

The first incomplete-list trial was confounded: the Mac remained on and a
preflight failure did not stop the old ad-hoc shell command from uploading.
It ended with clean teardown (`active=0`, `failed=0`) but is **not** evidence
against the incomplete-list hypothesis. Repeat only after the guarded preflight.

## Incomplete HID16 list prototype (0.6.0 / protocol 5)

Deployed for coordinated diagnostics; the incomplete-list hypothesis remains
unvalidated. Status-only and native UUID128 calibration passed on 0.6.0. Actual
Stadia testing with 0.5.0 produced
zero new driver advertisements, so neither alias generation nor target-address
matching ran. The Mac radio was subsequently verified off. SDK `btdrv_ids.h`
distinguishes AD 0x02=incomplete and 0x03=complete 16-bit service lists; the existing
private filter tested only 0x03. This is a hypothesis about Stadia, not a raw
capture proving its AD type.

A new explicit `--target ADDRESS --patched-incomplete-be` mode uses a separate
private marker and changes only the rewritten condition to type 0x02 for the same
HID UUID 1812 and hardware-validated `18 12` order. Existing complete-list modes
remain unchanged. Three independently tracked variants preserve symmetric delete,
atomic bounded aliases and native raw forwarding; there is no automatic fallback,
unfiltered scan, global lifecycle hook, pairing or GATT operation.

Protocol 5 remains 84 bytes but rejects older modules before Bluetooth calls.
ASan/UBSan tests, both builds and the private-only surface checks pass. The ARM64
bridge frame stays 80 bytes with a 48-byte byte-view helper. The working 0.5.0
artifacts and the real negative trace have been archived locally. A deployment
requires explicit approval, backup, a normal restart and fresh UUID128 calibration
before testing this one new private filter variant.

## Validated control prototype (0.5.0 / protocol 4)

The disabled, scan-only and HID16-only synthetic control phases have been tested
on hardware with fluid menus. This does **not** validate actual Stadia discovery
or prove the exact cause of the old 0.4.0 regression.

- No added interception of global commands 47/48/55/56/61. Native BLE lifecycle
  and filter-enable requests follow the original MissionControl forwarding path.
- Only commands 57/58/59 extend the interface. Non-private/disabled/wrong-client
  conditions and untracked index deletions return the SDK's
  `sm::mitm::ResultShouldForwardToSession`; they do not take our operation mutex
  or reconstruct the request. The SDK restores its saved incoming IPC message.
- Inactive/default-off event handling follows the original getter; it does not
  enter the advertisement bridge. The bridge reads one 31-byte AD record at a
  time through an unaligned-safe byte view of BTM's response. It never copies a
  full scan event onto the thread's small stack or changes the raw event cache.
- Protocol 4 is 84 bytes inline. Removed global lifecycle counters no longer
  appear in the probe; private-filter, event and bridge counters remain.
- Both experimental flags still default to false. `--status-only` reads module
  identity/diagnostic flags without a target, btdev/btmu initialization, or any
  Bluetooth request. It also works when the flags are disabled.

Actual 0.5.0 results: disabled flags booted with fluid menus and the status-only
check passed; scan-only native UUID128 control was detected and learned format 2.
With both flags enabled, the mixed HID16+unrelated-UUID128 control produced three
bridge appends and clean teardown, but **zero BTM candidates**. The following Mac
`--hid16-only` test with probe `--control-hid16` succeeded in the BE phase:
`copied` and driver HID counts each increased by one, BTM returned a candidate,
and all private filters were removed. Mixed-UUID first-selection/caching remains
a limitation/hypothesis. Actual Stadia subsequently produced **no** driver events
under the complete-list filter; this is a different, earlier-stage blocker.

The existing `--bridge-control` calibration/BE-control sequence and
`--target ADDRESS --patched-be` target scan remain available only for a later,
explicitly enabled diagnostic phase. No pairing/connection/GATT is implemented.

Local verification includes all u8 AD lengths, both length conventions and native
UUID128 byte orders, deliberately unaligned buffers, truncated spans, preservation
of existing bytes, private ownership/cleanup, and source contracts against global
hooks. ASan/UBSan tests and both pinned builds pass. AArch64 disassembly shows an
80-byte bridge frame plus a 32-byte byte-view helper, versus the previous bridge's
400-byte frame. This is not a measurement of the complete dispatch stack.

## Historical scoped bridge (0.4.0 / protocol 3)

**Still discovery-only, not Stadia support.** Status protocol 3 is 128 bytes inline;
old probes/modules refuse mismatches. Both config flags default to false:

```ini
[bluetooth]
experimental_stadia_ble_scan=true
experimental_stadia_ble_advertisement_bridge=true
```

The second flag is a separate explicit opt-in; merely enabling the original scan
experiment does not authorize copied-event translation. Installing the new module requires a normal restart, not HATS
installation. Keep all existing IPS files unchanged.

Hardware A/B evidence: a Mac UUID128 control was discovered in LE byte order.
HID16 plus an unrelated UUID128 reached the driver in the `18 12` phase but not
BTM's results. Replacing only that UUID128 with the matching private marker made
BTM return a candidate, with every filter removed. This supports an additional
UUID128 match in the result path; it is not a reverse-engineered specification.

The new bridge appends an appropriate private UUID128 AD record **only to BTM's
copied successful new-device scan result**, with a tracked private condition and
valid HID16 service. The shared raw event cache, existing AD records/order,
addresses, RSSI, and security/connection/other event types are untouched. Other
btdrv clients never receive the alias. No global enable/disable, reset, pairing, GATT
or additional driver request is initiated. Cleanup disables synthesis; failed or
saturated private-filter bookkeeping also disables it until restart.

The `BleAdvertisement.size` header comment and generated filter lengths are
ambiguous. Rather than guess, the observer validates the convention using the
known 16-byte synthetic control UUID in an actual event. It learns 1=data bytes,
2=includes AD type, with bit 2 (+4) indicating native canonical UUID128 byte order
instead of BLE byte order. Thus validated values are 1/2/5/6; output UUIDs mirror
the observed order. Values 0=unknown and 3=conflicting both prohibit synthesis. A conflict
stays latched until restart. Full/malformed lists remain byte-for-byte unchanged;
two markers require sufficient space atomically. Counters expose appends/skips.

After restart, advertise with Mac `stadia-ble-control --hid16` (the **unrelated**
control UUID plus HID16, not `--marker-hid16`) and netload probe `--bridge-control`.
One A press runs UUID128 calibration, stops it, then runs the BE HID16 private scan.
Calibration failure or unconfirmed cleanup prevents the next scan. Compare the
bridge counter as well as managed candidates; neither is full controller support.
Then, without another reboot, test the actual Stadia with `--target ADDRESS
--patched-be`, after the Mac advertiser ends and Stadia is disconnected from Mac.
A restart loses calibration and requires the synthetic control again.

## Purpose and limits

Test whether Nintendo's **managed BLE user APIs** can discover a specific Stadia
controller on the tester's firmware. This uses `btdev` / `btmu` instead of
reinitializing `btdrv`, consuming its system events or clearing global scan
filters. It requests a Smart Device scan filtered to HID service `1812` and
compares only the documented address field against an explicitly configured
target. Version 0.1.1 uses the full Bluetooth-base HID UUID128: the BTM API
ignores the UUID size member and always builds a 128-bit filter. A short,
zero-padded `1812` request (used in 0.1.0) is incorrect. Both forms returned zero
candidates on the tested Switch, while passive macOS scanning confirmed Stadia
advertises only UUID16 `1812`, with no manufacturer data or UUID128. Unknown scan fields are not interpreted. It never automatically tries a
broader scan or a different device if the target is not found.

**There are no connection, pairing/unpairing, GATT read/write, notification,
rumble, data-path override or virtual HID attachment calls.** Connection was
intentionally excluded after safety review: the managed API accepts a connection
request without returning a cancellation handle, so bounded cleanup of a late
completion is not established. A later transport implementation must resolve
pending-request cancellation, verify disconnect completion, and preserve exact
connection identity rather than treating any matching address as ownership.

Address type/private-address behavior and permitted scan UUID forms still need
firmware-specific verification. A missing result does not prove the controller
is not advertising. `TARGET DETECTED` is a discovery milestone, not a successful
connection, bonding, GATT subscription, usable input or reconnection test.

No characteristic values or opaque security events are logged. Logs contain only
API results, counts and scan status; addresses are deliberately omitted. The
separate target config is a local device identifier, not a pairing key. Do not
post it publicly.

## Build and prepare on macOS

From the repository root, with Docker running:

```sh
bash tools/test-stadia.sh
bash tools/build-switch.sh -C tools/stadia-probe
python3 tools/stadia-probe/prepare-target.py
```

The preparation script reads macOS's system Bluetooth report and selects exactly
one Stadia address. It does not read Bluetooth pairing databases, print addresses
or install anything. It refuses to guess if multiple controllers are present or
to replace an existing different target. The target is stored under the ignored
`package/` directory, never in source fixtures.

Alternatively, create `config/MissionControl/stadia-probe-address.txt` yourself,
containing just your controller's BLE address as `xx:xx:xx:xx:xx:xx` and a newline.
Do not use the Mac's host adapter address or a CoreBluetooth peripheral UUID.

Output ready for your own SD card:

```text
package/
  switch/stadia-ble-probe.nro
  config/MissionControl/stadia-probe-address.txt
```

The Docker build installs the pinned libnx inside the disposable container, not
on the host. Build products/local configuration are ignored by Git.

## Load over hbmenu netloader

No SD transfer is necessary when launching the NRO with a target argument:

```sh
# With nxlink available on your host; replace SWITCH_IP:
target=$(tr -d '\r\n' < tools/stadia-probe/package/config/MissionControl/stadia-probe-address.txt)
nxlink -a SWITCH_IP -s --args "--target $target" \
  tools/stadia-probe/package/switch/stadia-ble-probe.nro
```

The normal netloader port is `28280`; nxlink's callback server listens on `28771`.
If using nxlink inside Docker, publish TCP port `28771` and use `stdbuf -oL -eL`
for timely log output through `docker logs`. If the callback cannot connect, use
the on-screen output. The app preserves stdout for its console and duplicates
sanitized logs through nxlink stderr. Target arguments are not printed or saved
to the diagnostic log. This launch path needs no address config file on the Switch.

## Historical filter-only experiment (probe 0.2.0)

This changes the installed MissionControl binary and requires a reboot. **Back up
the current binary and configuration first.** It is not part of the original
standalone-only test. It remains a discovery diagnostic, not working Stadia BLE
compatibility. Never send `exefs.nsp` to DBI's FTP *installer* endpoint: use an
actual SD filesystem FTP server or SD/USB access.

The experimental sysmodule exposes a read-only status protocol on `mc`, command
6. Enable it explicitly in the existing `config/MissionControl/missioncontrol.ini`
without discarding other settings:

```ini
[bluetooth]
experimental_stadia_ble_scan=true
```

The source/template default is false; firmware below 9.0.0 is not enabled.
The probe must be launched with `--target ADDRESS --patched-be` or
`--target ADDRESS --patched-le`. It checks the matching protocol and enabled state
before scanning, and refuses to start over a previously tracked active private
filter. It asks BTM to scan a private marker UUID128, not the HID UUID itself.
Only that exact marker/condition shape from BTM is rewritten to a UUID16 complete
service-list filter; all unrelated conditions are passed through unchanged. Filter
index is preserved, add **57** and condition-delete **58** use symmetric rewriting,
and index-delete **59** is forwarded unchanged and tracked. Command **60**, which
clears every filter, is **never used or intercepted**.

The BE/LE modes explicitly test the driver's documented UUID16 byte-swap convention
(`18 12` versus `12 18`). Neither mode assumes success before hardware results.
There is no automatic fallback or scan broadening. The BLE observer only increments
counters for documented scan events while the private filter is tracked. It never
modifies an advertisement or logs addresses/security data. The status response
reports added/removed/active condition references, failures and driver advertising
counts. Distinct byte-order hypotheses at one index are tracked separately;
duplicate successful additions are conservatively counted, and saturation remains
uncertain until whole-index deletion. Driver forwarding and bookkeeping are
serialized together, and status snapshots cannot observe the interval between
them. The event thread only takes a short state lock, never the operation lock.

Compare the before/after counters to distinguish a patch that was not exercised,
driver advertisements that never reached BTM's filtered results, and actual target
detection. At this filter-only stage, the BTM-side matching hypothesis was not
established and no copied advertisement metadata was translated. See the latest
0.4.0 section for the subsequently authorized, scoped BTM response-copy bridge.
Stop IPC success alone is not proof of filter teardown; a remaining tracked active
filter is reported and requires a reboot before another experiment.

To roll back, restore the backed-up sysmodule and ini and reboot. Changing the ini
alone also needs a reboot. Do not update Atmosphere, install titles, or alter other
Bluetooth modules as part of this experiment.

## Historical lifecycle telemetry (probe 0.3.0 / status protocol 2)

Requires the matching experimental sysmodule; protocol 2 returns 104 bytes of
inline raw status data. The probe refuses a mismatched protocol before scanning.
The existing `sf::Out<Status>` / `serviceDispatchOut` ABI remains inline, not a
pointer buffer. Version 0.2.1 first queries ordinary MissionControl version command
0; a version string alone does not identify an experimental build.

The sysmodule now observes incoming driver commands 47/48 (BLE enable/disable),
55/56 (scan start/stop), and 61 (filter enable flag). Every incoming request is
forwarded **exactly once with unchanged arguments/result**. The diagnostic never
initiates any of these global operations itself. Counts are cumulative for
intercepted system calls, not proof of radio activity or ownership of other scans.
Accepted-state values mean 0=unknown, 1=enable IPC accepted, 2=disable IPC accepted;
errors retain the last known accepted state. Compare snapshots around the probe.

While a private condition is tracked, the observer counts *all* managed event
types and scan-result statuses, including unknown types/statuses and errors—not
only recognized `status=2` advertisements. Unknown event payloads are never read,
interpreted or logged. The event thread still never takes the driver-operation
lock; bookkeeping locks do not span IPC. This distinguishes missing start calls,
absence of managed events, and statuses not covered by the original observer.

### Actual hardware results so far

On Switch 21.2.0 / Atmosphere 1.11.2 emuMMC, the Mac sees Stadia UUID16 `1812`.
Managed UUID128 scans and both private UUID16 byte-order hypotheses returned zero
BTM candidates. Protocol-1 patched scans recorded `added=1, removed=1, active=0`
per test, no filter failures, and no *recognized* driver advertisement events.
That establishes the rewrite/cleanup path—not whether the radio received packets
or the stack delivered other event statuses. Lifecycle telemetry is the next test,
not a claimed compatibility fix. No connection or GATT operation has been tested.

The actual 0.3.0 LE-marker trace confirmed enable-BLE and enable-filter requests
accepted once at boot, then one driver scan-start and one scan-stop accepted.
The private filter was added/removed without error and left `active=0`. During
that interval, two managed events arrived (last type 7, filter status), but **zero
scan-result events of any status** arrived. These observations establish the
command/event plumbing, not RF reception. The next discriminator should be a
known-advertiser positive control or firmware-specific scanning/filter evidence,
not repeated button presses or assuming an IPC success proves radio activity.

### Reboot versus installing HATS

A normal restart is **POWER ~3 seconds → Power Options → Restart**, then
**Launch → CFW (EMUMMC)** if Hekate appears. Installing/updating HATS is not a
restart: during testing it replaced the verified experimental `exefs.nsp` with
standard MissionControl 0.16.0, which lacks command 6. Opening the `mc` service
still succeeded, but the unknown command closed the IPC session (`0xF601`). The
correct recovery was to back up the new HATS baseline, restore the experimental
artifact, and restart normally—not to change the status ABI. Do not run a pack
installer/updater between experimental deployment and its hardware test.

## Test on Switch (21.2.0 / Atmosphere 1.11.2 emuMMC)

1. Send via netloader as above, or copy the **contents** of `package/` to the SD
   root and launch **Stadia BLE Probe** in hbmenu. Do not replace existing
   `atmosphere/` files or the MissionControl sysmodule.
2. Disconnect Stadia from the Mac, unplug USB, and use Joy-Con for app navigation.
3. Press **A on Joy-Con** to start. Put Stadia in pairing mode (**Stadia + Y**,
   flashing orange). The scan lasts roughly 30 seconds. This app performs its
   own managed scan; do not simultaneously open Change Grip/Order or run other
   BLE homebrew.
4. Press **PLUS on Joy-Con** to cancel or exit. No Stadia button capture is needed
   in this stage. Do not close with HOME while scanning; use PLUS for cleanup.
5. Share nxlink output, `sd:/config/MissionControl/stadia-ble-probe.log`, or a
   screenshot of the result/error. The file log is overwritten each app launch
   and requires an existing `config/MissionControl` directory. Do not send address
   configs, keys or pairing databases.

If services/scanning fail, record the exact error and exit. Do not change system
permissions, disable existing modules or switch to raw btdrv commands as a
fallback. The probe attempts to stop its managed scan, retries stop IPC failures
up to three times, then closes its event/service handles. Stop acceptance is
logged, not falsely described as verified global Bluetooth teardown. If stop
remains unconfirmed, restart the console before another test. This diagnostic
never resets the adapter or disconnects any device.
