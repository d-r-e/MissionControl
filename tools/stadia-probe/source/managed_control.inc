/* SPDX-License-Identifier: GPL-2.0-only */
// Included after the scoped discovery helpers. SDK addresses remain RAM-only;
// opaque scan-record fields are never parsed. No lease, alias or raw driver use.
DiscoveryOutcome SelectManagedControl(Probe &diagnostics, std::array<std::uint8_t, 6> *target, bool canonical_only) {
    namespace cached = ams::bluetooth::cached_scan;
    namespace requests = ams::bluetooth::scan_requests;
    const std::array uuids{missioncontrol::probe::ControlUuidLittleEndian,
                          missioncontrol::probe::ControlUuidCanonical};
    Event scan_event{};
    if (!Status("Acquire native control scan event ONCE", btdevAcquireBleScanEvent(&scan_event)))
        return DiscoveryOutcome::Unavailable;
    struct EventOwner {
        Event &event;
        ~EventOwner() { eventClose(&event); }
    } event_owner{scan_event};
    // A scan event belongs to this SDK session, not an individual scan phase.
    // Reuse it across verified stops; requesting it twice failed on hardware.
    for (unsigned phase = canonical_only ? 1u : 0u; phase < uuids.size() && g_running; ++phase) {
        scan::Status before{};
        requests::Status request_before{};
        cached::Target query{{1,2,3,4,5,6}, cached::RequireControlUuid, 0}; // Metadata baseline only; never a radio target.
        cached::Status baseline{};
        if (!diagnostics.ReadPatchStatus(&before, false) || before.protocol_version != scan::ProtocolVersion ||
            !before.enabled || before.active || before.failed || before.added != before.removed ||
            R_FAILED(serviceDispatchOut(&diagnostics.mc_service, requests::StatusCommand, request_before)) ||
            request_before.protocol_version != requests::ProtocolVersion || !request_before.enabled || !request_before.ble_initialized ||
            R_FAILED(serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, query, baseline)) ||
            baseline.protocol_version != cached::ProtocolVersion) {
            Log("Control selection capability/clean baseline unavailable: no further scan or connect.\n");
            return DiscoveryOutcome::Unavailable;
        }
        eventWait(&scan_event, 0); // Discard an old phase notification before starting.
        BtdrvGattAttributeUuid uuid{};
        uuid.size = uuids[phase].size();
        std::copy(uuids[phase].begin(), uuids[phase].end(), uuid.uuid);
        Log("CONTROL DISCOVERY phase%u/2: %s UUID128 filter, max12sec. No payload rewrite or aliases.\n",
            phase + 1, phase == 0 ? "little-endian" : "canonical");
        const Result start_rc = btdevStartBleScanSmartDevice(&uuid);
        Status("Start native UUID128 control scan", start_rc);
        bool trusted = R_SUCCEEDED(start_rc), fresh = false;
        cached::Status last_qualified{}, metadata{};
        unsigned cache_samples = 0, address_matches = 0, reversed_matches = 0, equal_snapshots = 0, diagnostic_errors = 0;
        unsigned metadata_source = 0;
        missioncontrol::probe::SingleControlTarget selection;
        unsigned polls = 0, maximum_count = 0;
        const auto started = armGetSystemTick();
        while (trusted && g_running && armTicksToNs(armGetSystemTick() - started) < 12'000'000'000ULL && Pump()) {
            eventWait(&scan_event, 0);
            std::array<BtdrvBleScanResult, 10> rows{};
            u8 count = 0;
            const Result rc = btmuGetBleScanResultsForSmartDevice(rows.data(), rows.size(), &count);
            ++polls;
            maximum_count = std::max<unsigned>(maximum_count, count);
            if (R_FAILED(rc) || count >= rows.size()) { selection.Invalidate(); trusted = false; break; }
            for (unsigned i = 0; i < count; ++i) {
                std::array<std::uint8_t, 6> address{};
                std::memcpy(address.data(), rows[i].addr.address, address.size());
                selection.Observe(address);
                std::memcpy(query.address, address.data(), address.size());
                cached::Status current{};
                const Result cache_rc = serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, query, current);
                if (R_FAILED(cache_rc) || current.protocol_version != cached::ProtocolVersion ||
                    current.sequence < baseline.sequence ||
                    (current.sequence > baseline.sequence && current.event_type == 6 && current.result != 0)) {
                    selection.Invalidate(); trusted = false; break;
                }
                // This flagged cache comparison requires the exact known UUID128
                // in the unchanged native advertisement for THIS SDK address.
                fresh |= missioncontrol::probe::FreshNativeControl(query, current, baseline.sequence);
                last_qualified = current;
                ++cache_samples;
                if (!fresh) {
                    // Diagnostics ONLY: never alter selection, freshness, or the
                    // address passed to any radio/connection operation.
                    auto plain = query;
                    plain.flags = 0;
                    cached::Status address_only{}, reversed{};
                    const Result address_rc = serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, plain, address_only);
                    std::reverse(plain.address, plain.address + sizeof(plain.address));
                    const Result reverse_rc = serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, plain, reversed);
                    if (R_FAILED(address_rc) || R_FAILED(reverse_rc) ||
                        address_only.protocol_version != cached::ProtocolVersion || reversed.protocol_version != cached::ProtocolVersion) {
                        ++diagnostic_errors;
                    } else {
                        equal_snapshots += current.sequence == address_only.sequence && current.sequence == reversed.sequence;
                        if (address_only.matched) { ++address_matches; metadata = address_only; metadata_source = 1; }
                        if (reversed.matched) { ++reversed_matches; if (!metadata_source) { metadata = reversed; metadata_source = 2; } }
                    }
                }
            }
            if (selection.Issues()) { trusted = false; break; }
        }
        const bool stopped = Status("Stop native UUID128 control scan ONCE", btdevStopBleScanSmartDevice());
        scan::Status after{};
        const bool read_after = diagnostics.ReadPatchStatus(&after, false);
        if (read_after) diagnostics.LogPatchStatus("Native control post-stop", after);
        const bool clean = missioncontrol::probe::DiscoveryScanClean(stopped, read_after, before, after);
        requests::Status request_after{};
        const bool requests_read = R_SUCCEEDED(serviceDispatchOut(&diagnostics.mc_service, requests::StatusCommand, request_after));
        const bool request_delta = requests_read && request_after.protocol_version == requests::ProtocolVersion &&
            request_after.enabled == 1 && request_after.ble_initialized == 1 &&
            requests::Monotonic(request_before.btm, request_after.btm) &&
            requests::Monotonic(request_before.hid, request_after.hid) &&
            request_after.btm.starts - request_before.btm.starts == 1 &&
            request_after.btm.stops - request_before.btm.stops == 1 &&
            request_after.btm.parameters - request_before.btm.parameters == 1 &&
            request_after.hid.starts == request_before.hid.starts && request_after.hid.stops == request_before.hid.stops &&
            request_after.hid.parameters == request_before.hid.parameters;
        Log("CONTROL DISCOVERY RESULT: phase=%u polls=%u max_candidates=%u seen=%u issues=%u native_fresh_control=%u cleanup=%u request_delta=%u. No identifiers.\n",
            phase + 1, polls, maximum_count, selection.Seen() ? 1u : 0u, selection.Issues(), fresh ? 1u : 0u,
            clean ? 1u : 0u, request_delta ? 1u : 0u);
        Log("CONTROL CACHE DIAGNOSTIC: samples=%u baseline_seq=%llu last_qualified_seq=%llu event=%u result=0x%08x scan_status=%u qualified_match=%u address_matches=%u reversed_readonly_matches=%u equal_snapshots=%u diag_errors=%u. No identifiers.\n",
            cache_samples, static_cast<unsigned long long>(baseline.sequence), static_cast<unsigned long long>(last_qualified.sequence),
            last_qualified.event_type, last_qualified.result, last_qualified.scan_status, last_qualified.matched,
            address_matches, reversed_matches, equal_snapshots, diagnostic_errors);
        Log("  Matched metadata validity=%u source=%u device_type=%u address_type=%u records=%u. Values valid ONLY when validity=1; source1=SDK order/source2=reversed comparison only.\n",
            metadata_source ? 1u : 0u, metadata_source, metadata.device_type, metadata.address_type, metadata.advertisement_count);
        if (!clean) { g_scan_cleanup_confirmed = false; return DiscoveryOutcome::Unconfirmed; }
        if (!trusted || !request_delta || !g_running) return DiscoveryOutcome::NoTarget;
        std::array<std::uint8_t, 6> selected{};
        if (selection.Resolve(&selected)) {
            if (!fresh) {
                Log("SDK candidate lacks fresh exact native control-UUID evidence: no connect.\n");
                return DiscoveryOutcome::NoTarget;
            }
            Log("Unique fresh native control selected in RAM; tracked scan fully stopped. Keep Android advertising.\n");
            Log("Release A, then press fresh A for ONE managed connection. PLUS aborts. Do not accept pairing dialogs.\n");
            missioncontrol::probe::StartGate gate;
            const auto confirmation = armGetSystemTick();
            while (g_running && armTicksToNs(armGetSystemTick() - confirmation) < 30'000'000'000ULL && Pump()) {
                if (gate.Update((padGetButtons(&g_pad) & HidNpadButton_A) != 0,
                                (padGetButtonsDown(&g_pad) & HidNpadButton_A) != 0)) {
                    *target = selected;
                    return DiscoveryOutcome::Ready;
                }
            }
            return DiscoveryOutcome::NoTarget;
        }
        Log("No SDK candidate with this UUID byte order. Verified stop precedes any alternate filter.\n");
    }
    return DiscoveryOutcome::NoTarget;
}
