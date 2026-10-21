/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "stadia_controller.hpp"

namespace ams::controller {

    /*
     * What the pad sends when nothing is touched, taken from the captured reports rather than composed: the
     * buttons are zeroed, the sticks sit at 0x80, the triggers are released, and the hat reads 0x08 - where
     * the pad parks it at rest, the four directions around it being 0x00/0x02/0x04/0x06. Payload only, as it
     * arrives from a notification, with the report id carried separately.
     */
    constexpr u8 StadiaBleNeutralReport[] = { 0x08, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00 };

    /*
     * A Stadia controller met over Bluetooth LE: the same input report the pad sends over its other
     * transports, arriving as a notification on a HID Report characteristic.
     *
     * Input enters through StadiaController's GATT path, which holds a lease on one endpoint and refuses
     * reports that do not belong to it, so a report that contradicts the device's own report map cannot be
     * mistaken for input the system should see.
     *
     * Nothing is ever written to the pad. Its writable Report characteristic carries report 0x05 - two
     * 16-bit values, per the report map recovered from the device - and what those mean is undocumented;
     * consumer-page usages included. A pad that has just been paired is the wrong place to find out, so a
     * write is refused instead. The pad has no rumble motors on any transport, so nothing is lost.
     */
    class StadiaBleController final : public StadiaController {
        public:
            StadiaBleController(bluetooth::Address address, const bluetooth::hogp::Endpoint &endpoint, u8 input_report_id, u8 input_report_size)
            : StadiaController(address, hardware_ids[0]), m_endpoint(endpoint), m_input_report_id(input_report_id), m_input_report_size(input_report_size) { }

        public:
            // Input is only accepted while the lease on this endpoint is held, so a controller whose binding
            // failed must not be presented to the system
            bool BindInput(void) {
                return this->BindGattInput(m_endpoint);
            }

            // A notification as the stack delivers it: the report id stripped from the data and carried
            // separately
            Result HandleInputReport(const u8 *data, size_t size);

            const bluetooth::hogp::Endpoint &Endpoint(void) const {
                return m_endpoint;
            }

        protected:
            virtual Result WriteDataReport(const void *report_buffer, size_t size) override;

        private:
            bluetooth::hogp::Endpoint m_endpoint;
            u8 m_input_report_id;
            u8 m_input_report_size;
    };

}
