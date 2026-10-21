/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstdint>
#include "stadia_ble_scan_filter.hpp"

namespace missioncontrol::probe {
    // BTM SmartDevice scanning always builds a ServiceUuid128 filter. It copies
    // all 16 uuid bytes and ignores GattAttributeUuid.size; libnx does not expand
    // UUID16 values. A zero-padded {0x12,0x18} is NOT a HID service filter.
    // https://switchbrew.org/wiki/BTM_services#StartBleScanForSmartDevice
    // Full Bluetooth-base HID UUID: 00001812-0000-1000-8000-00805f9b34fb,
    // least-significant byte first, as used in BLE advertising.
    constexpr std::array<std::uint8_t, 16> HidScanUuid128 = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,
    };
    // Synthetic positive control: 9f6dca74-69b7-4a35-9f01-9ab841ab5832.
    // Advertised by tools/stadia-ble-control.swift, with no name/payload.
    // Test both API byte orders: unlike the HID case this is a real UUID128 AD.
    constexpr auto ControlUuidCanonical = ams::bluetooth::stadia_scan::ControlUuidCanonical;
    constexpr auto ControlUuidLittleEndian = ams::bluetooth::stadia_scan::ControlUuidLittleEndian;
    // Whether the Bluetooth-base HID filter matches UUID16 AD is NOT established.
}
