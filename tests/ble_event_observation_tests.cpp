#include "ble_event_observation.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

using namespace ams::ble::diagnostics;

constinit Counters early;

int main() {
    for (const auto count : early.Read()) {
        assert(count == 0);
    }
    for (std::size_t i = 0; i < MetricCount; ++i) {
        early.Record(static_cast<Metric>(i), false);
    }
    for (const auto count : early.Read()) {
        assert(count == 0); // BLE or logging disabled: no observations retained.
    }

    // No logger exists in this portable test: early boot observations must still be retained.
    early.Record(Metric::HidPlaceholder, true);
    early.Record(Metric::BtmPlaceholder, true);
    auto saved = early.Read();
    assert(saved[static_cast<std::size_t>(Metric::HidPlaceholder)] == 1);
    assert(saved[static_cast<std::size_t>(Metric::BtmPlaceholder)] == 1);
    early.Record(Metric::Count, true);
    early.Record(static_cast<Metric>(MetricCount + 1), true);
    assert(early.Read() == saved); // Invalid enum values cannot index beyond storage.

    Counters concurrent;
    constexpr unsigned Iterations = 10000;
    const auto writer = [&] {
        for (unsigned i = 0; i < Iterations; ++i) {
            concurrent.Record(Metric::HidSlotRead, true);
            concurrent.Record(Metric::ListenerWake, true);
        }
    };
    std::thread a(writer), b(writer), c(writer), d(writer);
    std::uint32_t previous = 0;
    for (unsigned i = 0; i < Iterations; ++i) {
        const auto snapshot = concurrent.Read();
        const auto current = snapshot[static_cast<std::size_t>(Metric::HidSlotRead)];
        assert(current >= previous && current <= 4 * Iterations);
        previous = current; // Individual counters are atomic; cross-field equality is not promised.
    }
    a.join(); b.join(); c.join(); d.join();
    saved = concurrent.Read();
    assert(saved[static_cast<std::size_t>(Metric::HidSlotRead)] == 4 * Iterations);
    assert(saved[static_cast<std::size_t>(Metric::ListenerWake)] == 4 * Iterations);
    for (std::size_t i = 0; i < MetricCount; ++i) {
        if (i != static_cast<std::size_t>(Metric::HidSlotRead) &&
            i != static_cast<std::size_t>(Metric::ListenerWake)) {
            assert(saved[i] == 0);
        }
    }
    assert(concurrent.Read() == saved); // Taking a snapshot must not consume/reset observations.
    std::puts("BLE ownership counter tests passed (gating, pre-log capture, bounds, concurrency, snapshots)");
}
