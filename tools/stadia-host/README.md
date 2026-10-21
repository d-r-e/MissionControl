# Stadia host runner

Native macOS HID adapter plus the **existing production C++** Stadia decoder,
HID report-layout parser and HOGP router. No third-party dependencies or
transpilation. No Switch installation, netloader or firmware files required.

## Build and offline verification

```sh
bash tools/stadia-host/build.sh
bash tools/stadia-host/test.sh

tools/stadia-host/build/stadia-host --replay \
  tests/fixtures/stadia_bluetooth_reports.txt --map \
  tests/fixtures/stadia_bluetooth_report_map.hex
```

Replay reads30 saved Bluetooth fixtures and prints normalized controller state.
It compares HID framing against an **explicitly modelled** GATT reference/route,
not a live service discovery or captured2908 descriptor. The completion record
always says `live_gatt_verified:false`. No Bluetooth/HID manager is created by
help, replay or tests. Raw packet hex/device identifiers are not output.

The native/macOS adapter compiles into this executable but runs ONLY for `--hid`.
Linux builds use an unsupported-live-backend stub; replay/tests work normally.
Full project regression: `bash tools/test-stadia.sh` also runs these host tests.

Docker (offline only, existing pinned build image):

```sh
docker run --rm -v "$PWD:/work:ro" -w /work \
  devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
  bash -lc 'bash tools/stadia-host/test.sh'
```

Docker Desktop does not make the Mac's built-in Bluetooth controller available
to Linux containers. Privileged/host-network flags do not solve that. No BlueZ,
USB/IP setup or new Bluetooth dongle is needed for this milestone.

## Live native macOS HID (explicit operator step)

Only when ready for a controller test:

1. Pair ONE Stadia controller to this Mac in macOS Bluetooth settings. Disconnect
   it from other hosts as needed; unplug USB. The runner does NOT pair/connect.
2. Run:

   ```sh
   tools/stadia-host/build/stadia-host --hid --seconds 30
   ```

3. Press/release a few buttons and move/release each stick. Watch normalized state
   on stdout. If access is denied, check the terminal's Input Monitoring permission;
   the program does not change permissions or retry. No administrator/root needed.

This adapter matches only vendor18d1/product9400, Bluetooth/Bluetooth Low Energy,
primary GamePad usage. It opens non-seizing and never sends output reports,
opens keyboards, logs serials/addresses/OS IDs or alters Bluetooth settings.
It validates the OS HID descriptor, callback report ID and exact known prefixed
framing. Missing/ambiguous devices, malformed framing and removal fail closed.
Capture is bounded1..180seconds (default30, monotonic deadline); it does not
reconnect. Completion/removal resets emitted controller state to neutral and
closes only its own HID manager, NOT the OS Bluetooth link.

**Evidence boundary:** successful live input proves macOS OS-delivered HID bytes
through the C++ decoder. It does NOT prove CoreBluetooth exposes1812, Report Map/
Report Reference access, raw2A4D framing, Switch discovery/connection/security or
MissionControl delivery. A separate, explicitly approved GATT experiment is
needed for those host-side GATT facts; Switch transport remains its own gate.

## Live validation recorded

The first approved30second macOS run received343 valid reports,340 state changes,
0 rejected reports and exited0. A+B, separate A/B press/releases and both stick
sweeps were observed; the last actual input and completion state were neutral.
This validates OS-HID input through the shared C++ decoder, NOT raw GATT or
Switch support. Removal/disconnect was not tested. Evidence is archived locally
under `~/.cache/missioncontrol/stadia-host/live-hid-20261020T073936Z/`.

## CoreBluetooth metadata capability probe

Build and offline-test on macOS:

```sh
bash tools/stadia-host/build-gatt.sh
bash tools/stadia-host/test.sh
```

Only after explicit approval for a live GATT session, with one Stadia already
connected in macOS Bluetooth settings:

```sh
tools/stadia-host/build/stadia-gatt-capability --connected-catalog
```

The probe retrieves system-connected peers associated with HID, Device
Information or Battery services. It requires exactly one Stadia-named candidate;
this name is a selection hint, **not authenticated vendor/product identity**.
It does not scan or fall back to cached identifiers. No candidate is inconclusive,
not evidence that the controller lacks a HID service.

It requests one app-owned connection reference, discovers services, and—if1812
is exposed—reads2A4B and the2908 descriptors of2A4D characteristics. The production
C++ parser validates the discovered map and reference sizes. Duplicate or absent
references fail closed. It never reads report values, subscribes, writes, sends
output reports or explicitly pairs. macOS may manage security/background traffic
independently; metadata reads are not a promise of zero OS activity.

The operation has a45second overall deadline plus up to8seconds for cleanup.
Only its own submitted connection reference can be cancelled, once. Cleanup is
confirmed by a terminal CoreBluetooth callback, not by the cancellation request
being accepted. Missing confirmation is reported as failure; there is no retry,
global reset or unowned disconnect. Cancelling this app's reference does not
request termination of the OS HID connection.

Output contains fixed diagnostics, numeric error codes, counts, report IDs/types
and lengths, never peer names/identifiers, raw maps or report payloads. A successful
metadata result still leaves **raw input notifications and Switch support
unvalidated**. Offline tests exercise the ownership model, production bridge,
malformed input and source-level privacy/call restrictions; they do not simulate
CoreBluetooth delegate delivery or establish live cleanup behavior.

Apple API reference: [Central-role tasks](https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/PerformingCommonCentralRoleTasks/PerformingCommonCentralRoleTasks.html).

## Development plan

See [`../STADIA-HOST-FIRST-PLAN.md`](../STADIA-HOST-FIRST-PLAN.md) for research
sources and acceptance gates: offline replay -> live native HID -> raw-GATT
capability -> shared asynchronous protocol core -> owned Switch vertical slice.
The next host step is a separately built/tested raw-GATT capability check, not
another HID capture, Switch scan or firmware dump.
