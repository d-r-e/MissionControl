/*
 * Copyright (c) 2020-2026 ndeadly
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Portable parsing of a Bluetooth LE advertisement into something worth deciding on. Kept free of Switch
 * SDK types so the rules that choose which device to connect to can be tested on a host against captured
 * advertisements, which matters because a console bench has one controller on it and not a fleet.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ams::ble::adv {

    /* AD type codes from the Bluetooth Assigned Numbers. The values are the ones the Switch stack reports in
       BtdrvBleAdvertisement::type (BtdrvAdType_*), repeated here so this header stays SDK-free. */
    constexpr std::uint8_t AdTypeFlags                        = 0x01;
    constexpr std::uint8_t AdTypeServiceUuid16Incomplete      = 0x02;
    constexpr std::uint8_t AdTypeServiceUuid16Complete        = 0x03;
    constexpr std::uint8_t AdTypeShortenedLocalName            = 0x08;
    constexpr std::uint8_t AdTypeCompleteLocalName            = 0x09;
    constexpr std::uint8_t AdTypeTxPowerLevel                   = 0x0A;
    constexpr std::uint8_t AdTypeAppearance                    = 0x19;
    constexpr std::uint8_t AdTypeManufacturerSpecificData     = 0xFF;

    /* Bluetooth SIG assigned numbers this module cares about. */
    constexpr std::uint16_t UuidHumanInterfaceDevice = 0x1812;
    constexpr std::uint16_t AppearanceGamepad        = 0x03C4;

    constexpr std::size_t MaxNameLength    = 0x20;
    constexpr std::size_t MaxTrackedUuids  = 8;

    struct Summary {
        char name[MaxNameLength];
        std::uint8_t flags;
        std::uint16_t appearance;
        std::uint16_t services[MaxTrackedUuids];
        std::uint16_t company_id;
        bool has_flags;
        bool has_appearance;
        bool has_manufacturer;
        std::uint8_t service_count;
    };

    inline void Reset(Summary *summary) {
        std::memset(summary, 0, sizeof(*summary));
    }

    // Feed one advertisement record. The stack hands over records already split into type and data, and the
    // same record may arrive twice (a device repeating its appearance, or a scan response echoing the primary
    // packet), so feeding a summary twice must be idempotent.
    inline void Feed(Summary *summary, std::uint8_t type, const std::uint8_t *data, std::size_t size) {
        if (!summary || !data) {
            return;
        }

        switch (type) {
            case AdTypeFlags:
                if (size >= 1) {
                    summary->flags = data[0];
                    summary->has_flags = true;
                }
                break;
            case AdTypeShortenedLocalName:
            case AdTypeCompleteLocalName: {
                const std::size_t length = size < (MaxNameLength - 1) ? size : (MaxNameLength - 1);
                std::memcpy(summary->name, data, length);
                summary->name[length] = '\0';
                break;
            }
            case AdTypeAppearance:
                if (size >= 2) {
                    summary->appearance = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
                    summary->has_appearance = true;
                }
                break;
            case AdTypeServiceUuid16Incomplete:
            case AdTypeServiceUuid16Complete:
                for (std::size_t offset = 0; offset + 1 < size && summary->service_count < MaxTrackedUuids; offset += 2) {
                    const std::uint16_t uuid = static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
                    bool seen = false;
                    for (std::uint8_t i = 0; i < summary->service_count; ++i) {
                        if (summary->services[i] == uuid) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen) {
                        summary->services[summary->service_count++] = uuid;
                    }
                }
                break;
            case AdTypeManufacturerSpecificData:
                if (size >= 2) {
                    summary->company_id = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
                    summary->has_manufacturer = true;
                }
                break;
            default:
                break;
        }
    }

    inline bool HasService(const Summary &summary, std::uint16_t uuid) {
        for (std::uint8_t i = 0; i < summary.service_count; ++i) {
            if (summary.services[i] == uuid) {
                return true;
            }
        }
        return false;
    }

    // ASCII, because controller names are ASCII
    inline bool NameContains(const Summary &summary, const char *needle) {
        const std::size_t needle_length = std::strlen(needle);
        if (needle_length == 0) {
            return false;
        }

        for (std::size_t offset = 0; offset + needle_length <= std::strlen(summary.name); ++offset) {
            std::size_t i = 0;
            for (; i < needle_length; ++i) {
                const char a = summary.name[offset + i];
                const char b = needle[i];
                if (a == b) {
                    continue;
                }
                if (a >= 'A' && a <= 'Z' && (a + ('a' - 'A')) == b) {
                    continue;
                }
                break;
            }
            if (i == needle_length) {
                return true;
            }
        }
        return false;
    }

}
