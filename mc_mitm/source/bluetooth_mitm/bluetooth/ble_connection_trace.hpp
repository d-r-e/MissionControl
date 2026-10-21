/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ble_connection_trace_protocol.hpp"
#include <array>
#include <cstring>
#include <limits>

namespace ams::bluetooth::connection_trace {
    using Address = std::array<std::uint8_t, 6>;

    class ConnectionTrace {
        private:
            Status m_status{};
            Address m_target{};
            std::uint64_t m_token = 0, m_aruid = 0, m_last_ms = 0, m_deadline_ms = 0, m_request_ms = 0;
            std::uint32_t m_connection_id = 0;
            std::uint8_t m_client_if = 0;
            bool m_connection_bound = false;

            bool Active() const { return m_status.state == static_cast<std::uint32_t>(State::Active); }
            static void Increment(std::uint32_t &counter) {
                if (counter != std::numeric_limits<std::uint32_t>::max()) ++counter;
            }
            void Finish(State state, Reason reason) {
                m_status.state = static_cast<std::uint32_t>(state);
                m_status.reason = static_cast<std::uint32_t>(reason);
                m_status.ambiguous = state == State::Ambiguous ? 1u : 0u;
                // Keep only the capability token and sanitized historical scalars.
                m_target = {};
                m_aruid = 0;
                m_client_if = 0;
                m_connection_id = 0;
                m_connection_bound = false;
                m_last_ms = m_deadline_ms = m_request_ms = 0;
            }
            void Ambiguous(Reason reason) { Finish(State::Ambiguous, reason); }
            bool Authorized(std::uint64_t token) const { return token != 0 && token == m_token; }
        public:
            ConnectionTrace() = default;
            ConnectionTrace(const ConnectionTrace &) = delete;
            ConnectionTrace &operator=(const ConnectionTrace &) = delete;

            bool Begin(const BeginRequest &request, std::uint64_t now_ms) {
                bool zero = true, broadcast = true;
                for (auto byte : request.address) { zero &= byte == 0; broadcast &= byte == 0xff; }
                if (m_status.consumed || !request.token || !request.aruid || zero || broadcast ||
                    request.reserved || request.reserved2 || !request.timeout_ms ||
                    request.timeout_ms > MaximumTimeoutMs ||
                    now_ms > std::numeric_limits<std::uint64_t>::max() - request.timeout_ms) return false;
                std::memcpy(m_target.data(), request.address, m_target.size());
                m_token = request.token;
                m_aruid = request.aruid;
                m_last_ms = now_ms;
                m_deadline_ms = now_ms + request.timeout_ms;
                m_status.consumed = 1;
                m_status.generation = 1;
                m_status.state = static_cast<std::uint32_t>(State::Active);
                return true;
            }
            void Tick(std::uint64_t now_ms) {
                if (!Active()) return;
                if (now_ms < m_last_ms) { Ambiguous(Reason::ClockRegression); return; }
                m_last_ms = now_ms;
                if (now_ms >= m_deadline_ms) Finish(State::Expired, Reason::Timeout);
            }
            std::uint64_t ObserveRequest(bool btm_client, std::uint64_t aruid, const Address &address,
                                         std::uint8_t client_if, bool is_direct, std::uint64_t now_ms) {
                Tick(now_ms);
                if (!Active() || address != m_target) return 0;
                if (!btm_client || aruid != m_aruid) { Ambiguous(Reason::ForeignRequest); return 0; }
                if (m_status.captured_requests) { Ambiguous(Reason::RepeatedRequest); return 0; }
                m_client_if = client_if;
                m_request_ms = now_ms;
                m_status.request_direct_valid = 1;
                m_status.request_is_direct = is_direct ? 1u : 0u;
                m_status.captured_requests = 1;
                return m_token; // Private return ticket; never sent to driver or exposed in Status.
            }
            void ObserveDriverReturn(std::uint64_t ticket, std::uint32_t result, std::uint64_t now_ms) {
                if (!Authorized(ticket)) return; // Foreign/zero tickets cannot tick or alter this observer.
                Tick(now_ms);
                if (!Active() || m_status.captured_requests != 1) return;
                if (m_status.driver_result_valid) { Ambiguous(Reason::RepeatedReturn); return; }
                m_status.driver_result_valid = 1;
                m_status.driver_result = result; // IPC result, NOT connection or callback status.
            }
            void ObserveCallback(const Address &address, std::uint8_t client_if,
                                 std::uint32_t result, std::uint32_t status, std::uint32_t reason,
                                 std::uint32_t connection_id, std::uint64_t now_ms) {
                Tick(now_ms);
                if (!Active() || address != m_target) return;
                Increment(m_status.callback_events);
                m_status.callback_result_valid = m_status.callback_status_valid = m_status.callback_reason_valid = 1;
                m_status.callback_result = result;
                m_status.callback_status = status;
                m_status.callback_reason = reason;
                m_status.callback_connection_id_valid = connection_id != 0xffffffffu ? 1u : 0u;
                if (!m_status.captured_requests) { Ambiguous(Reason::CallbackBeforeRequest); return; }
                if (client_if != m_client_if) { Ambiguous(Reason::ClientMismatch); return; }
                const auto delay = static_cast<std::uint32_t>(now_ms - m_request_ms); // <= bounded45s lifetime.
                if (!m_status.callback_delay_valid) m_status.first_callback_delay_ms = delay;
                m_status.last_callback_delay_ms = delay;
                m_status.callback_delay_valid = 1;
                if (result != 0) return; // Preserve raw failures; no HCI interpretation.
                if (status == 0 || status == 2) {
                    if (!m_status.callback_connection_id_valid) { Ambiguous(Reason::InvalidConnection); return; }
                    if (m_connection_bound && connection_id != m_connection_id) {
                        m_status.connection_id_conflict = 1;
                        Ambiguous(Reason::ConflictingConnection);
                        return;
                    }
                    m_connection_bound = true;
                    m_connection_id = connection_id;
                }
                if (status == 0) {
                    Increment(m_status.connected_events);
                } else if (status == 2) {
                    Increment(m_status.disconnected_events);
                }
            }
            bool Retire(std::uint64_t token, std::uint64_t now_ms) {
                if (!Authorized(token)) return false;
                Tick(now_ms);
                if (Active()) Finish(State::Retired, Reason::LocalRetire);
                return true;
            }
            Status Capability(bool enabled, bool legacy_busy = false) const {
                Status out{};
                out.protocol_version = ProtocolVersion;
                out.enabled = enabled ? 1u : 0u;
                out.consumed = m_status.consumed;
                out.available = enabled && !legacy_busy && !out.consumed ? 1u : 0u;
                return out;
            }
            bool Get(std::uint64_t token, std::uint64_t now_ms, bool enabled, Status &out,
                     bool legacy_busy = false) {
                if (!token) { out = Capability(enabled, legacy_busy); return true; }
                if (!Authorized(token)) return false;
                Tick(now_ms);
                out = m_status;
                out.protocol_version = ProtocolVersion;
                out.enabled = enabled ? 1u : 0u;
                return true;
            }
    };
}
