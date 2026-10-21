# Stadia / BLE development tools

**Current host-first path:** [`stadia-host/`](stadia-host/README.md) builds the
production C++ parser/router into a native macOS HID runner and a portable
fixture-replay CLI. Native and Docker/Linux offline tests pass; live HID and raw
GATT validation are separate pending steps. See
[`STADIA-HOST-FIRST-PLAN.md`](STADIA-HOST-FIRST-PLAN.md) for the development plan.
No Switch update, firmware extraction, new dependency or container Bluetooth
passthrough is required for this host milestone.

**Hardware regression warning:** the experimental 0.4.0 sysmodule was associated
with severely unresponsive power/connection menus and boot trouble. Restoring
standard HATS MissionControl recovered responsiveness. Do **not** install another
experimental build until the cause is isolated. Unit/build tests did not catch
this runtime problem; see `stadia-probe/REGRESSION.md`. The minimal 0.5.0 prototype
removed global telemetry hooks and passed the disabled, native-control and
HID16-only bridge control phases with responsive menus. Actual Stadia discovery
still failed before any driver advertisement arrived. The deployed 0.6.0
incomplete-list (AD 0x02) private filter trial was also negative with the Mac
radio confirmed off. Android subsequently confirmed that AD type, but its merged
35-byte data suggests HID may reside in scan response. This does not establish
the Switch scan mode. Neither prototype is working Stadia controller support.

**These tools do not enable Stadia support on Switch.** MissionControl currently
forwards BLE managed events but does not implement a controller BLE central,
HID-over-GATT discovery/subscriptions, or a virtual controller connection lifecycle.
The Stadia input parser/controller is implemented and verified against actual
Bluetooth reports. This does not yet make the controller connect to Switch.
The capture is a prerequisite for validating the input mapping, not a test of
Switch BLE transport.

## Reusable BLE/HOGP core

The portable Report Map layout reader and Report Reference router now pass
ASan/UBSan tests with generic multi-ID examples and all 30 real Stadia fixtures.
They are **not yet connected to the Switch runtime** and do not constitute a
generic HID usage mapper. See [BLE-TRANSPORT.md](BLE-TRANSPORT.md) for implemented
scope, verified SDK ownership/cancellation limitations, and integration gates.
Run `bash tools/test-stadia.sh`; no hardware is accessed by these tests.

## Reproducible Switch build

Start Docker, then run from the repository root:

```sh
bash tools/build-switch.sh
# To package the normal distribution:
bash tools/build-switch.sh dist
```

This initializes the repository's pinned submodules and uses a digest-pinned
`devkitpro/devkita64` image. It builds/installs the pinned libnx **inside the
container**, since the image's bundled libnx may lack APIs used by the pinned
Atmosphere libraries. No host toolchain installation is needed. Build outputs
are written to the checkout; the container is removed on exit.

The sysmodule is produced at
`mc_mitm/out/nintendo_nx_arm64_armv8a/release/mc_mitm.nsp`. This is a normal build,
not a BLE-enabled test build. Do not install it expecting Stadia support.

Override `MISSIONCONTROL_BUILD_IMAGE` only to intentionally test another toolchain.

## macOS Bluetooth HID capture

Requirements: Apple's command-line tools (`swift`) and a Stadia controller already
converted to Bluetooth mode. No third-party packages or administrator access are
needed.

1. Unplug the controller's USB cable. Hold **Stadia + Y** until the light flashes
   orange, then pair it in macOS Bluetooth settings. Connect only one Stadia
   controller for the capture.
2. In a local Terminal window, run:

   ```sh
   swift tools/stadia-capture.swift > /tmp/stadia-capture.jsonl
   ```

3. Follow the timed prompts in that Terminal. Keep other controls at rest. The
   sequence takes about two minutes and includes every button, full trigger
   travel, and stick directions. Briefly tap the Stadia button; do not hold it
   long enough to power off the controller. Press Ctrl-C to cancel.
4. Share `/tmp/stadia-capture.jsonl`. If the tool reports no device/reports, check
   pairing and Terminal's **Input Monitoring** permission in macOS Privacy &
   Security settings. Restart Terminal after changing permissions.

Only Bluetooth HID devices with VID:PID `18d1:9400` match. The JSONL records the
HID report descriptor, report IDs, unchanged report bytes, relative times, and
control labels. Consecutive identical reports within each phase are suppressed.
The report ID is recorded separately; no byte is stripped or prepended. This is
macOS IOHID data, **not raw GATT notifications**: the Switch transport will still
need independent verification of Report Reference descriptors and notification
framing. No output/feature reports are sent. Serial numbers, device addresses,
pairing keys and unrelated devices are not captured. Do not send Bluetooth
pairing databases or keys.

Tool-only checks (no hardware access):

```sh
swiftc -warnings-as-errors tools/stadia-capture.swift -o /tmp/stadia-capture
/tmp/stadia-capture --self-test
/tmp/stadia-capture --help
bash -n tools/build-switch.sh
```

## Passive macOS BLE advertisement capture

If Switch's filtered scan returns zero candidates, inspect what Stadia actually
advertises before changing transport code. Disconnect Stadia from macOS and leave
it blinking orange. Compile the standalone CoreBluetooth tool with its Bluetooth
usage-description metadata:

```sh
swiftc -warnings-as-errors tools/stadia-advertisement.swift \
  -o /tmp/missioncontrol-stadia-advertisement \
  -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist \
  -Xlinker tools/stadia-advertisement.Info.plist
/tmp/missioncontrol-stadia-advertisement --self-test
/tmp/missioncontrol-stadia-advertisement > /tmp/stadia-advertisement.jsonl
```

Allow Bluetooth access if macOS prompts. This performs a 45-second passive scan,
never connects, and records only Stadia-named devices' advertised service UUIDs,
connectable flag, manufacturer company ID and payload length. It does not record
addresses, peripheral identifiers, full names or manufacturer payload contents.
Do not run it as `sudo` or alter privacy settings automatically. No matches may
mean missing permission, expired pairing mode, or missing advertised names; it
is not proof of absent BLE radio traffic. Here “passive” means **nonconnecting**,
not a verified passive link-layer scan: CoreBluetooth abstracts scan requests and
may merge advertising/scan-response data. These summaries cannot establish which
packet carried UUID16 `1812`.

## Synthetic macOS BLE positive control

`stadia-ble-control.swift` advertises only a public synthetic UUID128 for up to
180 seconds, then stops its own advertisement. It does not enable/disable the
radio, request connections, publish GATT services, or capture device identities.
No name or manufacturer payload is advertised. Compile with the usage metadata:

```sh
swiftc -warnings-as-errors -framework CoreBluetooth tools/stadia-ble-control.swift \
  -o /tmp/missioncontrol-stadia-ble-control \
  -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist \
  -Xlinker tools/stadia-ble-control.Info.plist
/tmp/missioncontrol-stadia-ble-control --self-test
/tmp/missioncontrol-stadia-ble-control --check # No advertisement; radio must already be on.
/tmp/missioncontrol-stadia-ble-control
```

Use probe `--control` while this Mac is nearby; leave Stadia off. The probe checks
both UUID128 API byte orders, stopping early on a candidate, and stops each managed
scan before another pass. It requires no target address and logs no candidate
identities. On actual Switch 21.2.0, the least-significant-byte-first control found
a candidate promptly with successful start/stop and unchanged private counters.
Thus UUID128 managed scanning works; this is **not** Stadia compatibility.

Next discrimination: run advertiser `--hid16` and probe `--control-hid16`. This
requests both the same UUID128 and HID UUID16 `1812`, fitting within 31 bytes without
a name. The probe tests the existing private UUID16 rewrite in both byte orders;
no new sysmodule/reboot is needed. An accepted CoreBluetooth advertising request
does not independently verify over-the-air AD types/placement; a negative HID16
control remains inconclusive without that evidence. No broad filter clear or
additional global Bluetooth configuration operation is used.

Actual HID16 control result: both managed scans returned zero candidates, but the
`18 12` phase ended with one successful driver scan-result event containing HID16;
`active=0`, all filter removals succeeded. Attribution/timing are not proven by
these counters alone. A possible additional BTM-side UUID128 check is a hypothesis,
not documented behavior. Next A/B test: advertiser `--marker-hid16` replaces only
the unrelated UUID128 with the exact BE private marker (canonical
`9c7bc3ae-3f7a-4f14-92af-6b6d63737461`), while keeping HID16. Use the same existing
probe `--control-hid16`; no Switch files or events are changed. A positive managed
result would support the extra-matching hypothesis before any event translation
is considered.

## Verified Stadia input format

The BLE Report Map and 30 representative input samples are stored in
`tests/fixtures/stadia_bluetooth_report_map.hex` and
`tests/fixtures/stadia_bluetooth_reports.txt`. These contain no device identifiers
or capture timestamps. Run the native parser tests with ASan/UBSan:

```sh
bash tools/test-stadia.sh
```

Report ID `03` has a ten-byte payload (eleven bytes with its ID prepended):

| Payload offset | Contents |
| --- | --- |
| 0 | D-pad hat in low nibble: north=0, clockwise through 7, idle=8 |
| 1–2 | Little-endian button bits |
| 3–4 | Left X/Y, logical range 1–255, neutral 128 |
| 5–6 | Right X/Y, logical range 1–255, neutral 128 |
| 7–8 | Left/right triggers, range 0–255 |
| 9 | Consumer volume/media bits, not battery |

Button bits: Capture=0, Assistant=1, Stadia=4, Options=5, Menu=6, R3=7,
L3=8, R1=9, L1=10, Y=11, X=12, B=13, A=14. Face buttons map by physical
position; Menu/Options map to Plus/Minus, Stadia to Home, and Capture to Capture.
Assistant and consumer keys are ignored. Trigger activation uses the existing
configurable threshold. Sticks use their declared 1–255 range and invert Y;
neutral maps exactly to Switch center. Unknown IDs/lengths leave state unchanged.
Battery, motion, LEDs and rumble are not inferred from unverified fields. ID `05`
is an output report and is not accepted by the input parser.

**Transport contract:** `StadiaController::ParseInputReport` accepts exactly one
ID-prefixed input report. HOGP notification values lack that prefix: the transport
must first read the Report Reference descriptor (`2908`) to identify each Report
characteristic (`2A4D`). The portable `StadiaInputReport::ParseGatt` entry point
accepts an explicit ID and the ten-byte value; it does not heuristically infer an
ID from payload bytes. This matters because a valid diagonal hat is also `03`.

## Standalone Switch BLE discovery probe

See [`stadia-probe/README.md`](stadia-probe/README.md) for the separately built NRO
and hbmenu netloader instructions. Behavior depends on the selected mode:
- `--status-only`, `--event-progress` and `--service-latency` do not scan or connect.
- `--discovery-requests` runs a scoped scan and never connects, even on discovery.
- `--trace-managed-control` selects one fresh Android advertiser in RAM using
  scoped, unchanged UUID128 filters with verified cleanup, then a second A gate
  permits one managed connection. Requires the matching protocol6/3 module;
  Mac OFF, Android advertiser ON and Stadia OFF. No legacy gate is lifted.
- `--trace-managed-stadia` adds passive, target/app-correlated observation to one
  managed direct connection. It checks module capability before Bluetooth
  initialization, never scans or reads the GATT cache, and cannot cancel or bond.
- `--managed-stadia` submits one managed connection request without scanning;
  `--discover-stadia` requires trusted discovery first. Both require confirmed
  owned cleanup, or a reboot if a pending request cannot be retired.

Legacy leased/raw-cancel modes remain paused. No mode explicitly requests pairing
or installs files; native background security/setup is not excluded. A successful
IPC submission is not proof of connection or cancellation.

## Console testing prerequisites

Record Switch **system firmware**, **Atmosphere version**, and **MissionControl
version** separately. Keep a backup of the working SD installation. Hardware
validation must establish discovery, authenticated pairing, GATT report routing,
virtual HID attach/detach, sleep/reconnect, and coexistence with existing Classic
controllers before claiming working compatibility. A macOS capture alone cannot
validate these paths.
