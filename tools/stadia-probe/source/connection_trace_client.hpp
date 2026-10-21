/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ble_connection_trace_protocol.hpp"
#include "ble_event_progress.hpp"

namespace missioncontrol::probe {
    namespace trace = ams::bluetooth::connection_trace;

    inline bool TraceAvailable(const trace::Status &status) {
        return status.protocol_version == trace::ProtocolVersion && status.enabled == 1 &&
            status.available == 1 && status.consumed == 0;
    }
    inline bool TraceMaySubmit(const trace::Status &status) {
        return status.protocol_version == trace::ProtocolVersion && status.enabled == 1 &&
            status.consumed == 1 && status.generation == 1 &&
            status.state == static_cast<std::uint32_t>(trace::State::Active) &&
            status.ambiguous == 0 && status.captured_requests == 0 && status.callback_events == 0 &&
            status.driver_result_valid == 0 && status.callback_delay_valid == 0;
    }
    inline bool TracePipelineReady(const ams::bluetooth::event_progress::Status &status,
                                   std::uint64_t maximum_phase_ticks) {
        namespace progress = ams::bluetooth::event_progress;
        if (!maximum_phase_ticks || status.protocol_version != progress::ProtocolVersion ||
            status.lane_count != progress::LaneCount || status.connect_hook_attempts ||
            status.btm_connect_attempts || status.passthrough_attempts || status.connection_events) return false;
        for (const auto &lane : status.lanes) {
            if (lane.unstable || lane.phase > static_cast<std::uint32_t>(progress::Phase::ConsumerWait) ||
                (lane.phase && (status.sample_tick < lane.phase_since_tick ||
                 status.sample_tick - lane.phase_since_tick > maximum_phase_ticks))) return false;
        }
        return true;
    }
    // Correlated observation only; managed-list ownership must be checked separately.
    inline bool TraceSawConnectedCallback(const trace::Status &status) {
        return status.protocol_version == trace::ProtocolVersion && status.enabled == 1 &&
            status.consumed == 1 && status.generation == 1 &&
            status.state == static_cast<std::uint32_t>(trace::State::Active) &&
            status.ambiguous == 0 && status.captured_requests == 1 &&
            status.driver_result_valid == 1 && status.driver_result == 0 && status.callback_delay_valid == 1 &&
            status.request_direct_valid == 1 && status.request_is_direct <= 1 &&
            status.callback_events >= 1 && status.connected_events == 1 && status.disconnected_events == 0 &&
            status.callback_result_valid == 1 && status.callback_status_valid == 1 &&
            status.callback_reason_valid == 1 && status.callback_result == 0 && status.callback_status == 0 &&
            status.callback_connection_id_valid == 1 && status.connection_id_conflict == 0;
    }
}
