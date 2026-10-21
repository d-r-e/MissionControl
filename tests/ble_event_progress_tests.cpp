/* SPDX-License-Identifier: GPL-2.0-only */
#include "../mc_mitm/source/bluetooth_mitm/bluetooth/ble_event_progress.hpp"
#include "../mc_mitm/source/bluetooth_mitm/bluetooth/ble_scan_request_progress.hpp"
#include <cassert>
#include <cstdio>
#include <thread>

using namespace ams::bluetooth::event_progress;
namespace { constinit Progress StaticProgress; }

void TestScanRequests() {
    namespace r = ams::bluetooth::scan_requests;
    r::Counters counters;
    auto s = counters.Snapshot(false, false);
    assert(s.protocol_version == 1 && !s.enabled && !s.ble_initialized && !s.reserved);
    assert(!s.btm.starts && !s.btm.stops && !s.btm.parameters && !s.hid.starts);
    counters.Record(r::Request::Start, true, false, false);
    counters.Record(r::Request::Start, false, false, true);
    counters.Record(r::Request::Start, true, true, true);
    counters.Record(static_cast<r::Request>(3), true, false, true);
    assert(counters.Snapshot(true, true).btm.starts == 0);
    counters.Record(r::Request::Start, true, false, true);
    counters.Record(r::Request::Stop, true, false, true);
    counters.Record(r::Request::Parameters, true, false, true);
    counters.Record(r::Request::Start, false, true, true);
    s = counters.Snapshot(true, true);
    assert(s.enabled && s.ble_initialized && !s.reserved);
    assert(s.btm.starts == 1 && s.btm.stops == 1 && s.btm.parameters == 1);
    assert(s.hid.starts == 1 && s.hid.stops == 0 && s.hid.parameters == 0);
    assert(r::Monotonic({}, s.btm) && r::Monotonic(s.btm, s.btm));
    for (unsigned i = 0; i < 3; ++i) {
        auto decreased = s.btm;
        if (i == 0) decreased.starts = 0;
        if (i == 1) decreased.stops = 0;
        if (i == 2) decreased.parameters = 0;
        assert(!r::Monotonic(s.btm, decreased));
    }
    std::thread one([&] { for (unsigned i = 0; i < 10000; ++i) counters.Record(r::Request::Start, true, false, true); });
    std::thread two([&] { for (unsigned i = 0; i < 10000; ++i) counters.Record(r::Request::Parameters, true, false, true); });
    one.join(); two.join();
    s = counters.Snapshot(true, true);
    assert(s.btm.starts == 10001 && s.btm.parameters == 10001 && s.hid.starts == 1);
    static_assert(InlineReplyBytes(sizeof(r::Status)) < 256);
}

int main() {
    TestScanRequests();
    auto initial = StaticProgress.Snapshot(1);
    assert(initial.protocol_version == 3 && initial.lane_count == 3 && initial.sample_tick == 1);
    assert(initial.connect_hook_attempts == 0 && initial.connection_events == 0);
    for (const auto &lane : initial.lanes) {
        assert(lane.phase == 0 && lane.begun == 0 && lane.finished == 0);
        assert(lane.phase_since_tick == 0 && lane.unstable == 0 && lane.connection_details == 0 && lane.last_connection_result == 0);
    }
    // Regression: POD224 bytes fit by themselves, but framed inline HIPC is264
    // bytes and does NOT fit the256-byte TLS command buffer. Buffer transport
    // keeps the reply envelope small and the payload bounds exact.
    static_assert(InlineReplyBytes(sizeof(Status)) == 264);
    static_assert(InlineReplyBytes(0) == 40);
    std::array<unsigned char, sizeof(Status) + 2> guarded;
    guarded.fill(0x5a);
    const auto untouched = guarded;
    for (const auto bad_size : {std::size_t{0}, sizeof(Status) - 1, sizeof(Status) + 1}) {
        assert(!CopyStatus(initial, guarded.data() + 1, bad_size));
        assert(guarded == untouched);
    }
    assert(!CopyStatus(initial, nullptr, sizeof(Status)));
    assert(CopyStatus(initial, guarded.data() + 1, sizeof(Status)));
    assert(guarded.front() == 0x5a && guarded.back() == 0x5a);
    Status roundtrip{};
    std::memcpy(&roundtrip, guarded.data() + 1, sizeof(roundtrip));
    assert(roundtrip.protocol_version == 3 && roundtrip.sample_tick == 1);
    assert(roundtrip.connection_events == 0 && roundtrip.lane_count == 3);
    Progress callbacks;
    callbacks.ClientConnection(0x12345678, 2, 0x3e);
    callbacks.Begin(Lane::Ble, 10);
    callbacks.NativeRead(Lane::Ble, 6, 20); // An unrelated event must not erase the last result.
    callbacks.Finish(Lane::Ble, 30);
    auto callback = callbacks.Snapshot(40);
    assert(callback.connection_events == 1 && callback.lanes[2].last_event_type == 6);
    assert(callback.lanes[2].last_connection_result == 0x12345678);
    assert(callback.lanes[2].connection_details == (ConnectionDetailsValid | 2u | (0x3eu << 8)));
    assert(callback.lanes[0].connection_details == 0 && callback.lanes[1].connection_details == 0);
    callbacks.ClientConnection(0, 0, 0);
    callback = callbacks.Snapshot(50);
    assert(callback.connection_events == 2 && callback.lanes[2].last_connection_result == 0);
    assert(callback.lanes[2].connection_details == ConnectionDetailsValid);
    assert(CopyStatus(callback, guarded.data() + 1, sizeof(Status)));
    std::memcpy(&roundtrip, guarded.data() + 1, sizeof(roundtrip));
    assert(roundtrip.connection_events == 2 && roundtrip.lanes[2].connection_details == ConnectionDetailsValid);
    for (std::size_t i = 0; i < LaneCount; ++i) {
        Progress progress;
        const auto kind = static_cast<Lane>(i);
        progress.Begin(kind, 10);
        auto lane = progress.Snapshot(15).lanes[i];
        assert(lane.phase == 1 && lane.phase_since_tick == 10 && lane.begun == 1);
        progress.NativeRead(kind, 123, 20);
        progress.BeginWait(kind, 30);
        progress.ConsumerRead(kind);
        lane = progress.Snapshot(80).lanes[i];
        assert(lane.phase == 3 && lane.phase_since_tick == 30 && lane.last_event_type == 123);
        assert(lane.consumer_reads == 1 && lane.forward_waits == 1 && lane.wait_returns == 0);
        // A diagnostic read count MUST NOT invent a wait return or completion.
        assert(lane.finished == 0 && lane.last_wait_ticks == 0);
        progress.EndWait(kind, 130);
        progress.Finish(kind, 140);
        lane = progress.Snapshot(150).lanes[i];
        assert(lane.phase == 0 && lane.begun == 1 && lane.finished == 1);
        assert(lane.wait_returns == 1 && lane.last_wait_ticks == 100 && lane.max_wait_ticks == 100);
        progress.Begin(kind, 160);
        progress.NativeRead(kind, 99, 170);
        progress.BeginWait(kind, 180);
        progress.EndWait(kind, 190);
        progress.Finish(kind, 200);
        lane = progress.Snapshot(210).lanes[i];
        assert(lane.last_wait_ticks == 10 && lane.max_wait_ticks == 100);
        assert(lane.consumer_reads == 1 && lane.forward_waits == 2 && lane.wait_returns == 2);
        // Custom/redirected/no-forward dispatch may finish without a wait.
        progress.Begin(kind, 220);
        progress.NativeRead(kind, 8, 230);
        progress.Finish(kind, 240);
        lane = progress.Snapshot(250).lanes[i];
        assert(lane.finished == 3 && lane.forward_waits == 2 && lane.last_event_type == 8);
    }
    Progress counts;
    counts.ConnectHook(true, true);
    counts.ConnectHook(false, true);
    counts.ConnectHook(true, false);
    auto status = counts.Snapshot(50);
    assert(status.connect_hook_attempts == 3 && status.btm_connect_attempts == 2 && status.passthrough_attempts == 2);
    const auto invalid = static_cast<Lane>(999);
    counts.Begin(invalid, 1); counts.NativeRead(invalid, 2, 3);
    counts.BeginWait(invalid, 4); counts.ConsumerRead(invalid);
    counts.EndWait(invalid, 5); counts.Finish(invalid, 6);
    for (const auto &lane : counts.Snapshot(70).lanes) assert(lane.begun == 0);

    Progress concurrent;
    std::atomic<bool> done{};
    std::thread producer([&] {
        for (std::uint64_t i = 0; i < 10000; ++i) {
            const auto tick = i * 10;
            concurrent.Begin(Lane::Ble, tick);
            concurrent.NativeRead(Lane::Ble, static_cast<std::uint32_t>(i + 1), tick + 1);
            concurrent.ClientConnection(static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(i % 3), static_cast<std::uint16_t>(i));
            concurrent.BeginWait(Lane::Ble, tick + 2);
            concurrent.EndWait(Lane::Ble, tick + 5);
            concurrent.Finish(Lane::Ble, tick + 6);
        }
        done.store(true);
    });
    std::thread consumer([&] {
        // Real GetEventInfo can be called independently, including cached/fake
        // reads. Never assume a one-to-one pairing from these counters alone.
        for (unsigned i = 0; i < 10000; ++i) concurrent.ConsumerRead(Lane::Ble);
    });
    while (!done.load()) {
        const auto sample = concurrent.Snapshot(100001);
        const auto &lane = sample.lanes[2];
        assert(lane.phase <= 3 && lane.unstable <= 1);
        if (!lane.unstable && (lane.connection_details & ConnectionDetailsValid)) {
            assert((lane.connection_details & 0xffu) == lane.last_connection_result % 3);
            assert(((lane.connection_details >> 8) & 0xffffu) == (lane.last_connection_result & 0xffffu));
        }
        if (!lane.unstable) {
            // Detect a new type published before its phase generation. While
            // reading native event i, last type is i; after read it is i+1.
            const auto event = static_cast<std::uint32_t>(lane.phase_since_tick / 10);
            if (lane.phase == 1) assert(lane.last_event_type == event);
            else if (lane.phase_since_tick != 0) assert(lane.last_event_type == event + 1);
            else assert(lane.last_event_type == 0);
        }
    }
    producer.join(); consumer.join();
    const auto lane = concurrent.Snapshot(100002).lanes[2];
    assert(lane.phase == 0 && lane.unstable == 0);
    assert(lane.begun == 10000 && lane.finished == 10000 && lane.consumer_reads == 10000);
    assert(lane.forward_waits == 10000 && lane.wait_returns == 10000 && lane.max_wait_ticks == 3);
    assert(concurrent.Snapshot(100003).connection_events == 10000);
    assert(lane.last_connection_result == 9999);
    std::puts("Lock-free event progress, blocked-consumer evidence, connection-hook counts and concurrent snapshots passed");
}
