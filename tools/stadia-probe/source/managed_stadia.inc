/* SPDX-License-Identifier: GPL-2.0-only */
// Included inside main.cpp's anonymous namespace. This deliberately does NOT
// use ConnectionProbe/Arm/Stop or the raw driver. Only standard managed SDK APIs.
void HoldManagedSessionForReboot() {
    Log("REBOOT REQUIRED: managed request/cleanup unconfirmed. No cancel/retry. Session kept open; use normal full CFW EMUMMC restart.\n");
    while (appletMainLoop()) {
        padUpdate(&g_pad);
        if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus)
            Log("PLUS cannot retire this uncertain state. Please restart the console; no raw cancellation sent.\n");
        consoleUpdate(nullptr);
        svcSleepThread(50'000'000);
    }
    g_running = false;
}

namespace native_trace = ams::bluetooth::connection_trace;

bool ReadPassiveTrace(Service &service, std::uint64_t token, native_trace::Status &status) {
    const Result rc = serviceDispatchIn(&service, native_trace::StatusCommand, token,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = { { &status, sizeof(status) } });
    if (!Status("Read passive connection trace", rc)) return false;
    if (status.protocol_version != native_trace::ProtocolVersion) {
        Log("Passive trace protocol mismatch; observation unavailable.\n");
        return false;
    }
    return true;
}

void LogPassiveTrace(const native_trace::Status &status) {
    Log("Passive trace: state=%u reason=%u captured=%u target_callbacks=%u ambiguous=%u direct_valid=%u direct=%u connected=%u disconnected=%u. Observation, NOT ownership.\n",
        status.state, status.reason, status.captured_requests, status.callback_events, status.ambiguous,
        status.request_direct_valid, status.request_is_direct, status.connected_events, status.disconnected_events);
    Log("  Callback raw validity=%u/%u/%u result=0x%08x status=%u reason=%u id_valid=%u conflict=%u. No HCI/security interpretation.\n",
        status.callback_result_valid, status.callback_status_valid, status.callback_reason_valid,
        status.callback_result, status.callback_status, status.callback_reason,
        status.callback_connection_id_valid, status.connection_id_conflict);
    Log("  Driver IPC: valid=%u result=0x%08x (acceptance NOT link). Callback delay: valid=%u first_ms=%u last_ms=%u, from captured native request.\n",
        status.driver_result_valid, status.driver_result, status.callback_delay_valid,
        status.first_callback_delay_ms, status.last_callback_delay_ms);
}

#include "managed_control.inc"
#include "settled_stadia.inc"

enum class ManagedDiscovery { None, SavedStadia, AndroidControl, AndroidControlCanonical };

void RunManagedConnection(const char *address_text, ManagedDiscovery discovery, bool trace_stadia = false) {
    const bool select_control = discovery == ManagedDiscovery::AndroidControl ||
                                discovery == ManagedDiscovery::AndroidControlCanonical;
    const bool discover_first = discovery == ManagedDiscovery::SavedStadia;
    const bool trace_connection = trace_stadia || select_control;
    std::array<std::uint8_t, 6> bytes{};
    if (!select_control && (!address_text || !missioncontrol::probe::ParseTargetAddress(address_text, std::strlen(address_text), &bytes))) {
        Log("Managed Stadia: invalid target; no Bluetooth request.\n");
        return;
    }
    BtdrvAddress target{};
    std::memcpy(target.address, bytes.data(), bytes.size());
    Probe diagnostics;
    diagnostics.status_only = true;
    diagnostics.Run();
    if (!diagnostics.mc_open) return;
    scan::Status scan_status{};
    conn::Status lease_status{};
    if (R_FAILED(serviceDispatchOut(&diagnostics.mc_service, scan::StatusCommand, scan_status)) ||
        R_FAILED(serviceDispatchOut(&diagnostics.mc_service, conn::StatusCommand, lease_status)) ||
        scan_status.protocol_version != scan::ProtocolVersion || !scan_status.enabled ||
        scan_status.active || scan_status.failed || scan_status.added != scan_status.removed ||
        lease_status.protocol_version != conn::ProtocolVersion || lease_status.state || lease_status.armed ||
        lease_status.captured || lease_status.ambiguous || lease_status.terminal || lease_status.worker_started ||
        lease_status.cancel_requests || lease_status.disconnect_requests || lease_status.managed_disconnect_required) {
        Log("Managed Stadia refused: scan/lease state not clean; no Bluetooth initialization.\n");
        return;
    }
    progress::Status pipeline{};
    if (!diagnostics.ReadEventProgress(&pipeline)) return;
    if (pipeline.connect_hook_attempts || pipeline.connection_events) {
        Log("Managed Stadia refused: this boot already contains native connect activity. Restart before a new attempt.\n");
        return;
    }
    for (const auto &lane : pipeline.lanes) {
        if (lane.unstable || lane.phase > 3 || (lane.phase &&
            (pipeline.sample_tick < lane.phase_since_tick ||
             armTicksToNs(pipeline.sample_tick - lane.phase_since_tick) > 1'000'000'000))) {
            Log("Managed Stadia refused: unstable/long-running event phase; no Bluetooth initialization.\n");
            return;
        }
    }
    if (trace_connection) {
        native_trace::Status capability{};
        if (discover_first || !ReadPassiveTrace(diagnostics.mc_service, 0, capability) ||
            !missioncontrol::probe::TraceAvailable(capability)) {
            Log("Passive trace unavailable/consumed: no Bluetooth initialization or connect request. A fresh matching module/boot is required.\n");
            return;
        }
    }
    if (trace_connection && !ReadRunningMissionControlIdentity(missioncontrol::probe::NativeReturnBuildId.data())) {
        Log("Trace requires exact reviewed loaded module: no Bluetooth initialization.\n");
        return;
    }
    if (select_control) {
        namespace cached = ams::bluetooth::cached_scan;
        const cached::Target query{{1,2,3,4,5,6}, cached::RequireControlUuid, 0};
        cached::Status capability{};
        if (!Status("Read-only flagged control-cache capability",
                    serviceDispatchInOut(&diagnostics.mc_service, cached::StatusCommand, query, capability)) ||
            capability.protocol_version != cached::ProtocolVersion) {
            Log("Control-cache capability unavailable: no Bluetooth initialization.\n");
            return;
        }
    }
    if (!g_running || !Status("Managed-only btdevInitialize", btdevInitialize())) return;
    if (select_control) {
        std::array<BtdrvBleConnectionInfo, 16> existing{};
        u8 existing_count = 0;
        const Result list_rc = btdevGetBleConnectionInfoList(existing.data(), existing.size(), &existing_count);
        if (R_FAILED(list_rc) || existing_count != 0) {
            Log("Control preflight: existing/untrusted ARUID-managed connections. No scan or adoption.\n");
            btdevExit(); return;
        }
        const auto selected = SelectManagedControl(diagnostics, &bytes, discovery == ManagedDiscovery::AndroidControlCanonical);
        if (selected == DiscoveryOutcome::Unconfirmed) { HoldManagedSessionForReboot(); return; }
        if (selected != DiscoveryOutcome::Ready) {
            Log("No trusted RAM control target: NO connection request.\n");
            btdevExit(); return;
        }
        std::memcpy(target.address, bytes.data(), bytes.size());
    }
    Event connection_event{};
    const bool event_loaded = Status("Acquire BTMU connection-change event", btdevAcquireBleConnectionStateChangedEvent(&connection_event));
    if (!event_loaded) { btdevExit(); return; }

    auto inspect = [&](u32 *handle) {
        std::array<BtdrvBleConnectionInfo, 16> records{};
        u8 count = 0;
        const Result rc = btdevGetBleConnectionInfoList(records.data(), records.size(), &count);
        if (R_FAILED(rc) || count >= records.size()) {
            Log("Managed list untrusted: IPC=0x%08x count=%u; no handle action.\n", rc, count);
            return missioncontrol::probe::TargetPresence::Unknown;
        }
        const auto *raw = reinterpret_cast<const std::uint8_t *>(records.data());
        const auto presence = missioncontrol::probe::InspectTargetConnections(raw, sizeof(records), count, bytes);
        if (presence == missioncontrol::probe::TargetPresence::Present &&
            (count != 1 || !missioncontrol::probe::FindConnectionHandle(raw, sizeof(records), count, bytes, handle))) {
            Log("Managed connection ambiguous; refusing GATT/disconnect.\n");
            return missioncontrol::probe::TargetPresence::Unknown;
        }
        // Start only with an entirely empty ARUID-qualified managed list.
        if (presence == missioncontrol::probe::TargetPresence::Absent && count != 0)
            return missioncontrol::probe::TargetPresence::Unknown;
        return presence;
    };
    u32 handle = missioncontrol::probe::InvalidConnectionHandle;
    if (inspect(&handle) != missioncontrol::probe::TargetPresence::Absent) {
        Log("Managed Stadia refused: existing/untrusted owned connections. Nothing adopted.\n");
        eventClose(&connection_event); btdevExit(); return;
    }
    if (trace_stadia && !select_control) {
        if (!ConfirmSettledStadia() || inspect(&handle) != missioncontrol::probe::TargetPresence::Absent) {
            Log("Settled Stadia confirmation/pre-submit ownership gate refused: NO connect request.\n");
            eventClose(&connection_event); btdevExit(); return;
        }
    }
    if (discover_first) {
        const auto discovered = DiscoverSavedStadia(diagnostics, target);
        if (discovered == DiscoveryOutcome::Unconfirmed) { HoldManagedSessionForReboot(); return; }
        if (discovered != DiscoveryOutcome::Ready) {
            Log(discovered == DiscoveryOutcome::Unavailable ?
                "Discovery preflight unavailable: NO SCAN STARTED and NO connect request. No scan-cleanup claim.\n" :
                "No fresh trusted target observation; NO connect request. Scan cleanup confirmed.\n");
            eventClose(&connection_event); btdevExit(); return;
        }
        // Reject background native connection activity or a long-running event
        // phase between discovery and our request. Standard SDK connect stays
        // identical to the direct comparison; no address-type/cache writes.
        if (!diagnostics.ReadEventProgress(&pipeline)) { HoldManagedSessionForReboot(); return; }
        bool clean = pipeline.connect_hook_attempts == 0 && pipeline.connection_events == 0;
        for (const auto &lane : pipeline.lanes) {
            clean &= !lane.unstable && lane.phase < 4 && (!lane.phase ||
                (pipeline.sample_tick >= lane.phase_since_tick &&
                 armTicksToNs(pipeline.sample_tick - lane.phase_since_tick) <= 1'000'000'000));
        }
        if (!clean || inspect(&handle) != missioncontrol::probe::TargetPresence::Absent) {
            Log("Post-discovery state changed/uncertain; no connect or unowned cleanup.\n");
            HoldManagedSessionForReboot(); return;
        }
    }
    std::uint64_t trace_token = 0;
    auto retire_trace = [&]() {
        if (!trace_token) return;
        const auto token = trace_token;
        trace_token = 0;
        if (Status("Retire passive observation ONLY (not native cleanup)",
                   serviceDispatchIn(&diagnostics.mc_service, native_trace::RetireCommand, token))) {
            native_trace::Status status{};
            if (ReadPassiveTrace(diagnostics.mc_service, token, status)) LogPassiveTrace(status);
        } else {
            Log("Local observation retirement unconfirmed; bounded expiry remains. This says nothing about link cleanup.\n");
        }
    };
    // Discard a stale notification BEFORE submitting our one managed request.
    eventWait(&connection_event, 0);
    if (trace_connection) {
        if (!diagnostics.ReadEventProgress(&pipeline) ||
            !missioncontrol::probe::TracePipelineReady(pipeline, armNsToTicks(1'000'000'000ULL))) {
            Log("Native activity/lanes changed before observation: no connect request.\n");
            eventClose(&connection_event); btdevExit(); return;
        }
        native_trace::BeginRequest request{};
        request.token = missioncontrol::probe::MakeLeaseToken(armGetSystemTick());
        request.aruid = appletGetAppletResourceUserId();
        std::memcpy(request.address, target.address, sizeof(request.address));
        request.timeout_ms = native_trace::MaximumTimeoutMs;
        if (!Status("Begin passive observation (no lease/actions)",
                    serviceDispatchIn(&diagnostics.mc_service, native_trace::BeginCommand, request))) {
            eventClose(&connection_event); btdevExit(); return;
        }
        trace_token = request.token;
        native_trace::Status status{};
        if (!ReadPassiveTrace(diagnostics.mc_service, trace_token, status) ||
            !missioncontrol::probe::TraceMaySubmit(status) || !Pump()) {
            Log("Observation not ready or operator stopped: no connect request.\n");
            retire_trace(); eventClose(&connection_event); btdevExit(); return;
        }
    }
    Log(select_control ?
        "MANAGED CONTROL: ONE connect to RAM-only target. No lease arm, raw cancel, explicit pairing or GATT access. Native scan cleanup verified.\n" :
        "MANAGED STADIA: ONE connect to saved target. No lease arm, raw cancel, explicit pairing or GATT write. Any discovery scan is already stopped.\n");
    const bool managed_accepted = Status("Submit standard managed connect (acceptance is NOT connection)", btdevConnectToGattServer(target));
    if (!managed_accepted) Log("Managed submission rejected; still inspect observation/ownership. No automatic retry or cleanup claim.\n");
    bool connected = false;
    auto started = armGetSystemTick();
    while (g_running && armTicksToNs(armGetSystemTick() - started) < 20'000'000'000ULL && Pump()) {
        eventWait(&connection_event, 0);
        const auto state = inspect(&handle);
        if (state == missioncontrol::probe::TargetPresence::Present) { connected = true; break; }
        if (state == missioncontrol::probe::TargetPresence::Unknown) break;
    }
    bool retired = false;
    if (connected) {
        bool inspect_cache = !trace_connection;
        Log(select_control ? "CONTROL MANAGED CONNECTION CONFIRMED: unique target in this ARUID's managed list.\n" :
                             "STADIA MANAGED CONNECTION CONFIRMED: unique target in this ARUID's managed list.\n");
        diagnostics.ReadEventProgress();
        if (trace_connection) {
            native_trace::Status status{};
            const bool correlated = ReadPassiveTrace(diagnostics.mc_service, trace_token, status) &&
                missioncontrol::probe::TraceSawConnectedCallback(status);
            Log("Managed target handle present; native connection correlation=%u.\n", correlated ? 1u : 0u);
            if (status.protocol_version == native_trace::ProtocolVersion) LogPassiveTrace(status);
            inspect_cache = trace_stadia && !select_control && correlated;
        }
        if (inspect_cache) {
            // Native BTM may already discover services. Poll its managed cache,
            // never create a raw GATT client, pair, or guess an attribute handle.
            std::array<BtdevGattService, 16> services{};
            u8 count = 0;
            started = armGetSystemTick();
            while (g_running && armTicksToNs(armGetSystemTick() - started) < 5'000'000'000ULL && Pump()) {
                u32 current = missioncontrol::probe::InvalidConnectionHandle;
                if (inspect(&current) != missioncontrol::probe::TargetPresence::Present || current != handle) break;
                const Result rc = btdevGetGattServices(handle, services.data(), services.size(), &count);
                if (R_FAILED(rc) || count >= services.size()) {
                    Log("Managed GATT service cache unavailable/truncated: IPC=0x%08x count=%u.\n", rc, count);
                    count = 0; break;
                }
                if (count) break;
                svcSleepThread(200'000'000);
            }
            Log("Managed GATT service count=%u (no characteristic value reads/subscriptions yet).\n", count);
            for (unsigned i = 0; i < count && g_running && armTicksToNs(armGetSystemTick() - started) < 5'000'000'000ULL; ++i) {
                auto &service = services[i];
                if (service.attr.connection_handle != handle || service.attr.uuid.size != 2) continue;
                std::uint16_t uuid = 0;
                std::memcpy(&uuid, service.attr.uuid.uuid, sizeof(uuid));
                if (uuid != 0x1812) continue;
                if (!service.attr.handle || service.end_group_handle < service.attr.handle) {
                    Log("HID service handle range untrusted; no attribute operations.\n"); break;
                }
                u32 current = missioncontrol::probe::InvalidConnectionHandle;
                if (inspect(&current) != missioncontrol::probe::TargetPresence::Present || current != handle) break;
                static std::array<BtdevGattCharacteristic, 32> characteristics;
                u8 characteristic_count = 0;
                const Result rc = btdevGattServiceGetCharacteristics(&service, characteristics.data(), characteristics.size(), &characteristic_count);
                if (R_FAILED(rc) || characteristic_count >= characteristics.size()) {
                    Log("HID characteristic cache untrusted: IPC=0x%08x count=%u.\n", rc, characteristic_count);
                    break;
                }
                unsigned maps = 0, reports = 0, notify_reports = 0, references = 0, cccds = 0;
                bool topology = true;
                for (unsigned j = 0; j < characteristic_count && g_running; ++j) {
                    auto &c = characteristics[j];
                    if (c.attr.connection_handle != handle || c.attr.handle <= service.attr.handle ||
                        c.attr.handle > service.end_group_handle) { topology = false; break; }
                    if (c.attr.uuid.size != 2) continue;
                    std::uint16_t id = 0;
                    std::memcpy(&id, c.attr.uuid.uuid, sizeof(id));
                    maps += id == 0x2a4b;
                    if (id != 0x2a4d) continue;
                    if (++reports > 4 || armTicksToNs(armGetSystemTick() - started) >= 5'000'000'000ULL) {
                        topology = false; break;
                    }
                    notify_reports += (c.properties & BtdrvGattCharacteristicProperty_Notify) != 0;
                    if (inspect(&current) != missioncontrol::probe::TargetPresence::Present || current != handle) {
                        topology = false; break;
                    }
                    static std::array<BtdevGattDescriptor, 16> descriptors;
                    u8 descriptor_count = 0;
                    const Result desc_rc = btdevGattCharacteristicGetDescriptors(&c, descriptors.data(), descriptors.size(), &descriptor_count);
                    if (R_FAILED(desc_rc) || descriptor_count >= descriptors.size()) { topology = false; break; }
                    for (unsigned k = 0; k < descriptor_count; ++k) {
                        const auto &d = descriptors[k];
                        if (d.attr.connection_handle != handle || d.attr.handle <= c.attr.handle ||
                            d.attr.handle > service.end_group_handle) { topology = false; break; }
                        if (d.attr.uuid.size != 2) continue;
                        std::uint16_t descriptor_id = 0;
                        std::memcpy(&descriptor_id, d.attr.uuid.uuid, sizeof(descriptor_id));
                        references += descriptor_id == 0x2908; cccds += descriptor_id == 0x2902;
                    }
                    descriptors.fill({});
                    if (!topology) break;
                }
                Log("HID managed cache: topology_fields_agree=%u characteristics=%u maps=%u reports=%u notify_reports=%u reference_descriptors=%u CCCDs=%u. Metadata only; no values/IDs/payloads or verified callback-instance mapping.\n",
                    topology ? 1u : 0u, characteristic_count, maps, reports, notify_reports, references, cccds);
                characteristics.fill({});
            }
        }
        // Resolve current ARUID-qualified ownership again, never reuse a cached
        // native ID or disconnect another client. Exactly one disconnect call.
        u32 current = missioncontrol::probe::InvalidConnectionHandle;
        if (inspect(&current) == missioncontrol::probe::TargetPresence::Present && current == handle) {
            eventWait(&connection_event, 0);
            const Result rc = btdevDisconnectFromGattServer(current);
            Status("Managed disconnect owned target", rc);
            bool notified = false;
            started = armGetSystemTick();
            while (R_SUCCEEDED(rc) && armTicksToNs(armGetSystemTick() - started) < 5'000'000'000ULL) {
                notified |= R_SUCCEEDED(eventWait(&connection_event, 0));
                const auto state = inspect(&current);
                if (state == missioncontrol::probe::TargetPresence::Absent && notified) { retired = true; break; }
                if (state == missioncontrol::probe::TargetPresence::Unknown) break;
                // PLUS cannot cancel cleanup after an accepted connect.
                Pump();
            }
        }
    }
    diagnostics.ReadEventProgress();
    retire_trace();
    if (retired) {
        Log("BTMU change notification + owned handle absence observed after managed disconnect. Not proof of raw pending/native link retirement.\n");
        eventClose(&connection_event); btdevExit(); return;
    }
    // A failed/accepted connect may still be pending; no documented managed
    // cancel is available here. Do not close/retry/reset it and claim cleanup.
    // Forced applet exit can still tear down the process. This is NOT proof of
    // native pending retirement; the operator must still restart before retry.
    HoldManagedSessionForReboot();
}
