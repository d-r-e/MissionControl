/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include "stadia_report.hpp"
#include "../bluetooth_mitm/bluetooth/hogp_report_router.hpp"

namespace ams::controller {
    enum class StadiaGattResult { Applied, Ignored, Retired };

    // Input-side lifecycle only. Bind AFTER the transport owns the connection,
    // validates VID/PID and map/reference, confirms subscription and attaches
    // the virtual controller. Endpoints must come from verified discovery.
    // No radio, virtual attachment, output reports or connection cleanup here.
    class StadiaGattInput {
        using Endpoint = bluetooth::hogp::Endpoint;
        Endpoint m_endpoint{};
        bool m_active = false;
        StadiaInputReport m_state = Neutral();
        static constexpr StadiaInputReport Neutral() {
            return {8, 0, 128, 128, 128, 128, 0, 0, 0};
        }
        bool Matches(const Endpoint &endpoint) const {
            return m_active && endpoint.generation == m_endpoint.generation &&
                endpoint.connection == m_endpoint.connection && endpoint.service == m_endpoint.service &&
                endpoint.characteristic == m_endpoint.characteristic;
        }
    public:
        const StadiaInputReport &State() const { return m_state; }
        bool Active() const { return m_active; }
        bool HasBinding() const { return m_endpoint.generation != 0; }
        bool Bind(const Endpoint &endpoint) {
            // An invalid/foreign bind must not steal an active controller.
            if (m_active || endpoint.generation <= m_endpoint.generation ||
                endpoint.connection == UINT32_MAX || !endpoint.service ||
                endpoint.characteristic <= endpoint.service) return false;
            m_endpoint = endpoint; m_active = true; m_state = Neutral();
            return true;
        }
        StadiaGattResult Consume(const Endpoint &endpoint, const bluetooth::hogp::InputReportView &view) {
            if (!Matches(endpoint)) return StadiaGattResult::Ignored;
            StadiaInputReport parsed{};
            if (!StadiaInputReport::ParseGatt(view.id, view.data, view.size, &parsed)) {
                Retire(endpoint);
                return StadiaGattResult::Retired;
            }
            m_state = parsed;
            return StadiaGattResult::Applied;
        }
        // True exactly once for the active endpoint. Adapter must publish this
        // neutral state BEFORE virtual detach, even if link cleanup fails.
        bool Retire(const Endpoint &endpoint) {
            if (!Matches(endpoint)) return false;
            m_state = Neutral(); m_active = false;
            return true;
        }
    };
}
