#include "target_address.hpp"
#include "start_gate.hpp"
#include "control_evidence.hpp"
#include "discovery_evidence.hpp"
#include "module_build_id.hpp"
#include "scan_uuid.hpp"
#include "stadia_ble_scan_filter.hpp"
#include "stadia_ble_scan_tracker.hpp"
#include "connection_client.hpp"
#include "ble_cached_scan_protocol.hpp"
#include <cstring>
#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

using missioncontrol::probe::ParseTargetAddress;

void TestUnchangedNativeControlFilters() {
    namespace scan = ams::bluetooth::stadia_scan;
    const std::array orders{scan::ByteOrder::NativeControlLittleEndian, scan::ByteOrder::NativeControlCanonical};
    const std::array uuids{scan::ControlUuidLittleEndian, scan::ControlUuidCanonical};
    for (unsigned i = 0; i < uuids.size(); ++i) {
        scan::Filter filter{};
        filter.index = 255; filter.type = 7; filter.size = 17; filter.mask_size = 16;
        std::copy(uuids[i].begin(), uuids[i].end(), filter.data);
        std::fill_n(filter.mask, 16, 0xff);
        const auto original = filter;
        assert(scan::IdentifyMarker(filter) == orders[i]);
        assert(!scan::Rewrite(&filter, true, true));
        assert(!scan::PrepareTrackedFilter(&filter, false, true));
        assert(!scan::PrepareTrackedFilter(&filter, true, false));
        assert(std::memcmp(&filter, &original, sizeof(filter)) == 0);
        assert(scan::PrepareTrackedFilter(&filter, true, true));
        assert(std::memcmp(&filter, &original, sizeof(filter)) == 0); // Exact native UUID128 bytes, no rewrite.
        auto near_miss = original; near_miss.data[0] ^= 0x80;
        assert(!scan::PrepareTrackedFilter(&near_miss, true, true));
        near_miss = original; near_miss.mask[2] = 0;
        assert(!scan::PrepareTrackedFilter(&near_miss, true, true));
        near_miss = original; near_miss.size = 16;
        assert(!scan::PrepareTrackedFilter(&near_miss, true, true));
        scan::Tracker tracker;
        tracker.RecordCondition(255, orders[i], true, 0);
        assert(tracker.GetStatus().active == 1 && tracker.GetBridgeMarkers() == 0);
        tracker.RecordCondition(255, orders[i], false, 0x1234);
        assert(tracker.GetStatus().failed == 1 && tracker.GetStatus().active == 1);
        tracker.RecordIndexDeletion(255, 0);
        assert(tracker.GetStatus().active == 0 && tracker.GetStatus().removed == 1);
    }
    assert(!scan::PrepareTrackedFilter(nullptr, true, true));
    scan::Tracker shared_index;
    shared_index.RecordCondition(0, orders[0], true, 0);
    shared_index.RecordCondition(0, orders[1], true, 0);
    shared_index.RecordCondition(0, orders[0], false, 0);
    shared_index.RecordCondition(0, orders[0], false, 0); // Idempotent removal cannot hide the other byte order.
    assert(shared_index.GetStatus().active == 1 && shared_index.IsTrackedIndex(0));
    assert(shared_index.GetBridgeMarkers() == 0);
    shared_index.RecordCondition(0, orders[1], false, 0);
    assert(shared_index.GetStatus().active == 0 && shared_index.GetStatus().added == 2 && shared_index.GetStatus().removed == 2);
}

int main() {
    TestUnchangedNativeControlFilters();
    missioncontrol::probe::StartGate held_at_launch;
    assert(!held_at_launch.Update(true, true));
    assert(!held_at_launch.Update(true, false));
    assert(!held_at_launch.Update(true, true));
    assert(!held_at_launch.Update(false, false));
    assert(!held_at_launch.Update(true, false));
    assert(held_at_launch.Update(true, true));
    assert(!held_at_launch.Update(false, false));
    assert(!held_at_launch.Update(true, true)); // One-shot.
    missioncontrol::probe::StartGate initially_released;
    assert(!initially_released.Update(false, false));
    assert(initially_released.Update(true, true));
    std::array<std::uint8_t, 6> address{};
    const std::array<std::uint8_t, 6> expected{1, 0x23, 0x45, 0x67, 0x89, 0xab};
    for (const std::string text : {"01:23:45:67:89:aB", "01:23:45:67:89:AB\r\n", "01:23:45:67:89:ab \t"}) {
        assert(ParseTargetAddress(text.data(), text.size(), &address));
        assert(address == expected);
    }
    for (const std::string text : {"", "01:23:45:67:89:a", "01:23:45:67:89:abc",
                                  "01-23:45:67:89:ab", "01:23:45:67:89:ag",
                                  "00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff",
                                  "01:23:45:67:89:ab\nother", " 01:23:45:67:89:ab"}) {
        assert(!ParseTargetAddress(text.data(), text.size(), &address));
        assert(address == expected);
    }
    const std::string nul("01:23:45:67:89:ab\0", 18);
    assert(!ParseTargetAddress(nul.data(), nul.size(), &address));
    assert(!ParseTargetAddress(nullptr, 17, &address));
    assert(!ParseTargetAddress("01:23:45:67:89:ab", 17, nullptr));
    assert(!ParseTargetAddress("short", 64, &address));
    assert(address == expected);
    // SmartDevice requires all 16 bytes, regardless of the SDK's size member.
    // Compare against the canonical UUID reversed into BLE advertising order.
    const std::array<std::uint8_t, 16> canonical{
        0x00, 0x00, 0x18, 0x12, 0x00, 0x00, 0x10, 0x00,
        0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb
    };
    for (unsigned i = 0; i < canonical.size(); ++i) {
        assert(missioncontrol::probe::HidScanUuid128[i] == canonical[canonical.size() - i - 1]);
    }
    assert(missioncontrol::probe::HidScanUuid128[0] != 0x12); // Not zero-padded UUID16.
    namespace scan = ams::bluetooth::stadia_scan;
    scan::Status before{};
    before.protocol_version = scan::ProtocolVersion;
    before.enabled = before.bridge_enabled = 1;
    before.ad_size_convention = 2;
    auto fresh = before;
    fresh.active = fresh.added = fresh.driver_hid_advertisements = fresh.bridged_advertisements = 1;
    assert(missioncontrol::probe::HasFreshHidControlEvidence(before, fresh));
    assert(!missioncontrol::probe::HasFreshHidControlEvidence(before, before));
    for (auto member : {&scan::Status::added, &scan::Status::driver_hid_advertisements,
                        &scan::Status::bridged_advertisements, &scan::Status::active,
                        &scan::Status::bridge_enabled, &scan::Status::enabled}) {
        auto stale = fresh;
        stale.*member = 0;
        assert(!missioncontrol::probe::HasFreshHidControlEvidence(before, stale));
    }
    for (auto member : {&scan::Status::failed, &scan::Status::ad_size_convention,
                        &scan::Status::protocol_version, &scan::Status::active}) {
        auto conflict = fresh;
        ++(conflict.*member);
        assert(!missioncontrol::probe::HasFreshHidControlEvidence(before, conflict));
    }
    auto busy = before;
    busy.active = 1;
    assert(!missioncontrol::probe::HasFreshHidControlEvidence(busy, fresh));
    for (unsigned i = 0; i < 16; ++i) {
        assert(missioncontrol::probe::ControlUuidLittleEndian[i] == missioncontrol::probe::ControlUuidCanonical[15 - i]);
    }
    for (const auto &uuid : {missioncontrol::probe::ControlUuidCanonical, missioncontrol::probe::ControlUuidLittleEndian}) {
        scan::Filter control{};
        control.type = 7;
        control.size = 17;
        control.mask_size = 16;
        std::copy(uuid.begin(), uuid.end(), control.data);
        std::fill_n(control.mask, 16, 0xff);
        const auto before = control;
        assert(!scan::Rewrite(&control, true, true));
        assert(std::memcmp(&control, &before, sizeof(control)) == 0);
    }
    for (bool little : {false, true}) {
        scan::Filter original{};
        original.index = little ? 255 : 0;
        original.type = 7;
        original.size = 17;
        original.mask_size = 16;
        const auto &marker = little ? scan::MarkerLittleEndian : scan::MarkerBigEndian;
        std::copy(marker.begin(), marker.end(), original.data);
        std::fill_n(original.mask, 16, 0xff);
        original.data[28] = 0xab; // Undeclared bytes are irrelevant, then cleared.
        auto check = original;
        assert(!scan::Rewrite(&check, false, true));
        assert(std::memcmp(&check, &original, sizeof(check)) == 0);
        assert(!scan::Rewrite(&check, true, false));
        assert(std::memcmp(&check, &original, sizeof(check)) == 0);
        assert(scan::Rewrite(&check, true, true));
        assert(check.index == original.index && check.type == 3);
        assert(check.size == 3 && check.mask_size == 2);
        assert(check.data[0] == (little ? 0x12 : 0x18));
        assert(check.data[1] == (little ? 0x18 : 0x12));
        assert(check.mask[0] == 0xff && check.mask[1] == 0xff);
        for (unsigned i = 2; i < 29; ++i) assert(check.data[i] == 0 && check.mask[i] == 0);
        auto deletion = original;
        assert(scan::Rewrite(&deletion, true, true));
        assert(std::memcmp(&deletion, &check, sizeof(check)) == 0); // Symmetric add/delete.
        assert(!scan::Rewrite(&check, true, true)); // No retranslation of UUID16.
        for (unsigned i = 0; i < 16; ++i) {
            auto invalid = original;
            invalid.data[i] ^= 0x80; // Never mutate into another valid private marker.
            const auto before = invalid;
            assert(!scan::Rewrite(&invalid, true, true));
            assert(std::memcmp(&invalid, &before, sizeof(invalid)) == 0);
            invalid = original;
            invalid.mask[i] = 0xfe;
            assert(!scan::Rewrite(&invalid, true, true));
        }
        for (unsigned which = 0; which < 4; ++which) {
            auto invalid = original;
            if (which == 0) invalid.type = 6;
            if (which == 1) invalid.size = 16;
            if (which == 2) invalid.mask_size = 15;
            if (which == 3) invalid.data[16] = 1;
            const auto before = invalid;
            assert(!scan::Rewrite(&invalid, true, true));
            assert(std::memcmp(&invalid, &before, sizeof(invalid)) == 0);
        }
        auto unrelated = original;
        std::copy(missioncontrol::probe::HidScanUuid128.begin(), missioncontrol::probe::HidScanUuid128.end(), unrelated.data);
        assert(!scan::Rewrite(&unrelated, true, true));
    }
    scan::Filter native{};
    native.index = 19; native.type = 7; native.size = 17; native.mask_size = 16;
    std::copy(scan::MarkerNativeHid16.begin(), scan::MarkerNativeHid16.end(), native.data);
    std::fill_n(native.mask, 16, 0xff);
    const auto native_request = native;
    assert(scan::IdentifyMarker(native) == scan::ByteOrder::NativeHid16);
    auto denied = native;
    assert(!scan::Rewrite(&denied, false, true) && !scan::Rewrite(&denied, true, false));
    assert(std::memcmp(&denied, &native, sizeof(native)) == 0);
    assert(scan::Rewrite(&native, true, true));
    assert(native.index == 19 && native.type == 3 && native.size == 3 && native.mask_size == 2);
    assert(native.data[0] == 0x18 && native.data[1] == 0x12 && native.mask[0] == 0xff && native.mask[1] == 0xff);
    for (unsigned i = 2; i < 29; ++i) assert(native.data[i] == 0 && native.mask[i] == 0);
    auto native_delete = native_request;
    assert(scan::Rewrite(&native_delete, true, true));
    assert(std::memcmp(&native_delete, &native, sizeof(native)) == 0);
    auto legacy = native_request;
    std::copy(scan::MarkerBigEndian.begin(), scan::MarkerBigEndian.end(), legacy.data);
    assert(scan::Rewrite(&legacy, true, true));
    assert(std::memcmp(&legacy, &native, sizeof(native)) == 0); // Previously accepted driver condition, byte-for-byte.
    auto retired_flags = native_request;
    std::copy(scan::MarkerPrimaryFlags.begin(), scan::MarkerPrimaryFlags.end(), retired_flags.data);
    const auto retired_before = retired_flags;
    assert(scan::IdentifyMarker(retired_flags) == scan::ByteOrder::PrimaryFlags);
    assert(!scan::Rewrite(&retired_flags, true, true));
    assert(std::memcmp(&retired_flags, &retired_before, sizeof(retired_flags)) == 0); // NEVER emit unsupported condition1.
    for (unsigned i = 0; i < 16; ++i) {
        auto invalid = native_request;
        invalid.data[i] ^= 0x80;
        assert(!scan::Rewrite(&invalid, true, true));
        invalid = native_request; invalid.mask[i] = 0xfe;
        assert(!scan::Rewrite(&invalid, true, true));
    }
    scan::Tracker primary;
    primary.RecordCondition(19, scan::ByteOrder::PrimaryFlags, true, 0);
    assert(primary.GetStatus().active == 0); // Retired marker is not tracked as a real condition.
    primary.RecordCondition(19, scan::ByteOrder::NativeHid16, true, 0);
    assert(primary.GetStatus().active == 1 && primary.IsTrackedIndex(19));
    assert(primary.GetBridgeMarkers() == 0); // Native-only UUID16 NEVER synthesizes an alias.
    primary.RecordCondition(19, scan::ByteOrder::BigEndian, true, 0);
    assert(primary.GetBridgeMarkers() == 1 && primary.GetStatus().active == 2);
    primary.RecordCondition(19, scan::ByteOrder::NativeHid16, false, 0);
    assert(primary.GetBridgeMarkers() == 1 && primary.GetStatus().removed == 1);
    primary.RecordIndexDeletion(19, 0);
    assert(primary.GetStatus().active == 0 && primary.GetStatus().removed == 2);
    primary.RecordCondition(19, scan::ByteOrder::NativeHid16, true, 0);
    primary.RecordCondition(19, scan::ByteOrder::NativeHid16, false, 0x1234);
    assert(primary.GetStatus().active == 1 && primary.GetStatus().failed == 1 && primary.GetBridgeMarkers() == 0);
    primary.RecordIndexDeletion(19, 0);
    assert(primary.GetStatus().active == 0);
    assert(!scan::Rewrite(nullptr, true, true));
    scan::Tracker tracker;
    assert(tracker.GetStatus().protocol_version == scan::ProtocolVersion);
    assert(tracker.GetStatus().active == 0);
    tracker.ObserveAdvertisement(true);
    assert(tracker.GetStatus().driver_advertisements == 0);
    tracker.RecordCondition(7, scan::ByteOrder::None, true, 0);
    assert(tracker.GetStatus().added == 0);
    tracker.RecordCondition(7, scan::ByteOrder::BigEndian, true, 0x1234);
    assert(tracker.GetStatus().failed == 1 && tracker.GetStatus().active == 0);
    tracker.RecordCondition(7, scan::ByteOrder::BigEndian, true, 0);
    tracker.RecordCondition(7, scan::ByteOrder::LittleEndian, true, 0);
    assert(tracker.GetStatus().active == 2 && tracker.GetStatus().added == 2);
    tracker.RecordCondition(7, scan::ByteOrder::BigEndian, false, 0);
    assert(tracker.GetStatus().active == 1 && tracker.GetStatus().removed == 1);
    tracker.ObserveAdvertisement(false);
    tracker.ObserveAdvertisement(true);
    assert(tracker.GetStatus().driver_advertisements == 2);
    assert(tracker.GetStatus().driver_hid_advertisements == 1);
    tracker.RecordCondition(7, scan::ByteOrder::LittleEndian, false, 0x88);
    assert(tracker.GetStatus().active == 1 && tracker.GetStatus().failed == 2);
    tracker.RecordIndexDeletion(8, 0x99); // Unrelated deletion must not affect counters.
    assert(tracker.GetStatus().last_result == 0x88 && tracker.GetStatus().failed == 2);
    tracker.RecordIndexDeletion(7, 0x99);
    assert(tracker.GetStatus().active == 1 && tracker.GetStatus().failed == 3);
    tracker.RecordIndexDeletion(7, 0);
    assert(tracker.GetStatus().active == 0 && tracker.GetStatus().removed == 2);
    tracker.ObserveAdvertisement(true);
    assert(tracker.GetStatus().driver_advertisements == 2);

    scan::Filter incomplete{};
    incomplete.index = 9; incomplete.type = 7; incomplete.size = 17; incomplete.mask_size = 16;
    std::copy(scan::MarkerIncompleteBigEndian.begin(), scan::MarkerIncompleteBigEndian.end(), incomplete.data);
    std::fill_n(incomplete.mask, 16, 0xff);
    const auto incomplete_request = incomplete;
    assert(scan::IdentifyMarker(incomplete) == scan::ByteOrder::IncompleteBigEndian);
    assert(scan::Rewrite(&incomplete, true, true));
    assert(incomplete.index == 9 && incomplete.type == 2 && incomplete.size == 3 && incomplete.mask_size == 2);
    assert(incomplete.data[0] == 0x18 && incomplete.data[1] == 0x12);
    assert(incomplete.mask[0] == 0xff && incomplete.mask[1] == 0xff);
    auto incomplete_delete = incomplete_request;
    assert(scan::Rewrite(&incomplete_delete, true, true));
    assert(std::memcmp(&incomplete_delete, &incomplete, sizeof(incomplete)) == 0);
    scan::Tracker variants;
    variants.RecordCondition(9, scan::ByteOrder::IncompleteBigEndian, true, 0);
    assert(variants.IsTrackedIndex(9) && variants.GetBridgeMarkers() == 4);
    variants.RecordCondition(9, scan::ByteOrder::BigEndian, true, 0);
    assert(variants.GetBridgeMarkers() == 5 && variants.GetStatus().active == 2);
    variants.RecordCondition(9, scan::ByteOrder::IncompleteBigEndian, false, 0);
    assert(variants.GetBridgeMarkers() == 1 && variants.GetStatus().active == 1);
    variants.RecordIndexDeletion(9, 0);
    assert(!variants.IsTrackedIndex(9) && variants.GetBridgeMarkers() == 0);
    variants.RecordCondition(9, static_cast<scan::ByteOrder>(255), true, 0);
    assert(variants.GetStatus().active == 0); // Unknown variants cannot index bookkeeping.

    scan::Tracker duplicates;
    duplicates.RecordCondition(0, scan::ByteOrder::BigEndian, true, 0);
    duplicates.RecordCondition(0, scan::ByteOrder::BigEndian, true, 0);
    duplicates.RecordCondition(255, scan::ByteOrder::LittleEndian, true, 0);
    duplicates.RecordCondition(0, scan::ByteOrder::BigEndian, false, 0);
    assert(duplicates.GetStatus().active == 2); // Duplicate add cannot look clean.
    duplicates.RecordIndexDeletion(0, 0);
    assert(duplicates.GetStatus().active == 1);
    duplicates.RecordIndexDeletion(255, 0);
    assert(duplicates.GetStatus().active == 0);

    scan::Tracker saturated;
    for (unsigned i = 0; i <= 0xffff; ++i) saturated.RecordCondition(255, scan::ByteOrder::BigEndian, true, 0);
    assert(saturated.GetStatus().active == 0xffff);
    saturated.RecordCondition(255, scan::ByteOrder::BigEndian, false, 0);
    assert(saturated.GetStatus().active == 0xffff); // Uncertainty is sticky/fail-closed.
    saturated.RecordIndexDeletion(255, 0);
    assert(saturated.GetStatus().active == 0);
    scan::Tracker telemetry;
    assert(telemetry.GetStatus().active == 0 && telemetry.GetStatus().failed == 0);
    telemetry.ObserveManagedEvent(6, true, 0, 2, true);
    assert(telemetry.GetStatus().managed_events == 0);
    telemetry.RecordCondition(1, scan::ByteOrder::BigEndian, true, 0);
    telemetry.ObserveManagedEvent(6, true, 0, 255, false); // Started, not an advertisement.
    assert(telemetry.GetStatus().managed_events == 1 && telemetry.GetStatus().scan_events == 1);
    assert(telemetry.GetStatus().last_scan_status == 255 && telemetry.GetStatus().driver_advertisements == 0);
    telemetry.ObserveManagedEvent(99, false, 0, 0, false); // Unknown type is metadata, never guessed payload.
    assert(telemetry.GetStatus().managed_events == 2 && telemetry.GetStatus().last_event_type == 99);
    assert(telemetry.GetStatus().scan_events == 1);
    telemetry.ObserveManagedEvent(6, true, 0x1234, 2, true);
    assert(telemetry.GetStatus().scan_errors == 1 && telemetry.GetStatus().last_scan_result == 0x1234);
    assert(telemetry.GetStatus().driver_advertisements == 0);
    telemetry.ObserveManagedEvent(6, true, 0, 2, true);
    assert(telemetry.GetStatus().managed_events == 4 && telemetry.GetStatus().scan_events == 3);
    assert(telemetry.GetStatus().driver_advertisements == 1 && telemetry.GetStatus().driver_hid_advertisements == 1);
    telemetry.RecordIndexDeletion(1, 0);
    telemetry.ObserveManagedEvent(6, true, 0, 2, true);
    assert(telemetry.GetStatus().managed_events == 4); // Nothing captured after tracked cleanup.

    using namespace missioncontrol::probe;
    assert(IdentifyModuleBuild(NativeHidBuildId.data()) == KnownModuleBuild::NativeHidP2);
    assert(IdentifyModuleBuild(RetiredFlagsBuildId.data()) == KnownModuleBuild::RetiredFlagsP1);
    assert(IdentifyModuleBuild(PreDiscoveryBuildId.data()) == KnownModuleBuild::PreDiscovery);
    assert(IdentifyModuleBuild(ManagedControlBuildId.data()) == KnownModuleBuild::ManagedControl);
    assert(IdentifyModuleBuild(nullptr) == KnownModuleBuild::Unknown);
    for (unsigned i = 0; i < 32; ++i) {
        auto changed = NativeHidBuildId; changed[i] ^= 1;
        assert(IdentifyModuleBuild(changed.data()) == KnownModuleBuild::Unknown);
        auto changed_control = ManagedControlBuildId; changed_control[i] ^= 1;
        assert(IdentifyModuleBuild(changed_control.data()) == KnownModuleBuild::Unknown);
    }
    namespace cached = ams::bluetooth::cached_scan;
    cached::Target cache_target{{1,2,3,4,5,6},0,0};
    const std::array<std::uint8_t, 6> other_address{6,5,4,3,2,1};
    static_assert(cached::ProtocolVersion == 3); // P3 certifies the opt-in native control-UUID requirement.
    assert(cached::ValidTarget(cache_target));
    auto invalid_target = cache_target;
    invalid_target.reserved = 1;
    assert(!cached::ValidTarget(invalid_target));
    invalid_target = cache_target; invalid_target.flags = 2;
    assert(!cached::ValidTarget(invalid_target));
    auto control_target = cache_target; control_target.flags = cached::RequireControlUuid;
    assert(cached::ValidTarget(control_target));
    const auto not_control = cached::Inspect(control_target, 101, 6, true, 0, 2, control_target.address, 2, 1, 2, false);
    assert(not_control.matched == 0 && not_control.advertisement_count == 0);
    const auto native_control = cached::Inspect(control_target, 101, 6, true, 0, 2, control_target.address, 2, 1, 2, true);
    assert(cached::FreshTarget(native_control, 100));
    auto live_native = native_control;
    live_native.device_type = 0; live_native.advertisement_count = 3;
    assert(missioncontrol::probe::FreshNativeControl(control_target, live_native, 100));
    assert(cached::FreshTarget(live_native, 100)); // Same native u8 policy for Stadia and the proven control.
    for (const auto type : {0u, 1u, 2u, 99u, 255u}) {
        auto native = live_native; native.device_type = type;
        assert(cached::FreshTarget(native, 100));
        assert(missioncontrol::probe::FreshNativeControl(control_target, native, 100));
    }
    auto unqualified = control_target; unqualified.flags = 0;
    assert(!missioncontrol::probe::FreshNativeControl(unqualified, live_native, 100));
    assert(!missioncontrol::probe::FreshNativeControl(control_target, live_native, 101));
    struct InvalidNativeField { std::uint32_t cached::Status::*field; std::uint32_t value; };
    for (const auto invalid : {
        InvalidNativeField{&cached::Status::protocol_version, cached::ProtocolVersion + 1},
        InvalidNativeField{&cached::Status::event_type, 7},
        InvalidNativeField{&cached::Status::result, 1},
        InvalidNativeField{&cached::Status::scan_status, 0},
        InvalidNativeField{&cached::Status::matched, 0},
        InvalidNativeField{&cached::Status::device_type, 256},
        InvalidNativeField{&cached::Status::address_type, 4},
        InvalidNativeField{&cached::Status::advertisement_count, 0},
        InvalidNativeField{&cached::Status::advertisement_count, 11}}) {
        auto rejected = live_native; rejected.*invalid.field = invalid.value;
        assert(!missioncontrol::probe::FreshNativeControl(control_target, rejected, 100));
        assert(!cached::FreshTarget(rejected, 100));
    }
    assert(cached::Inspect(control_target, 101, 6, true, 0, 2, other_address.data(), 2, 1, 2, true).matched == 0);
    assert(cached::Inspect(invalid_target, 101, 6, true, 0, 2, invalid_target.address, 2, 1, 2, true).matched == 0);
    const auto observed = cached::Inspect(cache_target, 12, 6, true, 0, 2, cache_target.address, 1, 1, 3);
    assert(cached::FreshTarget(observed, 11));
    assert(!cached::FreshTarget(observed, 12) && !cached::FreshTarget(observed, 13));
    auto old_protocol = observed; old_protocol.protocol_version = 1;
    assert(!cached::FreshTarget(old_protocol, 11));
    for (auto member : {&cached::Status::protocol_version, &cached::Status::event_type,
                        &cached::Status::result, &cached::Status::scan_status,
                        &cached::Status::matched,
                        &cached::Status::address_type, &cached::Status::advertisement_count}) {
        auto bad = observed;
        bad.*member = 99;
        assert(!cached::FreshTarget(bad, 11));
    }
    for (unsigned fault = 0; fault < 7; ++fault) {
        const auto s = cached::Inspect(fault == 0 ? invalid_target : cache_target, 12, 6,
            fault != 1, fault == 2 ? 1 : 0, fault == 3 ? 1 : 2,
            fault == 4 ? nullptr : fault == 5 ? other_address.data() : cache_target.address,
            1, 1, fault == 6 ? 11 : 3);
        assert(s.matched == 0 && s.device_type == 0 && s.address_type == 0 && s.advertisement_count == 0);
        assert(!cached::FreshTarget(s, 11)); // Do not leak metadata for unrelated/bad events.
    }
    using missioncontrol::probe::DiscoveryScanClean;
    using missioncontrol::probe::DiscoveryAllowsConnect;
    scan::Status scan_before{};
    scan_before.protocol_version = scan::ProtocolVersion; scan_before.enabled = 1;
    scan_before.added = scan_before.removed = 8;
    auto scan_after = scan_before;
    ++scan_after.added; ++scan_after.removed;
    assert(DiscoveryScanClean(true, true, scan_before, scan_after));
    assert(DiscoveryAllowsConnect(true, true, true, observed, 11));
    // Exercise the real Stadia scan match and final connect predicate, not just
    // the control query. Type0 cannot compensate for stale/wrong-target data.
    const auto native_stadia = cached::Inspect(cache_target, 12, 6, true, 0, 2,
        cache_target.address, 0, 1, 3);
    assert(cached::FreshTarget(native_stadia, 11));
    assert(DiscoveryAllowsConnect(true, true, true, native_stadia, 11));
    assert(!DiscoveryAllowsConnect(false, true, true, native_stadia, 11));
    assert(!DiscoveryAllowsConnect(true, false, true, native_stadia, 11));
    assert(!DiscoveryAllowsConnect(true, true, false, native_stadia, 11));
    assert(!DiscoveryAllowsConnect(true, true, true, native_stadia, 12));
    const auto other_stadia = cached::Inspect(cache_target, 12, 6, true, 0, 2,
        other_address.data(), 0, 1, 3);
    assert(!DiscoveryAllowsConnect(true, true, true, other_stadia, 11));
    assert(!DiscoveryAllowsConnect(false, true, true, observed, 11));
    assert(!DiscoveryAllowsConnect(true, false, true, observed, 11));
    assert(!DiscoveryAllowsConnect(true, true, false, observed, 11));
    assert(!DiscoveryAllowsConnect(true, true, true, observed, 12));
    assert(!DiscoveryAllowsConnect(true, true, true, cached::Status{}, 11));
    assert(!DiscoveryScanClean(false, true, scan_before, scan_after));
    assert(!DiscoveryScanClean(true, false, scan_before, scan_after));
    for (auto member : {&scan::Status::protocol_version, &scan::Status::enabled,
                        &scan::Status::active, &scan::Status::failed, &scan::Status::added, &scan::Status::removed,
                        &scan::Status::scan_errors, &scan::Status::bridged_advertisements,
                        &scan::Status::bridge_no_space, &scan::Status::bridge_unvalidated, &scan::Status::bridge_rejected}) {
        auto bad = scan_after; bad.*member = 99;
        assert(!DiscoveryScanClean(true, true, scan_before, bad));
    }
    auto disabled = scan_after; disabled.enabled = 0;
    assert(!DiscoveryScanClean(true, true, scan_before, disabled));
    auto overflow = scan_before; overflow.added = overflow.removed = 0xffffffffu;
    auto wrapped = scan_after; wrapped.added = wrapped.removed = 0;
    assert(!DiscoveryScanClean(true, true, overflow, wrapped));
    auto preexisting = scan_before; preexisting.active = 1;
    assert(!DiscoveryScanClean(true, true, preexisting, scan_after));
    using missioncontrol::probe::FindConnectionHandle;
    using missioncontrol::probe::InvalidConnectionHandle;
    const std::array<std::uint8_t, 6> connection_target{0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    const std::array<std::uint8_t, 6> other_target{0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
    auto write_connection = [](std::uint8_t *record, std::uint32_t handle,
                               const std::array<std::uint8_t, 6> &address) {
        std::memcpy(record, &handle, sizeof(handle));
        std::memcpy(record + sizeof(handle), address.data(), address.size());
    };
    std::array<std::uint8_t, 3 * missioncontrol::probe::ConnectionInfoSize> records{};
    write_connection(records.data(), InvalidConnectionHandle, connection_target);
    write_connection(records.data() + missioncontrol::probe::ConnectionInfoSize, 4, other_target);
    write_connection(records.data() + 2 * missioncontrol::probe::ConnectionInfoSize, 7, connection_target);
    std::uint32_t handle = 0;
    assert(FindConnectionHandle(records.data(), records.size(), 3, connection_target, &handle) && handle == 7);
    assert(!FindConnectionHandle(records.data(), records.size(), 2, connection_target, &handle));
    assert(FindConnectionHandle(records.data(), records.size(), 3, other_target, &handle) && handle == 4);
    assert(!FindConnectionHandle(records.data(), records.size(), 1, connection_target, &handle)); // Invalid handle only.
    assert(!FindConnectionHandle(nullptr, records.size(), 3, connection_target, &handle));
    assert(!FindConnectionHandle(records.data(), records.size(), 3, connection_target, nullptr));
    assert(!FindConnectionHandle(records.data(), records.size(), 0, connection_target, &handle));
    assert(!FindConnectionHandle(records.data(), records.size(), 4, connection_target, &handle));
    assert(!FindConnectionHandle(records.data(), records.size() - 1, 3, connection_target, &handle));
    write_connection(records.data(), 9, connection_target);
    handle = 77;
    assert(!FindConnectionHandle(records.data(), records.size(), 3, connection_target, &handle));
    assert(handle == 77); // Failure must not publish a partial result.
    write_connection(records.data(), 7, connection_target);
    assert(!FindConnectionHandle(records.data(), records.size(), 3, connection_target, &handle));
    assert(handle == 77);
    using missioncontrol::probe::InspectTargetConnections;
    using missioncontrol::probe::TargetPresence;
    assert(InspectTargetConnections(records.data(), records.size(), 0, connection_target) == TargetPresence::Absent);
    assert(InspectTargetConnections(records.data(), records.size(), 1, connection_target) == TargetPresence::Present);
    assert(InspectTargetConnections(records.data(), records.size(), 1, other_target) == TargetPresence::Absent);
    assert(InspectTargetConnections(records.data(), records.size(), 3, connection_target) == TargetPresence::Unknown);
    assert(InspectTargetConnections(records.data(), records.size(), 4, connection_target) == TargetPresence::Unknown);
    assert(InspectTargetConnections(nullptr, records.size(), 0, connection_target) == TargetPresence::Unknown);
    assert(InspectTargetConnections(records.data(), 0, 0, connection_target) == TargetPresence::Unknown);
    write_connection(records.data(), InvalidConnectionHandle, connection_target);
    assert(InspectTargetConnections(records.data(), records.size(), 1, connection_target) == TargetPresence::Absent);

    using missioncontrol::probe::SingleControlTarget;
    SingleControlTarget selection;
    std::array<std::uint8_t, 6> selected{};
    assert(!selection.Resolve(&selected));
    assert(!selection.Seen() && selection.Issues() == 0);
    selection.Observe(connection_target);
    selection.Observe(connection_target);
    assert(selection.Resolve(&selected) && selected == connection_target);
    assert(!selection.Resolve(nullptr));
    selection.Observe(other_target);
    assert(selection.Seen() && selection.Issues() == SingleControlTarget::MultipleAddresses);
    assert(!selection.Resolve(&selected));
    selection.Observe(connection_target); // Ambiguity cannot be cleared.
    assert(!selection.Resolve(&selected));
    SingleControlTarget truncated;
    truncated.Observe(connection_target); truncated.Invalidate();
    assert(truncated.Issues() == SingleControlTarget::UntrustedBatch);
    assert(!truncated.Resolve(&selected));
    for (const auto bad : {std::array<std::uint8_t, 6>{}, std::array<std::uint8_t, 6>{255,255,255,255,255,255}}) {
        SingleControlTarget invalid;
        invalid.Observe(bad); invalid.Observe(connection_target);
        assert(invalid.Issues() == SingleControlTarget::InvalidAddress);
        assert(!invalid.Resolve(&selected));
    }
    assert(missioncontrol::probe::MakeLeaseToken(0) == 1);
    assert(missioncontrol::probe::MakeLeaseToken(0x12345678) != 0);
    using missioncontrol::probe::ClassifyLease;
    using missioncontrol::probe::LeaseOutcome;
    assert(ClassifyLease(0, 0, 0) == LeaseOutcome::Running);
    assert(ClassifyLease(3, 0, 0) == LeaseOutcome::Connected);
    assert(ClassifyLease(5, 1, 0) == LeaseOutcome::Retired);
    assert(ClassifyLease(6, 0, 0) == LeaseOutcome::Quarantined);
    assert(ClassifyLease(0, 0, 1) == LeaseOutcome::Quarantined);
    using missioncontrol::probe::CancellationEvidence;
    using missioncontrol::probe::CancellationOutcome;
    using missioncontrol::probe::ClassifyCancellation;
    CancellationEvidence evidence{1, 1, 0, 0, 1, 1, 0, true, false, true};
    assert(ClassifyCancellation(evidence) == CancellationOutcome::TerminalAfterCancel);
    auto late = evidence;
    late.connected = 1; late.disconnects = 1;
    assert(ClassifyCancellation(late) == CancellationOutcome::LateConnectionCleaned);
    for (bool was_late : {false, true}) {
        const auto valid = was_late ? late : evidence;
        for (unsigned fault = 0; fault < 10; ++fault) {
            auto bad = valid;
            switch (fault) {
                case 0: bad.captured = 2; break;
                case 1: bad.cancels = 2; break;
                case 2: bad.terminal = false; break;
                case 3: bad.ambiguous = true; break;
                case 4: bad.owned_handle_absent = false; break;
                case 5: bad.last_result = 1; break;
                case 6: bad.reason = 5; break; // Failed action, even with later terminal.
                case 7: bad.disconnected = 0; break;
                case 8: bad.connected = 2; break;
                case 9: bad.disconnects = 2; break;
            }
            assert(ClassifyCancellation(bad) == CancellationOutcome::Unconfirmed);
        }
    }
    auto not_exercised = evidence; not_exercised.cancels = 0;
    assert(ClassifyCancellation(not_exercised) == CancellationOutcome::NotExercised);
    not_exercised = evidence; not_exercised.captured = 0;
    assert(ClassifyCancellation(not_exercised) == CancellationOutcome::NotExercised);
    evidence.reason = 2;
    assert(ClassifyCancellation(evidence) == CancellationOutcome::TerminalAfterCancel);
    std::cout << "Probe target, UUID128, private rewrite, cleanup and managed-event tests passed\n";
}
