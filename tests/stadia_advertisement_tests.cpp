#include "stadia_ble_advertisement.hpp"
#include "stadia_ble_scan_tracker.hpp"
#include <cassert>
#include <bit>
#include <cstring>
#include <iostream>
using namespace ams::bluetooth::stadia_scan;

struct Packet {
    unsigned char prefix[13];
    Advertisement ads[10];
    std::uint8_t count;
    unsigned char suffix[17];
};

Packet HidPacket(unsigned convention) {
    Packet p{};
    std::memset(p.prefix, 0x42, sizeof(p.prefix));
    std::memset(p.suffix, 0x24, sizeof(p.suffix));
    p.ads[0].size = convention == 2 ? 3 : 2;
    p.ads[0].type = 3;
    p.ads[0].data[0] = 0x12;
    p.ads[0].data[1] = 0x18;
    p.count = 1;
    return p;
}

int main() {
    for (unsigned convention : {1u, 2u}) {
        auto flags_only = HidPacket(convention);
        const auto unchanged = flags_only;
        assert(BridgeAdvertisements(flags_only.ads, flags_only.count, 8, convention, true, true, true) == BridgeResult::Malformed);
        assert(std::memcmp(&flags_only, &unchanged, sizeof(unchanged)) == 0);
        Advertisement ads[10]{};
        ads[0].size = convention == 2 ? 17 : 16;
        ads[0].type = 7;
        std::copy(ControlUuidLittleEndian.begin(), ControlUuidLittleEndian.end(), ads[0].data);
        assert(FindSizeConvention(ads, 1) == convention);
        ads[0].type = 6;
        std::copy(ControlUuidCanonical.begin(), ControlUuidCanonical.end(), ads[0].data);
        assert(FindSizeConvention(ads, 1) == (convention | 4u));
        assert(FindSizeConvention(ads, 0) == 0 && FindSizeConvention(ads, 11) == 0);
        ads[0].data[0] ^= 1;
        assert(FindSizeConvention(ads, 1) == 0);
        ads[0].data[0] ^= 1;
        ads[1] = ads[0]; ads[1].size = convention == 2 ? 16 : 17;
        assert(FindSizeConvention(ads, 2) == 3);

        for (unsigned mask = 1; mask <= PrivateMarkerMask; ++mask) {
            auto p = HidPacket(convention);
            const auto original = p;
            assert(BridgeAdvertisements(p.ads, p.count, mask, convention, true, true, true) == BridgeResult::Appended);
            assert(p.count == 1 + std::popcount(mask));
            assert(std::memcmp(p.prefix, original.prefix, sizeof(p.prefix)) == 0);
            assert(std::memcmp(p.suffix, original.suffix, sizeof(p.suffix)) == 0);
            assert(std::memcmp(&p.ads[0], &original.ads[0], sizeof(Advertisement)) == 0);
            unsigned index = 1;
            for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) {
                if (!(mask & (1u << slot))) continue;
                const auto &ad = p.ads[index++];
                assert(ad.type == 7 && ad.size == (convention == 2 ? 17 : 16));
                assert(UuidEquals(ad, MarkerForSlot(slot)));
                for (unsigned off = 16; off < 29; ++off) assert(ad.data[off] == 0);
            }
            const auto after = p;
            assert(BridgeAdvertisements(p.ads, p.count, mask, convention, true, true, true) == BridgeResult::Unchanged);
            assert(std::memcmp(&p, &after, sizeof(p)) == 0); // Idempotent, no duplicate aliases.
        }
        for (unsigned gate = 0; gate < 4; ++gate) {
            auto p = HidPacket(convention);
            const auto before = p;
            assert(BridgeAdvertisements(p.ads, p.count, gate == 3 ? 0 : 1, convention,
                gate != 0, gate != 1, gate != 2) == BridgeResult::Unchanged);
            assert(std::memcmp(&p, &before, sizeof(p)) == 0);
        }
        for (unsigned malformed = 0; malformed < 6; ++malformed) {
            auto p = HidPacket(convention);
            if (malformed == 0) p.count = 255;
            if (malformed == 1) p.ads[0].size = 255;
            if (malformed == 2) p.ads[0].size += 1; // Odd UUID16 data, using learned convention.
            if (malformed == 3) { p.count = 2; p.ads[1].type = 7; p.ads[1].size = 3; }
            if (malformed == 4) { p.count = 2; p.ads[1].size = 255; }
            const auto before = p;
            assert(BridgeAdvertisements(p.ads, p.count, malformed == 5 ? 8 : 1, convention,
                true, true, true) == BridgeResult::Malformed);
            assert(std::memcmp(&p, &before, sizeof(p)) == 0);
        }
        for (unsigned count : {9u, 10u}) {
            auto p = HidPacket(convention);
            p.count = count;
            for (unsigned i = 1; i < count; ++i) { p.ads[i].size = 1; p.ads[i].type = 1; }
            const auto before = p;
            assert(BridgeAdvertisements(p.ads, p.count, 3, convention, true, true, true) == BridgeResult::NoSpace);
            assert(std::memcmp(&p, &before, sizeof(p)) == 0); // Atomic: no partial two-marker append.
        }
        auto three_markers = HidPacket(convention);
        three_markers.count = 8;
        for (unsigned i = 1; i < 8; ++i) { three_markers.ads[i].size = 1; three_markers.ads[i].type = 1; }
        const auto before_three = three_markers;
        assert(BridgeAdvertisements(three_markers.ads, three_markers.count, PrivateMarkerMask, convention,
            true, true, true) == BridgeResult::NoSpace);
        assert(std::memcmp(&three_markers, &before_three, sizeof(three_markers)) == 0);
        auto exact = HidPacket(convention);
        exact.count = 9;
        for (unsigned i = 1; i < 9; ++i) { exact.ads[i].size = 1; exact.ads[i].type = 1; }
        assert(BridgeAdvertisements(exact.ads, exact.count, 1, convention, true, true, true) == BridgeResult::Appended);
        assert(exact.count == 10);
        auto partial = HidPacket(convention);
        partial.ads[0].type = 2;
        assert(BridgeAdvertisements(partial.ads, partial.count, 4, convention, true, true, true) == BridgeResult::Appended);
        assert(partial.count == 2 && UuidEquals(partial.ads[1], MarkerIncompleteBigEndian));
        auto mixed = HidPacket(convention);
        mixed.count = 2;
        mixed.ads[1].size = convention == 2 ? 17 : 16;
        mixed.ads[1].type = 7;
        std::copy(ControlUuidLittleEndian.begin(), ControlUuidLittleEndian.end(), mixed.ads[1].data);
        const auto original_mixed = mixed;
        assert(BridgeAdvertisements(mixed.ads, mixed.count, 1, convention, true, true, true) == BridgeResult::Appended);
        assert(mixed.count == 3 && std::memcmp(mixed.ads, original_mixed.ads, 2 * sizeof(Advertisement)) == 0);
        auto nonhid = HidPacket(convention);
        nonhid.ads[0].data[1] = 0x19;
        const auto before = nonhid;
        assert(BridgeAdvertisements(nonhid.ads, nonhid.count, 1, convention, true, true, true) == BridgeResult::Unchanged);
        assert(std::memcmp(&nonhid, &before, sizeof(nonhid)) == 0);
    }
    for (unsigned format : {5u, 6u}) {
        auto p = HidPacket(format & 3u);
        assert(BridgeAdvertisements(p.ads, p.count, PrivateMarkerMask, format, true, true, true) == BridgeResult::Appended);
        assert(p.count == 1 + PrivateMarkerCount);
        for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) {
            for (unsigned i = 0; i < 16; ++i) assert(p.ads[slot + 1].data[i] == MarkerForSlot(slot)[15 - i]);
        }
        const auto before = p;
        assert(BridgeAdvertisements(p.ads, p.count, PrivateMarkerMask, format, true, true, true) == BridgeResult::Unchanged);
        assert(std::memcmp(&p, &before, sizeof(p)) == 0);
    }
    for (unsigned unknown : {0u, 3u, 4u, 7u, 255u}) {
        auto p = HidPacket(2); const auto before = p;
        assert(BridgeAdvertisements(p.ads, p.count, 1, unknown, true, true, true) == BridgeResult::Unvalidated);
        assert(std::memcmp(&p, &before, sizeof(p)) == 0);
    }
    // Byte-view and typed-array adapters must have identical results, including
    // deliberately unaligned response storage and every possible declared size.
    for (unsigned format : {1u, 2u, 5u, 6u}) {
        for (unsigned declared_size = 0; declared_size < 256; ++declared_size) {
            for (unsigned offset : {1u, 3u, 7u}) {
                Advertisement typed[10]{};
                typed[0] = HidPacket(format & 3u).ads[0];
                typed[0].size = declared_size;
                std::array<std::uint8_t, 328> bytes;
                bytes.fill(0xa5);
                std::memcpy(bytes.data() + offset, typed, sizeof(typed));
                const auto before = bytes;
                std::uint8_t typed_count = 1, byte_count = 1;
                const auto expected = BridgeAdvertisements(typed, typed_count, PrivateMarkerMask, format, true, true, true);
                const auto actual = BridgeAdvertisementView(ByteAdvertisements{std::span<std::uint8_t>{
                    bytes.data() + offset, sizeof(typed)}}, byte_count, PrivateMarkerMask, format, true, true, true);
                assert(actual == expected && byte_count == typed_count);
                assert(std::memcmp(bytes.data() + offset, typed, sizeof(typed)) == 0);
                for (unsigned i = 0; i < offset; ++i) assert(bytes[i] == before[i]);
                for (unsigned i = offset + sizeof(typed); i < bytes.size(); ++i) assert(bytes[i] == before[i]);
            }
        }
    }
    for (unsigned length = 0; length < 310; ++length) {
        std::array<std::uint8_t, 310> bytes{};
        const auto before = bytes;
        std::uint8_t count = 1;
        assert(BridgeAdvertisementView(ByteAdvertisements{std::span<std::uint8_t>{bytes.data(), length}},
            count, 1, 2, true, true, true) == BridgeResult::Malformed);
        assert(count == 1 && bytes == before);
    }
    Tracker tracker;
    assert(tracker.GetBridgeMarkers() == 0 && !tracker.IsTrackedIndex(1));
    Tracker failed;
    failed.RecordCondition(1, ByteOrder::BigEndian, true, 1);
    failed.RecordCondition(1, ByteOrder::BigEndian, true, 0);
    assert(failed.GetBridgeMarkers() == 0); // Error uncertainty never permits synthesis.
    tracker.RecordCondition(1, ByteOrder::BigEndian, true, 0);
    assert(tracker.GetBridgeMarkers() == 1 && tracker.IsTrackedIndex(1) && !tracker.IsTrackedIndex(2));
    tracker.RecordCondition(2, ByteOrder::LittleEndian, true, 0);
    assert(tracker.GetBridgeMarkers() == 3);
    tracker.RecordIndexDeletion(1, 0);
    assert(tracker.GetBridgeMarkers() == 2);
    tracker.RecordIndexDeletion(2, 0);
    assert(tracker.GetBridgeMarkers() == 0 && !tracker.IsTrackedIndex(1) && !tracker.IsTrackedIndex(2));
    tracker.LearnSizeConvention(0);
    assert(tracker.GetStatus().ad_size_convention == 0);
    tracker.LearnSizeConvention(2); tracker.LearnSizeConvention(2);
    assert(tracker.GetStatus().ad_size_convention == 2);
    tracker.LearnSizeConvention(1); tracker.LearnSizeConvention(2);
    assert(tracker.GetStatus().ad_size_convention == 3); // Conflict stays latched until reboot.
    tracker.RecordBridge(BridgeResult::Appended);
    tracker.RecordBridge(BridgeResult::NoSpace);
    tracker.RecordBridge(BridgeResult::Unvalidated);
    tracker.RecordBridge(BridgeResult::Malformed);
    const auto status = tracker.GetStatus();
    assert(status.bridged_advertisements == 1 && status.bridge_no_space == 1);
    assert(status.bridge_unvalidated == 1 && status.bridge_rejected == 1);
    std::cout << "Bounded BTM-copy advertisement bridge and learned-format tests passed\n";
}
