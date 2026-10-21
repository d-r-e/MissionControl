/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ams::bluetooth::connection_trace {
    constexpr std::uint32_t ProtocolVersion = 2;
    constexpr std::uint32_t BeginCommand = 14, StatusCommand = 15, RetireCommand = 16;
    constexpr std::uint32_t MaximumTimeoutMs = 45000;

    enum class State : std::uint32_t { Available = 0, Active = 1, Retired = 2, Expired = 3, Ambiguous = 4 };
    enum class Reason : std::uint32_t {
        None = 0, LocalRetire = 1, Timeout = 2, CallbackBeforeRequest = 3,
        ForeignRequest = 4, RepeatedRequest = 5, ClientMismatch = 6,
        ConflictingConnection = 7, InvalidConnection = 8, ClockRegression = 9, RepeatedReturn = 10
    };

    // One-shot correlation freshness only: neither exclusivity nor native cleanup authority.
    struct alignas(8) BeginRequest {
        std::uint64_t token;
        std::uint64_t aruid;
        std::uint8_t address[6];
        std::uint16_t reserved;
        std::uint32_t timeout_ms;
        std::uint32_t reserved2;
    };
    // Sanitized scalars only. Raw result/status/reason are not HCI interpretations
    // or evidence of pairing, encryption, ownership, or native cleanup.
    struct Status {
        std::uint32_t protocol_version;
        std::uint32_t enabled;
        std::uint32_t consumed;
        std::uint32_t available;
        std::uint32_t state;
        std::uint32_t reason;
        std::uint32_t generation;
        std::uint32_t captured_requests;
        std::uint32_t callback_events;
        std::uint32_t connected_events;
        std::uint32_t disconnected_events;
        std::uint32_t ambiguous;
        std::uint32_t callback_result_valid;
        std::uint32_t callback_status_valid;
        std::uint32_t callback_reason_valid;
        std::uint32_t callback_result;
        std::uint32_t callback_status;
        std::uint32_t callback_reason;
        std::uint32_t callback_connection_id_valid;
        std::uint32_t connection_id_conflict;
        std::uint32_t request_direct_valid;
        std::uint32_t request_is_direct;
        std::uint32_t driver_result_valid;
        std::uint32_t driver_result;
        std::uint32_t callback_delay_valid;
        std::uint32_t first_callback_delay_ms;
        std::uint32_t last_callback_delay_ms;
    };
    static_assert(std::is_trivial_v<BeginRequest> && std::is_standard_layout_v<BeginRequest>);
    static_assert(std::is_trivial_v<Status> && std::is_standard_layout_v<Status>);
    static_assert(sizeof(BeginRequest) == 32 && alignof(BeginRequest) == 8);
    static_assert(offsetof(BeginRequest, token) == 0);
    static_assert(offsetof(BeginRequest, aruid) == 8);
    static_assert(offsetof(BeginRequest, address) == 16);
    static_assert(offsetof(BeginRequest, reserved) == 22);
    static_assert(offsetof(BeginRequest, timeout_ms) == 24);
    static_assert(offsetof(BeginRequest, reserved2) == 28);
    static_assert(sizeof(Status) == 108 && alignof(Status) == 4);
#define MC_TRACE_OFFSET(field, offset) static_assert(offsetof(Status, field) == offset)
    MC_TRACE_OFFSET(protocol_version, 0);
    MC_TRACE_OFFSET(enabled, 4);
    MC_TRACE_OFFSET(consumed, 8);
    MC_TRACE_OFFSET(available, 12);
    MC_TRACE_OFFSET(state, 16);
    MC_TRACE_OFFSET(reason, 20);
    MC_TRACE_OFFSET(generation, 24);
    MC_TRACE_OFFSET(captured_requests, 28);
    MC_TRACE_OFFSET(callback_events, 32);
    MC_TRACE_OFFSET(connected_events, 36);
    MC_TRACE_OFFSET(disconnected_events, 40);
    MC_TRACE_OFFSET(ambiguous, 44);
    MC_TRACE_OFFSET(callback_result_valid, 48);
    MC_TRACE_OFFSET(callback_status_valid, 52);
    MC_TRACE_OFFSET(callback_reason_valid, 56);
    MC_TRACE_OFFSET(callback_result, 60);
    MC_TRACE_OFFSET(callback_status, 64);
    MC_TRACE_OFFSET(callback_reason, 68);
    MC_TRACE_OFFSET(callback_connection_id_valid, 72);
    MC_TRACE_OFFSET(connection_id_conflict, 76);
    MC_TRACE_OFFSET(request_direct_valid, 80);
    MC_TRACE_OFFSET(request_is_direct, 84);
    MC_TRACE_OFFSET(driver_result_valid, 88);
    MC_TRACE_OFFSET(driver_result, 92);
    MC_TRACE_OFFSET(callback_delay_valid, 96);
    MC_TRACE_OFFSET(first_callback_delay_ms, 100);
    MC_TRACE_OFFSET(last_callback_delay_ms, 104);
#undef MC_TRACE_OFFSET
}
