# Stadia: host-first implementation plan

## Decision

Use native macOS for device I/O and the existing portable C++ implementation for
protocol processing. Use Docker for deterministic Linux builds/replay/sanitizers.
Do not transpile an Apple Bluetooth implementation into Switch code: framework
calls, connection ownership, security and identifiers need distinct adapters.

This deliberately stops treating firmware-code extraction as a prerequisite for
all useful work. Both read-only code-export attempts failed; do not repeat them,
request keys/NAND dumps, or make another speculative Switch filter change.

## What already exists

- `mc_mitm/source/controllers/stadia_report.hpp`: Bluetooth report decoder and
  axis conversion, independent of libnx/Atmosphere runtime APIs.
- `bluetooth_mitm/bluetooth/hid_report_layout.hpp`: bounded HID descriptor parser.
- `bluetooth_mitm/bluetooth/hogp_report_router.hpp`: per-session Report Reference
  binding, subscription gating and report routing.
- Thirty sanitized Bluetooth HID fixtures plus a captured HID descriptor.
- `tools/stadia-capture.swift`: scoped macOS IOHID report capture. It identifies
  its output as OS-delivered HID, NOT raw GATT. No need for another complete
  button-capture exercise simply to repeat the same codec evidence.
- `tools/stadia-advertisement.swift`: unfiltered CoreBluetooth scan with private
  Stadia selection and redacted advertisement summaries, not a raw PDU sniffer.

The Switch's supported native-only UUID16 scan started/stopped cleanly but saw
zero advertisements; its direct managed connection previously failed pre-GATT.
Neither issue is resolved by a working Mac decoder. Keep that transport gate
explicit rather than claiming the entire Switch path is portable.

## Platform evidence and limits

### macOS

Google lists macOS13+Steam among tested Stadia Bluetooth configurations:
https://support.google.com/stadia/answer/13067284?hl=en
SDL's Stadia HID implementation and HIDAPI macOS backend establish a practical
IOHID report-byte path, rather than only semantic GameController button values:
https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_stadia.c
https://github.com/libusb/hidapi/blob/master/mac/hid.c
HIDAPI's callback copies the IOHID report buffer without inventing a GATT frame.
Our existing captures use ID-prefixed11byte reports. Validate the callback ID,
length, descriptor and prefix explicitly; never strip a leading byte by guessing.

CoreBluetooth supports discovery, service/descriptor reads and notification
callbacks, and a device can have services absent from its advertisement:
https://developer.apple.com/library/archive/documentation/NetworkingInternetWeb/Conceptual/CoreBluetooth_concepts/PerformingCommonCentralRoleTasks/PerformingCommonCentralRoleTasks.html
Its peer UUID is an OS identifier, NOT a transferable Bluetooth MAC address:
https://developer.apple.com/documentation/corebluetooth/cbpeer/identifier

There is no established macOS-specific guarantee here that CoreBluetooth exposes
standard HID service1812 to this client. Reports about iOS filtering must not be
promoted to macOS facts. A macOS capability test must distinguish:
- HID service not visible to this client;
- service visible but protected reads rejected;
- Report Map and Report Reference successfully read;
- input notification subscription acknowledged and actual values received.

### Docker

Docker Desktop uses a Linux VM. Privileged containers do not thereby gain access
to CoreBluetooth or the Mac's internal Bluetooth controller. Desktop host
networking is TCP/UDP layer4, not Bluetooth passthrough:
https://docs.docker.com/desktop/setup/install/mac-permission-requirements/
https://docs.docker.com/engine/network/drivers/host/#limitations
Docker's USB/IP feature can attach exported USB devices into the VM, but support
is device-dependent; it does not establish built-in Mac Bluetooth exportability:
https://docs.docker.com/desktop/features/usbip/
BlueZ requires an actual accessible Linux controller and D-Bus access:
https://bleak.readthedocs.io/en/latest/backends/linux.html

Do not install BlueZ in a container and claim that supplies Bluetooth hardware.
An external dongle/Linux host is a conditional future option, not a purchase or
setup requirement now. A native Mac bridge to containerized protocol logic is
possible, but adds no value over linking the same C++ core natively for this slice.

## Implementation and acceptance gates

### 1. Offline host runner (first)

Add `tools/stadia-host` with no third-party dependencies. Compile the existing
production C++ decoder/map parser/router into a small command-line program.
Replay the30fixtures; compare ID-prefixed HID decoding with an EXPLICITLY MODELLED
GATT route using a synthetic Report Reference. Print normalized controller state,
not report hex, addresses, names or credentials. A modelled reference is not a
captured2908 descriptor. Keep this limitation in output and documentation.

Acceptance: native macOS and Linux/Docker replay agree on all30states; malformed
input and invalid lifecycle/framing fail closed; ASan/UBSan clean. No Bluetooth
framework is initialized by replay/help/test modes.

### 2. Native macOS IOHID input

Use non-seizing IOHID access filtered to Google/Stadia Bluetooth gamepads only.
Read the OS HID descriptor and feed ID-prefixed input into the SAME C++ decoder.
Never open keyboards, use raw global input monitoring, send output reports,
pair/unpair in code, reset Bluetooth or disconnect another client's link.

Operator pairs ONE Stadia controller in macOS Bluetooth settings only when the
host runner is ready. Program attaches to an already OS-managed HID device;
closing its own manager does not mean requesting a physical BLE disconnect.
Reject ambiguous multiple matching devices, missing/invalid descriptor, unexpected
report ID/length and stale input after removal. Bounded duration; no reconnect loop.
No raw reports/device identifiers persisted by this runner.

Acceptance: actual button/stick changes appear in C++ normalized output; report
count and disconnect behavior observed. This proves OS-HID integration only—not
ATT handles, raw2A4D framing, encryption or Switch connectivity.

### 3. Bounded raw-GATT capability test (separate)

After the HID slice, evaluate a native CoreBluetooth adapter. Scan without a
service filter; select one user-approved Stadia candidate without logging its
name/UUID/address. Retain the peripheral; discover services without filtering.
If1812 is exposed: read2A4B, enumerate EVERY2A4D instance and its2908 reference,
select input reports by descriptor metadata, enable notifications via the OS API,
and wait for its subscription callback before accepting values.

No arbitrary characteristic writes, output/rumble, guessed handles/address types,
or programmatic pairing. The OS owns bonding/security. Stop on missing access;
do not reinterpret OS-delivered HID as a successful raw-GATT experiment.
Explicit operator approval/readiness before any connection or subscription.

### 4. Shared asynchronous HOGP session core

Once actual GATT access is established, use a portable per-device session state
machine: discovery -> validated map/references -> acknowledged subscriptions ->
input -> close. The backend supplies verified endpoint objects/opaque tokens;
CoreBluetooth tokens are NOT ATT handles. The current router's handle-like fields
must not be filled with fabricated ATT values and presented as captured metadata.
Define that adaptation explicitly before changing production interfaces.

The core issues abstract bounded actions; adapters implement platform calls.
Use generations to reject stale callbacks/native connection-ID reuse, exact
payload sizes, bounded queues, a single owned disconnect, and explicit terminal
cleanup. Scripted callback-order/reuse/error/timeout tests run on both hosts.
Do not preserve a pending request merely because a socket or applet exited.

### 5. Switch adapter and minimal vertical slice

Retain the same tested protocol core; implement only Switch transport/ownership
translation. Must FIRST establish one supported native discovery/owned managed
connection path; Mac success cannot populate the Switch cache or explain0x1a.
No blind identical connects, raw cancellation or guessed global scan writes.

Then verify service/characteristic/reference discovery, subscription completion,
first actual input, normalized state into the emulated controller, and owned
terminal disconnect. Host fixtures model protocol; only Switch evidence validates
native behavior. Preserve93baselineIPS/INI/boot metadata and explain any functional
module-only update before an approved deployment.

## Current validation and next gate

Steps1–2 now have concrete evidence. Native macOS and Docker/Linux passed all30
fixture comparisons and sanitizer/CLI/privacy tests. The first explicitly
approved30second native IOHID run received343 valid reports,340 emitted state
changes and0 rejected reports, exit0. The operator confirmed A+B, left-stick
circle, right-stick circle, A, B. The capture contains A+B and separate A/B
press/release transitions; all axes crossed neutral, three reached both0/4095
and left-Y reached0/4078. The last ACTUAL input and completion state were neutral.
No device-removal test was performed; do not claim disconnect coverage from this.

Evidence: `~/.cache/missioncontrol/stadia-host/live-hid-20261020T073936Z/`.
Only normalized state was retained; no raw packets/addresses/credentials. The
runner closed its own HID manager, not the OS-managed Bluetooth link. No Switch
operation or output/pairing request was sent by the runner.

Step3 now has a built, offline-tested metadata probe:
`tools/stadia-host/build/stadia-gatt-capability --connected-catalog`.
Its source, build script, C++ bridge and usage/safety scope are in
`tools/stadia-host/`. Full project tests and native sanitizer/CLI tests passed.
An explicitly approved live metadata session has now completed:
`~/.cache/missioncontrol/stadia-host/live-gatt-metadata-20261020T110752Z/`.
One matching system-connected candidate, app connection acknowledged, two
services visible but NO1812 exposed to this CoreBluetooth client. No map or
references were read. Exit1 reflects the unavailable capability, not a crash.
One connect/one cancel; terminal callback confirmed cleanup of this app reference.
No scans/subscriptions/writes/Switch operations. This does not prove the device
lacks HID: the native macOS HID path already worked. Raw GATT remains unvalidated.
Do not repeat unchanged or disconnect/unpair the OS-managed controller as a workaround.
The probe never scans/subscribes/writes; it reads map/references only if exposed,
and cancels its own app connection reference once with callback-confirmed cleanup. Do not repeat the HID
capture just to reproduce the same evidence. Raw GATT visibility/map/references/
notifications remain UNVALIDATED; no transitive claim about Switch support.
No Switch FTP, netloader, firmware dump or new module installation is needed.
No work is implied to run between turns unless a specific operation was started.
