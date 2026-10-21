#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
build="${STADIA_HOST_BUILD_DIR:-$here/build}"
mkdir -p "$build"
frameworks=()
if [[ "$(uname -s)" == Darwin ]]; then frameworks=(-framework IOKit -framework CoreFoundation); fi
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -O2 \
    -I "$here/source" -I "$root/mc_mitm/source/controllers" \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    "$here/source/main.cpp" "$here/source/native_hid.cpp" \
    "${frameworks[@]}" -o "$build/stadia-host"
printf 'Built %s (no device opened)\n' "$build/stadia-host"
