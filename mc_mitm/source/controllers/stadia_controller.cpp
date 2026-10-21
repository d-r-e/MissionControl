/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#include "stadia_controller.hpp"

namespace ams::controller {
    // Keep the SDK-free publication policy tied to the actual wire structs.
    static_assert(sizeof(SwitchHidBasicInputReport) == 49);
    static_assert(sizeof(SwitchHidCommandInputReport) == 49);
    static_assert(sizeof(SwitchHidCommandOutputReport) == 49);
    static_assert(sizeof(SwitchHidBasicOutputReport) == 10);
    static_assert(offsetof(SwitchHidCommandOutputReport, command) == 10);
    static_assert(offsetof(SwitchHidCommand, set_data_format) == 1);
    static_assert(static_cast<u8>(SwitchHidCommandId::SetDataFormat) == 0x03);
    static_assert(static_cast<u8>(SwitchHidReportId::BasicInputReport) == 0x30);

    Result StadiaController::HandleOutputDataReport(const u8 *report_buffer, size_t size) {
        if (!StadiaVirtualOutputAllowed(report_buffer, size))
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        R_RETURN(EmulatedSwitchController::HandleOutputDataReport(report_buffer, size));
    }

    Result StadiaController::InjectInputReport(const u8 *report_buffer, size_t size) {
        const bool published = PublishStadiaVirtualInput(report_buffer, size,
            [this](u8 *data, size_t count) { this->ModifyInputReport(data, count); },
            [this](const u8 *data, size_t count) {
                EmulatedSwitchController::InjectInputReport(data, count);
            });
        if (!published) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        R_SUCCEED();
    }

    void StadiaController::HandleDataReportEvent(const bluetooth::HidReportEventInfo *event_info) {
        if (!m_gatt_input.HasBinding()) EmulatedSwitchController::HandleDataReportEvent(event_info);
    }

    void StadiaController::ParseInputReport(const u8 *report_buffer, size_t size) {
        StadiaInputReport report;
        if (!StadiaInputReport::Parse(report_buffer, size, &report)) {
            return;
        }

        // Never mix the prefixed HID path into an active GATT input lease.
        if (!m_gatt_input.HasBinding()) this->ApplyInput(report);
    }

    bool StadiaController::BindGattInput(const bluetooth::hogp::Endpoint &endpoint) {
        if (m_id.vid != hardware_ids[0].vid || m_id.pid != hardware_ids[0].pid || !m_gatt_input.Bind(endpoint))
            return false;
        this->Reset();
        this->ApplyInput(m_gatt_input.State());
        return true;
    }

    bool StadiaController::HandleGattInput(const bluetooth::hogp::Endpoint &endpoint,
                                          const bluetooth::hogp::InputReportView &view) {
        const auto result = m_gatt_input.Consume(endpoint, view);
        if (result == StadiaGattResult::Ignored) return false;
        // Invalid owned input revokes the input lease and publishes neutral,
        // not the last held button state. Foreign/stale input publishes nothing.
        if (result == StadiaGattResult::Retired) this->Reset();
        this->ApplyInput(m_gatt_input.State());
        this->PublishGattInput();
        return result == StadiaGattResult::Applied;
    }

    bool StadiaController::RetireGattInput(const bluetooth::hogp::Endpoint &endpoint) {
        if (!m_gatt_input.Retire(endpoint)) return false;
        this->Reset();
        this->ApplyInput(m_gatt_input.State());
        this->PublishGattInput();
        return true;
    }

    void StadiaController::PublishGattInput() {
        u8 buffer[0x200]{};
        const auto size = this->FillInputReport(buffer, sizeof(buffer));
        if (!size || size > sizeof(buffer)) return;
        // Use the same remapping and virtual input buffer as the HID path.
        // This does not create an HID connection or send physical HID output.
        this->InjectInputReport(buffer, size);
    }

    void StadiaController::ApplyInput(const StadiaInputReport &report) {
        const DirectionalPad dpad(DirectionalPadType<0, 8>{report.dpad});
        m_left_stick.SetValues(StadiaAxisToSwitch(report.left_x), StadiaAxisToSwitch(report.left_y, true));
        m_right_stick.SetValues(StadiaAxisToSwitch(report.right_x), StadiaAxisToSwitch(report.right_y, true));

        SwitchButtons buttons = m_buttons;
        buttons.Assign(SwitchButton::Up,      dpad.IsUp());
        buttons.Assign(SwitchButton::Right,   dpad.IsRight());
        buttons.Assign(SwitchButton::Down,    dpad.IsDown());
        buttons.Assign(SwitchButton::Left,    dpad.IsLeft());
        // Match physical Pro Controller positions, like other Xbox-layout pads.
        buttons.Assign(SwitchButton::A,       report.IsPressed(StadiaInputReport::B));
        buttons.Assign(SwitchButton::B,       report.IsPressed(StadiaInputReport::A));
        buttons.Assign(SwitchButton::X,       report.IsPressed(StadiaInputReport::Y));
        buttons.Assign(SwitchButton::Y,       report.IsPressed(StadiaInputReport::X));
        buttons.Assign(SwitchButton::L,       report.IsPressed(StadiaInputReport::L1));
        buttons.Assign(SwitchButton::R,       report.IsPressed(StadiaInputReport::R1));
        buttons.Assign(SwitchButton::ZL,      report.left_trigger > m_trigger_threshold * UINT8_MAX);
        buttons.Assign(SwitchButton::ZR,      report.right_trigger > m_trigger_threshold * UINT8_MAX);
        buttons.Assign(SwitchButton::StickL,  report.IsPressed(StadiaInputReport::L3));
        buttons.Assign(SwitchButton::StickR,  report.IsPressed(StadiaInputReport::R3));
        buttons.Assign(SwitchButton::Plus,    report.IsPressed(StadiaInputReport::Menu));
        buttons.Assign(SwitchButton::Minus,   report.IsPressed(StadiaInputReport::Options));
        buttons.Assign(SwitchButton::Home,    report.IsPressed(StadiaInputReport::Home));
        buttons.Assign(SwitchButton::Capture, report.IsPressed(StadiaInputReport::Capture));
        // Assistant and volume/media keys have no native Pro Controller analog.
        // No battery, motion, LED or rumble format has been verified yet.
        m_buttons = buttons;
    }

}
