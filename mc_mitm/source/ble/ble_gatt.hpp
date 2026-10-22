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
#pragma once
#include "ble_types.hpp"

namespace ams::ble::gatt {

    /*
     * The stack reports a connected device's attributes (ClientCacheSave events) when it discovers them, and
     * serves them from its own cache without reporting on later connections. The table is therefore kept for
     * the device across connections and only replaced when the stack reports a fresh discovery.
     */

    constexpr size_t MaxAttributes = 64;

    struct AttributeTable {
        bluetooth::Address address;
        u8 pad[2];
        u32 count;
        BtdrvGattAttribute attributes[MaxAttributes];
    };

    struct HidService {
        BtdrvGattAttribute service;
        BtdrvGattAttribute output_report;
        bool has_output_report;
    };

    // Event sink, see events::SetSink
    void OnEvent(const EventRecord &record);

    // Starts this link's passive cache before Pair pumps events; does not issue a driver/radio request.
    void PrepareDiscovery(const bluetooth::Address &address);

    // Completes discovery/MTU work after the bond request, retaining this link's automatic cache records.
    void Discover(u32 conn_id, const bluetooth::Address &address);

    // Set when a cache report arrived after discovery completed, i.e. the stack re-discovered the services
    bool TakeRediscovered();

    // Enables notifications on the HID input Report characteristic(s). Fails with InsufficientAuthentication
    // when the link isn't encrypted; returns NotFound if the table holds no HID service.
    Result Subscribe(u32 conn_id, HidService *out);

    /*
     * What a device says its own reports are, read from its Report Map instead of assumed. Pads that are
     * indistinguishable in an advertisement - the Stadia and Xbox controllers both advertise the gamepad
     * appearance - differ here, which is where a pad should be recognised.
     */
    struct ReportFacts {
        u8 input_report_id;       // 0 = the report map could not be read or parsed, or declared no input
        u8 input_report_size;     // payload bytes without the report id, 0 = unknown
        u8 output_report_id;      // 0 = no output report declared
        u8 output_report_size;
        bool report_map_parsed;
        bool has_protocol_mode;       // 0x2A4E
        bool has_hid_control_point;   // 0x2A4C
    };

    // ClientNotify completions from either the managed sink or the LE HID listener feed the same
    // connection/UUID-scoped pending read.
    void OnClientNotify(const EventRecord &record);

    // Reads one characteristic of the HID service, selected by 16-bit uuid.
    Result Read(u32 conn_id, const HidService &hid, u16 uuid16, void *out, size_t out_size, size_t *out_len);

    // Parses the Report Map retained by Subscribe's authenticated read and notes optional characteristics.
    // No duplicate read is issued. Missing facts must not be guessed.
    Result Describe(u32 conn_id, const HidService &hid, ReportFacts *out);

    // Writes the attribute table to the log. With one controller on the bench this is the difference between
    // a bug report and a guess about an unfamiliar device.
    void Dump(const char *why);

}
