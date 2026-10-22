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
#include "ble_gatt.hpp"
#include "ble_results.hpp"
#include "ble_events.hpp"
#include "ble_log.hpp"
#include "ble_gatt_read.hpp"
#include "ble_diagnostics.hpp"
#include "../bluetooth_mitm/bluetooth/hid_report_layout.hpp"

namespace ams::ble::gatt {

    namespace {

        constinit AttributeTable g_table = {};

        // The stack reported more attributes than the table holds. Half a table is worse than none: a
        // characteristic dropped from the middle leaves a CCCD bound to the descriptor before it, and the
        // module would then enable notifications on a handle it never saw
        constinit bool g_table_overflowed = false;

        // The first cache report after a connection replaces the table; reports after discovery has completed
        // mean the stack re-discovered the services
        constinit bool g_replace_pending = false;
        constinit bool g_discovery_complete = false;
        constinit bool g_rediscovered = false;

        // Values from a characteristic read come back on the LE HID event stream as ClientNotify records, from
        // the LE HID thread, while the host thread waits. One read is in flight at a time: the host already
        // serialises its GATT requests, and two Report characteristics share a uuid, so a read answered out of
        // order could not be matched to the characteristic it asked for anyway.
        // The old ten-second allowance cut off observation before the protocol's thirty-second SMP/ATT
        // timeouts. ReadDeadline keeps the same one request pending for a bounded thirty-five seconds.
        // A timeout is never permission to issue the next request on a possibly busy/security-blocked link.
        constexpr size_t MaxReadSize = 0x200;   // the notification buffer the stack delivers values in

        os::SdkMutex g_read_lock;
        ReadState g_read;
        u8 g_read_data[MaxReadSize];

        // The authenticated report-map read is also the identification read. Do not ask for it a second time
        // after subscribing. Written/read only by the host thread; reset for every Subscribe invocation.
        u8 g_report_map[MaxReadSize];
        size_t g_report_map_size = 0;
        u32 g_report_map_connection = InvalidConnId;

        // Around 1.6 KiB: kept in module storage rather than on a stack, as hid_report_layout.hpp asks.
        bluetooth::hogp::ReportLayout g_layout;

        const BtdrvGattAttribute *FindService(u16 uuid16) {
            for (size_t i = 0; i < g_table.count; ++i) {
                auto &attr = g_table.attributes[i];
                if ((attr.type == BtdrvGattAttributeType_Service) && IsUuid16(attr.id.uuid, uuid16)) {
                    return &attr;
                }
            }
            return nullptr;
        }

        bool IsWithinService(const BtdrvGattAttribute &attr, const BtdrvGattAttribute &service) {
            return (attr.handle > service.handle) && (attr.handle <= service.group_end_handle);
        }

        // Descriptors belong to the characteristic that precedes them by handle
        const BtdrvGattAttribute *OwningCharacteristic(const BtdrvGattAttribute &desc) {
            const BtdrvGattAttribute *best = nullptr;
            for (size_t i = 0; i < g_table.count; ++i) {
                auto &attr = g_table.attributes[i];
                if ((attr.type == BtdrvGattAttributeType_Characteristic) && (attr.handle < desc.handle)) {
                    if (!best || (attr.handle > best->handle)) {
                        best = &attr;
                    }
                }
            }
            return best;
        }

        const BtdrvGattAttribute *FindCharacteristicIn(const BtdrvGattAttribute &service, u16 uuid16) {
            const size_t count = g_table.count;
            for (size_t i = 0; i < count; ++i) {
                auto &attr = g_table.attributes[i];
                if ((attr.type == BtdrvGattAttributeType_Characteristic) && IsWithinService(attr, service) && IsUuid16(attr.id.uuid, uuid16)) {
                    return &attr;
                }
            }
            return nullptr;
        }

        const BtdrvGattAttribute *FindDescriptor(const BtdrvGattAttribute &characteristic, u16 uuid16) {
            for (size_t i = 0; i < g_table.count; ++i) {
                auto &desc = g_table.attributes[i];
                if ((desc.type == BtdrvGattAttributeType_Descriptor) && IsUuid16(desc.id.uuid, uuid16) && (OwningCharacteristic(desc) == &characteristic)) {
                    return &desc;
                }
            }
            return nullptr;
        }

    }

    void OnEvent(const EventRecord &e) {
        if (e.type == BtdrvBleEventType_ClientNotify) {
            diagnostics::Note(diagnostics::Metric::ManagedNotifyType);
        }
        // Firmware may deliver a read completion on the managed stream rather than the HID stream. Pump()
        // dispatches this sink while the host waits; do not strand a valid completion in the event ring.
        if ((e.type == BtdrvBleEventType_ClientNotify) && (e.info.client_notify.conn_id == events::GetConnection())) {
            log::Write("GATT completion source=managed");
            OnClientNotify(e);
            events::NoteGattResult(e.info.client_notify.result);
            return;
        }
        if ((e.type != BtdrvBleEventType_ClientCacheSave) || (e.info.client_cache_save.conn_id != events::GetConnection())) {
            return;
        }

        if (g_discovery_complete) {
            g_rediscovered = true;
        }

        if (g_replace_pending) {
            g_replace_pending = false;
            g_table.count = 0;
        }

        for (u32 i = 0; i < std::min<u32>(e.info.client_cache_save.count, 10); ++i) {
            if (g_table.count >= MaxAttributes) {
                log::Write("attribute table is full (%zu) and the device reported more: refusing to use it", MaxAttributes);
                g_table_overflowed = true;
                break;
            }
            g_table.attributes[g_table.count++] = e.info.client_cache_save.attr_list[i];
        }
    }

    void PrepareDiscovery(const bluetooth::Address &address) {
        // Host-thread only, once per new link and before Pair pumps the event sink. Never carry the previous
        // link's attribute table into a security-first setup, even when reconnecting to the same address.
        g_discovery_complete = false;
        g_rediscovered = false;
        g_replace_pending = true;
        g_table_overflowed = false;
        g_table.count = 0;
        g_table.address = address;
    }

    void Discover(u32 conn_id, const bluetooth::Address &address) {
        // The driver also discovers automatically. Pair may already have delivered this connection's cache;
        // keep it instead of resetting the table or waiting for a first record that has already been consumed.
        const bool cached_during_bond = (g_table.count > 0) && AddressEqual(g_table.address, address);
        if (!cached_during_bond) {
            events::WaitFor(BtdrvBleEventType_ClientCacheSave, TimeSpan::FromSeconds(2), nullptr, [&](const EventRecord &r) { return r.info.client_cache_save.conn_id == conn_id; });
        }
        events::Pump(TimeSpan::FromMilliSeconds(400));

        Result rc = btdrvGetGattAttribute(address, conn_id);
        log::Write("btdrvGetGattAttribute rc=0x%x", rc.GetValue());

        rc = btdrvConfigureAttMtu(conn_id, 0x100);
        log::Write("btdrvConfigureAttMtu rc=0x%x", rc.GetValue());

        // The MTU result is the last thing the stack reports for a fresh link
        events::WaitFor(BtdrvBleEventType_ClientConfigureMtu, TimeSpan::FromSeconds(5), nullptr, [&](const EventRecord &r) { return r.info.client_configure_mtu.conn_id == conn_id; });
        events::Pump(TimeSpan::FromMilliSeconds(500));

        g_discovery_complete = true;
        g_replace_pending = false;
        log::Write("attribute table holds %u entries%s", g_table.count, cached_during_bond ? " (received during bond phase)" : "");
    }

    bool TakeRediscovered() {
        const bool rediscovered = g_rediscovered;
        g_rediscovered = false;
        return rediscovered;
    }

    Result Subscribe(u32 conn_id, HidService *out) {
        g_report_map_size = 0;
        g_report_map_connection = InvalidConnId;
        if (g_table_overflowed) {
            log::Write("attribute table was truncated, not subscribing to handles we may have misread");
            R_THROW(svc::ResultNotSupported());
        }

        auto found = FindService(GattServiceHid);
        if (!found) {
            log::Write("no HID service in attribute table");
            R_THROW(svc::ResultNotFound());
        }

        // The stack can report a re-discovery while this runs, appending to the table; work from a copy of the
        // service and only over the entries that were there to begin with
        const BtdrvGattAttribute service = *found;
        const size_t count = g_table.count;

        out->service = service;
        out->has_output_report = false;

        // Authenticated reads are what make the stack encrypt the link (pairing first if it has to)
        for (size_t i = 0; i < count; ++i) {
            auto &attr = g_table.attributes[i];
            if ((attr.type != BtdrvGattAttributeType_Characteristic) || !IsWithinService(attr, service)) {
                continue;
            }

            // The stack drops the link itself when encryption fails
            R_UNLESS(!events::IsDisconnected(), ResultNoConnection());

            if (IsUuid16(attr.id.uuid, GattCharHidInformation) || IsUuid16(attr.id.uuid, GattCharReportMap)) {
                const bool is_map = IsUuid16(attr.id.uuid, GattCharReportMap);
                const u16 uuid = is_map ? GattCharReportMap : GattCharHidInformation;
                u8 information[MaxReadSize];
                size_t size = 0;
                // An accepted IPC request is not a completed ATT read. In particular, the first bench run
                // timed out here and then issued another read, which the stack refused. Stop at that first
                // failure instead of guessing what the numeric refusal meant or retrying it on the radio.
                R_TRY(Read(conn_id, *out, uuid, is_map ? g_report_map : information, MaxReadSize, &size));
                if (is_map) {
                    g_report_map_size = size;
                    g_report_map_connection = conn_id;
                }
            }
        }

        Result result = svc::ResultNotFound();
        for (size_t i = 0; i < count; ++i) {
            auto &attr = g_table.attributes[i];
            if ((attr.type != BtdrvGattAttributeType_Characteristic) || !IsWithinService(attr, service) || !IsUuid16(attr.id.uuid, GattCharReport)) {
                continue;
            }

            R_UNLESS(!events::IsDisconnected(), ResultNoConnection());

            // Rumble goes to the writable Report
            if (attr.property & (BtdrvGattCharacteristicProperty_Write | BtdrvGattCharacteristicProperty_WriteNoResponse)) {
                out->output_report = attr;
                out->has_output_report = true;
            }

            if (!(attr.property & BtdrvGattCharacteristicProperty_Notify)) {
                continue;
            }

            auto cccd = FindDescriptor(attr, GattDescClientCharConfig);
            if (!cccd) {
                continue;
            }

            Result rc = btdrvRegisterGattNotification(conn_id, service.is_primary, &service.id, &attr.id);
            log::Write("btdrvRegisterGattNotification handle=0x%04x rc=0x%x", attr.handle, rc.GetValue());

            // The write's asynchronous result is the subscription's: an error means the link never encrypted
            const u32 results = events::GetGattResultCount();
            const u8 enable_notifications[2] = { 0x01, 0x00 };
            rc = btdrvWriteGattDescriptor(conn_id, service.is_primary, &service.id, &attr.id, &cccd->id, enable_notifications, sizeof(enable_notifications), BtdrvGattAuthReqType_NoMitm);
            log::Write("btdrvWriteGattDescriptor(CCCD) handle=0x%04x rc=0x%x", cccd->handle, rc.GetValue());
            R_TRY(rc);
            R_UNLESS(events::WaitForGattResult(results, TimeSpan::FromSeconds(2)), svc::ResultTimedOut());
            R_UNLESS(!events::IsDisconnected(), ResultNoConnection());
            R_UNLESS(events::GetGattFailure() == 0, ResultInsufficientAuthentication());
            result = ResultSuccess();
        }

        R_RETURN(result);
    }

    void OnClientNotify(const EventRecord &e) {
        const auto &notify = e.info.client_notify;

        std::scoped_lock lk(g_read_lock);
        if (!IsUuid16(notify.char_uuid, g_read.uuid) ||
            !g_read.Accept(notify.conn_id, g_read.uuid, notify.result, notify.size, sizeof(g_read_data))) {
            return;
        }

        std::memcpy(g_read_data, notify.data, g_read.size);
        log::Write("read uuid=%04x completed result=%u size=%u valid=%u", g_read.uuid, notify.result, notify.size, !g_read.failed);
    }

    Result Read(u32 conn_id, const HidService &hid, u16 uuid16, void *out, size_t out_size, size_t *out_len) {
        auto characteristic = FindCharacteristicIn(hid.service, uuid16);
        R_UNLESS(characteristic, svc::ResultNotFound());

        {
            std::scoped_lock lk(g_read_lock);
            R_UNLESS(g_read.Begin(conn_id, uuid16), svc::ResultNotSupported());
        }

        diagnostics::Dump("read-begin");
        const Result rc = btdrvReadGattCharacteristic(conn_id, hid.service.is_primary, &hid.service.id, &characteristic->id, BtdrvGattAuthReqType_NoMitm);
        log::Write("btdrvReadGattCharacteristic uuid=%04x handle=0x%04x rc=0x%x wait_ms=%lld", uuid16, characteristic->handle, rc.GetValue(), static_cast<long long>(ReadTimeoutMilliseconds));
        if (R_FAILED(rc)) {
            std::scoped_lock lk(g_read_lock);
            g_read.Finish();
            R_THROW(rc);
        }

        const ReadDeadline deadline(os::GetSystemTick().ToTimeSpan().GetMilliSeconds());
        for (;;) {
            {
                std::scoped_lock lk(g_read_lock);
                if (g_read.answered) {
                    g_read.Finish();
                    R_UNLESS(!g_read.failed, svc::ResultNotFound());
                    R_UNLESS(g_read.size <= out_size, svc::ResultNotSupported());

                    const size_t copied = g_read.size;
                    std::memcpy(out, g_read_data, copied);
                    if (out_len) {
                        *out_len = copied;
                    }
                    R_SUCCEED();
                }
            }

            // The system switched BLE off while we were waiting: nothing else may be asked of the radio, and
            // every answer to it has just been lost
            if (!events::IsRadioEnabled() || events::IsDisconnected() || events::IsShuttingDown()) {
                break;
            }

            if (deadline.Expired(os::GetSystemTick().ToTimeSpan().GetMilliSeconds())) {
                break;
            }
            // Dispatch managed-stream completions too, not just the separate LE HID listener.
            events::Pump(TimeSpan::FromMilliSeconds(20));
        }

        {
            std::scoped_lock lk(g_read_lock);
            g_read.Finish();
        }
        diagnostics::Dump("read-incomplete");
        log::Write("read uuid=%04x incomplete: radio=%u disconnected=%u; no next GATT request", uuid16,
            events::IsRadioEnabled(), events::IsDisconnected());
        R_THROW(svc::ResultTimedOut());
    }

    Result Describe(u32 conn_id, const HidService &hid, ReportFacts *out) {
        *out = {};
        out->has_protocol_mode = FindCharacteristicIn(hid.service, GattCharProtocolMode) != nullptr;
        out->has_hid_control_point = FindCharacteristicIn(hid.service, GattCharHidControlPoint) != nullptr;

        R_UNLESS((g_report_map_connection == conn_id) && (g_report_map_size > 0), svc::ResultNotFound());
        const auto &map = g_report_map;
        const size_t map_size = g_report_map_size;

        // A map exactly the size of the buffer has almost certainly been cut short, and a pad identified from
        // the prefix of its own report map is worse than one not identified at all
        if (map_size == sizeof(map)) {
            log::Write("report map filled the %zu byte read buffer: treating it as truncated", sizeof(map));
            R_THROW(svc::ResultNotSupported());
        }

        if (g_layout.Parse(map, map_size) != bluetooth::hogp::LayoutResult::Ok) {
            log::Write("report map of %zu bytes could not be parsed", map_size);
            R_THROW(svc::ResultNotSupported());
        }
        out->report_map_parsed = true;
        log::Write("report map: %zu bytes, %s", map_size, g_layout.UsesReportIds() ? "uses report ids" : "no report ids");

        u8 ids[8];
        size_t count = g_layout.ListReports(bluetooth::hogp::ReportType::Input, ids, util::size(ids));
        for (size_t i = 0; i < count; ++i) {
            log::Write("  input  report id=0x%02x size=%zu", ids[i], g_layout.PayloadSize(ids[i], bluetooth::hogp::ReportType::Input));
        }
        if (count > 1) {
            log::Write("  %zu input reports: taking the first, the notification size decides which one is meant", count);
        }
        if (count > 0) {
            out->input_report_id = ids[0];
            out->input_report_size = static_cast<u8>(std::min<size_t>(g_layout.PayloadSize(ids[0], bluetooth::hogp::ReportType::Input), 0xFF));
        }

        count = g_layout.ListReports(bluetooth::hogp::ReportType::Output, ids, util::size(ids));
        for (size_t i = 0; i < count; ++i) {
            log::Write("  output report id=0x%02x size=%zu", ids[i], g_layout.PayloadSize(ids[i], bluetooth::hogp::ReportType::Output));
        }
        if (hid.has_output_report && (count > 0)) {
            out->output_report_id = ids[0];
            out->output_report_size = static_cast<u8>(std::min<size_t>(g_layout.PayloadSize(ids[0], bluetooth::hogp::ReportType::Output), 0xFF));
        }

        R_SUCCEED();
    }

    void Dump(const char *why) {
        if (!log::IsEnabled()) {
            return;
        }

        log::Write("attributes (%s): %u entries", why, g_table.count);
        const size_t count = g_table.count;
        for (size_t i = 0; i < count; ++i) {
            auto &attr = g_table.attributes[i];
            char uuid[0x40];
            FormatUuid(attr.id.uuid, uuid, sizeof(uuid));
            log::Write("  [%02zu] handle=0x%04x end=0x%04x type=%u prop=0x%02x primary=%d uuid=%s",
                i, attr.handle, attr.group_end_handle, attr.type, attr.property, attr.is_primary ? 1 : 0, uuid);
        }
    }

}
