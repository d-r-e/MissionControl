#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
if [[ "$(uname -s)" != Darwin ]]; then
    echo 'The CoreBluetooth capability client builds on macOS only; portable replay remains available.' >&2
    exit 1
fi
build="${STADIA_HOST_BUILD_DIR:-$here/build}"
mkdir -p "$build"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -O2 \
    -I "$root/mc_mitm/source/controllers" -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    -c "$here/source/gatt_bridge.cpp" -o "$build/gatt_bridge.o"
swiftc -swift-version 5 -warnings-as-errors -O \
    -import-objc-header "$here/source/gatt_bridge.h" \
    "$here/gatt-capability.swift" "$build/gatt_bridge.o" \
    -framework CoreBluetooth -framework Foundation -Xlinker -lc++ \
    -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker "$here/gatt-capability.Info.plist" \
    -o "$build/stadia-gatt-capability"
printf 'Built %s (no Bluetooth manager initialized)\n' "$build/stadia-gatt-capability"
