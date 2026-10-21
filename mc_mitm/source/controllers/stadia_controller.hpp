/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "emulated_switch_controller.hpp"
#include "stadia_report.hpp"
#include "stadia_gatt_input.hpp"
#include "stadia_virtual_input.hpp"

namespace ams::controller {

    // Input translation only: BLE discovery/security and the virtual HID
    // connection lifecycle must be established by the transport first, which is
    // what StadiaBleController does for a Bluetooth LE connection.
    class StadiaController : public EmulatedSwitchController {
        public:
            static constexpr const HardwareID hardware_ids[] = {
                {0x18d1, 0x9400}, // Google Stadia Controller, Bluetooth mode
            };

            StadiaController(bluetooth::Address address, HardwareID id)
                : EmulatedSwitchController(address, id) { }

            virtual void ParseInputReport(const u8 *report_buffer, size_t size) override;
            virtual void HandleDataReportEvent(const bluetooth::HidReportEventInfo *event_info) override;
            virtual Result HandleOutputDataReport(const u8 *report_buffer, size_t size) override;
            virtual Result InjectInputReport(const u8 *report_buffer, size_t size) override;

            // Serialized worker-only API, after verified HOGP setup and virtual
            // attachment. No calls here initiate a Bluetooth connection.
            bool BindGattInput(const bluetooth::hogp::Endpoint &endpoint);
            bool HandleGattInput(const bluetooth::hogp::Endpoint &endpoint,
                                 const bluetooth::hogp::InputReportView &view);
            bool RetireGattInput(const bluetooth::hogp::Endpoint &endpoint);

        private:
            StadiaGattInput m_gatt_input;
            void ApplyInput(const StadiaInputReport &report);
            void PublishGattInput();
    };

}
