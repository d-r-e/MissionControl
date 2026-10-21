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
 * Which Bluetooth LE HID pad a set of endpoints belongs to, and what to do with it.
 *
 * An advertisement cannot identify a pad: the Stadia controller and an Xbox controller both report the
 * appearance 0x03C4 (gamepad), and a name is a marketing string that a firmware update is free to change.
 * So an advertisement only decides whether a device is worth connecting to, and the pad kind is decided
 * afterwards from what the device really exposes over GATT (see Fingerprint). Adding a pad means adding a
 * fingerprint and a policy here plus a controller class; the host state machine does not change.
 *
 * Portable, and tested against captured advertisements and report maps in tests/ble_pad_tests.cpp.
 */
#pragma once
#include "ble_advertisement.hpp"
#include <cstddef>
#include <cstdint>

namespace ams::ble::pad {

    enum class Kind : std::uint8_t {
        None,   // nothing we know how to decode
        Xbox,   // Xbox One S/Elite 2 on 5.x firmware and Xbox Series X|S
        Stadia, // Google Stadia Controller, 18d1:9400
    };

    /* From the advertisement: is this a gamepad worth spending a connection on? */

    inline bool AdvertisesHidService(const adv::Summary &summary) {
        return adv::HasService(summary, adv::UuidHumanInterfaceDevice);
    }

    // A gamepad appearance is what has always been looked for, and is enough to be worth a connection: the
    // HID service in the advertisement list is not a filter here but the evidence used afterwards, in
    // LegacyPolicy, to decide whether a device may be guessed at at all.
    inline bool IsWorthConnecting(const adv::Summary &summary) {
        return adv::NameContains(summary, "xbox")
            || (summary.has_appearance && (summary.appearance == adv::AppearanceGamepad));
    }

    /* From the link once it is up: what the device actually exposes. */

    struct Fingerprint {
        std::uint8_t input_report_id;     // from the device's Report Map; 0 = it could not be read
        std::uint8_t input_report_size;   // payload size without the report id; 0 = unknown
        std::uint8_t output_report_id;    // 0 = no writable Report characteristic
        std::uint8_t output_report_size;
        bool has_protocol_mode;           // 0x2A4E
        bool has_hid_control_point;       // 0x2A4C
        bool name_says_xbox;
        bool name_says_stadia;
    };

    /* What we know how to decode, and how. Sizes are payloads without the report id, as delivered by a
       notification; the id comes from the characteristic's Report Reference descriptor. */
    struct Policy {
        Kind kind;
        std::uint8_t input_report_id;
        std::uint8_t input_report_size;   // 0 = accept any size
        bool identified;                  // false = assumed, see LegacyPolicy
    };

    // Stadia: input Report 0x03, 10 bytes (HID report descriptor and a macOS Bluetooth LE capture agree);
    // its writable Report is 0x05, 4 bytes, and no host has told us what it means yet.
    // Xbox: input Report 0x01, 16 bytes. Older LE firmwares have been seen a byte shorter, hence a range.
    // For the log: an unfamiliar pad is only diagnosable afterwards if the log says what was looked at.
    inline const char *Name(Kind kind) {
        switch (kind) {
            case Kind::Xbox:
                return "Xbox";
            case Kind::Stadia:
                return "Stadia";
            default:
                return "unrecognised";
        }
    }

    inline Kind Identify(const Fingerprint &fingerprint) {
        if ((fingerprint.input_report_id == 0x03) && (fingerprint.input_report_size == 10)) {
            return Kind::Stadia;
        }
        if ((fingerprint.input_report_id == 0x01) && (fingerprint.input_report_size >= 15) && (fingerprint.input_report_size <= 16)) {
            return Kind::Xbox;
        }
        return Kind::None;
    }

    inline const Policy& PolicyFor(Kind kind) {
        static const Policy k_stadia   = { Kind::Stadia, 0x03, 10, true };
        static const Policy k_xbox     = { Kind::Xbox,   0x01, 16, true };
        static const Policy k_unknown  = { Kind::None,   0x00, 0,  false };

        switch (kind) {
            case Kind::Stadia: return k_stadia;
            case Kind::Xbox:   return k_xbox;
            default:           return k_unknown;
        }
    }

    /*
     * A link whose endpoints told us nothing still has to be dealt with. The Xbox assumption - that whatever
     * the scan picked out is an Xbox pad - is what shipped and was tested on hardware, so it is kept for
     * exactly the devices that rule matched, and only for those. A device that announced itself as a HOGP
     * device is never guessed at: presenting it with the wrong report layout would feed the system garbage,
     * where refusing and logging the endpoints gives the next person the fingerprint they need.
     */
    inline Policy LegacyPolicy(const adv::Summary &summary) {
        if (AdvertisesHidService(summary)) {
            return PolicyFor(Kind::None);
        }

        Policy policy = PolicyFor(Kind::Xbox);
        policy.identified = false;
        return policy;
    }

}
