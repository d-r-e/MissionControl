/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ams::bluetooth::stadia_scan {
    // Private diagnostic markers, NOT advertised controller services. Only these
    // exact requests are eligible for the opt-in MITM translation.
    constexpr std::array<std::uint8_t, 16> MarkerBigEndian = {
        0x61, 0x74, 0x73, 0x63, 0x6d, 0x6b, 0xaf, 0x92,
        0x14, 0x4f, 0x7a, 0x3f, 0xae, 0xc3, 0x7b, 0x9c,
    };
    constexpr auto MarkerLittleEndian = [] {
        auto marker = MarkerBigEndian;
        marker[0] = 0x62;
        return marker;
    }();

    // Separate explicit hypothesis: only incomplete-list AD type 0x02, same
    // validated UUID16 byte order. Never broaden to an unfiltered scan.
    constexpr auto MarkerIncompleteBigEndian = [] {
        auto marker = MarkerBigEndian;
        marker[0] = 0x63;
        return marker;
    }();
    // Retired marker: the v21 trial incorrectly treated AD Flags as a supported
    // BleAdvertiseFilter condition. Identify it only for explicit rewrite refusal;
    // no new probe may use it. AD type support does NOT imply condition support.
    constexpr auto MarkerPrimaryFlags = [] {
        auto marker = MarkerBigEndian;
        marker[0] = 0x64;
        return marker;
    }();
    // Flags is a valid AD type, NOT a documented BleAdvertiseFilter condition.
    // Keep its retired marker recognizable only to reject rewriting it.
    constexpr auto MarkerNativeHid16 = [] {
        auto marker = MarkerBigEndian;
        marker[0] = 0x65;
        return marker;
    }();
    constexpr unsigned PrivateMarkerCount = 3; // Existing HID alias slots only.
    constexpr unsigned TrackedMarkerCount = PrivateMarkerCount + 3; // Native HID + two unchanged control UUID128 byte orders.
    static_assert(TrackedMarkerCount <= 8); // Tracker saturation/bridge masks are bytes.
    constexpr unsigned HidBridgeMarkerMask = 0x7;
    constexpr unsigned PrivateMarkerMask = (1u << PrivateMarkerCount) - 1;

    // Known synthetic advertisement used to validate event length convention.
    constexpr std::array<std::uint8_t, 16> ControlUuidCanonical = {
        0x9f, 0x6d, 0xca, 0x74, 0x69, 0xb7, 0x4a, 0x35,
        0x9f, 0x01, 0x9a, 0xb8, 0x41, 0xab, 0x58, 0x32,
    };
    constexpr auto ControlUuidLittleEndian = [] {
        auto uuid = ControlUuidCanonical;
        for (unsigned i = 0; i < uuid.size(); ++i) uuid[i] = ControlUuidCanonical[uuid.size() - 1 - i];
        return uuid;
    }();

    constexpr const auto &MarkerForSlot(unsigned slot) {
        return slot == 0 ? MarkerBigEndian : slot == 1 ? MarkerLittleEndian :
               slot == 2 ? MarkerIncompleteBigEndian : slot == 3 ? MarkerNativeHid16 :
               slot == 4 ? ControlUuidLittleEndian : ControlUuidCanonical;
    }

    // Documented btdrv BleAdvertiseFilter wire layout, 0x3e bytes. All fields
    // are bytes: no packed/unaligned integer accesses or guessed event offsets.
    struct Filter {
        std::uint8_t index;
        std::uint8_t size;
        std::uint8_t type;
        std::uint8_t data[29];
        std::uint8_t mask[29];
        std::uint8_t mask_size;
    };
    static_assert(sizeof(Filter) == 0x3e);
    static_assert(offsetof(Filter, mask) == 0x20);
    static_assert(offsetof(Filter, mask_size) == 0x3d);

    // Read-only mc service command 6; fixed-width, trivially copyable IPC data.
    struct Status {
        std::uint32_t protocol_version;
        std::uint32_t enabled;
        std::uint32_t added;
        std::uint32_t removed;
        std::uint32_t failed;
        std::uint32_t last_result;
        std::uint32_t active;
        std::uint32_t driver_advertisements;
        std::uint32_t driver_hid_advertisements;
        std::uint32_t managed_events;
        std::uint32_t scan_events;
        std::uint32_t scan_errors;
        std::uint32_t last_event_type;
        std::uint32_t last_scan_status;
        std::uint32_t last_scan_result;
        std::uint32_t ad_size_convention; // Low bits: 0 unknown, 1 data, 2 includes type, 3 conflicting; bit 2: canonical UUID128.
        std::uint32_t bridged_advertisements;
        std::uint32_t bridge_no_space;
        std::uint32_t bridge_unvalidated;
        std::uint32_t bridge_rejected;
        std::uint32_t bridge_enabled;
    };
    static_assert(sizeof(Status) == 84);
    constexpr std::uint32_t ProtocolVersion = 6; // Also tracks unchanged synthetic UUID128 filters.
    constexpr std::uint32_t StatusCommand = 6;

    enum class ByteOrder { None, BigEndian, LittleEndian, IncompleteBigEndian, PrimaryFlags, NativeHid16, NativeControlLittleEndian, NativeControlCanonical };

    inline ByteOrder IdentifyMarker(const Filter &filter) {
        // Exact BTM SmartDevice shape: ServiceUuid128, 16-byte UUID plus unused
        // zero, 16-byte all-ones mask. Ignore only bytes outside declared sizes.
        if (filter.type != 7 || filter.size != 17 || filter.mask_size != 16 || filter.data[16] != 0) {
            return ByteOrder::None;
        }
        bool big = true, little = true, incomplete = true, flags = true, native_hid = true;
        bool control_little = true, control_canonical = true;
        for (unsigned i = 0; i < 16; ++i) {
            if (filter.mask[i] != 0xff) return ByteOrder::None;
            big &= filter.data[i] == MarkerBigEndian[i];
            little &= filter.data[i] == MarkerLittleEndian[i];
            incomplete &= filter.data[i] == MarkerIncompleteBigEndian[i];
            flags &= filter.data[i] == MarkerPrimaryFlags[i];
            native_hid &= filter.data[i] == MarkerNativeHid16[i];
            control_little &= filter.data[i] == ControlUuidLittleEndian[i];
            control_canonical &= filter.data[i] == ControlUuidCanonical[i];
        }
        return big ? ByteOrder::BigEndian : little ? ByteOrder::LittleEndian :
               incomplete ? ByteOrder::IncompleteBigEndian : flags ? ByteOrder::PrimaryFlags :
               native_hid ? ByteOrder::NativeHid16 :
               control_little ? ByteOrder::NativeControlLittleEndian :
               control_canonical ? ByteOrder::NativeControlCanonical : ByteOrder::None;
    }

    inline bool Rewrite(Filter *filter, bool enabled, bool btm_client) {
        if (!filter || !enabled || !btm_client) return false;
        const auto order = IdentifyMarker(*filter);
        if (order == ByteOrder::None || order == ByteOrder::PrimaryFlags ||
            order == ByteOrder::NativeControlLittleEndian || order == ByteOrder::NativeControlCanonical) return false;
        // Explicit complete/incomplete ServiceUuid16 hypotheses. Complete-list
        // bytes 18 12 are hardware-validated; the separately tagged incomplete
        // variant changes only AD type 0x03 -> 0x02, never a global scan filter.
        // https://switchbrew.org/wiki/Bluetooth_Driver_services#BleAdvertiseFilter
        const auto index = filter->index;
        *filter = {};
        filter->index = index;
        // NativeHid16 uses the same documented complete UUID16 condition as
        // BigEndian, but its separate tracked slot NEVER synthesizes aliases.
        filter->type = order == ByteOrder::IncompleteBigEndian ? 2 : 3;
        filter->size = 3;
        filter->mask_size = 2;
        filter->mask[0] = filter->mask[1] = 0xff;
        filter->data[0] = order == ByteOrder::LittleEndian ? 0x12 : 0x18;
        filter->data[1] = order == ByteOrder::LittleEndian ? 0x18 : 0x12;
        return true;
    }

    // The explicit synthetic UUID128 control is accounted for without rewriting
    // a single request byte. It is never eligible for advertisement aliases.
    inline bool PrepareTrackedFilter(Filter *filter, bool enabled, bool btm_client) {
        if (!filter || !enabled || !btm_client) return false;
        const auto order = IdentifyMarker(*filter);
        return order == ByteOrder::NativeControlLittleEndian || order == ByteOrder::NativeControlCanonical ||
               Rewrite(filter, true, true);
    }
}
