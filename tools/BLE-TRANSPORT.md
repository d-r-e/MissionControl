# BLE gamepad transport: Stadia first, reusable HOGP core

## Implemented and tested offline

- `mc_mitm/source/controllers/stadia_report.hpp`: real Stadia Bluetooth decoder.
- `mc_mitm/source/bluetooth_mitm/bluetooth/hid_report_layout.hpp`: bounded HID
  Report Map layout reader; per-ID input/output/feature sizes, padding, Push/Pop,
  numbered and unnumbered reports. It does **not** map generic HID usages to Switch
  buttons/axes. Unsupported/oversized/malformed maps fail closed.
- `mc_mitm/source/bluetooth_mitm/bluetooth/hogp_report_router.hpp`: per-connection
  Report Reference (2908) routing, characteristic/service identity, explicit
  subscription confirmation, exact payload length and local generation checks.
  No Report ID is stripped from or prepended to GATT values.
- `tests/hogp_report_tests.cpp`: generic multi-ID examples, stale sessions,
  capacity/duplicate checks, malformed maps, deterministic fuzz inputs and all
  30 captured Stadia input fixtures through the router/decoder.

Run `bash tools/test-stadia.sh` (ASan/UBSan). The portable headers also pass the
pinned ARM64 compiler's syntax/type checks. Limits are explicit: 4096 descriptor
bytes, 512 value bytes, 8 global Push levels, 16 collection levels, 16 bound input
characteristics per connection. Capacity failures must abort device setup, not
silently omit reports. Allocate the ~1.6 KiB layout in device storage, not the BLE
event-thread stack. Routed views borrow data only for synchronous consumption.

These headers are **not wired into the runtime**. Tests use the real macOS Report
Map and ID-prefixed inputs, removing that known capture prefix to model HOGP.
The test reference `{03, Input}` is modelled, not a Switch GATT capture or proof
of a live CCCD subscription. Synthetic generic maps are not hardware validation.
A local generation cannot identify an uncorrelated SDK event by itself.

## Asynchronous HOGP session core

`mc_mitm/source/bluetooth_mitm/bluetooth/hogp_session.hpp` now orchestrates an
already-owned connection through verified catalog -> Report Map -> per-report
Report Reference -> confirmed subscription -> ready. It uses the existing layout
parser/router, bounded storage, one outstanding operation, one-shot command
consumption, exact completion tickets/endpoints, 5s step and30s total setup
limits. All input routes are revoked on retirement; malformed owned payloads
retire the session. Output/feature references are checked against the map but
never subscribed or written. It has no IPC, allocations, address logging or
connection side effects.

Tests cover asynchronous ordering, stale generations/tickets, handle collisions,
malformed metadata, subscription failures/timeouts, retirement, total deadlines
and all30 fixture inputs through the session. These are modelled setup callbacks,
NOT evidence of live Switch GATT or CCCD acknowledgements.

Adapter obligations remain explicit: prove event ownership BEFORE supplying a
session ticket, retain characteristic-instance identity, validate actual service/
characteristic/descriptor UUIDs, serialize access on a worker, neutralize the
virtual controller on terminal transitions and independently confirm owned link/
subscription cleanup. `Session::Retire` is route revocation, NOT a disconnect or
cleanup acknowledgement. Allocate the session in device storage, not on the event
thread stack. Do not wire a UUID-only event into the current outstanding command
merely because it arrived next.

**Switch adapter gate:** `BtmGattCharacteristic::handle` is documented, but that
alone does not establish callback identity. Pinned `BtmBleDataPath` has an unknown
byte plus UUID, and `BtdrvBleClientGattOperationInfo` has undocumented fields plus
UUIDs/data. Their instance/ownership mapping is not verified. The previous claim
that managed data paths necessarily preserve instance identity was too strong.
The session remains NOT wired into the runtime; discovery/owned connection also
remain unresolved. No new module has been deployed from this session-core work.

## Stadia controller delivery seam (not yet runtime-connected)

`controllers/stadia_gatt_input.hpp` adds an endpoint/generation-qualified input
lease. It consumes only the router's explicit ID/value view, rejects foreign or
stale endpoints, and neutralizes once on retirement or malformed owned input.
The real `StadiaController` now exposes bind/input/retire worker entry points and
shares its physical-position mapping between prefixed HID and routed GATT input.
After a GATT binding, HID events cannot resurrect input, including after retirement.
The future adapter must serialize input, output and lifecycle access to each
controller; these APIs do not create virtual attachments or own radio resources.

`stadia_virtual_input.hpp` supplies a bounded copy/remap/publication seam used by
both controller input and virtual command replies. The initial profile accepts
only full input mode30 and command replies21; generic3f and MCU/attachment modes
are explicitly rejected before state mutation. This avoids the shared base's
incomplete generic serializer (zero hat would mean Up, not neutral). Physical
rumble/LED output remains unimplemented/no-op; no GATT write is introduced here.

Review found the generic-mode neutralization and command-reply remapping gaps;
both were fixed and the follow-up source review reported no remaining issue.
Tests cover lease lifecycle,30 fixtures, all256 format selections, malformed
lengths and copy/remap/emit ordering. Publication tests use explicit doubles,
NOT a live SDK controller or virtual HID connection. Full macOS and pinned Docker
regressions pass. The actual pinned-SDK native module build also completed
with exit0 and linked NSP/NSO. NSP183203 bytes SHA256
`ae5975be1ed866601320c945717bab7c1c47e05cc8f579462e71625d4b2d4209`.
Archive: `~/.cache/missioncontrol/stadia-host/switch-gatt-controller-build-20261020T115257Z/`.
The earlier timed-out build and missing-BSD-header SDK attempt were not counted
as success. No artifact from this work has been deployed. Compiling these entry
points does not connect them to real Switch BLE callbacks or virtual attachment.

Additional source trace: `libnx/nx/source/runtime/btdev.c` explicitly builds
service/characteristic instance-qualified read requests; its descriptor wrapper
leaves descriptor instance0. That documents the SDK wrapper's behavior, not
complete21.2 callback ownership. Public libnx PRs[644](https://github.com/switchbrew/libnx/pull/644)
and[658](https://github.com/switchbrew/libnx/pull/658) clarify request structures,
not missing callback instance fields. No firmware export was retried.

## Verified SDK contracts and gates

Pinned source: `lib/libnx/nx/{include/switch,source}/services/`.

1. `btmuBleConnect` / `btmBleConnect` return only an IPC result, not a pending
   request handle. `BleDisconnect` requires a known connected handle; FFFFFFFF
   is invalid. Do not guess an ID or assume connection timeout cancels it.
2. The lower-level `btdrvCancelConnectGattServer(client_if, address, is_direct)`
   exists on 5.1.0+ (command 66); disconnect is command 67. It is a possible
   cancellation primitive, **not** proof that MissionControl owns a given pending
   operation. Accepted IPC is not proof of asynchronous cancellation completion.
3. `btdrvRegisterGattClient` (62) accepts a UUID but its documented managed
   registration event exposes only result/client_if/status, no UUID/request ID.
   Never claim the next registration callback is ours without proven sequencing.
4. Raw `BtdrvBleEventInfo.client_notify` exposes connection and UUIDs, not the
   characteristic instance/handle. Different Report characteristics share 2a4d.
   A generic adapter must preserve verified instance identity via the managed
   data path or another proven route; it cannot guess the report ID from data[0].
5. `btdrvConnectGattServer` has no explicit peer-address-type argument. A directed
   connection to a known address is not yet a proven bypass for discovery of
   random-address peripherals. Cache/address-type behaviour remains to verify.

## Controlled scan-response evidence

Using the same Android advertiser and unchanged connectable/discoverable legacy
settings, the operator moved only HID's location: primary HID was detected,
scan-response-only HID produced no fresh driver advertisements, and restored
primary HID was detected again (both complete/incomplete filter conditions).
This A/B/A result supports a placement-dependent receive/filter limitation.
It does **not** distinguish passive scanning from controller/driver filtering
before scan-response delivery. The original non-connectable Android test was
negative but changed two properties at once and is not the placement baseline.

The native 128-bit UUID comparison reproduced the same placement dependence:

| Android field placement | HID16 private filter | Native UUID128 filter |
| --- | --- | --- |
| HID1812 primary, diagnostic UUID128 scan response | Positive (fresh driver HID/copy 6 -> 7) | Not confirmed |
| Diagnostic UUID128 primary, HID1812 scan response | Not confirmed (driver HID remains 7) | Positive |

Both comparisons ended `active=0 failed=0`, without connection, GATT or global
mode changes. The exact native UUID was therefore validated in primary, not
merely assumed correct from the negative response trial. This supports a general
scan-response matching/delivery limitation, not a UUID16-only failure. It still
does **not** distinguish passive scanning from filtering before response delivery.
Archives: `android-native128-response-negative-0.6.4-20261019T163951Z` and
`android-native128-primary-positive-hid-response-negative-0.6.4-20261019T164330Z`
under `~/.cache/missioncontrol/stadia-backups/`. Further work should establish the
native scan configuration and restoration guarantees before changing it.

## Integration order

1. Finish bounded discovery evidence. Android confirms legacy advertising, HID
   1812 in AD02; its merged 35-byte buffer suggests scan response, not a proven
   PDU boundary. See `tests/fixtures/stadia_advertisement_summary.txt`.
2. Establish a cancellable, owned connection path. Prefer reusing Nintendo's
   managed GATT machinery if its ownership/lifetime guarantees can be verified;
   otherwise solve private-client registration sequencing before issuing it.
   No speculative global scan-mode changes, filter clears or unregister-all.
   The probe-side one-shot client for this path now exists (`--connect`), arming
   the module lease with the NRO's own ARUID, connecting once, and performing
   only the managed disconnect the lease explicitly requests. It is
   offline-verified and unvalidated on hardware.
3. Discover 1812 / 2a4b Report Map / 2a4d Report / 2908 Report Reference / 2902
   CCCD. Feed complete verified maps and references into the common core. Confirm
   per-characteristic notification enable; authentication requirements are an
   explicit later pairing decision, never automatic key capture or retry loops.
4. Deliver correctly identified values to the Stadia codec (verified VID/PID
   18d1:9400). Add descriptor-usage mapping/device profiles for generic gamepads;
   do not assume every controller uses Stadia's axes, IDs or button numbering.
5. Add the virtual Switch controller lifecycle, disconnect/reconnect cleanup,
   bounded worker queues and multi-controller testing before claiming support.
   Preserve all unrelated BTM/HID event forwarding and raw event cache bytes.

No connection, pairing, GATT operation, Switch installation/configuration change,
IPS change, or Bluetooth radio toggle was performed to develop this core.
