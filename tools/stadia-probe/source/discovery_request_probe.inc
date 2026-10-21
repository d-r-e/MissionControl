/* SPDX-License-Identifier: GPL-2.0-only */
// Discovery-only comparison with passive native-request observations. NEVER
// falls through to managed connect, even on a positive target observation.
void RunDiscoveryRequests(const char *address_text) {
    std::array<std::uint8_t, 6> bytes{};
    if (!address_text || !missioncontrol::probe::ParseTargetAddress(address_text, std::strlen(address_text), &bytes)) {
        Log("Discovery-only: invalid target; no Bluetooth request.\n"); return;
    }
    Probe diagnostics;
    diagnostics.status_only = true; diagnostics.Run();
    if (!diagnostics.mc_open) return;
    namespace requests = ams::bluetooth::scan_requests;
    requests::Status capability{};
    scan::Status filters{};
    conn::Status lease{};
    if (R_FAILED(serviceDispatchOut(&diagnostics.mc_service, requests::StatusCommand, capability)) ||
        capability.protocol_version != requests::ProtocolVersion || capability.enabled != 1 ||
        capability.ble_initialized != 1 || capability.reserved ||
        !diagnostics.ReadPatchStatus(&filters) || filters.active || filters.failed || filters.added != filters.removed ||
        R_FAILED(serviceDispatchOut(&diagnostics.mc_service, conn::StatusCommand, lease)) ||
        lease.protocol_version != conn::ProtocolVersion || lease.state || lease.armed || lease.captured ||
        lease.ambiguous || lease.worker_started || lease.managed_disconnect_required) {
        Log("Discovery-only readiness gate refused: NO SCAN STARTED.\n"); return;
    }
    progress::Status pipeline{};
    if (!diagnostics.ReadEventProgress(&pipeline) || pipeline.connect_hook_attempts || pipeline.connection_events) {
        Log("Discovery-only native connection history not clean; no scan.\n"); return;
    }
    for (const auto &lane : pipeline.lanes) {
        if (lane.unstable || lane.phase != 0 || lane.begun != lane.finished || lane.forward_waits != lane.wait_returns) {
            Log("Discovery-only event lane not idle/balanced; no scan.\n"); return;
        }
    }
    if (!Status("Read-only BTM readiness open", btmInitialize())) return;
    BtmState state{};
    const Result state_rc = btmGetState(&state);
    btmExit();
    if (R_FAILED(state_rc) || (state != BtmState_Initialized && state != BtmState_Working)) {
        Log("Discovery-only BTM not initialized/working: IPC=0x%08x state=%u. No scan.\n", state_rc, static_cast<unsigned>(state)); return;
    }
    if (!g_running || !Status("Discovery-only btdevInitialize", btdevInitialize())) return;
    BtdrvAddress target{};
    std::memcpy(target.address, bytes.data(), bytes.size());
    Log("DISCOVERY REQUEST DIAGNOSTIC: unchanged supported HID UUID16 filter, NEW passive native request counters. Not a proposed filter fix. No connect even if target is seen.\n");
    const auto outcome = DiscoverSavedStadia(diagnostics, target, true);
    if (outcome == DiscoveryOutcome::Unconfirmed) {
        Log("Discovery-only scan cleanup unconfirmed; no retry, reset or connection request.\n");
        HoldManagedSessionForReboot(); return;
    }
    btdevExit(); // Close only this managed scan user's client, after its cleanup.
    Log("DISCOVERY-ONLY FINISHED: outcome=%u. NO connect, cancel, pair, GATT or physical output request.\n", static_cast<unsigned>(outcome));
}
