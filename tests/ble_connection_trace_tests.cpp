#include "../mc_mitm/source/bluetooth_mitm/bluetooth/ble_connection_trace.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

using namespace ams::bluetooth::connection_trace;
namespace {
    constexpr Address Target{1, 2, 3, 4, 5, 6}, Other{6, 5, 4, 3, 2, 1};
    constexpr std::uint64_t Token = 123, App = 42;
    constexpr std::uint8_t Client = 7;
    BeginRequest Request() {
        BeginRequest request{};
        request.token = Token;
        request.aruid = App;
        std::memcpy(request.address, Target.data(), Target.size());
        request.timeout_ms = 100;
        return request;
    }
    Status Read(ConnectionTrace &trace, std::uint64_t now = 100) {
        Status status{};
        assert(trace.Get(Token, now, true, status));
        return status;
    }
    void Begin(ConnectionTrace &trace) { assert(trace.Begin(Request(), 100)); }
    void Capture(ConnectionTrace &trace, bool direct = true, std::uint8_t client = Client) {
        trace.ObserveRequest(true, App, Target, client, direct, 100);
    }
    void Event(ConnectionTrace &trace, std::uint32_t status = 0, std::uint32_t id = 0,
               std::uint32_t result = 0, std::uint32_t reason = 0,
               std::uint64_t now = 100, std::uint8_t client = Client) {
        trace.ObserveCallback(Target, client, result, status, reason, id, now);
    }
    void IsAmbiguous(ConnectionTrace &trace, Reason reason) {
        const auto status = Read(trace);
        assert(status.state == static_cast<std::uint32_t>(State::Ambiguous));
        assert(status.reason == static_cast<std::uint32_t>(reason) && status.ambiguous == 1);
        assert(status.consumed == 1 && status.generation == 1 && status.available == 0);
    }
    void InvalidConfigurationDoesNotConsume() {
        static_assert(!std::is_copy_constructible_v<ConnectionTrace>);
        static_assert(!std::is_move_constructible_v<ConnectionTrace>);
        static_assert(std::is_trivial_v<BeginRequest> && std::is_standard_layout_v<BeginRequest>);
        static_assert(std::is_trivial_v<Status> && std::is_standard_layout_v<Status>);
        for (unsigned invalid = 0; invalid < 10; ++invalid) {
            ConnectionTrace trace;
            auto request = Request();
            std::uint64_t now = 100;
            switch (invalid) {
                case 0: request.token = 0; break;
                case 1: request.aruid = 0; break;
                case 2: std::memset(request.address, 0, 6); break;
                case 3: std::memset(request.address, 0xff, 6); break;
                case 4: request.reserved = 1; break;
                case 5: request.reserved2 = 1; break;
                case 6: request.timeout_ms = 0; break;
                case 7: request.timeout_ms = MaximumTimeoutMs + 1; break;
                case 8: now = std::numeric_limits<std::uint64_t>::max(); break;
                case 9: now = std::numeric_limits<std::uint64_t>::max() - request.timeout_ms + 1; break;
            }
            assert(!trace.Begin(request, now));
            const auto capability = trace.Capability(true);
            assert(capability.available == 1 && capability.consumed == 0 && capability.generation == 0);
            Begin(trace);
            assert(Read(trace).generation == 1);
        }
        for (auto timeout : {1u, MaximumTimeoutMs}) {
            ConnectionTrace trace;
            auto request = Request(); request.timeout_ms = timeout;
            const auto now = std::numeric_limits<std::uint64_t>::max() - timeout;
            assert(trace.Begin(request, now));
            assert(Read(trace, now).state == static_cast<std::uint32_t>(State::Active));
            assert(Read(trace, std::numeric_limits<std::uint64_t>::max()).state == static_cast<std::uint32_t>(State::Expired));
        }
    }
    void CapabilityAndTokens() {
        ConnectionTrace trace;
        Status out{};
        Event(trace, 0, 123, 0, 0, 999);
        Capture(trace);
        trace.Tick(999);
        assert(trace.Get(0, 999, true, out));
        assert(out.protocol_version == ProtocolVersion && out.enabled == 1 && out.available == 1);
        assert(trace.Capability(false).available == 0 && trace.Capability(true, true).available == 0);
        assert(!trace.Get(Token, 100, true, out) && !trace.Retire(Token, 100));
        Begin(trace); Capture(trace); Event(trace, 0, 17, 0, 26);
        const auto before = Read(trace);
        assert(!trace.Begin(Request(), 1000)); // Even an active trace cannot rearm or tick by Begin.
        std::memset(&out, 0xab, sizeof(out));
        const auto untouched = out;
        assert(!trace.Get(Token + 1, 1000, true, out));
        assert(std::memcmp(&out, &untouched, sizeof(out)) == 0);
        assert(!trace.Retire(Token + 1, 1000) && !trace.Retire(0, 1000));
        assert(trace.Get(0, 1000, true, out)); // Capability is read-only, even past deadline.
        Status capability{};
        capability.protocol_version = ProtocolVersion;
        capability.enabled = 1;
        capability.consumed = 1;
        assert(std::memcmp(&out, &capability, sizeof(out)) == 0);
        const auto after = Read(trace);
        assert(std::memcmp(&before, &after, sizeof(before)) == 0);
    }
    void ExpiryAndRetirement() {
        for (bool captured : {false, true}) for (bool expire : {false, true}) {
            ConnectionTrace trace; Begin(trace);
            if (captured) { Capture(trace); Event(trace); }
            const auto before = Read(trace, 199);
            assert(before.state == static_cast<std::uint32_t>(State::Active));
            if (expire) trace.Tick(200); else assert(trace.Retire(Token, 199));
            const auto finished = Read(trace, 200);
            assert(finished.state == static_cast<std::uint32_t>(expire ? State::Expired : State::Retired));
            assert(finished.reason == static_cast<std::uint32_t>(expire ? Reason::Timeout : Reason::LocalRetire));
            assert(trace.Retire(Token, 201) && trace.Retire(Token, 0));
            Event(trace, 0, 99, 0, 0, 201);
            Event(trace, 2, 0, 0, 0, 0); // Stale callbacks cannot change a retired trace.
            trace.ObserveRequest(true, App, Target, Client, true, 202);
            assert(!trace.Begin(Request(), 300));
            auto next = Request(); ++next.token;
            assert(!trace.Begin(next, 300));
            const auto after = Read(trace, 300);
            assert(std::memcmp(&finished, &after, sizeof(after)) == 0);
            assert(after.generation == 1 && after.consumed == 1 && after.ambiguous == 0);
        }
        ConnectionTrace trace; Begin(trace);
        trace.ObserveRequest(true, App, Target, Client, true, 200);
        Event(trace, 0, 0, 0, 0, 200);
        const auto status = Read(trace, 200);
        assert(status.captured_requests == 0 && status.callback_events == 0);
    }
    void RequestsAndCorrelation() {
        { // Only the actual BTM tuple binds, not a preselected native client.
            ConnectionTrace trace; Begin(trace);
            trace.ObserveRequest(false, 999, Other, 99, false, 100);
            trace.ObserveCallback(Other, 99, 0, 0, 0, 99, 100);
            assert(Read(trace).captured_requests == 0 && Read(trace).callback_events == 0);
            Capture(trace, false, 99);
            Event(trace, 0, 0, 0, 0, 100, 99);
            const auto observed = Read(trace);
            assert(observed.connected_events == 1 && observed.ambiguous == 0);
            assert(observed.request_direct_valid == 1 && observed.request_is_direct == 0);
        }
        { ConnectionTrace trace; Begin(trace); Event(trace); IsAmbiguous(trace, Reason::CallbackBeforeRequest); }
        for (bool captured : {false, true}) for (bool btm : {false, true}) {
            ConnectionTrace trace; Begin(trace);
            if (captured) Capture(trace);
            trace.ObserveRequest(btm, btm ? App + 1 : App, Target, Client, true, 100);
            IsAmbiguous(trace, Reason::ForeignRequest);
        }
        for (auto client : {Client, static_cast<std::uint8_t>(Client + 1)}) {
            ConnectionTrace trace; Begin(trace); Capture(trace);
            trace.ObserveRequest(true, App, Target, client, false, 100);
            IsAmbiguous(trace, Reason::RepeatedRequest);
            assert(Read(trace).captured_requests == 1);
        }
        { ConnectionTrace trace; Begin(trace); Capture(trace); Event(trace, 0, 0, 0, 0, 100, Client + 1);
          IsAmbiguous(trace, Reason::ClientMismatch); }
    }
    void RawCallbacksAndPrivateConnectionIds() {
        ConnectionTrace trace; Begin(trace); Capture(trace);
        assert(Read(trace).request_direct_valid == 1 && Read(trace).request_is_direct == 1);
        Event(trace, 0x1a, 0xffffffffu, 0x1234, 0x56);
        auto status = Read(trace);
        assert(status.callback_result_valid && status.callback_status_valid && status.callback_reason_valid);
        assert(status.callback_result == 0x1234 && status.callback_status == 0x1a && status.callback_reason == 0x56);
        assert(!status.callback_connection_id_valid && !status.connected_events && !status.disconnected_events && !status.ambiguous);
        Event(trace, 0x1a, 0, 0, 0x1a);
        assert(Read(trace).connected_events == 0 && Read(trace).ambiguous == 0);
        Event(trace); // Zero is a valid native connection ID.
        Event(trace);
        Event(trace, 2, 0);
        status = Read(trace);
        assert(status.connected_events == 2 && status.disconnected_events == 1 && status.callback_events == 5);
        assert(status.callback_connection_id_valid == 1 && status.connection_id_conflict == 0);
        Event(trace, 0, 9);
        IsAmbiguous(trace, Reason::ConflictingConnection);
        assert(Read(trace).connection_id_conflict == 1 && Read(trace).connected_events == 2);
        const auto terminal = Read(trace);
        Event(trace, 0, 0); Capture(trace); trace.Tick(0);
        assert(trace.Retire(Token, 999) && !trace.Begin(Request(), 100));
        status = Read(trace);
        assert(std::memcmp(&terminal, &status, sizeof(status)) == 0);
        ConnectionTrace invalid; Begin(invalid); Capture(invalid); Event(invalid, 0, 0xffffffffu);
        IsAmbiguous(invalid, Reason::InvalidConnection);
        assert(Read(invalid).connected_events == 0 && Read(invalid).callback_connection_id_valid == 0);
        ConnectionTrace failed; Begin(failed); Capture(failed); Event(failed, 0, 1, 5); Event(failed, 2, 1, 5);
        assert(Read(failed).connected_events == 0 && Read(failed).disconnected_events == 0);
        ConnectionTrace wrong_disconnect; Begin(wrong_disconnect); Capture(wrong_disconnect);
        Event(wrong_disconnect, 0, 7); Event(wrong_disconnect, 2, 8);
        IsAmbiguous(wrong_disconnect, Reason::ConflictingConnection);
        assert(Read(wrong_disconnect).disconnected_events == 0);
        for (auto subsequent : {0u, 2u}) {
            ConnectionTrace disconnected_first; Begin(disconnected_first); Capture(disconnected_first);
            Event(disconnected_first, 2, 7); Event(disconnected_first, subsequent, 8);
            IsAmbiguous(disconnected_first, Reason::ConflictingConnection);
            assert(Read(disconnected_first).connected_events == 0 && Read(disconnected_first).disconnected_events == 1);
        }
        ConnectionTrace invalid_disconnect; Begin(invalid_disconnect); Capture(invalid_disconnect);
        Event(invalid_disconnect, 2, 0xffffffffu);
        IsAmbiguous(invalid_disconnect, Reason::InvalidConnection);
        assert(Read(invalid_disconnect).disconnected_events == 0);
    }
    void DriverReturnsAndCallbackTiming() {
        ConnectionTrace trace; Begin(trace);
        assert(trace.ObserveRequest(true, App, Other, Client, true, 100) == 0);
        const auto ticket = trace.ObserveRequest(true, App, Target, Client, true, 110);
        assert(ticket == Token && Read(trace, 110).driver_result_valid == 0);
        trace.ObserveDriverReturn(0, 999, 999); // Cannot tick/expire another request.
        trace.ObserveDriverReturn(Token + 1, 999, 999);
        assert(Read(trace, 110).driver_result_valid == 0);
        Event(trace, 0, 7, 0, 0, 125); // Callback can beat IPC return.
        trace.ObserveDriverReturn(ticket, 0, 130);
        auto status = Read(trace, 130);
        assert(status.driver_result_valid == 1 && status.driver_result == 0);
        assert(status.callback_delay_valid == 1 && status.first_callback_delay_ms == 15 && status.last_callback_delay_ms == 15);
        Event(trace, 2, 7, 0, 22, 150);
        assert(Read(trace, 150).last_callback_delay_ms == 40);
        assert(trace.Retire(Token, 150));
        const auto retired = Read(trace, 150);
        trace.ObserveDriverReturn(ticket, 7, 160);
        const auto unchanged = Read(trace, 160);
        assert(std::memcmp(&retired, &unchanged, sizeof(retired)) == 0);
        ConnectionTrace rejected; Begin(rejected); Capture(rejected);
        rejected.ObserveDriverReturn(Token, 0x615, 100);
        Event(rejected, 2, 0xffffffffu, 0x1a, 0, 100);
        status = Read(rejected);
        assert(status.driver_result_valid == 1 && status.driver_result == 0x615);
        assert(status.callback_result == 0x1a && status.callback_delay_valid == 1 && status.first_callback_delay_ms == 0);
        rejected.ObserveDriverReturn(Token, 0, 100);
        IsAmbiguous(rejected, Reason::RepeatedReturn);
        ConnectionTrace expired; Begin(expired); Capture(expired);
        expired.ObserveDriverReturn(Token, 0, 200);
        assert(Read(expired, 200).driver_result_valid == 0);
    }
    void ClockRegression() {
        for (unsigned operation = 0; operation < 6; ++operation) {
            ConnectionTrace trace; Begin(trace); Capture(trace);
            trace.Tick(101);
            switch (operation) {
                case 0: trace.Tick(100); break;
                case 1: Event(trace); break;
                case 2: Capture(trace); break;
                case 3: assert(trace.Retire(Token, 100)); break;
                case 4: (void)Read(trace, 100); break;
                case 5: trace.ObserveDriverReturn(Token, 0, 100); break;
            }
            IsAmbiguous(trace, Reason::ClockRegression);
            assert(Read(trace).captured_requests == 1 && Read(trace).callback_events == 0);
        }
    }
}
int main() {
    InvalidConfigurationDoesNotConsume();
    CapabilityAndTokens();
    ExpiryAndRetirement();
    RequestsAndCorrelation();
    RawCallbacksAndPrivateConnectionIds();
    DriverReturnsAndCallbackTiming();
    ClockRegression();
    std::puts("Passive connection trace: one-shot lifecycle, capability privacy, correlation, raw callbacks and clock boundaries passed");
}
