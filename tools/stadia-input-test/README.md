# Stadia Input Lab

Standalone, input-only Switch NRO for testing MissionControl's virtual controller.
No Bluetooth/GATT service, output, rumble, network, storage, pairing or sysmodule commands.
No dependencies beyond the repository's pinned libnx/devkitA64.

## Build

```sh
bash tools/build-switch.sh -C tools/stadia-input-test
```

Output: `package/switch/stadia-input-test.nro`. Launch **Stadia Input Lab** from hbmenu.
No console reboot or new MissionControl installation is needed.

## Use

- Pick the player slot that responds to your Stadia. The app prefers the first connected
  Pro Controller at startup; this is not device identity detection. Each slot is read separately,
  including eight players and handheld. Progress resets when that slot reconnects.
- Physical Stadia labels are shown. Position-based Switch mapping swaps A/B and X/Y:
  the checklist also shows the resulting Switch label.
- Press **and release** each of 16 buttons. Mint checklist dots mean completed;
  orange means pressed but not yet released. Filled diagram buttons show live presses.
- Move each stick left/right/up/down beyond75% travel, then release to center (within12.5%).
  Four direction dots plus one return-to-center check per stick make **26 checks** total.
  Live coordinates and observed axis ranges are shown. This is a functional check, not calibration.
- Touch the slot selector, Reset or Exit. Docked alternatives, using physical Stadia labels:
  **Options + D-pad left/right** changes slot; **Options + X** resets checks;
  hold **Menu + Options for1second** to exit. Frames with a complete shortcut chord do not
  add checks; Options pressed separately before/after the chord can still count.
  Do not hold those combinations during ordinary input checks.
- No test report is written to the SD card. Take a screenshot or report any failing checks.

## Limits

This observes **Switch HID input after MissionControl mapping**, not raw BLE packets.
L2/R2 are digital threshold presses on a Pro Controller, not analog trigger measurements.
Home and Capture are reserved for the system: manually verify that they open HOME/take a
screenshot; they are not part of the automatic checklist. Assistant/media keys have no
native Pro Controller mapping. Motion, battery, LEDs, vibration and raw output0x05 are
not tested or claimed supported.

## Offline tests / UI preview

```sh
c++ -std=c++20 -Wall -Wextra -Werror -fsanitize=address,undefined \
  tests/model-ui-tests.cpp -o /tmp/stadia-input-test-tests
/tmp/stadia-input-test-tests /tmp/stadia-input-test-preview.rgba
```

The optional preview is a1280x720 RGBA image rendered by the same production UI.
Tests cover face mappings, press/release, disconnect/reconnect, slot isolation, stick
thresholds/centering, out-of-range inputs, progress bounds and padded framebuffer strides.
