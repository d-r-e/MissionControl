/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "stadia_ble_scan_filter.hpp"
#include "ble_cached_scan_protocol.hpp"

namespace missioncontrol::probe {
    inline bool DiscoveryScanClean(bool stopped, bool status_read,
                                   const ams::bluetooth::stadia_scan::Status &before,
                                   const ams::bluetooth::stadia_scan::Status &after) {
        namespace scan = ams::bluetooth::stadia_scan;
        // Exactly one private add/remove, no saturation/wrap, error, pending
        // filter, or advertisement-copy change. Counter equality alone is not
        // enough: SDK stop success and a current status read are mandatory.
        if (!stopped || !status_read || before.protocol_version != scan::ProtocolVersion ||
            after.protocol_version != scan::ProtocolVersion || before.enabled != 1 || after.enabled != 1 ||
            before.active || before.failed || before.added != before.removed ||
            before.added == 0xffffffffu || before.removed == 0xffffffffu) return false;
        return after.active == 0 && after.failed == 0 &&
            after.added == before.added + 1 && after.removed == before.removed + 1 &&
            after.added == after.removed && after.scan_errors == before.scan_errors &&
            after.bridged_advertisements == before.bridged_advertisements &&
            after.bridge_no_space == before.bridge_no_space &&
            after.bridge_rejected == before.bridge_rejected &&
            after.bridge_unvalidated == before.bridge_unvalidated;
    }
    // Qualified control identity comes from the exact-address + known UUID query,
    // not a guessed device-class enum. HOS21.2 reports device_type=0 on the
    // verified Android control, despite the SDK's nominal0=BR/EDR annotation.
    // Validate the u8 transport width, not that contradicted classification.
    inline bool FreshNativeControl(const ams::bluetooth::cached_scan::Target &query,
                                   const ams::bluetooth::cached_scan::Status &s,
                                   std::uint64_t baseline_sequence) {
        namespace cached = ams::bluetooth::cached_scan;
        return cached::ValidTarget(query) && query.flags == cached::RequireControlUuid &&
            cached::FreshTarget(s, baseline_sequence);
    }

    inline bool DiscoveryAllowsConnect(bool clean, bool trusted, bool running,
                                       const ams::bluetooth::cached_scan::Status &observed,
                                       std::uint64_t baseline_sequence) {
        return clean && trusted && running &&
            ams::bluetooth::cached_scan::FreshTarget(observed, baseline_sequence);
    }
}
