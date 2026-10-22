#include "ble_diagnostics.hpp"
#include "ble_hid_host.hpp"
#include "ble_log.hpp"
#include "../mcmitm_config.hpp"

namespace ams::ble::diagnostics {

    namespace {
        constinit Counters g_counters;
        constinit InputTimings g_input_timings;
        constinit std::atomic<std::uint32_t> g_input_session{};
        TimingSnapshot g_session_start{}; // Host-thread only.
        std::uint64_t g_session_failures = 0;
        std::uint64_t g_session_started_us = 0;

        bool Enabled() {
            return ams::ble::IsEnabled() && mitm::GetGlobalConfig()->ble.log;
        }
    }

    std::uint64_t InputNowUs() {
        return Enabled() ? static_cast<std::uint64_t>(os::GetSystemTick().ToTimeSpan().GetMicroSeconds()) : 0;
    }

    void RecordInputDuration(TimingMetric metric, std::uint64_t start_us) {
        const auto now = InputNowUs();
        if (start_us && now >= start_us) g_input_timings.Record(metric, now - start_us, Enabled());
    }

    void RecordInputValue(TimingMetric metric, std::uint64_t value) {
        g_input_timings.Record(metric, value, Enabled());
    }

    void RecordFifoWrite(std::uint64_t start_us, std::uint64_t free_bytes, bool succeeded) {
        if (!start_us || !Enabled()) return;
        RecordInputDuration(TimingMetric::FifoWrite, start_us);
        // This FIFO exposes 9999 usable bytes. This is an approximate pre-write byte occupancy,
        // not packet age or proof the consumer has processed an input.
        const auto free = std::min<std::uint64_t>(free_bytes, 9999);
        RecordInputValue(TimingMetric::FifoUsedBytes, 9999 - free);
        if (!succeeded) g_input_timings.NoteFifoFailure(true);
    }

    std::uint32_t InputSession() {
        return g_input_session.load(std::memory_order_relaxed);
    }

    void BeginInputSession() {
        g_session_start = g_input_timings.Read();
        g_session_failures = g_input_timings.Failures();
        g_session_started_us = InputNowUs();
        g_input_session.fetch_add(1, std::memory_order_relaxed);
    }

    void DumpInputTiming() {
        if (!Enabled() || !log::IsEnabled()) return;
        const auto values = g_input_timings.Read();
        const auto elapsed = InputNowUs() - g_session_started_us;
        log::Write("input timing session=%u elapsed_ms=%llu fifo_failures=%llu; approximate session deltas",
            InputSession(), static_cast<unsigned long long>(elapsed / 1000),
            static_cast<unsigned long long>(g_input_timings.Failures() - g_session_failures));
        log::Write("timing bins us<=100,500,2000,8000,16000,33000,100000,over; FIFO bytes<=0,90,360,1000,2500,5000,7500,over");
        const char *names[] = {"slot_ipc_us", "notification_delivery_gap_us", "delivery_lock_us",
            "translate_inject_us", "input_work_us", "all_hid_fifo_write_us", "all_hid_fifo_used_bytes"};
        for (std::size_t i = 0; i < TimingCount; ++i) {
            const auto count = values[i].count - g_session_start[i].count;
            const auto sum = values[i].sum - g_session_start[i].sum;
            char buckets[192] = {};
            std::size_t pos = 0;
            for (std::size_t j = 0; j < TimingBuckets; ++j) {
                const auto n = values[i].buckets[j] - g_session_start[i].buckets[j];
                const int written = std::snprintf(buckets + pos, sizeof(buckets) - pos, "%s%llu",
                    j ? "," : "", static_cast<unsigned long long>(n));
                if (written < 0 || static_cast<std::size_t>(written) >= sizeof(buckets) - pos) break;
                pos += written;
            }
            log::Write("timing %s n=%llu avg=%llu bins=%s", names[i],
                static_cast<unsigned long long>(count),
                static_cast<unsigned long long>(count ? sum / count : 0), buckets);
        }
        log::Write("timing limits: gaps include idle; no radio timestamp/pre-wake delay, FIFO age or end-to-end latency measured; priorities unchanged");
    }

    void Note(Metric metric) {
        g_counters.Record(metric, Enabled());
    }

    void Dump(const char *point) {
        if (!Enabled() || !log::IsEnabled()) {
            return;
        }
        const auto values = g_counters.Read();
        const auto get = [&](Metric metric) { return values[static_cast<size_t>(metric)]; };
        log::Write("ownership %s: boot counters (non-transactional snapshot)", point);
        log::Write("ownership system: hid_placeholder=%u btm_placeholder=%u hid_slot_read_calls=%u btm_slot_read_calls=%u",
            get(Metric::HidPlaceholder), get(Metric::BtmPlaceholder), get(Metric::HidSlotRead), get(Metric::BtmSlotRead));
        log::Write("ownership listener: started=%u registered=%u registration_failed=%u wakes=%u",
            get(Metric::ListenerStarted), get(Metric::ListenerRegistered), get(Metric::ListenerRegistrationFailed), get(Metric::ListenerWake));
        log::Write("ownership returns: ok=%u failed=%u notify_type=%u other_type=%u current_conn=%u managed_notify=%u",
            get(Metric::ListenerReadReturned), get(Metric::ListenerReadFailed), get(Metric::ListenerNotifyType),
            get(Metric::ListenerOtherType), get(Metric::ListenerCurrentConnection), get(Metric::ManagedNotifyType));
    }

}
