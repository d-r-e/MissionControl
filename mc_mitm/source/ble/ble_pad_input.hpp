/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "ble_pad.hpp"
#include "../controllers/stadia_report.hpp"

namespace ams::ble::pad {

    // Report-characteristic payload only: the verified endpoint supplied the pad kind/report identity.
    // Do not look for Xbox byte13 in Stadia's ten-byte payload, or mistake trigger axes for shoulders.
    inline bool HasLrShoulders(Kind kind, const std::uint8_t *data, std::size_t size) {
        if (!data) {
            return false;
        }
        switch (kind) {
            case Kind::Stadia: {
                ams::controller::StadiaInputReport report{};
                return ams::controller::StadiaInputReport::ParseGatt(
                           ams::controller::StadiaInputReport::ReportId, data, size, &report) &&
                       report.IsPressed(ams::controller::StadiaInputReport::L1) &&
                       report.IsPressed(ams::controller::StadiaInputReport::R1);
            }
            case Kind::Xbox:
                // Both input payload variants already supported by the Xbox BLE controller.
                return (size == 15 || size == 16) && ((data[13] & 0xc0) == 0xc0);
            default:
                return false;
        }
    }

}
