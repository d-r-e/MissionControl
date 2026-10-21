/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ams::bluetooth::cached_scan {
    // P3 adds an opt-in exact native control-UUID requirement. P2 introduced
    // native-only HID16 support; P1's unsupported Flags condition stays retired.
    constexpr std::uint32_t ProtocolVersion = 3;
    constexpr std::uint8_t RequireControlUuid = 1;
    constexpr std::uint32_t StatusCommand = 12;
    struct Target { std::uint8_t address[6], flags, reserved; };
    struct Status {
        std::uint32_t protocol_version, event_type;
        std::uint64_t sequence;
        std::uint32_t result, scan_status, matched, device_type;
        std::uint32_t address_type, advertisement_count;
    };
    static_assert(sizeof(Target) == 8 && sizeof(Status) == 40);
    static_assert(offsetof(Target, flags) == 6 && offsetof(Target, reserved) == 7);
    static_assert(std::is_trivial_v<Target> && std::is_trivial_v<Status>);
    inline bool ValidTarget(const Target &target) {
        bool zero = true, broadcast = true;
        for (auto b : target.address) { zero &= b == 0; broadcast &= b == 0xff; }
        return !zero && !broadcast && !(target.flags & ~RequireControlUuid) && !target.reserved;
    }
    // No address/payload in output. Unmatched peers expose NO peer metadata.
    inline Status Inspect(const Target &target, std::uint64_t sequence, std::uint32_t event_type,
                          bool scan, std::uint32_t result, std::uint8_t status,
                          const std::uint8_t *address, std::uint8_t device_type,
                          std::uint8_t address_type, std::uint8_t count, bool control_uuid_present = false) {
        Status out{};
        out.protocol_version = ProtocolVersion;
        out.sequence = sequence;
        out.event_type = event_type;
        if (!scan) return out;
        out.result = result;
        out.scan_status = status;
        if (!ValidTarget(target) || !address || result || status != 2 || count > 10 ||
            std::memcmp(address, target.address, sizeof(target.address)) ||
            ((target.flags & RequireControlUuid) && !control_uuid_present)) return out;
        out.matched = 1;
        out.device_type = device_type;
        out.address_type = address_type;
        out.advertisement_count = count;
        return out;
    }
    // Native scan fields are transported as u8. HOS21.2 returned device_type0
    // for the independently verified BLE control; classification is not an
    // identity/transport gate. Exact address matching is enforced by Inspect.
    inline bool FreshTarget(const Status &s, std::uint64_t before) {
        return s.protocol_version == ProtocolVersion && s.sequence > before &&
            s.event_type == 6 && s.result == 0 && s.scan_status == 2 && s.matched == 1 &&
            s.device_type <= 0xff && s.address_type <= 3 &&
            s.advertisement_count > 0 && s.advertisement_count <= 10;
    }
}
