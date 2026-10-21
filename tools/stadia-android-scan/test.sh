#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
JAVA_HOME="${JAVA_HOME:-$(/usr/libexec/java_home -v 17 2>/dev/null || true)}"
JAVAC="${JAVA_HOME:+$JAVA_HOME/bin/}javac"
JAVA="${JAVA_HOME:+$JAVA_HOME/bin/}java"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
"$JAVAC" -Xlint:all -Werror -d "$tmp" src/org/missioncontrol/stadia/scanprobe/ScanEvidence.java tests/ScanEvidenceTest.java
"$JAVA" -ea -cp "$tmp" org.missioncontrol.stadia.scanprobe.ScanEvidenceTest
python3 - <<'PY'
from pathlib import Path
import xml.etree.ElementTree as E
ns = '{http://schemas.android.com/apk/res/android}'
root = E.parse('AndroidManifest.xml').getroot()
assert [p.attrib[ns+'name'] for p in root.findall('uses-permission')] == ['android.permission.BLUETOOTH_SCAN']
assert root.find('uses-permission').attrib[ns+'usesPermissionFlags'] == 'neverForLocation'
assert root.find('application').attrib[ns+'allowBackup'] == 'false'
assert root.find('application').attrib[ns+'theme'] == '@android:style/Theme.Material.Light.NoActionBar'
assert len(root.findall('./application/*')) == 1
s = Path('src/org/missioncontrol/stadia/scanprobe/ScanActivity.java').read_text()
for forbidden in ('.connectGatt(', '.createBond(', '.getAddress(', '.getName(', '.startAdvertising(', '.enable(', '.disable(', '.setScanType(', 'printStackTrace', 'android.util.Log', 'getIntent('):
    assert forbidden not in s, forbidden
assert s.count('scanner.startScan(') == 1 and s.count('scanner.stopScan(') == 1
assert '.setLegacy(false)' in s and 'result.getTimestampNanos() >= startNs' in s
assert 'finishScan("backgrounded")' in s and 'stopCalled = true;' in s
assert 'state.putBoolean("scan_consumed", launch.consumed())' in s
assert 'state.getBoolean("scan_consumed", true)' in s
assert 'if (!launch.consume()) return;' in s
assert 'No retry without new approval (including SDK/start failures)' in s
assert 'finishScan("scan_failed")' in s and 'finishScan("start_exception")' in s
assert 'setOnApplyWindowInsetsListener' in s and 'WindowInsets.Type.displayCutout()' in s
assert 'margin + bars.top' in s and 'scroll.requestApplyInsets()' in s
print('Android source/manifest scan-only permissions, no identity/payload logging or auto-connect/enable contracts passed (not hardware proof).')
PY
