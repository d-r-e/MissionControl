# Stadia controller on Bluetooth LE: the bench test

One controller, one console. Everything here is written so that the answer comes out of
`/config/MissionControl/ble.log` afterwards, because the interesting failures happen once,
in a pairing window, with a pad in your hands.

The build under test is on branch `feat/stadia-ble`. `[ble]` is off by default upstream, and
the only settings this test turns on are `enable`, `log` and `radio_cycle`.

## What is already known, and what this test exists to find out

Known offline, from captures (see `tests/fixtures/stadia_*`):

- the pad's notifying input report is id `0x03`, 10 bytes; its writable report is `0x05`, 4 bytes;
- it advertises appearance `0x03C4` (gamepad) *and* an incomplete 16-bit service list containing
  `0x1812`, which Xbox pads were not seen to do;
- neutral state is `08 00 00 80 80 80 80 00 00 00`, with the four directions at `00/02/04/06`.

Unknown until this console sees it — do not fill these in from expectation:

| # | Question | What answers it |
|---|---|---|
| U1 | Does the report map read succeed over btdrv at all? | `report map: N bytes` vs `could not read the device's report map` |
| U2 | Which pad kind is identified from the live device? | `Stadia controller presented to the system` / a refusal with the attribute dump |
| U3 | Does pairing complete, and with what SMP capabilities? | `Pair`, then link events; the log prints the btdrv results |
| U4 | How long does a successfully presented connection remain usable? | time from `presented` to `link ended`, with reason; the old ~30 s report is not established as a current limitation |
| U5 | Does a bond survive power cycling? | `btdrvGetPairedDeviceInfo key_present=/key_type=` lines across a reboot |
| U6 | Do `0x2A4E` (Protocol Mode) or `0x2A4C` (HID Control Point) need writing? | only answerable if notifications never start: the attribute table shows both |
| U7 | What is report `0x05` for? | **not** answered by writing it — the module never writes to the pad |

## First-bench follow-up: bounded diagnostic build

The first live run connected and discovered 27 attributes, but the authenticated Report Map read
(handle `0x0048`) had no logged completion. The old code ignored that timeout, sent another read,
then disconnected/retried. `paired` was only a core lifecycle-event heuristic, not proof of encryption.
The underlying reason for the missing completion remains unknown.

The follow-up accepts read completions from either event stream, retains the first Report Map value
instead of reading it twice, and stops on the first failed setup **until reboot**. It does not retry
connections or request a recovery radio cycle after setup failure. The configured startup BLE bring-up
is unchanged. No authentication/radio parameters or Stadia output writes were added.

For this build, reboot with the pad off, wait 25 seconds at HOME, and make **one** pairing attempt.
If the controller does not appear within 20 seconds, turn it off and leave Change Grip/Order.
Start FTP manually and retrieve `/config/MissionControl/ble.log`. Do not repeat pairing or test
sleep/wake until the first connection is known to work. The log now labels lifecycle events honestly,
prints the completion source/result/size, or `incomplete ...; no next GATT request` and `setup stopped`.

## Passive ownership diagnostic (installed; first test still fails)

The diagnostic passes the portable suite, pinned Switch build, and independent source review. Its source-contract tests are not a runtime IPC or console test. The subsequent approved operator test still timed out: HID received the placeholder, no HID/BTM slot-read calls were observed, and the listener never woke. That boot also included sleep/wake before pairing. See `STADIA-FW23-GATT-FINDINGS.md`; no further test is currently requested.

The diagnostic preserves the bounded setup behavior above. It adds boot-cumulative counters
only when **both** experimental BLE and its logging setting are enabled. Recording does not depend
on the log file already being open; the system IPC callbacks perform no SD I/O.

Before the existing read and at its timeout, `ownership read-begin` / `ownership read-incomplete`
snapshots report:

- `hid_placeholder` / `btm_placeholder`: system calls that received the unsignalled placeholder;
- `hid_slot_read_calls` / `btm_slot_read_calls`: observed command-96 requests, forwarded unchanged;
- listener starts, successful/failed registrations, wakes, and read IPC returns;
- returned notification/other event types, current-connection matches, and managed notifications.

The snapshots are not transactional across counters. Compare boot totals at the two points.
A read call is **not proof it consumed a completion**. A successful listener IPC return or stored
event type is **not proof the slot was nonempty**. Other processes outside the existing HID/BTM
MITM selection are not observed. Zero observed competing reads does not prove successful encryption.
No additional event read, GATT request, authentication argument, radio action, or output write is added.
Calls made while BLE/logging is disabled are deliberately not retained.

Do not use the general sleep/reconnect procedure below for this diagnostic. Installation requires
verified backups/readback and operator readiness; after installation, the bounded **one reboot /
one pairing attempt** instructions above take precedence.

## Bounded observation window (installed; awaiting operator test)

The current host abandons the first read after ten seconds. Bluetooth specifies thirty-second SMP
and ATT timeout rules; cutting the link at ten seconds can prevent their outcome from appearing.
The next diagnostic therefore waits up to **35 seconds after read IPC acceptance for that same
request**, keeping the existing disconnect/radio-off/shutdown aborts and first-setup-failure latch.
The complete portable suite and pinned Switch build pass; this small delta received direct source review.
The previous independent ownership-review session had expired and did not review this delta.
After explicit installation approval, the module-only FTP transfer was verified byte-for-byte; INI
remained unchanged. Receipt: `receipt-window35-20261021T210254Z` in the bench cache. No reboot or
pairing command was sent remotely; the following controlled procedure is for the operator.

This does **not** establish a security cause, promise that every multi-stage procedure fits35s, or
change authentication, event routing, radio behavior, or the number of outstanding requests.
It is an observation patch, not a confirmed connection fix. A valid completion still ends the wait
immediately; an error stops setup. No second request is sent to recover from an incomplete read.

Only after verified installation and operator readiness: reboot with Stadia off, keep the console
awake without a sleep/wake cycle, and make **one** pairing attempt. Once the logo changes to white,
allow a full **60 seconds** without restarting pairing. Then turn Stadia off, leave Change Grip/Order,
and start full SD FTP manually. Do not use the earlier20-second stop instruction for this build.
No repeat attempt or sleep/wake test is requested.

Protocol references (bounds, not proof of this firmware's behavior):
- [Core5.4 Vol3 PartH §3.4, SMP timeout](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/security-manager-specification.html):
  the timer may reset as pairing progresses; reaching30seconds fails the procedure and notifies the higher layer.
- [Core5.4 Vol3 PartF, ATT transactions](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/host/attribute-protocol--att-.html):
  a transaction not completed within30seconds times out; it is not permission to send another request
  on the failed ATT bearer. These newer specification passages do not prove BSA1.6 conformance.

## Procedure

`tools/ble-deploy.py` moves the module on a mounted SD card and refuses to leave anything
else changed. It never reads or hashes anything named `*.keys`, and never prints INI values.

```sh
# 0. Card in the Mac (or any mount you give it). Read-only look first:
python3 tools/ble-deploy.py --sd /Volumes/SDCARD

# 1. Install the branch build. --expect-module pins which live module you believe is
#    installed; drop it if you have replaced the card's module since.
python3 tools/ble-deploy.py --sd /Volumes/SDCARD \
    --module mc_mitm/out/nintendo_nx_arm64_armv8a/release/mc_mitm.nsp \
    --ble on --backup-dir ~/.cache/missioncontrol/ble-bench-$(date -u +%Y%m%dT%H%M%SZ) \
    --apply
```

It prints the two files it changed and writes a `receipt.json` with hashes and a before/after
inventory of `atmosphere/`, `switch/` and `config/MissionControl/`. If anything else differed
it exits non-zero and says to restore.

```sh
# 2. Reboot to CFW. Wait ~25 s after the homepage: the host comes up on a radio cycle.
# 3. Controllers -> Change Grip/Order. Hold the Stadia sync button until the logo flashes fast.
#    Expect a pad within ~15 s. Touch nothing for two minutes, then press a few buttons.
# 4. Take the log off the card:
cp /Volumes/SDCARD/config/MissionControl/ble.log ~/ble-$(date -u +%H%M%S).log
# 5. Sleep/wake once with the pad connected, then power the pad off and pair it again
#    (that is radio_cycle's job, and U5's evidence).
```

Rollback, in any order:

```sh
python3 tools/ble-deploy.py --sd /Volumes/SDCARD --ble off --backup-dir <same dir> --apply   # keep the build, stop the radio
python3 tools/ble-deploy.py --sd /Volumes/SDCARD --backup-dir <same dir> --restore --apply   # put the old module back
```

## Stop conditions

Abort and pull the module back if any of these appear — they are the failure modes that cost
the port its earlier attempts, and each has a known cause:

- `0x300c71` after a sleep/wake: the stack stopped serving GATT until reboot. Expected if
  someone reintroduced `DisableBle`/`EnableBle` around sleep; the branch must not.
- Five or more consecutive `btdrvConnectGattServer` attempts without a connection: pending
  connections wedge the stack. The host is supposed to back off; if it does not, stop.
- Classic (non-BLE) controllers failing to reconnect after the boot radio cycle: revert, and
  the `[ble] enable=false` path must be re-checked for being a true no-op.
- Any `BluetoothCrash`/`btdrvEmulateBluetoothCrash` line outside a deliberate `radio_cycle`.

## Reading the log afterwards

The lines that decide U1/U2, in order, should read:

```
btdrvEnableBle rc=0x0
btdrvRegisterGattClient client_if=...           # borrowed from the slots btm registered
found controller ...
btdrvConnectGattServer ... rc=0x0
... ClientConnection result=0 status=0          # status 2 with result 0x1a = stale bond/slots
report map: 118 bytes, uses report ids
  input  report id=0x03 size=10
  output report id=0x05 size=4
Stadia controller presented to the system
```

If instead it says `no pad matched input report id=... size=...` followed by
`nothing presented: this device is not a pad this module knows how to decode`, the
`attributes (unrecognised device)` block beneath it is the evidence to act on: it lists every
handle, type, property and UUID the pad exposed, which is what a new entry in `ble_pad.hpp`
and `ble_gatt.cpp` has to be written against.
