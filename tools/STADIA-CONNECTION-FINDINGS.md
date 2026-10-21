# Direct Stadia connection: evidence and next discriminating test

## Hardware evidence (2026-10-19)

- The host saved target was privately compared with the phone's current Stadia
  scanner entry and matched. No address value is recorded here.
- Probe0.6.20 used standard `btdevConnectToGattServer` once, after an empty
  ARUID-qualified connection list and clean module status. No scan, experimental
  lease arm, raw cancellation, explicit pairing or characteristic writes.
- Connect IPC accepted; existing native hook counted one BTM passthrough attempt.
  No managed target handle appeared within20seconds.
- Native ClientConnection observations increased0->1; last raw result0x1a,
  status2(Disconnected), reason field0. This global observation is temporally
  associated, not an exact-target correlation or BTM retirement guarantee.
- BLE dispatch/read/wait counters all advanced9->10 and balanced; the last
  completed acknowledgement wait was44us. All lanes were idle afterward.
  This observation does not demonstrate the previous shared-event-thread stall.
- No GATT cache/report path was reached. Report mapping cannot explain this
  pre-GATT failure. Another identical connect is not a discriminating test.

## What the SDK actually passes

Pinned `lib/libnx/nx/source/runtime/btdev.c:106-107` forwards to `btmuBleConnect`.
`lib/libnx/nx/source/services/btmu.c:179-188` sends address+ARUID and PID, with no
explicit public/random address type or transport field. This is a fact about
our SDK request, NOT proof that native BTM lacks that information internally.

Public BTM documentation describes already-connected and four-device checks,
but does not document a prior-discovery requirement or raw callback error map:
https://switchbrew.org/wiki/BTM_services#BleConnect

## Public Broadcom-family comparison: device metadata matters

Found actual vendor headers and example, rather than relying on generic search
summaries:
https://github.com/khadas/buildroot_vendor_broadcom_brcm-bsa/tree/buildroot/3rdparty/embedded/bsa_examples/linux

- `libbsa/include/bsa_ble_api.h:518-523`: `tBSA_BLE_CL_OPEN` has client_if,
  address and is_direct, no explicit address type.
- `app_ble/source/app_ble_client.c:802-906`: the example offers a device from
  the bonded XML database or the last discovery list, then connects by address.
  This establishes example usage, not a mandatory requirement.
- `libbsa/include/bsa_status.h`: generic client/server statuses start100/200;
  this header does NOT establish that the Switch's raw26 is an HCI error.

Historical AOSP Bluedroid provides a concrete mechanism worth testing, not
proof of the exact HOS21.2 implementation:
https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/android-5.1.1_r38/stack/btm/btm_ble.c

- `BTM_ReadDevInfo`, lines592-621, starts with BR/EDR/public defaults and consults
  the security-device/inquiry records for device type and BLE address type.
- `BTM_UseLeLink`, lines714-734, can select LE using that cached device type.
- `BTM_SecAddBleDevice`, lines68-137, stores address/device types and syncs the
  inquiry record. We are NOT proposing to invoke it or edit native databases.

https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/android-5.1.1_r38/stack/l2cap/l2c_ble.c

- `l2cble_init_direct_conn`, lines669-718, obtains the peer address type from the
  link record and supplies it to the LE Create Connection command.

Thus a correct address can still be insufficient when the lower stack lacks
its type/transport metadata. Neither address high bits nor the screenshot alone
prove the controller's actual on-air address type. No guessed type/flag write,
raw HCI connection or new GATT-client registration is justified.

## Next experiment requirements

A discovery-assisted comparison is now a concrete candidate, not a demonstrated
fix. It must differ from the failing direct attempt in ONE intentional factor:
Switch-native observation of this target before standard managed connect.

1. Use a documented, scoped discovery path with fresh exact-target native-cache
   evidence. Negative BTM managed delivery is NOT absence of native advertisements;
   earlier complete HID16 scans saw native HID advertisements. Do not assume those
   were the exact target without the new cache comparison.
2. Observe the exact saved target inside the Switch; phone discovery cannot
   populate the Switch's cache. No other peer may be selected, connected or logged.
3. Successfully stop and verify removal of only this test's scan/filter before
   attempting the same single managed connect in the SAME boot/session.
4. Preserve event-progress/native-result observations before/after; no invented
   metadata injection or global reset/filter clear. Fail closed on no target,
   malformed/truncated evidence, scan teardown uncertainty or event stall.
5. Do not request another operator session until the actual scoped discovery
   mechanism and its test contract are specified. If it requires another module
   change, explain the functional change before deployment; do not call it merely
   telemetry.
6. No raw pending cancellation, unowned disconnect or guessed native ID. A
   connection failure's error namespace and BTM pending retirement remain unknown.

## Historical rejected candidate (0.6.21; deployed, start failed)

**Correction:** this candidate confused AD types with supported filter conditions.
`BtdrvAdType_Flags == 1` proves an advertisement type, NOT native filter support.
The documented `BleAdvertiseFilter.CondType` lists UUID conditions2..7 and
manufacturer255, NOT Flags1. Do not reuse the Flags rewrite below. One trial
returned raw BTM IPC0x00052a8f immediately, then one stop succeeded; no target poll
or connection occurred. Missing post-stop counters prevent identifying the exact
failed native operation or certifying cleanup. A reboot was requested. Module143,
description661 are mechanical Result decoding, not a verified symbolic error/HCI
mapping. This raw value differs from the earlier native callback0x1a.

References: https://switchbrew.org/wiki/Bluetooth_Driver_services#BleAdvertiseFilter
and https://switchbrew.org/wiki/BTM_services#StartBleScanForSmartDevice.

The following records the superseded implementation, NOT a supported approach.

`--discover-stadia` implements this comparison with a fourth exact private UUID
marker, rewritten ONLY to AD Flags(type1) value0x05 with one-byte mask0xff. This
value was observed in the earlier Stadia advertisement fixture. It is not a
scan-mode setting. Unrelated requests still raw-forward; deletion uses the same
rewrite and tracked index. Original three HID aliases remain unchanged; the new
Flags slot is tracked for cleanup but excluded from all alias synthesis.

New read-only MCcmd12 takes an8byte target+zero-reserved request, returns40byte
POD: cache sequence/event type, scan result/status, target-match, and matched
peer's device/address type and advertisement-count. It reads the existing
native BLE event cache under its existing mutex, never signals an ack, consumes
an event, changes cache, or stores another address/payload. Native reads advance
one sequence under the same mutex. Only a successful target advertisement at a
sequence newer than baseline qualifies. The cache is only the CURRENT event;
a fast replacement can cause a false-negative, never a guessed positive.

The NRO keeps its BTMU session open across one30second maximum scan, one scoped
stop and—only if evidence passes—the same single managed connect. Requires a
fresh native target observation, valid device/address metadata, successful stop,
exact one-add/one-remove delta, no active/failed filters, no scan error increment
and unchanged bridge counters. Long-running/unstable event phase, existing
native connection activity or a nonempty owned list forbids connection. No
observation means no connection; uncertain cleanup holds session for reboot.
No raw cancellation, global filter/radio writes, metadata injection, new GATT
client, pairing API or characteristic writes.

Host ASan/UBSan suites include exact filter add/delete symmetry, default-off and
foreign requests, fourth-slot no-alias tracking, stale/wrong-target/error event
rejection, metadata privacy, bounded cache fields, cleanup failure/error/wrap
cases. Source guards verify native call/signal/wait order preserved and no ack
inside cache inspection. Both pinned ARM64 builds and diff check pass.
Independent review did not run (provider model_not_supported); only parent
local review completed. Do not represent tests as native lifecycle certification.

Artifacts archived at
`~/.cache/missioncontrol/stadia-backups/primary-discovery-20261019T215241Z/`.
Candidate module182900bytes SHA256
`2d6fdd54f841a5b8bc8253cbe17c9b56e47724ac94b6abe75284a087b9d4f875`;
NRO348216bytes SHA256
`f28db0f5a802e00e3143166bde565f1c0cc5934629a23c6eb5d9ddafd4cf4b67`.
This historical module was deployed with the established99file/93IPS protection
checks. See its archive's manifest and run-0.6.21.log for failed trial evidence.

## Corrected candidate (0.6.22; built/tested locally, NOT deployed)

The supported complete UUID16 condition already produced native HID advertisements
in a previous cleanly scoped batch (3 native HID advertisements and balanced1/1
cleanup), despite NEGATIVE BTM-managed delivery. New read-only cache inspection
makes that a useful native discovery path WITHOUT needing a synthetic UUID alias.
Those earlier advertisements were not exact-target-correlated; seeing the saved
Stadia target next time remains a test outcome, not a promise.

New exact private marker(firstbyte65) rewrites to condition3 UUID16. Its entire
condition struct is byte-for-byte equal to the earlier accepted BigEndian filter
at the same native index; add/delete symmetry preserved. It uses the fourth
tracked slot excluded from all alias synthesis. Retired Flags marker(firstbyte64)
is explicitly denied rewriting; no code emits condition1. Native event cache and
ack/signal/wait ordering remain unchanged. Scan/control/global radio APIs unchanged.

MCcmd12 protocol2 now also certifies the corrected native-only marker. A v22 NRO
against the installed P1 module refuses BEFORE scan; oldv21 NRO against P2 likewise
refuses. Existing fresh exact-target, native metadata, balanced cleanup, ownership
and post-scan idle checks still gate the ONE same standard managed connect. The
NRO additionally logs baseline+post-stop private condition counters/errors, even
if start fails, so failure attribution won't depend on a single boolean again.
No raw cancellation/retry/cache injection/global filter clear or new GATT client.

Full host ASan/UBSan suites and both pinned ARM builds pass. New tests reject the
Flags marker without mutation, compare native-only condition bytes with the legacy
accepted filter, verify alias exclusion/tracking, reject P1 evidence and assert
protocol capability checked BEFORE scan. Host tests cannot certify native scan
behavior. No new console operation or deployment during this investigation.

Archive: `~/.cache/missioncontrol/stadia-backups/native-hid-discovery-20261020T053803Z/`.
Module182784bytes SHA256
`60293ec940020e70f0c81724569d8552cc36930113d66edef21d50efb20008e9`;
NRO0.6.22 348216bytes SHA256
`1a65447f88498467a3afb62408052d7f8200fc1937ca76c91ae8a3f138d5f8c9`.
Needs another functional module-only update and normal CFW reboot. Do NOT request
Stadia pairing until that update is verified and the operator is ready for the
one corrected trial. No need to hold Stadia in pairing during FTP or investigation.

## Runtime result and subsequent source investigation

The P2 module was eventually verified ACTIVE using loader NSO build ID
`03ff63e3187d2cf0dec6d80b68fe2c85f44b4a73000000000000000000000000`
after the operator explicitly rebooted from Hekate. Before that, loader metadata
identified the retired Flags module still running despite the correct SD file.
This was a real loaded-code mismatch, not a cache reply ABI problem. Why prior
reported Power-menu Restart did not activate it is not established; do not blame
the operator or infer a general warm-reboot rule. Package and SD hashes matched.

Probe0.6.24's first supported native-only HID discovery attempt then succeeded
at SDK start/stop and scoped cleanup, but yielded ZERO native advertisements:
30seconds/575polls, added1/removed1/active0/failed0, scan_errors0, aliases0.
No exact target => NO managed connection attempt. Native event lanes ended idle
and balanced. This does not reach the discovery-cache hypothesis, establish an
absent peripheral or demonstrate GATT/report behavior. No cleanup reboot needed.
Archive: `native-hid-discovery-20261020T053803Z/run-0.6.24-native-hid-discovery-negative.log`.

### Corrected provenance: earlier three native HID advertisements

The earlier three advertisements were recorded during an ANDROID synthetic
primary-data control, not an exact-target Stadia observation. Its log explicitly
says 'Stadia is not needed', the manifest retained no peer identities, and no
contemporaneous Stadia orange-pairing confirmation was found:
`scan-batch-dual-uuid-hid-delivery-negative-0.6.14-20261019T193147Z/netloader.log:65-87`.
It confirms native filter acceptance/synthetic observations, NOT that Switch had
seen Stadia. The earlier comparison rationale was a feasible supported-filter
test, not evidence of an already-discovered Stadia controller.

### Why condition2 is not a justified next fix

Switchbrew groups2/3 as ServiceUuid16 with byteswapped input:
https://switchbrew.org/wiki/Bluetooth_Driver_services#BleAdvertiseFilter
That is public parameter documentation, not exact HOS21.2 implementation proof.
Historical BSA has one `srvc_uuid` filtering condition and no independent
complete/incomplete selector in that structure:
https://github.com/khadas/buildroot_vendor_broadcom_brcm-bsa/blob/1a0adf239ee4efca9f4c0d1e8c2101080a1b961e/3rdparty/embedded/bsa_examples/linux/libbsa/include/bsa_dm_api.h#L250-L281
Historical Bluedroid `btm_ble_update_uuid_filter` likewise configures one UUID
filter command; its advertisement parser handles partial/complete UUID16 together:
https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/2c0b232ee45d41c568731a0c433b96d9f02075b8/stack/btm/btm_ble_adv_filter.c#774
https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/2c0b232ee45d41c568731a0c433b96d9f02075b8/stack/btm/btm_ble_gap.c#1955
None proves Nintendo firmware equivalence, but no distinct remedy is established.
Do not request another module update/scan merely to flip3->2.

### Scan-response boundary: observed behavior versus implementation

Android primary/response placement controls were positive/negative for BOTH UUID
widths. This establishes our tested path's placement dependence, not a universal
primary-only Broadcom filter. The official Android APCF contract explicitly says:
'Every advertisement and related scan response will have to go through all the
filters' and describes receiving either frame through serial filters:
https://source.android.com/docs/core/connect/bluetooth/hci_requirements#le_apcf_command-set_filtering_parameters_sub_cmd
Historical `btm_ble_cache_adv_data` appends scan-response AD structures, while
`btm_ble_update_inq_result` can wait for scan response before reporting an active
scan result. These are mechanisms to investigate, NOT diagnoses of Switch21.2:
https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/2c0b232ee45d41c568731a0c433b96d9f02075b8/stack/btm/btm_ble_gap.c#2000
https://android.googlesource.com/platform/external/bluetooth/bluedroid/+/2c0b232ee45d41c568731a0c433b96d9f02075b8/stack/btm/btm_ble_gap.c#2265

Pinned `btmuStartBleScanForSmartDevice` sends UUID+ARUID+PID; public
`btdrvSetBleScanParameter` exposes interval/window only, no explicit scan mode.
Historical vendor active/passive constants are NOT justified Switch IPC values.
BSA's `is_scan_rsp` field configures OUTGOING advertisements, not scanner response
enablement. Do not translate it into a guessed Switch scan flag.

### Next concrete evidence source; not a new radio trial

No safe source-backed discovery fix is established under the current constraints.
Inspect exact native HOS21.2 `btdrv` and `btm` executable code before another radio
experiment. The existing foreground probe/SD broker supports an explicit bounded
read-only code-export route: fixed two program IDs, normal filesystem code access,
`/main` only, <=16MiB, NSO header and SHA256 checks, host-only output. No caller-
supplied title/path, keys, raw NAND, memory dumps, native filter changes or SD
writes. If access is denied, stop and report it—no privilege workaround or blind
fallback to raw storage. The offline investigation itself made no console request.

### Subsequent explicitly authorized export attempts: both unavailable

The loader-code route failed on the first btdrv request with raw0x00000615:
pinned `sm_results.hpp` identifies module21/description3 as **OutOfSessions**.
That session stopped, with no second module or fallback. A later, separately
explained normal-filesystem route also failed the first request, raw0xdd6ee202:
its low Result module2/description6001 falls in the FS **InvalidArgument** range;
upper context bits are not interpreted. No failing-stage trace was retained,
so neither permission denial nor a particular bad mount argument is proven.
A documented enum Code0 does not certify that all FS methods/callers can mount
arbitrary system code. Do not guess arguments or attempt additional routes.

Both sessions sent explicit quit after failure, logged cleanup/Finished, and
exported ZERO executable files. Operator reported a brief green flash followed
by hbmenu and explicitly no actual segfault/fatal screen; this is consistent
with requested applet exit, not evidence of a kernel/module crash. The second
host connection-reset after quit does not establish a console crash either.
Only the probe's normal diagnostic log was updated; no installation/config/IPS/
boot files or Bluetooth state were changed by these export operations.

Evidence archives inside `native-hid-discovery-20261020T053803Z/`:
`native-code-export-20261020T064415Z/` and
`native-code-filesystem-20261020T064556Z/`. Native code analysis is BLOCKED, not
complete. Any further step needs a new justified safe-access plan or the two
already-decrypted main NSOs from the operator's own21.2 firmware, if available.
No keys, raw NAND dump, privilege changes or another unchanged radio trial are
requested. Netloader availability alone is not permission for another attempt.

### Corrected-export investigation after the settled connection trace

The operator approved offline investigation of a corrected existing-probe export.
No corrected access method was established, and no new export was run.

The pinned libnx normal-FS wrapper uses command10 on HOS16+, with attribute at
byte0, filesystem type at byte4 and program ID at byte8, plus the documented
fixed path input. This matches the published modern client contract. An older
Atmosphere interface declaration still labels the attribute-taking method8;
that declaration is not evidence that libnx's command10 should be changed.
The actual Atmosphere remote adapter also delegates to libnx. Do not substitute
obsolete command8, guessed filesystem types, attributes, storage IDs or paths.

`fsldrInitialize` contains three distinct steps: service acquisition, domain
conversion and SetCurrentProcess. The old export retained only the aggregate
raw result; it does not locate the exact failure within those steps. Likewise,
normal-FS export spans NCM initialization/database/content-ID/storage/path lookup,
filesystem open, main-file open, size and read. Its InvalidArgument result does
not identify which of those failed. Tests using successful SDK mocks certify
neither native Code0 support nor loader-session availability.

Independent public-source investigation confirmed the client contracts but found
no version-matched server implementation proving `fsp-srv` Code0 support or a
normally available extra `fsp-ldr` session for this caller. The inspected
nxdumptool NCA reader instead uses header keys and section decryption; it is not
a keyless ordinary-filesystem workaround and is outside the current key-access
boundary. Sources:
- https://github.com/switchbrew/libnx/blob/master/nx/source/services/fs.c
- https://github.com/switchbrew/libnx/blob/master/nx/source/services/fsldr.c
- https://switchbrew.org/wiki/Filesystem_services#OpenFileSystemWithId
- https://github.com/Atmosphere-NX/Atmosphere/blob/master/stratosphere/loader/source/ldr_main.cpp
- https://github.com/DarkMatterCore/nxdumptool/blob/ade1a481121498ec649fbc832afe733050790acd/source/core/nca.c

**Result:** native code acquisition remains blocked, not fixed. A possible next
proposal is stage-specific reporting in the EXISTING exporter, with no changed
mount parameters or fallback; that would diagnose the access failure, not
establish an export solution. It is not authorization to repeat the failed
operation, and no such diagnostic was implemented or uploaded in this audit.
Any runtime access test requires a separately explained operator decision.
Already-decrypted operator-owned version-matched executables remain usable if
obtained separately without sharing keys. No new apps, permissions, service
session theft, raw storage, process-memory dump or radio trial were used.
