/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "stadia_ble_controller.hpp"
#include <stratosphere.hpp>

namespace ams::controller {

    Result StadiaBleController::HandleInputReport(const u8 *data, size_t size) {
        // The length came from the device's own report map, so anything else is not the report that was
        // subscribed to
        R_UNLESS((data != nullptr) && (size == m_input_report_size), svc::ResultInvalidSize());

        const bluetooth::hogp::InputReportView view {
            .id = m_input_report_id,
            .data = data,
            .size = size,
        };

        // A report for another endpoint is refused; a report that fails to decode revokes the lease inside
        // StadiaController and publishes neutral, which is why nothing is retried here
        R_UNLESS(this->HandleGattInput(m_endpoint, view), MAKERESULT(Module_Libnx, LibnxError_BadInput));
        R_SUCCEED();
    }

    Result StadiaBleController::WriteDataReport(const void *report_buffer, size_t size) {
        AMS_UNUSED(report_buffer, size);

        R_THROW(sf::ResultNotSupported());
    }

}
