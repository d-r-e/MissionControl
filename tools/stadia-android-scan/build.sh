#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export JAVA_HOME="${JAVA_HOME:-$(/usr/libexec/java_home -v 17)}"
export PATH="$JAVA_HOME/bin:$PATH"
sdk="${STADIA_ANDROID_SDK:-$HOME/.cache/missioncontrol/android-scan-sdk}"
platform="$sdk/platform-36_r02/android-36/android.jar"
bt="$sdk/build-tools_r36_macosx/android-16"
for f in "$platform" "$bt/aapt2" "$bt/d8" "$bt/apksigner" "$bt/zipalign"; do
    test -f "$f" || { printf 'Missing official SDK component: %s\n' "$f" >&2; exit 1; }
done
bash test.sh
mkdir -p build/classes build/dex
# Clean only this tool's generated class/dex files; never touch SDK, apps or SD.
find build/classes build/dex -type f -delete
"$JAVA_HOME/bin/javac" -Xlint:all -Werror -source 17 -target 17 -classpath "$platform" -d build/classes src/org/missioncontrol/stadia/scanprobe/*.java
"$bt/aapt2" link -o build/unsigned.apk -I "$platform" --manifest AndroidManifest.xml --min-sdk-version 31 --target-sdk-version 36
"$bt/d8" --min-api 31 --lib "$platform" --output build/dex $(find build/classes -name '*.class' -print)
python3 - <<'PY'
from pathlib import Path
import zipfile
with zipfile.ZipFile('build/unsigned.apk', 'a', compression=zipfile.ZIP_DEFLATED) as out:
    for dex in sorted(Path('build/dex').glob('classes*.dex')):
        out.write(dex, dex.name)
PY
"$bt/zipalign" -f 4 build/unsigned.apk build/aligned.apk
# A new local disposable development signer, never a user's existing key.
# Its private file is outside the repository, is not printed, and is never archived.
keydir="$(mktemp -d "$sdk/local-debug-signing-XXXXXX")"
chmod 700 "$keydir"
"$JAVA_HOME/bin/keytool" -genkeypair -keystore "$keydir/debug.p12" -storetype PKCS12 -storepass android -keypass android -alias localdebug -dname 'CN=MissionControl local disposable debug' -keyalg RSA -keysize 2048 -validity 30 >/dev/null 2>&1
chmod 600 "$keydir/debug.p12"
"$bt/apksigner" sign --ks "$keydir/debug.p12" --ks-key-alias localdebug --ks-pass pass:android --key-pass pass:android --out build/stadia-scan-evidence.apk build/aligned.apk
"$bt/apksigner" verify --verbose build/stadia-scan-evidence.apk
python3 - <<'PY'
from pathlib import Path
import hashlib
p=Path('build/stadia-scan-evidence.apk')
print('APK:', p, 'bytes=',p.stat().st_size,'SHA256=',hashlib.sha256(p.read_bytes()).hexdigest())
PY
