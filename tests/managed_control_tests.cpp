// Exercises the real selection function with an SDK test double, NOT hardware/ABI proof.
#include "connection_client.hpp"
#include "discovery_evidence.hpp"
#include "scan_uuid.hpp"
#include "start_gate.hpp"
#include "ble_scan_request_progress.hpp"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using u8 = std::uint8_t;
using Result = std::uint32_t;
#define R_FAILED(value) ((value) != 0)
#define R_SUCCEEDED(value) ((value) == 0)
namespace scan = ams::bluetooth::stadia_scan;
namespace requests = ams::bluetooth::scan_requests;
namespace cached = ams::bluetooth::cached_scan;
enum class DiscoveryOutcome { Ready, NoTarget, Unavailable, Unconfirmed };
struct Event { bool valid{}; };
struct BtdrvAddress { u8 address[6]; };
struct BtdrvBleScanResult { BtdrvAddress addr; };
struct BtdrvGattAttributeUuid { unsigned size; u8 uuid[16]; };
enum class Scenario { Empty, AcquireFailure, CapabilityFailure, StartFailure, StopFailure, FullBatch, MetadataRejected, MetadataValid, MetadataZeroDeviceType };
Scenario scenario;
scan::Status scan_state;
requests::Status request_state;
unsigned acquires, closes, starts, stops, confirmation_reads;
std::array<std::array<u8, 16>, 2> observed_uuids;
std::uint64_t ticks;
bool g_running = true, g_scan_cleanup_confirmed = true;
int g_pad;
constexpr unsigned HidNpadButton_A = 1;
void Log(const char *, ...) {}
bool Status(const char *, Result rc) { return rc == 0; }
bool Pump() {
    if (stops > 0 && scan_state.active == 0) ++confirmation_reads; // Advance one frame, outside argument evaluation.
    return g_running;
}
unsigned padGetButtons(int *) {
    assert(stops > 0 && scan_state.active == 0); // Only confirm after verified stop.
    return confirmation_reads >= 2 ? HidNpadButton_A : 0;
}
unsigned padGetButtonsDown(int *) { return confirmation_reads == 2 ? HidNpadButton_A : 0; }
std::uint64_t armGetSystemTick() { return ticks += 1'000'000'000; }
std::uint64_t armTicksToNs(std::uint64_t value) { return value; }
Result btdevAcquireBleScanEvent(Event *event) {
    ++acquires;
    if (scenario == Scenario::AcquireFailure || acquires > 1) return 0x6159;
    event->valid = true;
    return 0;
}
void eventClose(Event *event) { assert(event->valid); event->valid = false; ++closes; }
Result eventWait(Event *event, unsigned) { assert(event->valid); return 0; }
Result btdevStartBleScanSmartDevice(const BtdrvGattAttributeUuid *uuid) {
    assert(starts < observed_uuids.size());
    std::copy_n(uuid->uuid, 16, observed_uuids[starts].begin());
    ++starts; ++request_state.btm.starts; ++request_state.btm.parameters;
    if (scenario == Scenario::StartFailure) return 1;
    ++scan_state.added; ++scan_state.active;
    return 0;
}
Result btdevStopBleScanSmartDevice() {
    ++stops; ++request_state.btm.stops;
    if (scenario == Scenario::StopFailure) { ++scan_state.failed; return 1; }
    if (scan_state.active) { --scan_state.active; ++scan_state.removed; }
    return 0;
}
Result btmuGetBleScanResultsForSmartDevice(BtdrvBleScanResult *rows, unsigned, u8 *count) {
    const bool candidate = scenario == Scenario::MetadataRejected || scenario == Scenario::MetadataValid || scenario == Scenario::MetadataZeroDeviceType;
    *count = scenario == Scenario::FullBatch ? 10 : candidate ? 1 : 0;
    if (candidate)
        for (unsigned i = 0; i < 6; ++i) rows[0].addr.address[i] = i + 1;
    return 0;
}
Result serviceDispatchOut(int *, unsigned command, requests::Status &out) {
    assert(command == requests::StatusCommand); out = request_state; return 0;
}
Result serviceDispatchInOut(int *, unsigned command, const cached::Target &target, cached::Status &out) {
    assert(command == cached::StatusCommand && cached::ValidTarget(target));
    out = {}; out.protocol_version = cached::ProtocolVersion; out.sequence = starts ? 11 : 10;
    if ((scenario == Scenario::MetadataRejected || scenario == Scenario::MetadataValid || scenario == Scenario::MetadataZeroDeviceType) && starts) {
        out.event_type = 6; out.scan_status = 2;
        const std::array<u8, 6> sdk_address{1,2,3,4,5,6};
        out.matched = std::memcmp(target.address, sdk_address.data(), sdk_address.size()) == 0;
        if (out.matched) { out.device_type = scenario == Scenario::MetadataValid ? 2 : 0; out.address_type = scenario == Scenario::MetadataRejected ? 4 : 1; out.advertisement_count = 2; }
    }
    return 0;
}
struct Probe {
    int mc_service{};
    bool ReadPatchStatus(scan::Status *out, bool) {
        *out = scan_state;
        return scenario != Scenario::CapabilityFailure;
    }
    void LogPatchStatus(const char *, const scan::Status &) {}
};
#include "../tools/stadia-probe/source/managed_control.inc"

void Reset(Scenario value) {
    scenario = value;
    scan_state = {}; scan_state.protocol_version = scan::ProtocolVersion; scan_state.enabled = 1;
    request_state = {}; request_state.protocol_version = requests::ProtocolVersion;
    request_state.enabled = request_state.ble_initialized = 1;
    acquires = closes = starts = stops = confirmation_reads = 0; ticks = 0; observed_uuids = {};
    g_running = g_scan_cleanup_confirmed = true;
}
int main() {
    Probe probe;
    std::array<u8, 6> target{};
    Reset(Scenario::Empty);
    assert(SelectManagedControl(probe, &target, false) == DiscoveryOutcome::NoTarget);
    assert(acquires == 1 && closes == 1 && starts == 2 && stops == 2);
    assert(observed_uuids[0] == missioncontrol::probe::ControlUuidLittleEndian);
    assert(observed_uuids[1] == missioncontrol::probe::ControlUuidCanonical);
    assert(g_scan_cleanup_confirmed && scan_state.active == 0);
    Reset(Scenario::Empty);
    assert(SelectManagedControl(probe, &target, true) == DiscoveryOutcome::NoTarget);
    assert(acquires == 1 && closes == 1 && starts == 1 && stops == 1);
    assert(observed_uuids[0] == missioncontrol::probe::ControlUuidCanonical);
    for (auto failure : {Scenario::AcquireFailure, Scenario::CapabilityFailure, Scenario::StartFailure,
                         Scenario::StopFailure, Scenario::FullBatch}) {
        Reset(failure);
        const auto outcome = SelectManagedControl(probe, &target, false);
        assert(acquires == 1);
        assert(closes == (failure == Scenario::AcquireFailure ? 0u : 1u));
        if (failure == Scenario::AcquireFailure || failure == Scenario::CapabilityFailure) {
            assert(outcome == DiscoveryOutcome::Unavailable && starts == 0 && stops == 0);
        } else {
            assert(starts == 1 && stops == 1); // No alternate after an error/ambiguous phase.
            if (failure == Scenario::FullBatch) assert(outcome == DiscoveryOutcome::NoTarget);
            else assert(outcome == DiscoveryOutcome::Unconfirmed && !g_scan_cleanup_confirmed);
        }
    }
    Reset(Scenario::MetadataRejected);
    target = {};
    assert(SelectManagedControl(probe, &target, false) == DiscoveryOutcome::NoTarget);
    assert(acquires == 1 && closes == 1 && starts == 1 && stops == 1);
    assert((target == std::array<u8, 6>{})); // Extra comparisons must not bypass freshness or choose a radio target.
    assert(confirmation_reads == 0); // Reject before confirmation, even though the stub WOULD press A.
    Reset(Scenario::MetadataValid);
    assert(SelectManagedControl(probe, &target, false) == DiscoveryOutcome::Ready);
    assert(acquires == 1 && closes == 1 && starts == 1 && stops == 1 && confirmation_reads == 2);
    assert((target == std::array<u8, 6>{1,2,3,4,5,6}));
    Reset(Scenario::MetadataZeroDeviceType);
    target = {};
    assert(SelectManagedControl(probe, &target, false) == DiscoveryOutcome::Ready);
    assert(acquires == 1 && closes == 1 && starts == 1 && stops == 1 && confirmation_reads == 2);
    assert((target == std::array<u8, 6>{1,2,3,4,5,6}));
    std::puts("Managed control model: one event/session, canonical-only stage, all-path closure and stop/error guards passed");
}
