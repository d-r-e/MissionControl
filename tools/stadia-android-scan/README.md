# Bounded Android scan-record evidence

Operator-approved scan-only helper, Android12+ / target36. No AndroidX, Gradle,
network, connection/pairing, privileged grants, location permission, key access,
radio toggles, or raw-data logging. Only BLUETOOTH_SCAN with neverForLocation;
that assertion can filter some beacons, so negative results are inconclusive.
The application requests normal Nearby Devices permission through its UI.

## What this measures (and does not)

The app reads public `ScanRecord.getBytes()` itself, removing uncertainty about
nRF Connect's closed-source RAW rendering. It accepts only fresh-timestamped,
complete LEGACY results. Candidate selection requires a full-name AD beginning
with the public ASCII brand prefix `Stadia`, gamepad appearance0x03c4 and HID1812.
A single stable framework device identity must supply at least3 records during
one20-second foreground window; multiple candidates, clock regression, callback
or layout limits abort. This identifies a unique **Stadia-like candidate**, not
a cryptographically authenticated controller. Operator isolation matters.

Device identity and raw bytes are RAM-only. Retained output is counts and AD
length/type/offset layout, including the earliest HID AD offset. No addresses,
full names, arbitrary UUIDs, manufacturer values, payload hex or identifiers are
serialized. Raw arrays are not modified or retained. One scan per operator
launch; its consumed flag is saved/restored across Activity recreation (missing
saved flag fails closed). Permission grant, resume and launch intents never start
a scan. A recreated consumed launch keeps Start disabled and OFF/no-retry guidance.

**NOT a direct over-air PDU capture.** Android may normalize and reassemble
records. In AOSP16, response data is appended after retained primary data and
empty/overflowing structures are trimmed. A legacy HID AD starting at offset31
or later is therefore response-carried **conditional on that pipeline and the
handset's corresponding implementation**. This does not establish a boundary at
31, simultaneous radio packets, or all future Stadia transmissions. OEM/module
behavior remains a premise. Padding is not treated as a real PDU boundary.

Sources inspected:
- [AOSP16 reassembler](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/tags/android-16.0.0_r1/system/gd/hci/le_scanning_reassembler.cc)
- [AOSP16 ScanController](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/tags/android-16.0.0_r1/android/app/src/com/android/bluetooth/le_scan/ScanController.java)
- [AOSP16 ScanRecord](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/tags/android-16.0.0_r1/framework/java/android/bluetooth/le/ScanRecord.java)
- [Nordic docs / closed-source notice](https://github.com/NordicSemiconductor/Android-nRF-Connect)

`setLegacy(false)` permits legacy events without requesting the AOSP legacy-
settings62-byte padding branch. No API36.1 passive-scan method is used; public
Android default scanning behavior is not described as passive. No service UUID
scan filter is installed that might discard the very response-carried data under
investigation. Other scan results are inspected transiently but never retained.

## Build and tests

`bash tools/stadia-android-scan/test.sh` runs portable Java parser/session/privacy
models and source/manifest contracts. They are NOT Android hardware proof.
`bash tools/stadia-android-scan/build.sh` additionally compiles the real Activity
against official android-36_r02/android.jar, assembles/dexes/signs and verifies APK.

Uses installed JDK17 and official Google platform36_r02/build-tools36.0.0 cached
at `~/.cache/missioncontrol/android-scan-sdk` (override STADIA_ANDROID_SDK only for
an equivalent reviewed layout). SDK provenance/checksums are in that cache's
packages.json. No system SDK/Java configuration or existing secrets are changed.
A disposable local development signing key is generated in a private cache
subdirectory, never printed or archived. APK output: build/stadia-scan-evidence.apk.
Do not silently update an installed package signed by a previous disposable key.

## Approved operator flow

This is a **separate Android-ON observation**, not a Switch/Stadia connection
trial. Switch netloader stays idle, Mac Bluetooth OFF. Stop nRF advertising and
leave its GATT connection closed; turn Android Bluetooth ON only for this test.
No pairing/bond changes. Background OS activity is not excluded by this helper.

After independent review, install ONLY package org.missioncontrol.stadia.scanprobe
on the single authorized Android device. Refuse if already installed unless an
explicit update is separately approved. No adb permission grants, appops/settings
writes, global Bluetooth reset, uninstall or capture of system logcat/bugreports.
Launch Activity, operator grants scan permission (does not start scanning), then
enter Stadia orange pairing mode and tap **Start ONE20-second scan** in THIS app,
not A on the Switch. Keep app foreground. Backgrounding or Stop aborts and stops
only this callback, exactly once; no automatic retry. Deadline is app-scheduled,
not a hardware-enforced scan timeout. `stopScan` returns void, so success means
stop requested, NOT independent physical-radio acknowledgement. A stop exception
requires operator Android Bluetooth OFF and no retry. Every completion also
instructs manual Bluetooth OFF/no retry, including SDK scan failures and start
exceptions even when the void stop call did not throw.

Read only this app's private sanitized files/summary.json using run-as after
completion. Debuggable is intentional for that scoped export; no broad logcat,
other app data or keys are read. Archive summary before a later explicit run;
the app's summary filename is overwritten on a future operator-started session.
Turn Stadia and Android Bluetooth OFF after observation before any Switch trial.
No Switch scan/connect or module/configuration deployment is part of this step.
