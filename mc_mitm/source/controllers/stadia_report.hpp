/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace ams::controller {

    // Bluetooth HID Report Map and input reports verified on a real Stadia
    // Controller (18d1:9400). This is NOT the USB report format.
    struct StadiaInputReport {
        static constexpr std::uint8_t ReportId = 0x03;
        static constexpr std::size_t PayloadSize = 10;
        static constexpr std::size_t Size = PayloadSize + 1;

        enum Button : std::uint16_t {
            Capture   = 1u << 0,
            Assistant = 1u << 1,
            Home      = 1u << 4,
            Options   = 1u << 5,
            Menu      = 1u << 6,
            R3        = 1u << 7,
            L3        = 1u << 8,
            R1        = 1u << 9,
            L1        = 1u << 10,
            Y         = 1u << 11,
            X         = 1u << 12,
            B         = 1u << 13,
            A         = 1u << 14,
        };

        std::uint8_t dpad;
        std::uint16_t buttons;
        std::uint8_t left_x, left_y;
        std::uint8_t right_x, right_y;
        std::uint8_t left_trigger, right_trigger;
        std::uint8_t consumer_buttons;

        constexpr bool IsPressed(Button button) const {
            return (buttons & button) != 0;
        }

        // HOGP Report characteristic values do not contain the Report ID.
        // The caller MUST obtain it from that characteristic's Report Reference
        // descriptor (2908); never guess framing from the first payload byte.
        static bool ParseGatt(std::uint8_t report_id, const std::uint8_t *data,
                              std::size_t size, StadiaInputReport *out) {
            if (!data || !out || report_id != ReportId || size != PayloadSize) {
                return false;
            }
            *out = {
                static_cast<std::uint8_t>(data[0] & 0x0f),
                static_cast<std::uint16_t>(data[1] | (std::uint16_t(data[2]) << 8)),
                data[3], data[4], data[5], data[6], data[7], data[8],
                static_cast<std::uint8_t>(data[9] & 0x07)
            };
            return true;
        }

        // ID-prefixed HID input (as captured by macOS IOHID).
        static bool Parse(const std::uint8_t *data, std::size_t size, StadiaInputReport *out) {
            return data && size == Size && ParseGatt(data[0], data + 1, size - 1, out);
        }
    };

    // The Report Map declares sticks in [1,255], neutral 128, not [0,255].
    // Preserve the exact Switch neutral (2047) and both endpoints, including
    // inverted Y. Clamp zero rather than underflow on malformed input.
    constexpr std::uint16_t StadiaAxisToSwitch(std::uint8_t value, bool invert = false) {
        unsigned axis = value < 1 ? 1 : value;
        if (invert) {
            axis = 256 - axis;
        }
        return axis <= 128 ? (axis - 1) * 2047 / 127
                           : 2047 + (axis - 128) * 2048 / 127;
    }

}
