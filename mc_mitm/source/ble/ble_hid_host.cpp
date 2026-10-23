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
#include "ble_hid_host.hpp"
#include "ble_diagnostics.hpp"
#include "ble_advertisement.hpp"
#include "ble_events.hpp"
#include "ble_gatt.hpp"
#include "ble_pad_input.hpp"
#include "ble_log.hpp"
#include "ble_results.hpp"
#include "../mcmitm_config.hpp"
#include "../mcmitm_initialization.hpp"
#include "../bluetooth_mitm/bluetooth/bluetooth_ble.hpp"
#include "../controllers/controller_management.hpp"
#include "../controllers/stadia_ble_controller.hpp"
#include "../controllers/xbox_one_ble_controller.hpp"

namespace ams::ble {

    namespace {

        constexpr s32 ThreadPriority = 9;
        constexpr size_t ThreadStackSize = 0x4000;
        alignas(os::ThreadStackAlignment) constinit u8 g_thread_stack[ThreadStackSize];
        constinit os::ThreadType g_thread;
        constinit bool g_launched = false;

        constexpr size_t LeHidThreadStackSize = 0x2000;
        alignas(os::ThreadStackAlignment) constinit u8 g_le_hid_thread_stack[LeHidThreadStackSize];
        constinit os::ThreadType g_le_hid_thread;
        constinit EventRecord g_le_hid_record;

        // Application UUID for our own GATT client, should we need to register one
        constexpr BtdrvGattAttributeUuid GattClientAppUuid = { 0x10, { 0x4d, 0x43, 0x42, 0x4c, 0x45, 0x48, 0x4f, 0x53, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } };

        // Transport argument to btdrvCreateBond (BTA convention: 1 = BR/EDR, 2 = LE)
        constexpr u32 BondTransport_Le = 2;

        constinit u8 g_client_if = InvalidClientIf;

        // A setup failure may leave an ATT/security request outstanding. Do not reconnect or cycle the radio
        // automatically after it: preserve the first failure for diagnosis and require an operator reboot.
        bool g_setup_failed = false;

        void StopAfterSetupFailure(const char *stage) {
            g_setup_failed = true;
            log::Write("setup stopped at %s; no automatic retry or recovery radio cycle until reboot", stage);
        }

        /* The controller currently presented to the system */
        constinit os::SdkMutex g_controller_lock;
        std::shared_ptr<controller::SwitchController> g_controller;

        // Reports are delivered from the LE HID thread, the resting reports from this thread, and the
        // controller is attached and detached from whichever one noticed the Controllers screen opening. The
        // controller classes keep input state (a report being assembled, the parsed sticks and buttons) with
        // no lock of their own, because upstream only ever touches a controller from one thread; everything
        // that enters one, or attaches and detaches it, therefore takes this lock. It is only ever taken
        // before controller_management's own, never while holding it.
        constinit os::SdkMutex g_delivery_lock;

        // Which pad the controller above turned out to be, read under g_controller_lock together with it. The
        // host never decodes a report: this only chooses which controller class a notification is handed to.
        constinit pad::Kind g_kind = pad::Kind::None;

        // Each presentation owns one endpoint identity, so a report is only ever accepted by the controller
        // it was subscribed for (see StadiaGattInput)
        constinit u64 g_endpoint_generation = 0;

        // The link the presented controller belongs to. The stack recycles connection ids, so a notification
        // still queued from a dying link would otherwise be handed to the controller of the next one
        constinit u32 g_presented_conn_id = InvalidConnId;

        // Withdrawn from hid while the Controllers screen is open, as the screen does with every controller;
        // the link stays up and the controller comes back on LB+RB (any button once the screen is closed)
        constinit bool g_parked = false;
        constinit s64 g_present_tick = 0;

        // A device that turned out not to be a pad this module can decode, and when it was last refused.
        // Without this, the loop reconnects to it, refuses it and disconnects again for as long as the
        // Controllers screen stays open
        constinit bluetooth::Address g_refused_address = {};
        constinit s64 g_refused_tick = 0;
        constexpr TimeSpan RefusalBackoff = TimeSpan::FromSeconds(60);

        bool RecentlyRefused(const bluetooth::Address &address) {
            return (g_refused_tick != 0) && AddressEqual(address, g_refused_address) &&
                   ((os::GetSystemTick() - os::Tick(g_refused_tick)).ToTimeSpan() < RefusalBackoff);
        }

        // hid registers a controller on the first input report it sees after re-reading btm's device list, which
        // it does some time after being signalled; a report sent straight after the signal is dropped and the
        // controller only appeared once a button was pressed. The pad's resting state is therefore delivered a
        // few times over the next seconds (ticks at which a copy is due, zero once sent).
        constexpr size_t NeutralReportCount = 4;
        constexpr s64 NeutralReportDelaysMs[NeutralReportCount] = { 0, 300, 1000, 2500 };
        constinit s64 g_neutral_due[NeutralReportCount] = {};

        constexpr u8 XboxNeutralReport[sizeof(controller::XboxOneInputReport0x01)] = { 0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80 };

        // A pad reports nothing until something is touched, so the resting state has to come from the pad
        // itself rather than from a composition: Stadia's is the first row of the captured reports.
        const u8 *RestingReport(pad::Kind kind, size_t *out_size) {
            switch (kind) {
                case pad::Kind::Stadia:
                    *out_size = sizeof(controller::StadiaBleNeutralReport);
                    return controller::StadiaBleNeutralReport;
                default:
                    *out_size = sizeof(XboxNeutralReport);
                    return XboxNeutralReport;
            }
        }

        bool IsRestingReport(pad::Kind kind, const u8 *data, size_t size) {
            size_t resting_size = 0;
            const u8 *resting = RestingReport(kind, &resting_size);
            return (size == resting_size) && (std::memcmp(data, resting, size) == 0);
        }

        // A notification arrives as the payload of one Report characteristic with the report id stripped off;
        // the pad's own class knows what to do with it. Kept here rather than in a member for the same reason
        // the resting report is: the host holds a SwitchController, the type it was attached as.
        Result DeliverInput(const std::shared_ptr<controller::SwitchController> &controller, pad::Kind kind, const u8 *data, size_t size) {
            const auto lock_start = diagnostics::InputNowUs();
            std::scoped_lock lk(g_delivery_lock);
            diagnostics::RecordInputDuration(diagnostics::TimingMetric::DeliveryLock, lock_start);
            const auto processing_start = diagnostics::InputNowUs();
            ON_SCOPE_EXIT { diagnostics::RecordInputDuration(diagnostics::TimingMetric::TranslateInject, processing_start); };

            switch (kind) {
                case pad::Kind::Stadia:
                    return std::static_pointer_cast<controller::StadiaBleController>(controller)->HandleInputReport(data, size);
                default:
                    return std::static_pointer_cast<controller::XboxOneBleController>(controller)->HandleInputReport(data, size);
            }
        }

        void ScheduleNeutralReports() {
            const s64 now = os::GetSystemTick().GetInt64Value();
            std::scoped_lock lk(g_controller_lock);
            for (size_t i = 0; i < NeutralReportCount; ++i) {
                g_neutral_due[i] = now + os::ConvertToTick(TimeSpan::FromMilliSeconds(NeutralReportDelaysMs[i])).GetInt64Value();
            }
        }

        void ServiceNeutralReports() {
            std::shared_ptr<controller::SwitchController> controller;
            pad::Kind kind = pad::Kind::None;
            bool due = false;
            {
                std::scoped_lock lk(g_controller_lock);
                const s64 now = os::GetSystemTick().GetInt64Value();
                for (size_t i = 0; i < NeutralReportCount; ++i) {
                    if ((g_neutral_due[i] != 0) && (now >= g_neutral_due[i])) {
                        g_neutral_due[i] = 0;
                        due = true;
                    }
                }
                if (due && !g_parked) {
                    controller = g_controller;
                    kind = g_kind;
                }
            }
            if (controller) {
                size_t resting_size = 0;
                const u8 *resting = RestingReport(kind, &resting_size);
                static_cast<void>(DeliverInput(controller, kind, resting, resting_size));
            }
        }

        /* Report layout on the LE HID stream: XboxOneInputReport0x01 without the report id */
        constexpr size_t ReportOffsetTriggers = 8;
        bool ReportHasAnyInput(const u8 *data, size_t size) {
            for (size_t i = ReportOffsetTriggers; i < std::min(size, sizeof(controller::XboxOneInputReport0x01)); ++i) {
                if (data[i] != 0) {
                    return true;
                }
            }
            return false;
        }

        // Whether a report carries something a human did. The Xbox offsets are that pad's layout; a pad whose
        // resting report is known is compared against it instead of against a guess.
        bool ReportCarriesInput(pad::Kind kind, const u8 *data, size_t size) {
            if (kind != pad::Kind::Xbox) {
                return !IsRestingReport(kind, data, size);
            }
            return ReportHasAnyInput(data, size);
        }

        void Unpark() {
            std::scoped_lock dk(g_delivery_lock);

            std::shared_ptr<controller::SwitchController> controller;
            {
                std::scoped_lock lk(g_controller_lock);
                if (!g_parked || !g_controller) {
                    return;
                }
                g_parked = false;
                controller = g_controller;
            }
            controller::ReattachVirtualController(controller);
            ScheduleNeutralReports();
            log::Write("controller presented to the system again");
        }

        /* Advertisement parsing: btdrv hands over already-separated AD records, which the portable parser in
           ble_advertisement.hpp turns into what is known about a device. An advertisement only decides whether
           a device is worth connecting to; what it *is* gets settled from its attributes later, because the
           Stadia and Xbox controllers advertise the same gamepad appearance. */

        adv::Summary g_device_advertisement;    // the advertisement of the device being connected to

        void ParseAdvertisements(const BtdrvBleAdvertisement *ad_list, u8 count, adv::Summary *out) {
            adv::Reset(out);

            count = std::min<u8>(count, 10);
            for (u8 i = 0; i < count; ++i) {
                auto &ad = ad_list[i];
                adv::Feed(out, ad.type, ad.data, std::min<size_t>(ad.size, sizeof(ad.data)));
            }
        }

        /* LE HID event stream: notifications from services registered with RegisterGattHidDataPath, and the results
           of our GATT requests on them, arrive here rather than on the managed BLE event */

        void LeHidThreadFunc(void *) {
            diagnostics::Note(diagnostics::Metric::ListenerStarted);
            Event event;
            Result rc = btdrvRegisterBleHidEvent(&event);
            log::Write("btdrvRegisterBleHidEvent rc=0x%x", rc.GetValue());
            diagnostics::Note(R_SUCCEEDED(rc) ? diagnostics::Metric::ListenerRegistered : diagnostics::Metric::ListenerRegistrationFailed);
            if (R_FAILED(rc)) {
                return;
            }

            os::SystemEvent system_event;
            system_event.AttachReadableHandle(event.revent, false, os::EventClearMode_AutoClear);

            u64 last_report_us = 0;
            u32 last_session = 0;
            for (;;) {
                system_event.Wait();
                const auto work_start = diagnostics::InputNowUs();
                diagnostics::Note(diagnostics::Metric::ListenerWake);

                const auto ipc_start = diagnostics::InputNowUs();
                rc = btdrvGetLeHidEventInfo(&g_le_hid_record.info, sizeof(g_le_hid_record.info), &g_le_hid_record.type);
                const auto report_return_us = diagnostics::InputNowUs();
                diagnostics::RecordInputDuration(diagnostics::TimingMetric::SlotIpc, ipc_start);
                if (R_FAILED(rc)) {
                    diagnostics::Note(diagnostics::Metric::ListenerReadFailed);
                    log::Write("btdrvGetLeHidEventInfo rc=0x%x", rc.GetValue());
                    continue;
                }

                // A successful IPC return (and even a previously stored type) does not prove a nonempty slot.
                diagnostics::Note(diagnostics::Metric::ListenerReadReturned);
                diagnostics::Note(g_le_hid_record.type == BtdrvBleEventType_ClientNotify ?
                    diagnostics::Metric::ListenerNotifyType : diagnostics::Metric::ListenerOtherType);
                auto &notify = g_le_hid_record.info.client_notify;
                if ((g_le_hid_record.type != BtdrvBleEventType_ClientNotify) || (notify.conn_id != events::GetConnection())) {
                    events::Log(g_le_hid_record);
                    continue;
                }

                diagnostics::Note(diagnostics::Metric::ListenerCurrentConnection);
                if ((notify.type == GattNotifyType_Notification) && IsUuid16(notify.char_uuid, GattCharReport)) {
                    if (notify.conn_id != g_presented_conn_id) {
                        // The stack recycles connection ids: this is a report from a link that is not the one
                        // the presented controller was built for, most often the tail of one just torn down
                        log::Write("notification for conn_id=%u ignored, the presented controller is conn_id=%u", notify.conn_id, g_presented_conn_id);
                        continue;
                    }

                    const u32 session = diagnostics::InputSession();
                    if (session != last_session) { last_session = session; last_report_us = 0; }
                    if (last_report_us && report_return_us >= last_report_us) {
                        diagnostics::RecordInputValue(diagnostics::TimingMetric::NotificationGap, report_return_us - last_report_us);
                    }
                    last_report_us = report_return_us;
                    ON_SCOPE_EXIT { diagnostics::RecordInputDuration(diagnostics::TimingMetric::InputWork, work_start); };
                    const size_t size = std::min<size_t>(notify.size, sizeof(notify.data));
                    std::shared_ptr<controller::SwitchController> controller;
                    pad::Kind kind;
                    bool parked;
                    {
                        std::scoped_lock lk(g_controller_lock);
                        controller = g_controller;
                        kind = g_kind;
                        parked = g_parked;
                    }
                    if (parked) {
                        // What brings a controller back on the Controllers screen is L+R; afterwards any button does
                        if (pad::HasLrShoulders(kind, notify.data, size) || (!events::IsPairingWindowOpen() && ReportCarriesInput(kind, notify.data, size))) {
                            Unpark();
                        } else {
                            continue;
                        }
                    }
                    if (controller) {
                        const Result rc = DeliverInput(controller, kind, notify.data, size);
                        if (R_FAILED(rc)) {
                            log::Write("report of %zu bytes refused: rc=0x%x", size, rc.GetValue());
                        }
                    }
                } else {
                    // The value of a characteristic we asked to read arrives here as well
                    log::Write("GATT completion source=LE-HID");
                    gatt::OnClientNotify(g_le_hid_record);
                    events::NoteGattResult(notify.result);
                    events::Log(g_le_hid_record);
                }
            }
        }

        void StartLeHidListener() {
            R_ABORT_UNLESS(os::CreateThread(&g_le_hid_thread,
                LeHidThreadFunc,
                nullptr,
                g_le_hid_thread_stack,
                LeHidThreadStackSize,
                ThreadPriority
            ));

            os::SetThreadNamePointer(&g_le_hid_thread, "mc::BleHidEventThread");
            os::StartThread(&g_le_hid_thread);
        }

        /* Bring-up */

        Result RegisterGattClient() {
            events::SetRegistering(true);
            ON_SCOPE_EXIT { events::SetRegistering(false); };

            Result rc = btdrvRegisterGattClient(&GattClientAppUuid);
            log::Write("btdrvRegisterGattClient rc=0x%x", rc.GetValue());
            R_TRY(rc);

            EventRecord e;
            if (!events::WaitFor(BtdrvBleEventType_ClientRegistration, TimeSpan::FromSeconds(5), &e, [](const EventRecord &r) { return r.info.client_registration.status == 1; })) {
                R_THROW(svc::ResultTimedOut());
            }

            g_client_if = e.info.client_registration.client_if;
            R_SUCCEED();
        }

        Result PrepareClient() {
            events::TakeOwnership();

            // btm leaves BLE disabled on a retail console. Enabling it here makes btdrv raise a BluetoothCrash
            // event, which btm answers by cycling the radio and bringing BLE up itself, registering four GATT
            // clients on the way; scans only return results after that, so the event is deliberately forwarded.
            Result rc = btdrvEnableBle();
            log::Write("btdrvEnableBle rc=0x%x", rc.GetValue());
            events::Pump(TimeSpan::FromSeconds(6));

            // btm's registrations take most of the stack's client slots; borrow the last one rather than risk
            // finding none free
            g_client_if = events::GetSpareClientIf();
            if (g_client_if != InvalidClientIf) {
                log::Write("using gatt client_if=%u", g_client_if);
                R_SUCCEED();
            }

            for (int attempt = 0; attempt < 3; ++attempt) {
                if (R_SUCCEEDED(RegisterGattClient())) {
                    log::Write("registered gatt client_if=%u", g_client_if);
                    R_SUCCEED();
                }
                os::SleepThread(TimeSpan::FromSeconds(2));
            }

            R_THROW(svc::ResultTimedOut());
        }

        // Blocks while the system has BLE switched off (sleep): any radio activity then wakes the console. A wake
        // re-registers btm's clients, so ours is picked up again afterwards.
        // False once the system is going away: the radio will not come back on, and the host must not be
        // waiting for it when the shutdown joins this thread
        bool WaitForRadio() {
            while (!events::IsRadioEnabled() && !events::IsShuttingDown()) {
                log::Write("ble switched off by the system, waiting");
                events::WaitRadioEnabled();
                if (!events::IsShuttingDown()) {
                    log::Write("ble switched back on");
                }
            }

            if (events::IsShuttingDown()) {
                return false;
            }

            if (events::TakeRadioCycled()) {
                events::Pump(TimeSpan::FromSeconds(2));
                if (u8 spare = events::GetSpareClientIf(); spare != InvalidClientIf) {
                    g_client_if = spare;
                }
                log::Write("using gatt client_if=%u", g_client_if);
            }
            return true;
        }

        /* Auto-connect: the remembered controller, reconnected by the stack when it comes back */

        constexpr const char LastDevicePath[] = "sdmc:/config/MissionControl/ble_last_device.bin";

        constinit os::SdkMutex g_autoconnect_lock;
        constinit bool g_have_remembered = false;
        constinit bluetooth::Address g_remembered = {};

        bool AutoConnectEnabled() {
            return mitm::GetGlobalConfig()->ble.auto_connect;
        }

        void LoadRememberedDevice() {
            if (!AutoConnectEnabled()) {
                return;
            }

            fs::FileHandle file;
            if (R_FAILED(fs::OpenFile(std::addressof(file), LastDevicePath, fs::OpenMode_Read))) {
                log::Write("auto-connect: no remembered controller");
                return;
            }
            ON_SCOPE_EXIT { fs::CloseFile(file); };

            bluetooth::Address address;
            size_t read = 0;
            if (R_FAILED(fs::ReadFile(std::addressof(read), file, 0, std::addressof(address), sizeof(address))) || (read != sizeof(address))) {
                return;
            }

            {
                std::scoped_lock lk(g_autoconnect_lock);
                g_remembered = address;
                g_have_remembered = true;
            }
            events::RememberAttempted(&address);
            events::LogAddress("auto-connect: remembered controller", address);
        }

        void SaveRememberedDevice(const bluetooth::Address &address) {
            {
                std::scoped_lock lk(g_autoconnect_lock);
                if (g_have_remembered && AddressEqual(g_remembered, address)) {
                    return;
                }
                g_remembered = address;
                g_have_remembered = true;
            }

            if (!AutoConnectEnabled()) {
                return;
            }

            static_cast<void>(fs::CreateFile(LastDevicePath, sizeof(address)));
            fs::FileHandle file;
            if (R_FAILED(fs::OpenFile(std::addressof(file), LastDevicePath, fs::OpenMode_Write))) {
                log::Write("auto-connect: could not save the controller");
                return;
            }
            ON_SCOPE_EXIT { fs::CloseFile(file); };
            static_cast<void>(fs::SetFileSize(file, sizeof(address)));
            static_cast<void>(fs::WriteFile(file, 0, std::addressof(address), sizeof(address), fs::WriteOption::Flush));
            events::LogAddress("auto-connect: saved controller", address);
        }

        bool GetRememberedDevice(bluetooth::Address *out) {
            std::scoped_lock lk(g_autoconnect_lock);
            if (g_have_remembered) {
                *out = g_remembered;
            }
            return g_have_remembered;
        }

        // A background connection request (ConnectGattServer with is_direct=false) was tried first: the stack
        // accepted it but never connected when the controller was switched on by its Xbox button, and the
        // request could not be cancelled afterwards (0x29e71). Without a Secure Connections bond the controller
        // has no console to reconnect to and just switches itself off again, so the Xbox button alone cannot
        // bring it back. What works outside the Controllers screen is the sync button: the controller then
        // advertises to anyone, and a scan finds it at once since btm's inquiry only runs while the screen is
        // open. Scanning in bursts while no controller is connected keeps the radio mostly free.
        constexpr TimeSpan BackgroundScanDuration = TimeSpan::FromMilliSeconds(2500);
        constexpr TimeSpan BackgroundScanPause    = TimeSpan::FromMilliSeconds(2000);

        bool BackgroundScanWanted() {
            return AutoConnectEnabled() && g_have_remembered && events::IsRadioEnabled() && !events::IsPairingWindowOpen();
        }

        // Set when PrepareForSleep dropped the link: that end of link is no reason to cycle the radio on wake
        constinit bool g_dropped_for_sleep = false;

        // EXPERIMENT. The stack refuses every CreateBond for a controller that has been switched off until the
        // console restarts. A restart is not the only thing that resets the stack: btm cycles the radio whenever
        // btdrv reports a BluetoothCrash, which is how BLE came up in PrepareClient. Ask btdrv for one and pick up
        // the GATT client btm registers afterwards, as the bring-up does.
        void RecycleRadio() {
            if (g_dropped_for_sleep) {
                g_dropped_for_sleep = false;
                log::Write("link was dropped for sleep, no radio cycle");
                return;
            }

            if (g_setup_failed || !mitm::GetGlobalConfig()->ble.radio_cycle || !events::IsRadioEnabled()) {
                return;
            }

            events::ClearSpareClientIf();

            Result rc = btdrvEmulateBluetoothCrash(BtdrvFatalReason_Unknown1);
            log::Write("radio cycle: btdrvEmulateBluetoothCrash rc=0x%x", rc.GetValue());
            if (R_FAILED(rc)) {
                return;
            }

            events::Pump(TimeSpan::FromSeconds(6));

            if (u8 spare = events::GetSpareClientIf(); spare != InvalidClientIf) {
                g_client_if = spare;
                log::Write("radio cycle: btm brought BLE back up, using gatt client_if=%u", g_client_if);
                return;
            }

            // btm did not bring BLE back up on its own; do what the bring-up does
            rc = btdrvEnableBle();
            log::Write("radio cycle: btdrvEnableBle rc=0x%x", rc.GetValue());
            events::Pump(TimeSpan::FromSeconds(6));

            if (u8 spare = events::GetSpareClientIf(); spare != InvalidClientIf) {
                g_client_if = spare;
                log::Write("radio cycle: using gatt client_if=%u", g_client_if);
                return;
            }

            for (int attempt = 0; attempt < 3; ++attempt) {
                if (R_SUCCEEDED(RegisterGattClient())) {
                    log::Write("radio cycle: registered gatt client_if=%u", g_client_if);
                    return;
                }
                os::SleepThread(TimeSpan::FromSeconds(2));
            }

            log::Write("radio cycle: no gatt client available, keeping client_if=%u", g_client_if);
        }

        /* Finding and connecting the controller */

        // With `only` set, nothing but that controller is accepted (background scans outside the pairing window)
        Result Scan(bluetooth::Address *out_address, TimeSpan duration, const bluetooth::Address *only = nullptr) {
            events::SetScanning(true);
            ON_SCOPE_EXIT { events::SetScanning(false); };

            // Nothing is reported while btm's scan filter conditions are installed; the system itself clears
            // them whenever the HOME menu is left, so nothing depends on them
            events::ExpectFilterEvent();
            static_cast<void>(btdrvClearBleScanFilters());
            events::ExpectFilterEvent();
            static_cast<void>(btdrvEnableBleScanFilter(false));

            // The two filter calls answer asynchronously; let them land before asking for the scan
            for (int i = 0; i < 2; ++i) {
                events::WaitFor(BtdrvBleEventType_ScanFilter, TimeSpan::FromMilliSeconds(400), nullptr, [](const EventRecord &) { return true; });
            }

            // The stack refuses the scan (0x2a871) for as long as btm's classic inquiry has the radio: 10.24 s at
            // a time from the moment the Controllers screen opens, repeated with hardly a gap while it stays
            // open. Keep asking while the screen is open; outside it the scan starts at once.
            Result rc;
            int attempts = 0;
            const bool in_window = events::IsPairingWindowOpen();
            for (;;) {
                rc = btdrvStartBleScan();
                if (R_SUCCEEDED(rc) || (++attempts >= (in_window ? 150 : 3)) || (in_window && !events::IsPairingWindowOpen())) {
                    break;
                }
                if (attempts == 1) {
                    log::Write("btdrvStartBleScan rc=0x%x, retrying", rc.GetValue());
                }
                events::Pump(TimeSpan::FromMilliSeconds(200));
            }
            if (R_FAILED(rc)) {
                log::Write("btdrvStartBleScan rc=0x%x after %d attempts", rc.GetValue(), attempts);
                events::Pump(TimeSpan::FromSeconds(1));
                return rc;
            }
            if (attempts > 0) {
                log::Write("scanning (%d refused attempts first)", attempts);
            }

            EventRecord e;
            // Ends early if a link came up by itself or the console is going to sleep
            const bool found = events::WaitFor(events::InvalidType, duration, &e, [only](const EventRecord &r) {
                if (r.type != BtdrvBleEventType_ScanResult) {
                    return events::HasAdoptedConnection() || !events::IsRadioEnabled();
                }
                if (r.info.scan_result.status != BleScanStatus_NewDevice) {
                    return false;
                }
                if (only) {
                    return AddressEqual(r.info.scan_result.address, *only);
                }
                adv::Summary summary;
                ParseAdvertisements(r.info.scan_result.ad_list, r.info.scan_result.count, &summary);
                if (pad::IsWorthConnecting(summary)) {
                    g_device_advertisement = summary;
                    return true;
                }
                return false;
            });

            // Once the system has switched BLE off (and the wait above can end on exactly that), every radio
            // call from here would wake the console straight back up
            if (events::IsRadioEnabled()) {
                static_cast<void>(btdrvStopBleScan());
                events::ExpectFilterEvent();
                static_cast<void>(btdrvEnableBleScanFilter(true));
            }

            // The scan-complete and filter events trail the calls above
            events::Pump(TimeSpan::FromSeconds(1));

            R_UNLESS(!events::HasAdoptedConnection() && events::IsRadioEnabled(), svc::ResultCancelled());
            R_UNLESS(found, svc::ResultTimedOut());

            *out_address = e.info.scan_result.address;
            events::LogAddress("found controller", *out_address);

            if (log::IsEnabled()) {
                SetSysBluetoothDevicesSettings settings = {};
                Result info_rc = btdrvGetPairedDeviceInfo(*out_address, &settings);
                if (R_SUCCEEDED(info_rc)) {
                    log::Write("btdrvGetPairedDeviceInfo rc=0x0 key_present=%u key_type=%u device_type=%u",
                        settings.link_key_present, settings.key_type, settings.device_type);
                } else {
                    log::Write("btdrvGetPairedDeviceInfo rc=0x%x (no valid metadata)", info_rc.GetValue());
                }
            }

            R_SUCCEED();
        }

        Result WaitForConnection(const bluetooth::Address &address, u32 *out_conn_id, TimeSpan timeout) {
            EventRecord e;
            if (!events::WaitFor(BtdrvBleEventType_ClientConnection, timeout, &e, [&](const EventRecord &r) {
                return AddressEqual(r.info.client_connection.address, address) || (r.info.client_connection.result != 0);
            })) {
                R_THROW(svc::ResultTimedOut());
            }

            if ((e.info.client_connection.result != 0) || (e.info.client_connection.status != BleConnectionStatus_Connected)) {
                log::Write("connection failed result=%u status=%u", e.info.client_connection.result, e.info.client_connection.status);
                R_THROW(ResultNoConnection());
            }

            *out_conn_id = e.info.client_connection.conn_id;
            events::SetConnection(*out_conn_id);
            log::Write("connected conn_id=%u", *out_conn_id);
            R_SUCCEED();
        }

        Result Connect(const bluetooth::Address &address, u32 *out_conn_id, TimeSpan timeout) {
            events::SetTarget(&address);

            // A record left for this address, including the one a classic firmware left behind, is encrypted
            // with the moment the link comes up, and the controller drops the link over a key it doesn't hold
            Result rc = btdrvRemoveBond(address);
            log::Write("btdrvRemoveBond rc=0x%x", rc.GetValue());

            rc = btdrvConnectGattServer(g_client_if, address, true, 0);
            log::Write("btdrvConnectGattServer rc=0x%x", rc.GetValue());
            R_TRY(rc);

            rc = WaitForConnection(address, out_conn_id, timeout);
            if (R_FAILED(rc)) {
                static_cast<void>(btdrvCancelConnectGattServer(g_client_if, address, true));
            }
            R_RETURN(rc);
        }

        bool Adopt(bluetooth::Address *out_address, u32 *out_conn_id) {
            if (!events::TakeAdoptedConnection(out_address, out_conn_id)) {
                return false;
            }

            // A link that came up without being asked for was never seen in an advertisement, so nothing is
            // known about it from one. Left unset, the fallback rule treats it as it always did.
            adv::Reset(&g_device_advertisement);

            log::Write("controller connected on its own, conn_id=%u", *out_conn_id);
            return true;
        }

        // The stack never pairs on its own: an authenticated request on an unbonded link just fails. Pairing is
        // Just Works and needs no confirmation from the controller.
        bool Pair(const bluetooth::Address &address) {
            if (events::IsDisconnected() || !events::IsRadioEnabled()) {
                return false;
            }
            // Discovery can already have produced this lifecycle event. Do not let that old flag satisfy the
            // upcoming request; even a new event is only a heuristic, not proof that encryption succeeded.
            static_cast<void>(events::TakeBondComplete());
            log::Write("requesting LE bond; waiting for a new core lifecycle event");
            Result rc = btdrvCreateBond(address, BondTransport_Le);
            log::Write("btdrvCreateBond rc=0x%x", rc.GetValue());
            if (R_FAILED(rc)) {
                return false;
            }

            // The link-state event that reports the outcome is the same for success (~5 s) and refusal (at once,
            // followed by the stack dropping the link)
            const auto deadline = os::GetSystemTick().ToTimeSpan() + TimeSpan::FromSeconds(10);
            while (os::GetSystemTick().ToTimeSpan() < deadline) {
                events::Pump(TimeSpan::FromMilliSeconds(200));
                if (events::IsDisconnected()) {
                    break;
                }
                if (events::TakeBondComplete()) {
                    // A refusal drops the link within a few hundred milliseconds of this event
                    events::Pump(TimeSpan::FromMilliSeconds(500));
                    if (!events::IsDisconnected()) {
                        log::Write("bond lifecycle event observed; authentication not yet verified");
                        return true;
                    }
                    break;
                }
            }
            log::Write("pairing failed");
            return false;
        }

        /* Presenting the controller to the system */

        // False if the pad could not be made to accept input, in which case nothing was attached
        bool Present(u32 conn_id, const bluetooth::Address &address, const gatt::HidService &hid, const pad::Policy &policy) {
            const bluetooth::hogp::Endpoint endpoint {
                .generation = ++g_endpoint_generation,
                .connection = conn_id,
                .service = GattServiceHid,
                .characteristic = GattCharReport,
            };

            std::shared_ptr<controller::SwitchController> controller;
            if (policy.kind == pad::Kind::Stadia) {
                if (!hid.has_output_report) {
                    log::Write("no output Report characteristic, and nothing is written to a Stadia controller anyway");
                }

                auto stadia = std::make_shared<controller::StadiaBleController>(address, endpoint, policy.input_report_id, policy.input_report_size);
                if (!stadia->BindInput()) {
                    log::Write("could not bind input to the discovered report endpoint, nothing presented");
                    return false;
                }
                controller = stadia;
            } else {
                if (!hid.has_output_report) {
                    log::Write("no output Report characteristic, rumble unavailable");
                }
                controller = std::make_shared<controller::XboxOneBleController>(address, conn_id, hid.service.id, hid.service.is_primary, hid.output_report.id, hid.has_output_report);
            }

            {
                std::scoped_lock lk(g_controller_lock);
                g_controller = controller;
                g_kind = policy.kind;
                g_presented_conn_id = conn_id;
                g_parked = false;
                g_present_tick = os::GetSystemTick().GetInt64Value();
            }
            controller::AttachVirtualController(controller);
            SaveRememberedDevice(address);

            // The pad only reports when something is touched, so its resting state (sticks centred, nothing
            // pressed) is delivered for it, now and again over the next seconds, see ScheduleNeutralReports
            ScheduleNeutralReports();
            ServiceNeutralReports();

            if (policy.identified) {
                log::Write("%s controller presented to the system", pad::Name(policy.kind));
            } else {
                log::Write("controller presented to the system, assumed to be an Xbox pad");
            }
            return true;
        }

        void Dismiss(const bluetooth::Address &address) {
            std::shared_ptr<controller::SwitchController> controller;
            bool parked;
            {
                std::scoped_lock lk(g_controller_lock);
                controller = std::move(g_controller);
                g_controller = nullptr;
                g_kind = pad::Kind::None;
                g_presented_conn_id = InvalidConnId;
                parked = g_parked;
                g_parked = false;
                for (auto &due : g_neutral_due) {
                    due = 0;
                }
            }

            if (controller && !parked) {
                controller::DetachVirtualController(address);
                log::Write("controller removed from the system");
            }
        }

        void Disconnect() {
            const u32 conn_id = events::GetConnection();

            bluetooth::Address address;
            if (events::GetTarget(&address)) {
                Dismiss(address);
            }

            // Nothing may touch the radio while the system has BLE switched off: it wakes the console
            if ((conn_id != InvalidConnId) && !events::IsDisconnected() && events::IsRadioEnabled()) {
                static_cast<void>(btdrvDisconnectGattServer(conn_id));
            }

            // Keep the target until the stack has finished tearing the link down
            events::Pump(TimeSpan::FromSeconds(1));

            events::SetConnection(InvalidConnId);
            events::SetTarget(nullptr);
            events::SetClaimedAddress(nullptr);
        }

        void RunConnected(u32 conn_id) {
            for (;;) {
                events::Pump(TimeSpan::FromMilliSeconds(250));
                ServiceNeutralReports();

                if (gatt::TakeRediscovered()) {
                    log::Write("services re-discovered, subscribing again");
                    gatt::HidService hid = {};
                    const Result rc = gatt::Subscribe(conn_id, &hid);
                    if (R_FAILED(rc)) {
                        log::Write("re-subscription failed rc=0x%x", rc.GetValue());
                        StopAfterSetupFailure("GATT rediscovery");
                        return;
                    }
                }

                if (events::IsDisconnected()) {
                    log::Write("controller disconnected reason=0x%04x", events::GetDisconnectReason());
                    return;
                }

                if (!events::IsRadioEnabled()) {
                    return;
                }
            }
        }

        /*
         * A controller is paired while the Controllers screen has the system searching, the same window in which
         * a Pro Controller pairs, and used until its link drops. Every connection is a fresh pairing: the console's
         * stack (Broadcom BSA 1.6) only knows LE legacy pairing and these pads keep a bond only from LE Secure
         * Connections, so a pad that reconnected on its own would refuse the stored key anyway.
         */
        void HostThreadFunc(void *) {
            mitm::WaitInitialized();
            bluetooth::ble::WaitInitialized();

            // Bringing BLE up cycles the radio (see PrepareClient); let the system finish its own bring-up first
            os::SleepThread(TimeSpan::FromSeconds(20));
            log::Write("starting");

            if (R_FAILED(PrepareClient())) {
                log::Write("giving up: no gatt client available");
                return;
            }

            StartLeHidListener();
            events::SetSink(gatt::OnEvent);
            LoadRememberedDevice();

            for (;;) {
                if (!WaitForRadio()) {
                    log::Write("system is shutting down, the host stops here");
                    break;
                }

                if (g_setup_failed) {
                    // Drain withheld events, but leave the radio alone. Shutdown still wakes/exits this loop.
                    events::Pump(TimeSpan::FromMilliSeconds(250));
                    continue;
                }

                bluetooth::Address address;
                u32 conn_id = InvalidConnId;

                bool connected = Adopt(&address, &conn_id);

                // Any gamepad in pairing mode while the Controllers screen is open; only the remembered one,
                // in short bursts, the rest of the time
                bluetooth::Address remembered;
                const bool in_window = events::IsPairingWindowOpen() && events::IsRadioEnabled();
                const bool background = !in_window && BackgroundScanWanted() && GetRememberedDevice(&remembered);
                if (!connected && (in_window || background)) {
                    Result rc = in_window ? Scan(&address, TimeSpan::FromSeconds(10)) : Scan(&address, BackgroundScanDuration, &remembered);
                    if (Adopt(&address, &conn_id)) {
                        connected = true;
                    } else if (R_SUCCEEDED(rc)) {
                        if (background) {
                            log::Write("remembered controller is in pairing mode, connecting");
                        }
                        if (RecentlyRefused(address)) {
                            log::Write("this device was refused less than a minute ago, not reconnecting to it");
                        } else {
                            connected = R_SUCCEEDED(Connect(address, &conn_id, TimeSpan::FromSeconds(15)));
                            if (!connected) {
                                StopAfterSetupFailure("connect");
                            }
                        }
                        if (!connected) {
                            Disconnect();
                        }
                    }
                }

                if (!connected) {
                    os::SleepThread(background ? BackgroundScanPause : TimeSpan::FromSeconds(2));
                    continue;
                }

                // Pairing traffic for this pad is answered on its behalf from here on
                events::SetClaimedAddress(&address);

                // Start the bond immediately, before this host requests discovery/MTU work. A Stadia-capable
                // reference host starts security at link-up, not after GATT setup. Prepare the passive cache
                // first so automatic discovery records delivered while Pair pumps events are not discarded.
                diagnostics::BeginInputSession();
                gatt::PrepareDiscovery(address);
                log::Write("security-first setup: requesting bond before explicit GATT discovery/MTU");
                if (!Pair(address)) {
                    StopAfterSetupFailure("bond request");
                    Disconnect();
                    os::SleepThread(TimeSpan::FromSeconds(1));
                    continue;
                }

                // Pair's lifecycle heuristic is still not encryption proof. The authenticated read below must
                // complete successfully; neither the advertisement nor this ordering permits presentation.
                Result rc = gatt::Discover(conn_id, address);
                if (R_FAILED(rc)) {
                    StopAfterSetupFailure("GATT discovery cache");
                    Disconnect();
                    continue;
                }

                // Hands the HID service to the stack's HID host layer for this link: its notifications, and the results
                // of our requests on it, then arrive on the LE HID event stream
                auto hid_uuid = MakeUuid16(GattServiceHid);
                rc = btdrvRegisterGattHidDataPath(&hid_uuid);
                log::Write("btdrvRegisterGattHidDataPath rc=0x%x", rc.GetValue());
                if (R_FAILED(rc)) {
                    StopAfterSetupFailure("HID data path");
                    Disconnect();
                    continue;
                }

                // The subscription is judged by the CCCD write's result; the pad only reports on input
                events::ClearGattFailure();
                gatt::HidService hid = {};
                rc = gatt::Subscribe(conn_id, &hid);
                if (R_FAILED(rc)) {
                    log::Write("subscription failed rc=0x%x last_recorded_gatt_error=%u (zero is not proof of completion)", rc.GetValue(), events::GetGattFailure());
                    StopAfterSetupFailure("GATT setup");
                    Disconnect();
                    os::SleepThread(TimeSpan::FromSeconds(1));
                    continue;
                }

                // What the device says about its own reports, asked before anything is presented. A pad that
                // cannot be placed is dropped rather than driven with the nearest known layout: reports
                // decoded at the wrong offset reach the system as garbage input, which is worse usefully than
                // a controller that plainly refuses to work.
                gatt::ReportFacts facts;
                const bool have_facts = R_SUCCEEDED(gatt::Describe(conn_id, hid, &facts));

                auto policy = pad::PolicyFor(pad::Kind::None);
                if (have_facts) {
                    const pad::Fingerprint fingerprint {
                        .input_report_id = facts.input_report_id,
                        .input_report_size = facts.input_report_size,
                        .output_report_id = facts.output_report_id,
                        .output_report_size = facts.output_report_size,
                        .has_protocol_mode = facts.has_protocol_mode,
                        .has_hid_control_point = facts.has_hid_control_point,
                        .name_says_xbox = adv::NameContains(g_device_advertisement, "Xbox"),
                        .name_says_stadia = adv::NameContains(g_device_advertisement, "Stadia"),
                    };

                    policy = pad::PolicyFor(pad::Identify(fingerprint));
                    if (policy.kind != pad::Kind::None) {
                        // The device described itself, so use what it said rather than the table's numbers
                        policy.input_report_id = facts.input_report_id;
                        policy.input_report_size = facts.input_report_size;
                    } else {
                        log::Write("no pad matched input report id=0x%02x size=%u, output id=0x%02x size=%u",
                            facts.input_report_id, facts.input_report_size, facts.output_report_id, facts.output_report_size);
                    }
                } else {
                    log::Write("could not read the device's report map");
                }

                if (policy.kind == pad::Kind::None) {
                    /*
                     * Fall back on the rule this module had before it read report maps - anything that looked
                     * like a gamepad was an Xbox controller - but only for a device that is not itself claiming
                     * to be a HID device, since such a device has told us enough to know we do not recognise
                     * it. The assumption is logged as an assumption.
                     */
                    policy = pad::LegacyPolicy(g_device_advertisement);
                    if (policy.kind == pad::Kind::None) {
                        log::Write("nothing presented: this device is not a pad this module knows how to decode");
                        gatt::Dump("unrecognised device");
                        g_refused_address = address;
                        g_refused_tick = os::GetSystemTick().GetInt64Value();
                        StopAfterSetupFailure("pad identification");
                        Disconnect();
                        os::SleepThread(TimeSpan::FromSeconds(1));
                        continue;
                    }
                    gatt::Dump("assumed pad");
                }

                if (!Present(conn_id, address, hid, policy)) {
                    StopAfterSetupFailure("presentation");
                    Disconnect();
                    os::SleepThread(TimeSpan::FromSeconds(1));
                    continue;
                }
                RunConnected(conn_id);
                // No SD writes from the notification thread or periodically while the test is running.
                if (events::IsDisconnected() && events::IsRadioEnabled() && !events::IsShuttingDown()) diagnostics::DumpInputTiming();

                // Nothing beyond the teardown is asked of the stack once the link is gone: removing the bond and
                // detaching the HID host layer here were both tried and changed nothing. A radio cycle is the one
                // thing left to try (skipped when the link ended because the console is going to sleep).
                Disconnect();
                RecycleRadio();
            }
        }

    }

    bool IsEnabled() {
        return mitm::GetGlobalConfig()->ble.enable && (hos::GetVersion() >= hos::Version_13_0_0);
    }

    void Launch() {
        if (!IsEnabled()) {
            return;
        }

        auto config = mitm::GetGlobalConfig();
        if (config->ble.log) {
            log::Initialize();
        }
        events::Activate();

        R_ABORT_UNLESS(os::CreateThread(&g_thread,
            HostThreadFunc,
            nullptr,
            g_thread_stack,
            ThreadStackSize,
            ThreadPriority
        ));

        os::SetThreadNamePointer(&g_thread, "mc::BleHidHostThread");
        os::StartThread(&g_thread);

        g_launched = true;
    }

    void WaitFinished() {
        if (g_launched) {
            // The host parks waiting for the radio whenever the system switches BLE off, and shutdown does
            // exactly that; without this the join below would never return
            events::BeginShutdown();
            os::WaitThread(&g_thread);
        }
    }

    void OnPairingWindow(bool open) {
        // btm restarts its inquiry every 10 s while the screen stays open, and stops and restarts it within a
        // few seconds when a controller connects; only a start after a quiet spell is the screen being opened.
        // A controller that was presented from within the screen stays, as any newly paired controller does.
        constexpr TimeSpan NewSessionGap = TimeSpan::FromSeconds(15);
        static constinit s64 s_last_close_tick = 0;

        const os::Tick now = os::GetSystemTick();
        if (!open) {
            // A parked controller comes back on its next button press, see LeHidThreadFunc
            s_last_close_tick = now.GetInt64Value();
            return;
        }

        if ((s_last_close_tick != 0) && ((now - os::Tick(s_last_close_tick)).ToTimeSpan() < NewSessionGap)) {
            return;
        }

        std::shared_ptr<controller::SwitchController> controller;
        {
            // Held over the detach below: a report arriving in between would otherwise re-attach the
            // controller and this thread would then take it back off again, leaving it marked unpresented
            // while the system still thinks it is there
            std::scoped_lock dk(g_delivery_lock);
            std::scoped_lock lk(g_controller_lock);
            if (!g_controller || g_parked || ((now - os::Tick(g_present_tick)).ToTimeSpan() < NewSessionGap)) {
                return;
            }
            g_parked = true;
            controller = g_controller;
            for (auto &due : g_neutral_due) {
                due = 0;
            }
        }
        controller::DetachVirtualController(controller->Address());
        log::Write("controller withdrawn while the Controllers screen is open, LB+RB brings it back");
    }

    // EXPERIMENT. A controller left connected wakes the console a few seconds after it goes to sleep. The link
    // is dropped here, from the power module thread, while the bluetooth module is still awake (it is listed as
    // a dependency of ours), so that nothing is left on the air by the time the radio goes down. The host thread
    // sees the radio marked unavailable and stays off it until FullAwake.
    void PrepareForSleep() {
        const bool was_enabled = events::IsRadioEnabled();
        events::SetRadioEnabled(false);

        // MinimumAwake and SleepReady both come through here; only the first one has anything to do
        if (!g_launched || !was_enabled) {
            return;
        }

        const u32 conn_id = events::GetConnection();
        if ((conn_id == InvalidConnId) || events::IsDisconnected()) {
            log::Write("going to sleep, no link to drop");
            return;
        }

        g_dropped_for_sleep = true;
        Result rc = btdrvDisconnectGattServer(conn_id);
        log::Write("going to sleep: btdrvDisconnectGattServer conn_id=%u rc=0x%x", conn_id, rc.GetValue());

        // The disconnection is reported on btdrv's event thread, independently of the host thread
        for (int i = 0; (i < 30) && !events::IsDisconnected(); ++i) {
            os::SleepThread(TimeSpan::FromMilliSeconds(50));
        }
        log::Write(events::IsDisconnected() ? "link down before sleep" : "link still up as sleep begins");
    }

}
