// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace ams::ble::gatt {

    enum class CacheAppendResult { Ignored, Appended, Overflow };

    struct CacheState {
        bool overflowed = false;
        bool replace_pending = false;
        bool discovery_complete = false;
        bool rediscovered = false;
        bool operator==(const CacheState &) const = default;
    };

    // Actual production cache lifecycle, independent of SDK wire types for portable tests.
    // Host-thread only: neither synchronization nor radio/GATT requests belong here.
    // This intentionally preserves the existing append policy, including duplicate batches.
    // A connection ID is not an epoch: a delayed record with a recycled ID cannot be identified
    // as stale from these inputs alone. Preparing a new link clears local state, not driver queues.
    class DiscoveryCache {
        CacheState m_state{};

    public:
        template <typename Table, typename Address>
        void Prepare(Table &table, const Address &address) {
            m_state = {};
            m_state.replace_pending = true;
            table.count = 0;
            table.address = address;
        }

        template <typename Table, typename Attribute, std::size_t BatchSize>
        CacheAppendResult Append(Table &table, std::uint32_t event_connection,
                                 std::uint32_t current_connection,
                                 const Attribute (&attributes)[BatchSize], std::uint32_t count) {
            if (event_connection != current_connection) return CacheAppendResult::Ignored;
            if (m_state.discovery_complete) m_state.rediscovered = true;
            if (m_state.replace_pending) {
                m_state.replace_pending = false;
                table.count = 0;
            }
            // The native event contains ten slots. Never trust its advertised count past the
            // actual array extent, including malformed counts; reaching capacity is fail-closed.
            const auto available = std::min<std::size_t>(BatchSize, 10);
            for (std::size_t i = 0; i < std::min<std::size_t>(count, available); ++i) {
                if (table.count >= std::size(table.attributes)) {
                    m_state.overflowed = true;
                    return CacheAppendResult::Overflow;
                }
                table.attributes[table.count++] = attributes[i];
            }
            return CacheAppendResult::Appended;
        }

        void Complete() {
            m_state.discovery_complete = true;
            m_state.replace_pending = false;
        }

        bool TakeRediscovered() {
            const bool result = m_state.rediscovered;
            m_state.rediscovered = false;
            return result;
        }

        bool Overflowed() const { return m_state.overflowed; }
        CacheState State() const { return m_state; }
    };
}
