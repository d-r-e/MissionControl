/* SPDX-License-Identifier: GPL-2.0-only */
// A bounded, scoped supported UUID16 discovery attempt BEFORE managed connect. Does
// not use BTM alias replies for evidence: read-only cmd12 compares native cache.
enum class DiscoveryOutcome { Ready, NoTarget, Unavailable, Unconfirmed };

DiscoveryOutcome DiscoverSavedStadia(Probe &diagnostics, const BtdrvAddress &target, bool observe_requests = false) {
    namespace requests = ams::bluetooth::scan_requests;
    requests::Status request_before{};
    auto read_requests = [&](requests::Status *out) {
        const Result rc = serviceDispatchOut(&diagnostics.mc_service, requests::StatusCommand, *out);
        return R_SUCCEEDED(rc) && out->protocol_version == requests::ProtocolVersion &&
            out->enabled == 1 && out->ble_initialized == 1 && out->reserved == 0;
    };
    if (observe_requests && !read_requests(&request_before)) {
        Log("Native request telemetry unavailable/BLE uninitialized: NO SCAN STARTED. Requires matching diagnostic module.\n");
        return DiscoveryOutcome::Unavailable;
    }
    if (observe_requests) Log("Native request BASELINE: BTM start=%u stop=%u parameters=%u; HID start=%u stop=%u parameters=%u. Attempts only, not IPC success/scanner state.\n",
        request_before.btm.starts, request_before.btm.stops, request_before.btm.parameters,
        request_before.hid.starts, request_before.hid.stops, request_before.hid.parameters);
    namespace cached = ams::bluetooth::cached_scan;
    cached::Target request{};
    std::memcpy(request.address, target.address, sizeof(request.address));
    auto inspect = [&](cached::Status *out) {
        return Status("Inspect native scan cache (non-consuming)",
            serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, request, *out));
    };
    cached::Status before{};
    scan::Status baseline{};
    if (!inspect(&before)) return DiscoveryOutcome::Unavailable;
    Log("Native cache capability: returned_protocol=%u expected=%u event_type=%u sequence=%llu.\n",
        before.protocol_version, cached::ProtocolVersion, before.event_type,
        static_cast<unsigned long long>(before.sequence));
    if (before.protocol_version != cached::ProtocolVersion) {
        Log("Native cache protocol mismatch: NO SCAN STARTED. Numeric mismatch alone does not identify the running file.\n");
        return DiscoveryOutcome::Unavailable;
    }
    if (!diagnostics.ReadPatchStatus(&baseline, false) || baseline.active || baseline.failed ||
        baseline.added != baseline.removed || baseline.protocol_version != scan::ProtocolVersion ||
        !baseline.enabled) return DiscoveryOutcome::Unavailable;
    Event scan_event{};
    if (!Status("Acquire scoped scan event", btdevAcquireBleScanEvent(&scan_event))) return DiscoveryOutcome::Unavailable;
    BtdrvGattAttributeUuid marker{};
    marker.size = scan::MarkerNativeHid16.size();
    std::copy(scan::MarkerNativeHid16.begin(), scan::MarkerNativeHid16.end(), marker.uuid);
    diagnostics.LogPatchStatus("Native HID discovery baseline", baseline);
    Log("NATIVE HID DISCOVERY: one supported complete UUID16 scoped filter, max30sec. No aliases, address/name/payload output, global clear or scan-mode write.\n");
    const Result start_rc = btdevStartBleScanSmartDevice(&marker);
    Status("Start scoped native HID scan", start_rc);
    bool found = false, trusted = R_SUCCEEDED(start_rc);
    unsigned polls = 0;
    cached::Status observation{};
    const auto started = armGetSystemTick();
    while (trusted && g_running && armTicksToNs(armGetSystemTick() - started) < 30'000'000'000ULL && Pump()) {
        eventWait(&scan_event, 0);
        cached::Status current{};
        const Result rc = serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, request, current);
        ++polls;
        if (R_FAILED(rc) || current.protocol_version != cached::ProtocolVersion || current.sequence < before.sequence ||
            (current.sequence > before.sequence && current.event_type == 6 && current.result != 0)) {
            Log("Native scan observation untrusted: IPC=0x%08x; no connection.\n", rc);
            trusted = false;
            break;
        }
        if (cached::FreshTarget(current, before.sequence)) {
            found = true;
            observation = current;
            break;
        }
    }
    // Stop exactly once, including after an ambiguous start error. Do not
    // rewrite filter indexes or clear native/global filters to force cleanup.
    const bool stopped = Status("Stop scoped native HID scan", btdevStopBleScanSmartDevice());
    eventClose(&scan_event);
    scan::Status after{};
    const bool status_read = diagnostics.ReadPatchStatus(&after, false);
    if (status_read) diagnostics.LogPatchStatus("Native HID discovery post-stop", after);
    const bool cleanup = missioncontrol::probe::DiscoveryScanClean(stopped, status_read, baseline, after);
    Log("NATIVE HID DISCOVERY RESULT: target_seen=%u polls=%u cleanup=%u trusted=%u (cached-event absence is inconclusive).\n",
        found ? 1u : 0u, polls, cleanup ? 1u : 0u, trusted ? 1u : 0u);
    if (found) Log("  Exact target native metadata: device_type=%u ble_address_type=%u advertisement_records=%u. No metadata was injected.\n",
        observation.device_type, observation.address_type, observation.advertisement_count);
    if (observe_requests) {
        requests::Status request_after{};
        if (!read_requests(&request_after) || !requests::Monotonic(request_before.btm, request_after.btm) ||
            !requests::Monotonic(request_before.hid, request_after.hid)) {
            trusted = false;
            Log("Native request telemetry post-stop unavailable/decreased; no request-delta interpretation.\n");
        } else {
            Log("Native request DELTA: BTM start=%u stop=%u parameters=%u; HID start=%u stop=%u parameters=%u. Not target attribution or native scan acknowledgement.\n",
                request_after.btm.starts - request_before.btm.starts, request_after.btm.stops - request_before.btm.stops,
                request_after.btm.parameters - request_before.btm.parameters,
                request_after.hid.starts - request_before.hid.starts, request_after.hid.stops - request_before.hid.stops,
                request_after.hid.parameters - request_before.hid.parameters);
        }
    }
    diagnostics.ReadEventProgress();
    if (!cleanup) {
        g_scan_cleanup_confirmed = false;
        return DiscoveryOutcome::Unconfirmed;
    }
    return missioncontrol::probe::DiscoveryAllowsConnect(cleanup, trusted, g_running, observation, before.sequence) ?
        DiscoveryOutcome::Ready : DiscoveryOutcome::NoTarget;
}
