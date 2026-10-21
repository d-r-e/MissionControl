/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "stadia_ble_scan_filter.hpp"

namespace missioncontrol::probe {
    // A retained BTM candidate is not evidence that THIS private scan worked.
    // Counts establish fresh filtered HID delivery/bridging, not device identity
    // or primary-versus-scan-response packet placement.
    constexpr bool HasFreshHidControlEvidence(
        const ams::bluetooth::stadia_scan::Status &before,
        const ams::bluetooth::stadia_scan::Status &after) {
        namespace scan = ams::bluetooth::stadia_scan;
        return before.protocol_version == scan::ProtocolVersion &&
               after.protocol_version == scan::ProtocolVersion &&
               before.enabled == 1 && after.enabled == 1 &&
               before.bridge_enabled == 1 && after.bridge_enabled == 1 &&
               (before.ad_size_convention == 1 || before.ad_size_convention == 2) &&
               after.ad_size_convention == before.ad_size_convention &&
               before.active == 0 && after.active == 1 &&
               after.added != before.added &&
               after.driver_hid_advertisements != before.driver_hid_advertisements &&
               after.bridged_advertisements != before.bridged_advertisements &&
               after.failed == before.failed;
    }
}
