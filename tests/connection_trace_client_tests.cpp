#include "../tools/stadia-probe/source/connection_trace_client.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

namespace trace = ams::bluetooth::connection_trace;
namespace client = missioncontrol::probe;

int main() {
    namespace progress = ams::bluetooth::event_progress;
    progress::Status pipeline{};
    pipeline.protocol_version = progress::ProtocolVersion;
    pipeline.lane_count = progress::LaneCount;
    pipeline.sample_tick = 100;
    assert(client::TracePipelineReady(pipeline, 10));
    // Native counters can remain zero while the12s settling wait exposes a stalled lane.
    pipeline.lanes[2].phase = static_cast<std::uint32_t>(progress::Phase::ConsumerWait);
    pipeline.lanes[2].phase_since_tick = 89;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.lanes[2].phase_since_tick = 90;
    assert(client::TracePipelineReady(pipeline, 10));
    pipeline.lanes[2].phase_since_tick = 101;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.lanes[2].phase_since_tick = 100;
    pipeline.lanes[2].unstable = 1;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.lanes[2].unstable = 0; pipeline.lanes[2].phase = 4;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.lanes[2].phase = 0; pipeline.connect_hook_attempts = 1;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.connect_hook_attempts = 0; pipeline.connection_events = 1;
    assert(!client::TracePipelineReady(pipeline, 10));
    pipeline.connection_events = 0; pipeline.lane_count = 0;
    assert(!client::TracePipelineReady(pipeline, 10));
    trace::Status status{};
    status.protocol_version = trace::ProtocolVersion;
    status.enabled = status.available = 1;
    assert(client::TraceAvailable(status));
    status.consumed = 1;
    assert(!client::TraceAvailable(status));
    status.available = 0;
    status.generation = 1;
    status.state = static_cast<std::uint32_t>(trace::State::Active);
    assert(client::TraceMaySubmit(status));
    status.captured_requests = 1;
    assert(!client::TraceMaySubmit(status));
    status.callback_events = status.connected_events = 1;
    status.request_direct_valid = status.request_is_direct = 1;
    status.callback_result_valid = status.callback_status_valid = status.callback_reason_valid = 1;
    status.callback_connection_id_valid = 1;
    status.driver_result_valid = status.callback_delay_valid = 1;
    assert(client::TraceSawConnectedCallback(status));
    status.request_is_direct = 0; // Background mode is observed, never rewritten.
    assert(client::TraceSawConnectedCallback(status));
    struct InvalidField { std::uint32_t trace::Status::*field; std::uint32_t value; };
    for (const auto invalid : {
        InvalidField{&trace::Status::protocol_version, trace::ProtocolVersion + 1},
        InvalidField{&trace::Status::enabled, 0},
        InvalidField{&trace::Status::consumed, 0},
        InvalidField{&trace::Status::generation, 0},
        InvalidField{&trace::Status::state, static_cast<std::uint32_t>(trace::State::Expired)},
        InvalidField{&trace::Status::ambiguous, 1},
        InvalidField{&trace::Status::captured_requests, 2},
        InvalidField{&trace::Status::request_direct_valid, 0},
        InvalidField{&trace::Status::driver_result_valid, 0},
        InvalidField{&trace::Status::driver_result, 1},
        InvalidField{&trace::Status::callback_delay_valid, 0},
        InvalidField{&trace::Status::request_is_direct, 2},
        InvalidField{&trace::Status::callback_events, 0},
        InvalidField{&trace::Status::connected_events, 2},
        InvalidField{&trace::Status::disconnected_events, 1},
        InvalidField{&trace::Status::callback_result_valid, 0},
        InvalidField{&trace::Status::callback_status_valid, 0},
        InvalidField{&trace::Status::callback_reason_valid, 0},
        InvalidField{&trace::Status::callback_result, 0x1a},
        InvalidField{&trace::Status::callback_status, 2},
        InvalidField{&trace::Status::callback_connection_id_valid, 0},
        InvalidField{&trace::Status::connection_id_conflict, 1}}) {
        auto rejected = status;
        rejected.*invalid.field = invalid.value;
        assert(!client::TraceSawConnectedCallback(rejected));
    }
    std::puts("Connection trace client: capability, pre-submit and correlated-observation gates passed");
}
