#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT
includes=(-I "$here/source" -I "$root/mc_mitm/source/controllers" -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth")
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
    -fno-omit-frame-pointer -g "${includes[@]}" "$root/tests/stadia_host_tests.cpp" \
    "$here/source/gatt_bridge.cpp" -o "$build/tests"
reports="$root/tests/fixtures/stadia_bluetooth_reports.txt"
map="$root/tests/fixtures/stadia_bluetooth_report_map.hex"
"$build/tests" "$reports" "$map"
STADIA_HOST_BUILD_DIR="$build" bash "$here/build.sh"
python3 "$here/test_cli.py" "$build/stadia-host" "$reports" "$map"
if [[ "$(uname -s)" == Darwin ]]; then
    STADIA_HOST_BUILD_DIR="$build" bash "$here/build-gatt.sh"
    python3 "$here/test_gatt.py" "$build/stadia-gatt-capability" "$map"
fi
