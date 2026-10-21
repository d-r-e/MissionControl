/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace ams::bluetooth::scan_requests {
    constexpr std::uint32_t ProtocolVersion = 1, StatusCommand = 13;
    enum class Request : unsigned { Start, Stop, Parameters };
    struct Counts { std::uint32_t starts, stops, parameters; };
    struct Status {
        std::uint32_t protocol_version, enabled, ble_initialized, reserved;
        Counts btm, hid;
    };
    static_assert(sizeof(Status) == 40);
    static_assert(std::is_trivial_v<Status> && std::is_standard_layout_v<Status>);
    // Independent monotonic request-attempt counters. NOT an atomic cross-field
    // snapshot, IPC success, scanner state, owner attribution or target evidence.
    // No parameters, addresses, event buffers, resets, IPC, waits or locks.
    class Counters {
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
        std::atomic<std::uint32_t> m_counts[2][3]{};
    public:
        void Record(Request request, bool btm, bool hid, bool enabled) {
            const auto slot = static_cast<unsigned>(request);
            if (!enabled || btm == hid || slot >= 3) return;
            m_counts[btm ? 0 : 1][slot].fetch_add(1, std::memory_order_relaxed);
        }
        Status Snapshot(bool enabled, bool initialized) const {
            Status status{};
            status.protocol_version = ProtocolVersion;
            status.enabled = enabled ? 1u : 0u;
            status.ble_initialized = initialized ? 1u : 0u;
            auto read = [&](unsigned row) {
                return Counts{m_counts[row][0].load(std::memory_order_relaxed),
                    m_counts[row][1].load(std::memory_order_relaxed),
                    m_counts[row][2].load(std::memory_order_relaxed)};
            };
            status.btm = read(0); status.hid = read(1);
            return status;
        }
    };
    constinit inline Counters g_counters;
    inline bool Monotonic(const Counts &before, const Counts &after) {
        return after.starts >= before.starts && after.stops >= before.stops && after.parameters >= before.parameters;
    }
}
