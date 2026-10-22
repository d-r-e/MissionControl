#pragma once
#include "ble_event_observation.hpp"
#include "ble_input_timing.hpp"

namespace ams::ble::diagnostics {

    // Recording requires both experimental BLE and its logging setting, not an already-open log file.
    void Note(Metric metric);
    // Called only on the host thread, never while servicing a system IPC request.
    void Dump(const char *point);

    // Zero when disabled. These helpers never write a log from the input path.
    std::uint64_t InputNowUs();
    void RecordInputDuration(TimingMetric metric, std::uint64_t start_us);
    void RecordInputValue(TimingMetric metric, std::uint64_t value);
    void RecordFifoWrite(std::uint64_t start_us, std::uint64_t free_bytes, bool succeeded);
    // Host-thread session boundaries; dumps ONLY after disconnection, while the system is awake.
    void BeginInputSession();
    std::uint32_t InputSession();
    void DumpInputTiming();

}
