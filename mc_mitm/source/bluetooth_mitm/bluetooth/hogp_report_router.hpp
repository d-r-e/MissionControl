/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "hid_report_layout.hpp"

namespace ams::bluetooth::hogp {

    struct ReportReference {
        std::uint8_t id;
        ReportType type;

        static bool Parse(const std::uint8_t *data, std::size_t size, ReportReference *out) {
            if (!out) return false;
            *out = {};
            if (!data || size != 2 || data[1] < 1 || data[1] > 3) return false;
            *out = {data[0], static_cast<ReportType>(data[1])};
            return true;
        }
    };

    // Handles must come from verified discovery of HID service 1812 / Report
    // characteristic 2a4d / Report Reference 2908, not a UUID-only notification.
    struct Endpoint {
        std::uint64_t generation;
        std::uint32_t connection;
        std::uint16_t service;
        std::uint16_t characteristic;
    };
    struct InputReportView {
        std::uint8_t id = 0;
        const std::uint8_t *data = nullptr;
        std::size_t size = 0;
    };
    enum class RouteResult {
        Ok, WrongSession, InvalidEndpoint, InvalidReference, NotInput,
        UnsupportedProperties, MissingLayoutReport, DuplicateCharacteristic,
        DuplicateReport, CapacityExceeded, UnknownCharacteristic,
        NotSubscribed, InvalidPayload
    };

    // Pure per-device worker state: no IPC, locks, Bluetooth settings, addresses,
    // pairing, logging or retained report values. Caller serializes all access.
    class ReportRouter {
    public:
        static constexpr std::size_t MaxBindings = 16;
        static constexpr std::uint8_t NotifyProperty = 0x10;

    private:
        struct Binding {
            std::uint16_t service = 0, characteristic = 0;
            std::uint16_t payload_size = 0;
            std::uint8_t report_id = 0;
            bool subscribed = false;
        };
        std::array<Binding, MaxBindings> m_bindings{};
        std::size_t m_count = 0;
        std::uint64_t m_generation = 0;
        std::uint32_t m_connection = 0;
        bool m_active = false;

        bool Matches(const Endpoint &endpoint) const {
            return m_active && endpoint.generation == m_generation && endpoint.connection == m_connection;
        }
        Binding *Find(const Endpoint &endpoint) {
            for (std::size_t i = 0; i < m_count; ++i)
                if (m_bindings[i].service == endpoint.service &&
                    m_bindings[i].characteristic == endpoint.characteristic) return &m_bindings[i];
            return nullptr;
        }

    public:
        // Generation is an adapter-issued monotonic lease, not something the
        // SDK supplies. The adapter MUST prove ownership before tagging events.
        // Invalid reset retires the old session too; it never preserves routing.
        bool Reset(std::uint64_t generation, std::uint32_t connection) {
            Close();
            if (generation <= m_generation || connection == UINT32_MAX) return false;
            m_generation = generation;
            m_connection = connection;
            m_active = true;
            return true;
        }
        void Close() {
            m_active = false;
            m_count = 0;
            for (auto &binding : m_bindings) binding = {};
        }
        RouteResult BindInput(const Endpoint &endpoint, const std::uint8_t *reference,
                              std::size_t reference_size, std::uint8_t properties,
                              const ReportLayout &layout) {
            if (!Matches(endpoint)) return RouteResult::WrongSession;
            if (endpoint.service == 0 || endpoint.characteristic <= endpoint.service)
                return RouteResult::InvalidEndpoint;
            ReportReference parsed{};
            if (!ReportReference::Parse(reference, reference_size, &parsed)) return RouteResult::InvalidReference;
            if (parsed.type != ReportType::Input) return RouteResult::NotInput;
            if (!(properties & NotifyProperty)) return RouteResult::UnsupportedProperties;
            const auto size = layout.PayloadSize(parsed.id, parsed.type);
            if (size == 0) return RouteResult::MissingLayoutReport;
            for (std::size_t i = 0; i < m_count; ++i) {
                const auto &existing = m_bindings[i];
                if (existing.characteristic == endpoint.characteristic)
                    return RouteResult::DuplicateCharacteristic;
                if (existing.service == endpoint.service && existing.report_id == parsed.id)
                    return RouteResult::DuplicateReport;
            }
            if (m_count == MaxBindings) return RouteResult::CapacityExceeded;
            m_bindings[m_count++] = {endpoint.service, endpoint.characteristic,
                static_cast<std::uint16_t>(size), parsed.id, false};
            return RouteResult::Ok;
        }
        // Only call after verified notification registration AND CCCD enable
        // success. An IPC request being accepted is not itself subscription ACK.
        RouteResult SetSubscribed(const Endpoint &endpoint, bool subscribed) {
            if (!Matches(endpoint)) return RouteResult::WrongSession;
            auto *binding = Find(endpoint);
            if (!binding) return RouteResult::UnknownCharacteristic;
            binding->subscribed = subscribed;
            return RouteResult::Ok;
        }
        RouteResult Route(const Endpoint &endpoint, const std::uint8_t *data,
                          std::size_t size, InputReportView *out) {
            if (out) *out = {};
            if (!Matches(endpoint)) return RouteResult::WrongSession;
            auto *binding = Find(endpoint);
            if (!binding) return RouteResult::UnknownCharacteristic;
            if (!binding->subscribed) return RouteResult::NotSubscribed;
            if (!out || !data || size != binding->payload_size) return RouteResult::InvalidPayload;
            // Borrowed only for synchronous consumption; a worker queue must
            // copy the exact bounded value, never keep a pointer to SDK storage.
            *out = {binding->report_id, data, size};
            return RouteResult::Ok;
        }
    };
}
