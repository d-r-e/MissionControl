/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "hogp_report_router.hpp"

namespace ams::bluetooth::hogp {

    // Adapter must obtain these identities from verified 1812/2a4b/2a4d/2908/
    // 2902 discovery. No synthesized handles, UUID-only notification routing,
    // or assigning the current ticket to an otherwise uncorrelated SDK event.
    struct ReportMetadata {
        std::uint16_t characteristic = 0, reference = 0, cccd = 0;
        std::uint8_t properties = 0;
    };
    struct HidCatalog {
        std::uint16_t service = 0, report_map = 0;
        std::array<ReportMetadata, ReportRouter::MaxBindings> reports{};
        std::size_t count = 0;
    };
    enum class SetupOperation { None, DiscoverCatalog, ReadMap, ReadReference, Subscribe };
    struct SetupCommand {
        SetupOperation operation = SetupOperation::None;
        Endpoint endpoint{};
        std::uint64_t ticket = 0;
        std::uint16_t attribute = 0;
    };
    enum class SessionState { Idle, SettingUp, Ready, Retired };
    enum class SessionFailure { None, InvalidStart, InvalidCatalog, OperationFailed,
        InvalidMap, InvalidReference, BindingFailed, NoInputs, Timeout, InvalidPayload, Closed };

    // Pure serialized worker state. Does not connect/disconnect, issue IPC,
    // retain input bytes, allocate, or infer Bluetooth ownership. Begin requires
    // an already-owned connection. Retirement revokes routes immediately; the
    // adapter must separately release its connection/subscriptions and emit
    // neutral state on every terminal transition. This is not a cleanup ACK.
    class Session {
        ReportLayout m_layout;
        ReportRouter m_router;
        HidCatalog m_catalog{};
        SetupCommand m_command{};
        SessionState m_state = SessionState::Idle;
        SessionFailure m_failure = SessionFailure::None;
        std::uint64_t m_generation = 0, m_ticket = 0, m_deadline = 0, m_total_deadline = 0;
        std::uint32_t m_connection = UINT32_MAX;
        std::size_t m_index = 0, m_inputs = 0;
        bool m_dispatched = false;
        static constexpr std::uint64_t StepMs = 5000, TotalMs = 30000;

        void Issue(SetupOperation operation, std::uint16_t characteristic,
                   std::uint16_t attribute, std::uint64_t now) {
            if (m_ticket == UINT64_MAX) { Retire(SessionFailure::OperationFailed); return; }
            m_command = {operation, {m_generation, m_connection, m_catalog.service, characteristic},
                         ++m_ticket, attribute};
            m_dispatched = false;
            m_deadline = now + StepMs;
        }
        bool Accept(const SetupCommand &command, SetupOperation expected, std::uint64_t now) {
            Tick(now);
            const auto &a = command.endpoint;
            const auto &b = m_command.endpoint;
            return m_state == SessionState::SettingUp && m_dispatched &&
                command.operation == expected && m_command.operation == expected &&
                command.ticket == m_command.ticket && a.generation == b.generation &&
                a.connection == b.connection && a.service == b.service &&
                a.characteristic == b.characteristic && command.attribute == m_command.attribute;
        }
        void Next(std::uint64_t now) {
            ++m_index;
            if (m_index < m_catalog.count) {
                const auto &report = m_catalog.reports[m_index];
                Issue(SetupOperation::ReadReference, report.characteristic, report.reference, now);
            } else if (!m_inputs) {
                Retire(SessionFailure::NoInputs);
            } else {
                m_command = {}; m_dispatched = false; m_state = SessionState::Ready;
            }
        }
        static bool ValidCatalog(const HidCatalog &catalog) {
            if (!catalog.service || catalog.report_map <= catalog.service || !catalog.count ||
                catalog.count > catalog.reports.size()) return false;
            // All attribute identities within this single service must be
            // distinct, including metadata descriptors and report-map handle.
            std::array<std::uint16_t, 2 + ReportRouter::MaxBindings * 3> handles{};
            std::size_t count = 0;
            handles[count++] = catalog.service; handles[count++] = catalog.report_map;
            for (std::size_t i = 0; i < catalog.count; ++i) {
                const auto &r = catalog.reports[i];
                if (r.characteristic <= catalog.service || r.reference <= r.characteristic ||
                    (r.cccd && r.cccd <= r.characteristic)) return false;
                for (auto handle : {r.characteristic, r.reference, r.cccd}) {
                    if (!handle) continue;
                    for (std::size_t j = 0; j < count; ++j) if (handles[j] == handle) return false;
                    handles[count++] = handle;
                }
            }
            return true;
        }

    public:
        SessionState State() const { return m_state; }
        SessionFailure Failure() const { return m_failure; }
        std::size_t InputCount() const { return m_inputs; }
        void Retire(SessionFailure failure = SessionFailure::Closed) {
            m_router.Close(); m_layout = {}; m_catalog = {}; m_command = {};
            m_dispatched = false; m_inputs = 0; m_index = 0;
            m_state = SessionState::Retired; m_failure = failure;
        }
        bool Begin(std::uint64_t generation, std::uint32_t connection, std::uint64_t now) {
            // Do not recycle a generation, even when a native handle is reused.
            Retire(SessionFailure::InvalidStart);
            if (!generation || generation <= m_generation || connection == UINT32_MAX ||
                now > UINT64_MAX - TotalMs || !m_router.Reset(generation, connection)) return false;
            m_generation = generation; m_connection = connection;
            m_state = SessionState::SettingUp; m_failure = SessionFailure::None;
            m_total_deadline = now + TotalMs;
            Issue(SetupOperation::DiscoverCatalog, 0, 0, now);
            return m_state == SessionState::SettingUp;
        }
        void Tick(std::uint64_t now) {
            if (m_state == SessionState::SettingUp && (now >= m_deadline || now >= m_total_deadline))
                Retire(SessionFailure::Timeout);
        }
        SetupCommand TakeCommand(std::uint64_t now) {
            Tick(now);
            if (m_state != SessionState::SettingUp || m_dispatched) return {};
            m_dispatched = true;
            return m_command;
        }
        // Request acceptance alone must NEVER call completion. Adapter must
        // correlate the actual asynchronous result to the submitted command.
        bool CompleteCatalog(const SetupCommand &command, bool success,
                             const HidCatalog &catalog, std::uint64_t now) {
            if (!Accept(command, SetupOperation::DiscoverCatalog, now)) return false;
            if (!success) { Retire(SessionFailure::OperationFailed); return true; }
            if (!ValidCatalog(catalog)) { Retire(SessionFailure::InvalidCatalog); return true; }
            m_catalog = catalog;
            Issue(SetupOperation::ReadMap, catalog.report_map, catalog.report_map, now);
            return true;
        }
        bool CompleteRead(const SetupCommand &command, bool success, const std::uint8_t *data,
                          std::size_t size, std::uint64_t now) {
            if (command.operation != SetupOperation::ReadMap && command.operation != SetupOperation::ReadReference)
                return false;
            if (!Accept(command, command.operation, now)) return false;
            if (!success) { Retire(SessionFailure::OperationFailed); return true; }
            if (command.operation == SetupOperation::ReadMap) {
                if (m_layout.Parse(data, size) != LayoutResult::Ok) { Retire(SessionFailure::InvalidMap); return true; }
                m_index = 0;
                const auto &r = m_catalog.reports[0];
                Issue(SetupOperation::ReadReference, r.characteristic, r.reference, now);
                return true;
            }
            ReportReference ref{};
            if (!ReportReference::Parse(data, size, &ref) || !m_layout.PayloadSize(ref.id, ref.type)) {
                Retire(SessionFailure::InvalidReference); return true;
            }
            if (ref.type != ReportType::Input) { Next(now); return true; }
            const auto &r = m_catalog.reports[m_index];
            if (!r.cccd || m_router.BindInput(command.endpoint, data, size, r.properties, m_layout) != RouteResult::Ok) {
                Retire(SessionFailure::BindingFailed); return true;
            }
            Issue(SetupOperation::Subscribe, r.characteristic, r.cccd, now);
            return true;
        }
        // success means BOTH registration and CCCD enable are confirmed, not
        // merely that an IPC command or descriptor write was accepted.
        bool CompleteSubscription(const SetupCommand &command, bool success, std::uint64_t now) {
            if (!Accept(command, SetupOperation::Subscribe, now)) return false;
            if (!success || m_router.SetSubscribed(command.endpoint, true) != RouteResult::Ok) {
                Retire(SessionFailure::OperationFailed); return true;
            }
            ++m_inputs; Next(now); return true;
        }
        RouteResult Route(const Endpoint &endpoint, const std::uint8_t *data,
                          std::size_t size, InputReportView *out) {
            if (out) *out = {};
            if (m_state != SessionState::Ready) return RouteResult::NotSubscribed;
            const auto result = m_router.Route(endpoint, data, size, out);
            if (result == RouteResult::InvalidPayload) Retire(SessionFailure::InvalidPayload);
            return result;
        }
    };
}
