// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace ams::ble::gatt {
    // Bound to the SDK enums by static_asserts in ble_gatt.cpp.
    constexpr std::uint16_t CacheIncludedService = 0, CacheCharacteristic = 1,
        CacheDescriptor = 2, CacheService = 3;

    template <typename Attribute>
    bool ValidCacheAttribute(const Attribute &a) {
        const auto n = a.id.uuid.size;
        return a.handle != 0 && a.type <= CacheService && (n == 2 || n == 4 || n == 16) &&
            (a.type != CacheService || a.group_end_handle >= a.handle);
    }

    template <typename Attribute>
    bool SameCacheAttribute(const Attribute &a, const Attribute &b) {
        if (!ValidCacheAttribute(a) || !ValidCacheAttribute(b) || a.handle != b.handle ||
            a.type != b.type || a.group_end_handle != b.group_end_handle ||
            a.id.instance_id != b.id.instance_id || a.id.uuid.size != b.id.uuid.size) return false;
        // No memcmp over padding or inactive UUID bytes. Property/primary have type-specific meaning.
        for (std::size_t i = 0; i < a.id.uuid.size; ++i)
            if (a.id.uuid.uuid[i] != b.id.uuid.uuid[i]) return false;
        return (a.type != CacheCharacteristic || a.property == b.property) &&
            (a.type != CacheService || a.is_primary == b.is_primary);
    }

    enum class CacheFailure : std::uint8_t { None, Overflow, Conflict, Malformed, Changed, DriverError };
    enum class CacheAppendResult { Ignored, Appended, Unchanged, Rejected, Overflow, Conflict, Malformed, Changed, DriverError };
    struct CacheState {
        CacheFailure failure = CacheFailure::None;
        bool replace_pending = false;
        bool discovery_complete = false;
        bool rediscovered = false;
        bool operator==(const CacheState &) const = default;
    };

    // Host-thread only, fixed storage and no radio/GATT calls. Preparing clears LOCAL state, not
    // driver queues: a delayed old record bearing a recycled connection ID is still indistinguishable.
    class DiscoveryCache {
        CacheState m_state{};
        CacheAppendResult Reject(CacheFailure failure) {
            m_state.failure = failure;
            if (m_state.discovery_complete) m_state.rediscovered = true;
            switch (failure) {
                case CacheFailure::Overflow: return CacheAppendResult::Overflow;
                case CacheFailure::Conflict: return CacheAppendResult::Conflict;
                case CacheFailure::Changed: return CacheAppendResult::Changed;
                case CacheFailure::DriverError: return CacheAppendResult::DriverError;
                default: return CacheAppendResult::Malformed;
            }
        }

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
                                 const Attribute (&attributes)[BatchSize], std::uint32_t count,
                                 std::uint32_t status = 0) {
            if (event_connection != current_connection) return CacheAppendResult::Ignored;
            if (Unusable()) return CacheAppendResult::Rejected; // First failure is sticky until Prepare.
            if (status != 0) return Reject(CacheFailure::DriverError);
            const std::size_t base = m_state.replace_pending ? 0 : table.count;
            const auto capacity = std::size(table.attributes);
            if (base > capacity) return Reject(CacheFailure::Malformed);
            const auto limit = std::min<std::size_t>(count, std::min<std::size_t>(BatchSize, 10));
            std::array<std::size_t, 10> additions{};
            std::size_t added = 0;
            // Preflight the whole bounded batch before mutating any table entry/count. A bad tail
            // must never leave a valid-looking prefix or overwrite a live subscription's handles.
            for (std::size_t i = 0; i < limit; ++i) {
                const auto &incoming = attributes[i];
                if (!ValidCacheAttribute(incoming)) return Reject(CacheFailure::Malformed);
                bool duplicate = false;
                for (std::size_t j = 0; j < base + added; ++j) {
                    const auto &existing = j < base ? table.attributes[j] : attributes[additions[j - base]];
                    if (existing.handle != incoming.handle) continue;
                    if (!SameCacheAttribute(existing, incoming)) return Reject(CacheFailure::Conflict);
                    duplicate = true;
                    break;
                }
                if (duplicate) continue; // Even a full table can accept exact replays.
                // There is no native end-of-rediscovery marker here. Never resubscribe from a
                // potentially partial new table after local completion; require a new link instead.
                if (m_state.discovery_complete) return Reject(CacheFailure::Changed);
                if (added >= capacity - base) return Reject(CacheFailure::Overflow);
                additions[added++] = i;
            }
            for (std::size_t i = 0; i < added; ++i)
                table.attributes[base + i] = attributes[additions[i]];
            table.count = static_cast<decltype(table.count)>(base + added);
            m_state.replace_pending = false;
            return added ? CacheAppendResult::Appended : CacheAppendResult::Unchanged;
        }

        void Complete() { m_state.discovery_complete = true; m_state.replace_pending = false; }
        bool TakeRediscovered() {
            const bool result = m_state.rediscovered;
            m_state.rediscovered = false;
            return result;
        }
        bool Overflowed() const { return m_state.failure == CacheFailure::Overflow; }
        bool Unusable() const { return m_state.failure != CacheFailure::None; }
        CacheState State() const { return m_state; }
    };
}
