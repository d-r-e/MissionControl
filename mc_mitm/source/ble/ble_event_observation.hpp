#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ams::ble::diagnostics {

    enum class Metric : std::size_t {
        HidPlaceholder,
        BtmPlaceholder,
        HidSlotRead,
        BtmSlotRead,
        ListenerStarted,
        ListenerRegistered,
        ListenerRegistrationFailed,
        ListenerWake,
        ListenerReadReturned,
        ListenerReadFailed,
        ListenerNotifyType,
        ListenerOtherType,
        ListenerCurrentConnection,
        ManagedNotifyType,
        Count,
    };

    constexpr std::size_t MetricCount = static_cast<std::size_t>(Metric::Count);
    using Snapshot = std::array<std::uint32_t, MetricCount>;

    // No allocation, locks, payloads, or logger dependency. May record IPC calls before log initialization.
    // Counters are boot-cumulative; relaxed snapshots are individually atomic, not a transaction across fields.
    class Counters {
        static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
        std::array<std::atomic<std::uint32_t>, MetricCount> m_counts{};
    public:
        void Record(Metric metric, bool enabled) {
            const auto index = static_cast<std::size_t>(metric);
            if (enabled && index < MetricCount) {
                m_counts[index].fetch_add(1, std::memory_order_relaxed);
            }
        }

        Snapshot Read() const {
            Snapshot values{};
            for (std::size_t i = 0; i < MetricCount; ++i) {
                values[i] = m_counts[i].load(std::memory_order_relaxed);
            }
            return values;
        }
    };

}
