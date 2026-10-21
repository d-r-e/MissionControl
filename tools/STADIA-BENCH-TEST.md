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
| U4 | Does it drop after ~30 s (ndeadly's note on private Stadia BLE work)? | time from `presented` to `link ended`, with reason |
| U5 | Does a bond survive power cycling? | `btdrvGetPairedDeviceInfo key_present=/key_type=` lines across a reboot |
| U6 | Do `0x2A4E` (Protocol Mode) or `0x2A4C` (HID Control Point) need writing? | only answerable if notifications never start: the attribute table shows both |
| U7 | What is report `0x05` for? | **not** answered by writing it — the module never writes to the pad |

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
