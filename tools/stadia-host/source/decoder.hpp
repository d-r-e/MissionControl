/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "stadia_report.hpp"
#include "hogp_report_router.hpp"
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace stadia_host {
using Report = ams::controller::StadiaInputReport;
namespace hogp = ams::bluetooth::hogp;

struct State {
    std::uint8_t dpad = 8;
    std::uint16_t buttons = 0;
    std::uint16_t left_x = 2047, left_y = 2047, right_x = 2047, right_y = 2047;
    std::uint8_t left_trigger = 0, right_trigger = 0, consumer_buttons = 0;
    bool operator==(const State &) const = default;
};
inline State Normalize(const Report &r) {
    using ams::controller::StadiaAxisToSwitch;
    return {r.dpad, r.buttons,
        StadiaAxisToSwitch(r.left_x), StadiaAxisToSwitch(r.left_y, true),
        StadiaAxisToSwitch(r.right_x), StadiaAxisToSwitch(r.right_y, true),
        r.left_trigger, r.right_trigger, r.consumer_buttons};
}
inline void PrintState(std::ostream &out, const State &s) {
    // Derived controller state only. Never print report bytes or OS identifiers.
    out << "{\"dpad\":" << +s.dpad << ",\"buttons\":" << s.buttons
        << ",\"left_x\":" << s.left_x << ",\"left_y\":" << s.left_y
        << ",\"right_x\":" << s.right_x << ",\"right_y\":" << s.right_y
        << ",\"left_trigger\":" << +s.left_trigger << ",\"right_trigger\":" << +s.right_trigger
        << ",\"consumer_buttons\":" << +s.consumer_buttons << '}';
}

// IOHID input is a separate framing/evidence boundary from GATT. Known Stadia
// captures are ID-prefixed11bytes; require the callback ID to agree as well.
// Do not infer another framing mode from the first byte or silently strip it.
class HidDecoder {
    hogp::ReportLayout layout_;
    bool active_ = false;
public:
    bool Open(const std::uint8_t *descriptor, std::size_t size) {
        Close();
        active_ = layout_.Parse(descriptor, size) == hogp::LayoutResult::Ok &&
            layout_.UsesReportIds() &&
            layout_.PayloadSize(Report::ReportId, hogp::ReportType::Input) == Report::PayloadSize;
        return active_;
    }
    void Close() { active_ = false; }
    bool Decode(std::uint32_t callback_id, const std::uint8_t *bytes, std::size_t size, State *out) const {
        if (out) *out = {};
        Report r{};
        if (!active_ || !out || callback_id != Report::ReportId ||
            !Report::Parse(bytes, size, &r)) return false;
        *out = Normalize(r);
        return true;
    }
};

// Fixture-only differential oracle. References/endpoints/subscription ACK here
// are MODELLED, not read from a peripheral. Never use this as a live GATT backend.
class GattFixtureModel {
    hogp::ReportLayout layout_;
    hogp::ReportRouter router_;
    hogp::Endpoint endpoint_{};
    std::uint64_t generation_ = 0;
public:
    bool Open(const std::uint8_t *descriptor, std::size_t size) {
        Close();
        if (layout_.Parse(descriptor, size) != hogp::LayoutResult::Ok ||
            !layout_.UsesReportIds() ||
            layout_.PayloadSize(Report::ReportId, hogp::ReportType::Input) != Report::PayloadSize ||
            generation_ == UINT64_MAX) return false;
        endpoint_ = {++generation_, 1, 1, 2};
        const std::uint8_t modelled_reference[] = {Report::ReportId, 1};
        return router_.Reset(generation_, 1) &&
            router_.BindInput(endpoint_, modelled_reference, sizeof(modelled_reference),
                              hogp::ReportRouter::NotifyProperty, layout_) == hogp::RouteResult::Ok &&
            router_.SetSubscribed(endpoint_, true) == hogp::RouteResult::Ok;
    }
    void Close() { router_.Close(); }
    bool Decode(const std::uint8_t *value, std::size_t size, State *out) {
        if (out) *out = {};
        hogp::InputReportView view;
        Report r{};
        if (!out || router_.Route(endpoint_, value, size, &view) != hogp::RouteResult::Ok ||
            !Report::ParseGatt(view.id, view.data, view.size, &r)) return false;
        *out = Normalize(r);
        return true;
    }
};
}
