// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ams::ble::diagnostics {
    enum class TimingMetric : std::size_t {
        SlotIpc, NotificationGap, DeliveryLock, TranslateInject, InputWork,
        FifoWrite, FifoUsedBytes, Count
    };
    constexpr std::size_t TimingCount=static_cast<std::size_t>(TimingMetric::Count);
    constexpr std::size_t TimingBuckets=8;
    constexpr std::array<std::uint64_t,7> DurationLimitsUs{100,500,2000,8000,16000,33000,100000};
    constexpr std::array<std::uint64_t,7> QueueLimitsBytes{0,90,360,1000,2500,5000,7500};
    struct TimingValues {
        std::uint64_t count{},sum{};
        std::array<std::uint64_t,TimingBuckets> buckets{};
    };
    using TimingSnapshot=std::array<TimingValues,TimingCount>;
    inline constexpr std::size_t BucketFor(TimingMetric metric,std::uint64_t value) {
        const auto &limits=metric==TimingMetric::FifoUsedBytes?QueueLimitsBytes:DurationLimitsUs;
        std::size_t i=0;while(i<limits.size() && value>limits[i])++i;return i;
    }
    // Fixed RAM, relaxed lock-free atomic additions only. No locks, allocation, payload or SD access.
    // Snapshot fields are approximate, independently atomic observations, not one transaction.
    class InputTimings {
        static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
        struct Sample {
            std::atomic<std::uint64_t> count{},sum{};
            std::array<std::atomic<std::uint64_t>,TimingBuckets> buckets{};
        };
        std::array<Sample,TimingCount> samples{};
        std::atomic<std::uint64_t> fifo_failures{};
    public:
        void Record(TimingMetric metric,std::uint64_t value,bool enabled) {
            const auto index=static_cast<std::size_t>(metric);
            if(!enabled || index>=TimingCount)return;
            auto &s=samples[index];
            s.buckets[BucketFor(metric,value)].fetch_add(1,std::memory_order_relaxed);
            s.sum.fetch_add(value,std::memory_order_relaxed);
            s.count.fetch_add(1,std::memory_order_relaxed);
        }
        void NoteFifoFailure(bool enabled) {if(enabled)fifo_failures.fetch_add(1,std::memory_order_relaxed);}
        std::uint64_t Failures() const {return fifo_failures.load(std::memory_order_relaxed);}
        TimingSnapshot Read() const {
            TimingSnapshot result{};
            for(std::size_t i=0;i<TimingCount;++i) {
                result[i].count=samples[i].count.load(std::memory_order_relaxed);
                result[i].sum=samples[i].sum.load(std::memory_order_relaxed);
                for(std::size_t j=0;j<TimingBuckets;++j)
                    result[i].buckets[j]=samples[i].buckets[j].load(std::memory_order_relaxed);
            }
            return result;
        }
    };
}
