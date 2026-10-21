/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Foreground BLE diagnostic, NOT a controller driver. Uses BTM's managed
 * user APIs, not a second btdrv event reader. Connection tests require the
 * separately enabled module-side ownership lease; no pairing or GATT calls.
 */
#include <switch.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include "target_address.hpp"
#include "start_gate.hpp"
#include "scan_uuid.hpp"
#include "control_evidence.hpp"
#include "discovery_evidence.hpp"
#include "module_build_id.hpp"
#include "console_command.hpp"
#include "sd_file_session.hpp"
#include "stadia_ble_scan_filter.hpp"
#include "ble_connection_probe_protocol.hpp"
#include "ble_event_progress.hpp"
#include "ble_cached_scan_protocol.hpp"
#include "ble_scan_request_progress.hpp"
#include "connection_client.hpp"
#include "connection_trace_client.hpp"

namespace {
    namespace scan = ams::bluetooth::stadia_scan;
    namespace conn = ams::bluetooth::ble_connection_probe;
    namespace progress = ams::bluetooth::event_progress;
    enum class ScanMode { Base, PatchedBigEndian, PatchedLittleEndian, PatchedIncompleteBigEndian, ControlLittleEndian, ControlCanonical, ControlHid16LittleEndian, ControlHid16BigEndian, ControlHid16IncompleteBigEndian };
    // Scan-only batch also reproduced reported sidebar/power-menu sluggishness.
    // Keep all radio-changing diagnostics blocked until the shared path is understood.
    constexpr bool ScanDiagnosticsPaused = true;
    constexpr const char *TargetPath = "sdmc:/config/MissionControl/stadia-probe-address.txt";
    constexpr const char *LogPath = "sdmc:/config/MissionControl/stadia-ble-probe.log";
    FILE *g_log;
    int g_network = -1;
    PadState g_pad;
    bool g_running = true;
    bool g_scan_cleanup_confirmed = true;
    bool g_console_mode = false, g_remote_ready = false, g_busy = false, g_cancel_scan = false;
    using Command = missioncontrol::probe::ConsoleCommand;
    Command g_pending = Command::None;
    missioncontrol::probe::ConsoleCommandParser g_command_parser;
    void PollRemote();

    void Log(const char *format, ...) {
        va_list args;
        va_start(args, format);
        std::vprintf(format, args);
        va_end(args);
        if (g_network >= 0) {
            va_start(args, format);
            if (g_console_mode) {
                char message[512];
                const int length = std::vsnprintf(message, sizeof(message), format, args);
                // Never block Bluetooth cleanup on a stalled host. A broken or
                // backpressured stream ends this session; no silent log drops.
                if (length < 0 || static_cast<std::size_t>(length) >= sizeof(message) ||
                    send(g_network, message, length, MSG_DONTWAIT | MSG_NOSIGNAL) != length) {
                    close(g_network);
                    g_network = -1;
                    g_running = false;
                }
            } else {
                std::vfprintf(stderr, format, args);
                std::fflush(stderr);
            }
            va_end(args);
        }
        if (g_log) {
            va_start(args, format);
            std::vfprintf(g_log, format, args);
            va_end(args);
            std::fflush(g_log);
        }
        consoleUpdate(nullptr);
    }

    void PollRemote() {
        if (!g_console_mode || g_network < 0 || !g_running) return;
        unsigned char data[256];
        const auto size = recv(g_network, data, sizeof(data), MSG_DONTWAIT);
        if (size == 0 || (size < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
            close(g_network);
            g_network = -1;
            g_running = false;
            Log("REMOTE DISCONNECTED: stopping diagnostic session.\n");
            return;
        }
        for (int i = 0; g_running && i < size; ++i) {
            const auto command = g_command_parser.Feed(data[i]);
            if (command == Command::None) continue;
            if (command == Command::Invalid) { Log("REMOTE REJECT: command not allowed.\n"); continue; }
            if (command == Command::Quit) { g_running = false; continue; }
            if (!g_remote_ready) { Log("REMOTE REJECT: physical A authorization required.\n"); continue; }
            if (command == Command::Cancel) {
                g_cancel_scan = true;
                g_pending = Command::None;
                Log("REMOTE CANCEL requested.\n");
            } else if (g_busy || g_pending != Command::None) {
                Log("REMOTE REJECT: busy.\n");
            } else {
                g_pending = command;
            }
        }
    }

    bool Pump() {
        PollRemote();
        if (!appletMainLoop()) g_running = false;
        padUpdate(&g_pad);
        if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus) g_running = false;
        consoleUpdate(nullptr);
        svcSleepThread(50'000'000);
        return g_running;
    }

    bool Status(const char *operation, Result result) {
        Log("%s: IPC result 0x%08x\n", operation, result);
        return R_SUCCEEDED(result);
    }

    bool ReadTarget(BtdrvAddress *target) {
        FILE *file = std::fopen(TargetPath, "r");
        if (!file) { Log("Missing %s\n", TargetPath); return false; }
        char text[64]{};
        const auto length = std::fread(text, 1, sizeof(text), file);
        std::fclose(file);
        std::array<std::uint8_t, 6> address{};
        if (!missioncontrol::probe::ParseTargetAddress(text, length, &address)) return false;
        std::memcpy(target->address, address.data(), address.size());
        return true;
    }

    bool SameAddress(const BtdrvAddress &a, const BtdrvAddress &b) {
        return std::memcmp(a.address, b.address, sizeof(a.address)) == 0;
    }

    struct Probe {
        BtdrvAddress target{};
        bool target_from_args = false;
        bool discovered = false;
        bool require_bridge = false;
        bool require_learned_format = false;
        bool require_fresh_hid_control = false;
        bool status_only = false;
        ScanMode mode = ScanMode::Base;
        Service mc_service{};
        bool mc_open = false;
        scan::Status initial_status{};
        bool initialized = false;
        bool scanning = false;
        bool event_acquired = false;
        Event scan_event{};

        bool ReadPatchStatus(scan::Status *status, bool require_enabled = true, bool quiet = false) {
            if (!mc_open) {
                mc_open = Status("Open MissionControl diagnostic service", smGetService(&mc_service, "mc"));
                if (!mc_open) return false;
                u32 version = 0;
                if (!Status("Get running MissionControl version", serviceDispatchOut(&mc_service, 0, version))) return false;
                Log("Running MissionControl version: %u.%u.%u (0x%06x)\n", (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff, version);
                if (version < 0x001000) {
                    Log("Old MissionControl is still running. This test build requires our staged 0.16.0 experimental module.\n");
                    Log("Reboot into the updated emuMMC before retrying. No status/scan command sent.\n");
                    return false;
                }
            }
            const Result rc = serviceDispatchOut(&mc_service, scan::StatusCommand, *status);
            if (R_FAILED(rc)) { Status("Get patch status", rc); return false; }
            if (!quiet) Log("Diagnostic handshake: protocol=%u expected=%u scan=%u\n",
                status->protocol_version, scan::ProtocolVersion, status->enabled);
            if (status->protocol_version != scan::ProtocolVersion || (require_enabled && status->enabled != 1)) {
                Log("Scan patch unavailable/disabled. No scan attempted; check installed build, config and reboot.\n");
                return false;
            }
            return true;
        }

        void LogPatchStatus(const char *label, const scan::Status &status) {
            Log("%s: added=%u removed=%u active=%u failed=%u last=0x%08x driver_ads=%u driver_hid_ads=%u\n",
                label, status.added, status.removed, status.active, status.failed, status.last_result,
                status.driver_advertisements, status.driver_hid_advertisements);
            Log("  Managed events while private filter tracked: total=%u scan=%u scan_errors=%u last_type=%u last_scan_status=%u last_scan_result=0x%08x\n",
                status.managed_events, status.scan_events, status.scan_errors,
                status.last_event_type, status.last_scan_status, status.last_scan_result);
            Log("  Advertisement bridge: enabled=%u size_convention=%u copied=%u no_space=%u unvalidated=%u rejected=%u\n",
                status.bridge_enabled, status.ad_size_convention, status.bridged_advertisements, status.bridge_no_space,
                status.bridge_unvalidated, status.bridge_rejected);
        }

        bool IsControl() const {
            return mode == ScanMode::ControlLittleEndian || mode == ScanMode::ControlCanonical ||
                   mode == ScanMode::ControlHid16LittleEndian || mode == ScanMode::ControlHid16BigEndian ||
                   mode == ScanMode::ControlHid16IncompleteBigEndian;
        }

        bool ReadEventProgress(progress::Status *out_status = nullptr) {
            if (!mc_open) return false;
            progress::Status status{};
            const Result rc = serviceDispatch(&mc_service, progress::StatusCommand,
                .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
                .buffers = { { &status, sizeof(status) } },
            );
            if (R_FAILED(rc)) {
                Log("Event progress IPC failed: 0x%08x module=%u description=%u; no snapshot/fallback. Check matching transport/protocol, not just command presence.\n",
                    rc, static_cast<unsigned>(rc & 0x1ff), static_cast<unsigned>(rc >> 9));
                return false;
            }
            if (status.protocol_version != progress::ProtocolVersion || status.lane_count != progress::LaneCount) {
                Log("Event progress ABI mismatch: protocol=%u lanes=%u; snapshot not interpreted.\n", status.protocol_version, status.lane_count);
                return false;
            }
            Log("Event progress: protocol=%u uptime_ms=%llu connect_hook_attempts=%u btm=%u passthrough=%u (attempts, NOT owned links).\n",
                status.protocol_version, static_cast<unsigned long long>(armTicksToNs(status.sample_tick) / 1'000'000),
                status.connect_hook_attempts, status.btm_connect_attempts, status.passthrough_attempts);
            Log("  Native ClientConnection events=%u (global observations, not target ownership).\n", status.connection_events);
            constexpr const char *names[] = { "Core", "HID", "BLE" };
            constexpr const char *phases[] = { "idle", "native-read/lock", "processing", "consumer-wait" };
            for (std::size_t i = 0; i < progress::LaneCount; ++i) {
                const auto &lane = status.lanes[i];
                const bool valid_age = lane.unstable == 0 && lane.phase != 0 && lane.phase < 4 &&
                    status.sample_tick >= lane.phase_since_tick;
                const auto age_ms = valid_age ? armTicksToNs(status.sample_tick - lane.phase_since_tick) / 1'000'000 : 0;
                Log("  %s phase=%s age_valid=%u age_ms=%llu last_type=%u unstable=%u begun=%u finished=%u\n",
                    names[i], lane.phase < 4 ? phases[lane.phase] : "unknown", valid_age ? 1u : 0u,
                    static_cast<unsigned long long>(age_ms), lane.last_event_type, lane.unstable, lane.begun, lane.finished);
                Log("    waits=%u consumer_reads=%u returns=%u last_wait_us=%llu max_wait_us=%llu\n",
                    lane.forward_waits, lane.consumer_reads, lane.wait_returns,
                    static_cast<unsigned long long>(armTicksToNs(lane.last_wait_ticks) / 1000),
                    static_cast<unsigned long long>(armTicksToNs(lane.max_wait_ticks) / 1000));
                if (i == static_cast<std::size_t>(progress::Lane::Ble) && !lane.unstable &&
                    (lane.connection_details & progress::ConnectionDetailsValid)) {
                    Log("    Last native ClientConnection: result=0x%08x status=%u reason_field=0x%04x; uncorrelated with target.\n",
                        lane.last_connection_result, lane.connection_details & 0xffu,
                        (lane.connection_details >> 8) & 0xffffu);
                }
            }
            if (out_status) *out_status = status;
            return true;
        }

        void Run() {
            if (status_only) {
                scan::Status status{};
                if (ReadPatchStatus(&status, false)) {
                    Log("Diagnostic flags: scan=%u bridge=%u\n", status.enabled, status.bridge_enabled);
                    LogPatchStatus("Status only", status);
                }
                if (mc_open) {
                    // Read-only command 7: learns whether this module exposes the
                    // owned-connection IPC and whether its experimental flag is
                    // on. No address, pair, connect, GATT or key operation.
                    conn::Status connection{};
                    const Result rc = serviceDispatchOut(&mc_service, conn::StatusCommand, connection);
                    if (R_SUCCEEDED(rc)) {
                        Log("Connection probe: protocol=%u expected=%u enabled=%u state=%u reason=%u armed=%u\n",
                            connection.protocol_version, conn::ProtocolVersion, connection.enabled,
                            connection.state, connection.reason, connection.armed);
                        Log("  captured=%u ambiguous=%u terminal=%u connected_events=%u disconnected_events=%u managed_disconnect=%u\n",
                            connection.captured, connection.ambiguous, connection.terminal,
                            connection.connected_events, connection.disconnected_events,
                            connection.managed_disconnect_required);
                        Log("  worker=%u cancels=%u disconnect_requests=%u blocked_bonds=%u last_result=0x%08x\n",
                            connection.worker_started, connection.cancel_requests, connection.disconnect_requests,
                            connection.blocked_bond_requests, connection.last_result);
                    } else {
                        Log("Connection probe status: IPC result 0x%08x (installed module may not expose commands 7-10).\n", rc);
                    }
                    ReadEventProgress();
                }
                Log("Status-only check: no btdev/btmu initialization or Bluetooth request sent.\n");
                return;
            }
            if (ScanDiagnosticsPaused) {
                Log("SCAN DIAGNOSTICS PAUSED after scan-only sidebar/power-menu slowdown. No Bluetooth initialization or scan sent.\n");
                return;
            }
            if (!IsControl() && !target_from_args && !ReadTarget(&target)) {
                Log("Target address missing/invalid. No Bluetooth commands sent.\n");
                return;
            }
            if (mode != ScanMode::Base) {
                if (!ReadPatchStatus(&initial_status)) return;
                if (require_bridge && initial_status.bridge_enabled != 1) {
                    Log("Advertisement bridge is not explicitly enabled. No scan attempted.\n");
                    return;
                }
                if (require_learned_format && initial_status.ad_size_convention != 1 && initial_status.ad_size_convention != 2) {
                    Log("HID list comparison requires native UUID128 calibration first. No scan attempted.\n");
                    return;
                }
                LogPatchStatus("Patch before scan", initial_status);
                if (initial_status.active != 0 || initial_status.failed != 0 || initial_status.ad_size_convention == 3) {
                    Log("Previous private filter/error or conflicting AD format. Restart console before testing.\n");
                    return;
                }
            }
            initialized = Status("btdevInitialize", btdevInitialize());
            if (!initialized) return;
            event_acquired = Status("Acquire scan event", btdevAcquireBleScanEvent(&scan_event));
            if (!event_acquired) return;
            BtdrvGattAttributeUuid hid_uuid{};
            hid_uuid.size = missioncontrol::probe::HidScanUuid128.size();
            const auto &uuid = (mode == ScanMode::PatchedIncompleteBigEndian || mode == ScanMode::ControlHid16IncompleteBigEndian) ? scan::MarkerIncompleteBigEndian :
                               mode == ScanMode::ControlLittleEndian ? missioncontrol::probe::ControlUuidLittleEndian :
                               mode == ScanMode::ControlCanonical ? missioncontrol::probe::ControlUuidCanonical :
                               (mode == ScanMode::PatchedBigEndian || mode == ScanMode::ControlHid16BigEndian) ? scan::MarkerBigEndian :
                               (mode == ScanMode::PatchedLittleEndian || mode == ScanMode::ControlHid16LittleEndian) ? scan::MarkerLittleEndian :
                               missioncontrol::probe::HidScanUuid128;
            std::memcpy(hid_uuid.uuid, uuid.data(), hid_uuid.size);
            Log("Scan mode: %s\n", (mode == ScanMode::PatchedIncompleteBigEndian || mode == ScanMode::ControlHid16IncompleteBigEndian) ? "private marker -> INCOMPLETE UUID16 list, bytes 18 12" :
                mode == ScanMode::ControlHid16LittleEndian ? "synthetic HID16 control, private marker -> bytes 12 18" :
                mode == ScanMode::ControlHid16BigEndian ? "synthetic HID16 control, private marker -> bytes 18 12" :
                mode == ScanMode::ControlLittleEndian ? "synthetic UUID128 control, least-significant byte first" :
                mode == ScanMode::ControlCanonical ? "synthetic UUID128 control, canonical byte order" :
                mode == ScanMode::Base ? "Bluetooth-base HID UUID128" :
                mode == ScanMode::PatchedBigEndian ? "private marker -> UUID16 bytes 18 12" :
                                                    "private marker -> UUID16 bytes 12 18");
            scanning = Status("Start managed scan", btdevStartBleScanSmartDevice(&hid_uuid));
            if (!scanning) { g_scan_cleanup_confirmed = false; return; }
            if (IsControl()) {
                Log("CONTROL: synthetic advertiser must be running now. Stadia is not needed.\n");
                Log("Any UUID-filtered candidate is a control result; no addresses/names/payloads logged.\n");
            } else {
                Log("Put Stadia in pairing mode now (Stadia + Y). Scan lasts about 30 seconds.\n");
                Log("Only the configured target is matched; candidate addresses are not logged.\n");
            }
            bool &found = discovered;
            // Results are opaque apart from the documented address field.
            for (unsigned tick = 0; g_running && !g_cancel_scan && tick < 120; ++tick) {
                eventWait(&scan_event, 0);
                std::array<BtdrvBleScanResult, 10> results{};
                u8 count = 0;
                const Result rc = btmuGetBleScanResultsForSmartDevice(results.data(), results.size(), &count);
                if (R_FAILED(rc)) { Status("Get managed scan results", rc); break; }
                if (count >= results.size() && tick % 20 == 0) Log("Scan list at capacity; results may omit target.\n");
                if (IsControl() && count != 0) found = true;
                for (unsigned i = 0; !IsControl() && i < std::min<std::size_t>(count, results.size()); ++i) {
                    if (SameAddress(results[i].addr, target)) found = true;
                }
                if (found && require_fresh_hid_control) {
                    scan::Status observed{};
                    if (!ReadPatchStatus(&observed, true, true)) { found = false; break; }
                    // A retained BTM candidate alone is not a fresh control.
                    found = missioncontrol::probe::HasFreshHidControlEvidence(initial_status, observed);
                }
                if (found) break;
                if (tick % 20 == 0) Log("Scan: %u candidates, %s not seen.\n", count, IsControl() ? "control" : "target");
                for (unsigned i = 0; i < 5 && !g_cancel_scan && Pump(); ++i) { }
            }
            if (!g_running || g_cancel_scan) {
                Log("SCAN CANCELLED. Do not interpret this as a negative discovery result.\n");
            } else if (IsControl()) {
                Log(found ? "CONTROL CANDIDATE SEEN by managed UUID128 scan. Not a Stadia compatibility result.\n" :
                            "CONTROL NOT CONFIRMED. This does not isolate a Stadia-specific problem.\n");
            } else if (found) {
                Log("TARGET DETECTED by managed BLE scan. Discovery milestone only.\n");
            } else {
                Log("TARGET NOT CONFIRMED. Scan filtering/address behavior still need verification.\n");
            }
            Log("No connection, pairing, GATT read/write or notification request was sent.\n");
        }

        ~Probe() {
            // Retry only our managed scan stop; never reset the global adapter.
            for (unsigned attempt = 0; scanning && attempt < 3; ++attempt) {
                if (Status("Stop diagnostic managed scan", btdevStopBleScanSmartDevice())) scanning = false;
                else svcSleepThread(100'000'000);
            }
            if (scanning) {
                g_scan_cleanup_confirmed = false;
                Log("SCAN STOP UNCONFIRMED. Exit and restart console before another test.\n");
            }
            if (mode != ScanMode::Base && mc_open && initialized) {
                scan::Status status{};
                bool observed = ReadPatchStatus(&status);
                for (unsigned retry = 0; observed && status.active != 0 && retry < 10; ++retry) {
                    svcSleepThread(100'000'000);
                    observed = ReadPatchStatus(&status);
                }
                if (observed) {
                    LogPatchStatus("Patch after stop", status);
                    const bool private_marker = !IsControl() || mode == ScanMode::ControlHid16LittleEndian || mode == ScanMode::ControlHid16BigEndian || mode == ScanMode::ControlHid16IncompleteBigEndian;
                    if (private_marker && status.added == initial_status.added) Log("PATCH NOT EXERCISED: no private filter translation observed.\n");
                    if (!private_marker) Log("Control is not a private marker: no translation or private-event observation expected.\n");
                    if (status.active != 0) {
                        g_scan_cleanup_confirmed = false;
                        Log("PRIVATE FILTER CLEANUP UNCONFIRMED. Restart console before another test.\n");
                    }
                } else {
                    g_scan_cleanup_confirmed = false;
                    Log("Patch cleanup status unavailable; do not assume successful filter removal.\n");
                }
            }
            if (event_acquired) eventClose(&scan_event);
            if (initialized) btdevExit();
            if (mc_open) serviceClose(&mc_service);
        }
    };

    // One-shot, owned connection diagnostic: arm -> connect -> wait -> stop ->
    // managed disconnect. No scan, pairing, GATT or key access. The module-side
    // lease owns the native command-65 tuple; this client never guesses a
    // connection id and never disconnects by a cached value.
    struct ConnectionProbe {
        using ArmRequest = conn::ArmRequest;
        using ManagedDisconnectResult = conn::ManagedDisconnectResult;
        using ConnectionStatus = conn::Status;

        static constexpr std::uint32_t RequestTimeoutMs = 15000;
        static constexpr std::uint64_t PollIntervalMs = 50;
        static constexpr std::uint64_t ConnectWaitMs = RequestTimeoutMs + 2000;
        static constexpr std::uint64_t HoldMs = 20000;
        static constexpr std::uint64_t CleanupWaitMs = 10000;

        BtdrvAddress target{};
        bool target_from_args = false;
        bool select_control = false;
        bool cancel_control = false;
        bool stop_requested = false;
        Service mc_service{};
        bool mc_open = false;
        bool initialized = false;
        bool armed = false;
        bool managed_disconnect_attempted = false;
        std::uint64_t token = 0;
        std::array<std::uint8_t, 6> target_bytes{};

        static std::uint64_t NowMs() { return armTicksToNs(armGetSystemTick()) / 1'000'000; }

        bool ReadStatus(ConnectionStatus *status, bool require_enabled) {
            if (!mc_open) {
                mc_open = Status("Open MissionControl diagnostic service", smGetService(&mc_service, "mc"));
                if (!mc_open) return false;
                u32 version = 0;
                if (!Status("Get running MissionControl version", serviceDispatchOut(&mc_service, 0, version))) return false;
                Log("Running MissionControl version: %u.%u.%u (0x%06x)\n", (version >> 16) & 0xff, (version >> 8) & 0xff, version & 0xff, version);
                if (version < 0x001000) {
                    Log("Old MissionControl is still running. This test build requires our staged 0.16.0 experimental module.\n");
                    Log("Reboot into the updated emuMMC before retrying. No status/connection command sent.\n");
                    return false;
                }
            }
            const Result rc = serviceDispatchOut(&mc_service, conn::StatusCommand, *status);
            if (R_FAILED(rc)) { Status("Get connection probe status", rc); return false; }
            if (status->protocol_version != conn::ProtocolVersion || (require_enabled && status->enabled != 1)) {
                Log("Connection probe unavailable/disabled (protocol=%u expected=%u enabled=%u). No Bluetooth request sent.\n",
                    status->protocol_version, conn::ProtocolVersion, status->enabled);
                return false;
            }
            return true;
        }

        void LogStatus(const char *label, const ConnectionStatus &status) {
            Log("%s: protocol=%u enabled=%u state=%u reason=%u armed=%u captured=%u ambiguous=%u terminal=%u\n",
                label, status.protocol_version, status.enabled, status.state, status.reason,
                status.armed, status.captured, status.ambiguous, status.terminal);
            Log("  connected_events=%u disconnected_events=%u cancels=%u disconnects=%u blocked_bonds=%u last=0x%08x worker=%u managed_disconnect=%u\n",
                status.connected_events, status.disconnected_events, status.cancel_requests,
                status.disconnect_requests, status.blocked_bond_requests, status.last_result,
                status.worker_started, status.managed_disconnect_required);
        }

        bool SelectControlTarget() {
            scan::Status before{};
            if (!Status("Read scan state before control selection", serviceDispatchOut(&mc_service, scan::StatusCommand, before)) ||
                before.protocol_version != scan::ProtocolVersion || before.active || before.failed) {
                Log("Control selection refused: scan state not clean. No connection sent.\n");
                return false;
            }
            Event scan_event{};
            if (!Status("Acquire control scan event", btdevAcquireBleScanEvent(&scan_event))) return false;
            BtdrvGattAttributeUuid uuid{};
            uuid.size = missioncontrol::probe::ControlUuidLittleEndian.size();
            std::memcpy(uuid.uuid, missioncontrol::probe::ControlUuidLittleEndian.data(), uuid.size);
            const Result start_rc = btdevStartBleScanSmartDevice(&uuid);
            Status("Start native UUID128 control selection", start_rc);
            if (R_FAILED(start_rc)) {
                g_scan_cleanup_confirmed = false;
                Log("SCAN START UNCONFIRMED; no connection. Restart before another test.\n");
                eventClose(&scan_event);
                return false;
            }
            Log("Scanning ONLY the diagnostic UUID128 for 30 seconds. No address/name/payload output or persistence.\n");
            missioncontrol::probe::SingleControlTarget selection;
            unsigned batches = 0, max_count = 0;
            const auto started = NowMs();
            while (NowMs() - started < 30000 && Pump()) {
                eventWait(&scan_event, 0);
                std::array<BtdrvBleScanResult, 10> results{};
                u8 count = 0;
                const Result rc = btmuGetBleScanResultsForSmartDevice(results.data(), results.size(), &count);
                ++batches;
                max_count = std::max<unsigned>(max_count, count);
                if (R_FAILED(rc) || count >= results.size()) {
                    Log("Control scan batch untrusted: result=0x%08x count=%u. No connection.\n", rc, count);
                    selection.Invalidate();
                    break;
                }
                for (unsigned i = 0; i < count; ++i) {
                    std::array<std::uint8_t, 6> address{};
                    std::memcpy(address.data(), results[i].addr.address, address.size());
                    selection.Observe(address);
                }
            }
            Log("Control selection summary: batches=%u max_candidates=%u seen=%u issue_bits=%u cancelled=%u\n",
                batches, max_count, selection.Seen() ? 1u : 0u, selection.Issues(), g_running ? 0u : 1u);
            const bool stopped = Status("Stop native control selection scan", btdevStopBleScanSmartDevice());
            eventClose(&scan_event);
            scan::Status after{};
            const bool clean = stopped && R_SUCCEEDED(serviceDispatchOut(&mc_service, scan::StatusCommand, after)) &&
                after.protocol_version == scan::ProtocolVersion && !after.active && !after.failed;
            if (!clean) {
                g_scan_cleanup_confirmed = false;
                Log("SCAN CLEANUP UNCONFIRMED; no connection. Restart before another test.\n");
                return false;
            }
            std::array<std::uint8_t, 6> selected{};
            if (!g_running || !selection.Resolve(&selected)) {
                Log("CONTROL TARGET UNCONFIRMED: cancelled, absent, multiple, rotating or truncated candidates. No connection.\n");
                return false;
            }
            std::memcpy(target.address, selected.data(), selected.size());
            target_from_args = true;
            Log("Exactly one diagnostic UUID candidate selected in RAM. Native scan stopped.\n");
            if (cancel_control) {
                Log("CANCELLATION TEST: STOP Android advertising now; leave Bluetooth ON.\n");
                Log("Release A, then press fresh A ONLY AFTER advertising is stopped: one connection request followed immediately by stop. PLUS aborts.\n");
            } else {
                Log("Confirm only the intended Android advertiser is running. Release A, then press fresh A for ONE connection; PLUS aborts.\n");
            }
            missioncontrol::probe::StartGate gate;
            const auto confirmation_started = NowMs();
            while (NowMs() - confirmation_started < 30000 && Pump()) {
                if (gate.Update((padGetButtons(&g_pad) & HidNpadButton_A) != 0,
                                (padGetButtonsDown(&g_pad) & HidNpadButton_A) != 0)) return true;
            }
            Log("Control connection confirmation cancelled/timed out. No connection.\n");
            return false;
        }

        void RequestStop() {
            if (!armed || stop_requested) return;
            stop_requested = true; // An uncertain IPC result is never retried.
            Status("Request lease stop", serviceDispatchIn(&mc_service, conn::StopCommand, token));
        }

        void Run() {
            if (missioncontrol::probe::ConnectionDiagnosticsPaused) {
                Log("CONNECTION DIAGNOSTICS PAUSED after system sluggishness. No scan/init/arm/connect/cancel performed.\n");
                return;
            }
            if (cancel_control && !select_control) {
                Log("Cancellation test requires RAM-only control selection. No Bluetooth request.\n");
                return;
            }
            if (select_control) {
                ConnectionStatus initial{};
                if (!ReadStatus(&initial, true) || initial.state || initial.armed || initial.captured || initial.terminal) {
                    Log("Control connection unavailable or previous lease not idle. No scan or connection.\n");
                    return;
                }
                initialized = Status("btdevInitialize for control selection", btdevInitialize());
                if (!initialized || !SelectControlTarget()) return;
            }
            if (!target_from_args && !ReadTarget(&target)) {
                Log("Target address missing/invalid. No Bluetooth commands sent.\n");
                return;
            }
            std::memcpy(target_bytes.data(), target.address, target_bytes.size());
            bool zero = true, ff = true;
            for (auto byte : target_bytes) { zero &= byte == 0; ff &= byte == 0xff; }
            if (zero || ff) { Log("Refusing an empty/broadcast target. No Bluetooth commands sent.\n"); return; }

            ConnectionStatus status{};
            if (!ReadStatus(&status, true)) return;
            LogStatus("Connection status before arm", status);
            if (status.state || status.armed || status.captured || status.terminal) {
                Log("Previous lease state is not clean. Restart the console before another test.\n");
                return;
            }

            if (!initialized) initialized = Status("btdevInitialize", btdevInitialize());
            if (!initialized) return;
            BtdrvBleConnectionInfo existing[4]{};
            u8 existing_count = 0;
            if (!Status("Read owned connections before arm", btdevGetBleConnectionInfoList(existing, 4, &existing_count)) ||
                existing_count >= 4) {
                Log("Existing connection list unavailable/truncated. No connection request.\n");
                return;
            }
            for (unsigned i = 0; i < existing_count; ++i) {
                if (existing[i].connection_handle != missioncontrol::probe::InvalidConnectionHandle &&
                    SameAddress(existing[i].addr, target)) {
                    Log("Target already present in this ARUID's connection list. Refusing to adopt it.\n");
                    return;
                }
            }

            ArmRequest request{};
            token = missioncontrol::probe::MakeLeaseToken(armGetSystemTick());
            request.token = token;
            request.aruid = appletGetAppletResourceUserId();
            if (!request.aruid) {
                Log("No applet resource user id; the owned lease cannot be armed. No Bluetooth request sent.\n");
                armed = false;
                return;
            }
            std::memcpy(request.address, target.address, sizeof(request.address));
            request.timeout_ms = RequestTimeoutMs;
            if (!Status("Arm owned connection lease", serviceDispatchIn(&mc_service, conn::ArmCommand, request))) return;
            armed = true;
            Log("LEASE ARMED: reserved target only; no address/ARUID/key is logged.\n");

            const Result connect_rc = btdevConnectToGattServer(target);
            if (cancel_control) {
                // Do not wait for a connection event or poll before asking to
                // stop. The module alone decides whether it owns a pending tuple
                // (cancel), an established connection (managed disconnect), or
                // insufficient/ambiguous evidence (no automatic action).
                RequestStop();
                Log("Managed connect IPC result 0x%08x. Immediate stop requested; no retry.\n", connect_rc);
                Cleanup();
                return;
            }
            Log("Managed connect IPC result 0x%08x. Waiting for the owned connection.\n", connect_rc);

            bool connected = false;
            const auto connect_started = NowMs();
            while (NowMs() - connect_started < ConnectWaitMs && Pump()) {
                if (!ReadStatus(&status, false)) break;
                const auto outcome = missioncontrol::probe::ClassifyLease(status.state, status.terminal, status.ambiguous);
                if (outcome == missioncontrol::probe::LeaseOutcome::Connected) { connected = true; break; }
                if (outcome != missioncontrol::probe::LeaseOutcome::Running) break;
            }

            if (connected && status.captured) {
                LogStatus("Connection status after connect", status);
                Log("CONNECTED to the reserved target under the owned lease. Hold for inspection; PLUS ends and cleans up.\n");
                const auto hold_started = NowMs();
                while (NowMs() - hold_started < HoldMs && Pump()) {
                    if (!ReadStatus(&status, false) ||
                        missioncontrol::probe::ClassifyLease(status.state, status.terminal, status.ambiguous) !=
                            missioncontrol::probe::LeaseOutcome::Connected) break;
                }
            } else if (connected) {
                // The owned lease never observed the reserved tuple: never claim it.
                Log("CONNECTION NOT CAPTURED by the owned lease; not treated as an owned connection.\n");
            } else {
                Log("CONNECT NOT CONFIRMED. Lease state/reason logged below; no blind retry.\n");
            }
            Cleanup();
        }

        void PerformManagedDisconnect() {
            BtdrvBleConnectionInfo infos[4]{};
            u8 count = 0;
            static_assert(sizeof(BtdrvBleConnectionInfo) == missioncontrol::probe::ConnectionInfoSize);
            std::uint32_t handle = 0;
            const Result list_rc = btdevGetBleConnectionInfoList(infos, 4, &count);
            if (R_FAILED(list_rc) || !missioncontrol::probe::FindConnectionHandle(
                    reinterpret_cast<const std::uint8_t *>(infos), sizeof(infos), count, target_bytes, &handle)) {
                // Fail closed: a guessed or cached id must never be disconnected.
                Log("MANAGED DISCONNECT UNRESOLVED: no matching owned handle; reporting failure, not guessing.\n");
                ManagedDisconnectResult result{token, 1, 0};
                Status("Complete managed disconnect (unresolved)", serviceDispatchIn(&mc_service, conn::ManagedDisconnectCommand, result));
                return;
            }
            const Result disconnect_rc = btdevDisconnectFromGattServer(handle);
            Log("Managed disconnect IPC result 0x%08x.\n", disconnect_rc);
            ManagedDisconnectResult result{token, disconnect_rc, 0};
            Status("Complete managed disconnect", serviceDispatchIn(&mc_service, conn::ManagedDisconnectCommand, result));
        }

        void ReportCancellation(const ConnectionStatus &status) {
            BtdrvBleConnectionInfo infos[4]{};
            u8 count = 0;
            const Result rc = btdevGetBleConnectionInfoList(infos, 4, &count);
            const auto presence = R_SUCCEEDED(rc) ? missioncontrol::probe::InspectTargetConnections(
                reinterpret_cast<const std::uint8_t *>(infos), sizeof(infos), count, target_bytes) :
                missioncontrol::probe::TargetPresence::Unknown;
            Log("Post-stop managed target presence: %s (IPC=0x%08x). No native pending-state conclusion implied.\n",
                presence == missioncontrol::probe::TargetPresence::Absent ? "ABSENT" :
                presence == missioncontrol::probe::TargetPresence::Present ? "PRESENT" : "UNKNOWN", rc);
            const missioncontrol::probe::CancellationEvidence evidence{
                status.captured, status.cancel_requests, status.disconnect_requests,
                status.connected_events, status.disconnected_events, status.reason, status.last_result,
                status.terminal != 0, status.ambiguous != 0, presence == missioncontrol::probe::TargetPresence::Absent};
            using Outcome = missioncontrol::probe::CancellationOutcome;
            switch (missioncontrol::probe::ClassifyCancellation(evidence)) {
                case Outcome::NotExercised:
                    Log("PENDING CANCELLATION NOT EXERCISED: no single captured cancel request. Lease cleanup outcome is separate.\n");
                    break;
                case Outcome::TerminalAfterCancel:
                    Log("NATIVE TERMINAL OBSERVED AFTER CANCEL: no connected event, no managed target handle. Not proof of all BTM pending bookkeeping or causality.\n");
                    break;
                case Outcome::LateConnectionCleaned:
                    Log("LATE CONNECTION AFTER CANCEL CLEANED: matching terminal and managed-handle absence confirmed.\n");
                    break;
                case Outcome::Unconfirmed:
                    Log("CANCELLATION CLEANUP UNCONFIRMED. Restart before another trial; no blind retry.\n");
                    break;
            }
        }

        void ReportFinal(const ConnectionStatus &status) {
            const auto outcome = missioncontrol::probe::ClassifyLease(status.state, status.terminal, status.ambiguous);
            if (outcome == missioncontrol::probe::LeaseOutcome::Retired) {
                Log("LEASE RETIRED cleanly.\n");
            } else if (outcome == missioncontrol::probe::LeaseOutcome::Quarantined) {
                Log("LEASE QUARANTINED (fail-closed). Restart the console before another test.\n");
            } else {
                Log("LEASE CLEANUP UNCONFIRMED. Restart the console before another test; do not blindly retry.\n");
            }
            if (cancel_control) ReportCancellation(status);
        }

        void Cleanup() {
            if (!armed) return;
            RequestStop(); // Stop intent is sent even if the next status query fails.
            armed = false;
            ConnectionStatus status{};
            if (!ReadStatus(&status, false)) { Log("LEASE CLEANUP UNCONFIRMED: status unavailable.\n"); return; }
            LogStatus("Connection status before stop", status);
            auto final_state = [](const ConnectionStatus &state) {
                const auto outcome = missioncontrol::probe::ClassifyLease(
                    state.state, state.terminal, state.ambiguous);
                // Quarantine alone is not completion: keep observing for a late
                // owned success. Ambiguity forbids automatic cleanup actions.
                return outcome == missioncontrol::probe::LeaseOutcome::Retired || state.ambiguous != 0;
            };
            if (final_state(status)) {
                LogStatus("Connection status final", status);
                ReportFinal(status);
                return;
            }
            const auto cleanup_started = NowMs();
            while (NowMs() - cleanup_started < CleanupWaitMs) {
                if (!ReadStatus(&status, false)) break;
                if (!status.ambiguous && status.managed_disconnect_required && !managed_disconnect_attempted) {
                    managed_disconnect_attempted = true;
                    PerformManagedDisconnect();
                }
                if (final_state(status)) break;
                // Keep local input/applet events responsive without abandoning
                // cleanup when PLUS was already pressed.
                Pump();
            }
            if (ReadStatus(&status, false)) {
                LogStatus("Connection status final", status);
                ReportFinal(status);
            } else {
                Log("LEASE CLEANUP UNCONFIRMED: final status unavailable.\n");
            }
        }

        ~ConnectionProbe() {
            Cleanup();
            if (mc_open) serviceClose(&mc_service);
            if (initialized) btdevExit();
        }
    };

    bool ReadRunningMissionControlIdentity(const std::uint8_t *expected = nullptr);
    #include "managed_discovery.inc"
    #include "managed_stadia.inc"
    #include "discovery_request_probe.inc"
    #include "module_identity.inc"

    void RunEventObservation() {
        Log("READ-ONLY EVENT OBSERVATION: loaded MC build IDs and 12 MC snapshots. No native Bluetooth requests.\n");
        Log("A blocked synchronous MC call may exceed the sampling interval. PLUS stops between calls.\n");
        Probe status_probe;
        status_probe.status_only = true;
        status_probe.Run();
        if (!status_probe.mc_open) return;
        ReadRunningMissionControlIdentity();
        ams::bluetooth::scan_requests::Status native_requests{};
        const Result requests_rc = serviceDispatchOut(&status_probe.mc_service,
            ams::bluetooth::scan_requests::StatusCommand, native_requests);
        if (R_SUCCEEDED(requests_rc) && native_requests.protocol_version == ams::bluetooth::scan_requests::ProtocolVersion)
            Log("Native scan-request capability: protocol=%u enabled=%u ble_initialized=%u BTM start=%u stop=%u parameters=%u; HID start=%u stop=%u parameters=%u. Attempts only.\n",
                native_requests.protocol_version, native_requests.enabled, native_requests.ble_initialized,
                native_requests.btm.starts, native_requests.btm.stops, native_requests.btm.parameters,
                native_requests.hid.starts, native_requests.hid.stops, native_requests.hid.parameters);
        else Log("Native scan-request capability unavailable: IPC=0x%08x. No scan requested.\n", requests_rc);
        native_trace::Status trace_capability{};
        if (ReadPassiveTrace(status_probe.mc_service, 0, trace_capability))
            Log("Passive connection-trace capability: protocol=%u enabled=%u available=%u consumed=%u. Read-only; no observation started.\n",
                trace_capability.protocol_version, trace_capability.enabled, trace_capability.available, trace_capability.consumed);
        // Pure MC cache comparison only: this synthetic token is NEVER passed
        // to a Bluetooth service, scan or connection request. No peer fields
        // or payload are logged, even if the current cache happens to match it.
        namespace cached = ams::bluetooth::cached_scan;
        const cached::Target token{{1,2,3,4,5,6},0,0};
        cached::Status capability{};
        const Result capability_rc = serviceDispatchInOut(&status_probe.mc_service,
            cached::StatusCommand, token, capability);
        Status("Read-only native cache capability", capability_rc);
        if (R_SUCCEEDED(capability_rc)) Log("CACHE CAPABILITY (read-only): returned_protocol=%u expected=%u event_type=%u sequence=%llu. Not a target-discovery result.\n",
            capability.protocol_version, cached::ProtocolVersion, capability.event_type,
            static_cast<unsigned long long>(capability.sequence));
        // Public build strings can distinguish module versions with the same
        // base MissionControl version. Fixed-size replies, bounded formatting.
        struct { char value[64]; } build{};
        struct { char value[32]; } date{};
        if (Status("Read-only MC build version", serviceDispatchOut(&status_probe.mc_service, 1, build)))
            Log("Running MC build: %.*s\n", 64, build.value);
        if (Status("Read-only MC build date", serviceDispatchOut(&status_probe.mc_service, 2, date)))
            Log("Running MC build date: %.*s\n", 32, date.value);
        for (unsigned sample = 0; sample < 12 && g_running; ++sample) {
            Log("EVENT SAMPLE %u/12\n", sample + 1);
            if (!status_probe.ReadEventProgress()) return;
            const auto deadline = armGetSystemTick() + armNsToTicks(1'000'000'000);
            if (sample != 11) while (armGetSystemTick() < deadline && Pump()) { }
        }
        Log("READ-ONLY EVENT OBSERVATION FINISHED. No scan or connection operation.\n");
    }

    void RunServiceLatency() {
        Log("READ-ONLY SERVICE TIMING: MC status and BTM GetState only. No btdev/btmu/btdrv initialization or radio changes.\n");
        Log("Synchronous service IPC can stall: last BEGIN identifies the in-flight query, not its internal cause. No retries.\n");
        Probe status_probe;
        status_probe.status_only = true;
        status_probe.Run();
        auto sample = [](const char *name, auto operation) {
            Log("SERVICE SAMPLE BEGIN: %s\n", name);
            const auto start = armGetSystemTick();
            const Result rc = operation();
            const auto elapsed_us = armTicksToNs(armGetSystemTick() - start) / 1000;
            Log("SERVICE SAMPLE END: %s result=0x%08x elapsed_us=%llu\n",
                name, rc, static_cast<unsigned long long>(elapsed_us));
            return rc;
        };
        if (status_probe.mc_open) {
            scan::Status scan_status{};
            sample("MC scan status (cmd6)", [&]() {
                return serviceDispatchOut(&status_probe.mc_service, scan::StatusCommand, scan_status);
            });
            conn::Status connection_status{};
            sample("MC connection status (cmd7)", [&]() {
                return serviceDispatchOut(&status_probe.mc_service, conn::StatusCommand, connection_status);
            });
        }
        // SDK _btmInitialize only opens the btm service; it does NOT send adapter
        // initialize/enable/reset commands. GetState is read-only command0.
        const Result open_rc = sample("Open BTM service (SM only)", []() { return btmInitialize(); });
        if (R_FAILED(open_rc)) {
            Log("BTM timing unavailable; no fallback or retry.\n");
            return;
        }
        BtmState state{};
        const Result state_rc = sample("BTM GetState (cmd0)", [&]() { return btmGetState(&state); });
        if (R_SUCCEEDED(state_rc)) Log("BTM state enum=%u (read-only).\n", static_cast<unsigned>(state));
        btmExit(); // Close only our read-only service session, not the adapter.
        Log("POST-BTM QUERY EVENT PROGRESS (compare with initial snapshot; background activity may be native/foreign).\n");
        status_probe.ReadEventProgress();
        Log("READ-ONLY SERVICE TIMING FINISHED. No scan/connect/cancel/pairing/GATT request.\n");
    }

    void RunControlInspection(bool hid_only) {
        Log(hid_only ? "SCAN-ONLY HID COMPARISON: complete HID16 then incomplete HID16 using already learned format; stop on first failure.\n" :
                       "SCAN-ONLY INSPECTION: status, native UUID128, complete HID16, incomplete HID16; stop on first failure.\n");
        Log("No arm/connect/cancel/pairing/GATT request in this batch. Android advertiser stays ON.\n");
        Probe status_probe;
        scan::Status initial{};
        if (!status_probe.ReadPatchStatus(&initial) || initial.active || initial.failed || initial.bridge_enabled != 1) {
            Log("INSPECTION ABORT: scan/bridge state unavailable or not clean.\n");
            return;
        }
        if (hid_only && initial.ad_size_convention != 1 && initial.ad_size_convention != 2) {
            Log("HID COMPARISON REFUSED: native format calibration not available; no scan.\n");
            return;
        }
        auto lease_idle = [&]() {
            conn::Status status{};
            const Result rc = serviceDispatchOut(&status_probe.mc_service, conn::StatusCommand, status);
            const bool idle = R_SUCCEEDED(rc) && status.protocol_version == conn::ProtocolVersion &&
                status.state == 0 && !status.armed && !status.captured && !status.ambiguous &&
                !status.terminal && !status.worker_started && !status.connected_events &&
                !status.disconnected_events && !status.cancel_requests && !status.disconnect_requests &&
                !status.managed_disconnect_required;
            Log("Inspection connection state: idle=%u IPC=0x%08x (read-only).\n", idle ? 1u : 0u, rc);
            if (R_SUCCEEDED(rc)) {
                Log("  protocol=%u enabled=%u state=%u reason=%u armed=%u captured=%u ambiguous=%u terminal=%u worker=%u\n",
                    status.protocol_version, status.enabled, status.state, status.reason, status.armed,
                    status.captured, status.ambiguous, status.terminal, status.worker_started);
                Log("  connected=%u disconnected=%u cancels=%u disconnect_requests=%u managed_disconnect=%u last=0x%08x\n",
                    status.connected_events, status.disconnected_events, status.cancel_requests,
                    status.disconnect_requests, status.managed_disconnect_required, status.last_result);
            }
            return idle;
        };
        if (!lease_idle()) { Log("INSPECTION ABORT: previous connection state; no scan.\n"); return; }
        status_probe.status_only = true;
        status_probe.Run();
        constexpr ScanMode modes[] = {ScanMode::ControlLittleEndian, ScanMode::ControlHid16BigEndian,
                                      ScanMode::ControlHid16IncompleteBigEndian};
        const unsigned begin = hid_only ? 1 : 0;
        const unsigned total = 3 - begin;
        for (unsigned pass = begin; pass < 3 && g_running; ++pass) {
            bool found = false;
            {
                Probe probe;
                probe.mode = modes[pass];
                probe.require_bridge = pass != 0;
                probe.require_learned_format = pass != 0;
                probe.require_fresh_hid_control = pass != 0;
                Log("INSPECTION SCAN %u/%u BEGIN\n", pass + 1 - begin, total);
                probe.Run();
                found = probe.discovered;
            } // Stop the owned scan and verify cleanup before proceeding.
            scan::Status after{};
            const bool clean = g_scan_cleanup_confirmed && status_probe.ReadPatchStatus(&after) &&
                after.active == 0 && after.failed == 0 && after.added == after.removed;
            const bool idle = lease_idle();
            Log("INSPECTION SCAN %u/%u END: candidate=%u cleanup=%u lease_idle=%u\n",
                pass + 1 - begin, total, found ? 1u : 0u, clean ? 1u : 0u, idle ? 1u : 0u);
            if (!g_running || !found || !clean || !idle) {
                Log("INSPECTION STOPPED: no subsequent scans or connection attempted.\n");
                return;
            }
        }
        if (g_running) Log("SCAN-ONLY INSPECTION COMPLETE: all %u controls passed, filters balanced, lease unused.\n", total);
    }

    void RunRemoteSession() {
        g_remote_ready = true;
        Log("REMOTE READY: status/control/hid-complete/hid-incomplete/real-complete/real-incomplete/cancel/file/quit.\n");
        while (Pump()) {
            if (g_pending == Command::None) continue;
            const auto command = g_pending;
            g_pending = Command::None;
            if (command == Command::FileTransfer) {
                if (!g_scan_cleanup_confirmed) {
                    Log("REMOTE REJECT: cleanup unconfirmed; file session blocked.\n");
                    continue;
                }
                g_cancel_scan = false;
                g_busy = true;
                Log("REMOTE FILE SESSION START\n");
                const auto res = missioncontrol::probe::sd::RunFileSession(__nxlink_host,
                    []() { return Pump() && !g_cancel_scan; });
                Log("REMOTE FILE SESSION END: result=%u\n", static_cast<unsigned>(res));
                g_busy = false;
                if (g_running) Log("REMOTE IDLE: cleanup_confirmed=1\n");
                continue;
            }
            if (!g_scan_cleanup_confirmed && command != Command::Status) {
                Log("REMOTE REJECT: previous cleanup unconfirmed; status or quit only.\n");
                continue;
            }
            g_cancel_scan = false;
            g_busy = true;
            {
                Probe probe;
                probe.status_only = command == Command::Status;
                if (command == Command::NativeControl) probe.mode = ScanMode::ControlLittleEndian;
                else if (command == Command::HidComplete) probe.mode = ScanMode::ControlHid16BigEndian;
                else if (command == Command::HidIncomplete) probe.mode = ScanMode::ControlHid16IncompleteBigEndian;
                else if (command == Command::RealComplete) probe.mode = ScanMode::PatchedBigEndian;
                else if (command == Command::RealIncomplete) probe.mode = ScanMode::PatchedIncompleteBigEndian;
                const bool synthetic_hid = command == Command::HidComplete || command == Command::HidIncomplete;
                probe.require_bridge = synthetic_hid;
                probe.require_learned_format = synthetic_hid;
                probe.require_fresh_hid_control = synthetic_hid;
                Log("REMOTE BEGIN\n");
                probe.Run();
            } // Owned scan cleanup completes BEFORE idle or another command.
            g_busy = false;
            if (g_running) Log("REMOTE IDLE: cleanup_confirmed=%u\n", g_scan_cleanup_confirmed ? 1 : 0);
        }
    }
}

int main(int argc, char **argv) {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);
    g_console_mode = argc == 2 && std::strcmp(argv[1], "--console") == 0;
    const bool sockets = __nxlink_host.s_addr != 0 && R_SUCCEEDED(socketInitializeDefault());
    if (sockets) g_network = g_console_mode ? nxlinkConnectToHost(false, false) : nxlinkStdioForDebug(); // Keep stdout on the console screen.
    g_log = std::fopen(LogPath, "w");
    const bool discovery_requests = argc == 4 && std::strcmp(argv[1], "--target") == 0 && std::strcmp(argv[3], "--discovery-requests") == 0;
    const bool discover_stadia = argc == 4 && std::strcmp(argv[1], "--target") == 0 && std::strcmp(argv[3], "--discover-stadia") == 0;
    const bool canonical_control = argc == 2 && std::strcmp(argv[1], "--trace-managed-control-canonical") == 0;
    const bool trace_managed_control = canonical_control || (argc == 2 && std::strcmp(argv[1], "--trace-managed-control") == 0);
    const bool trace_managed_stadia = argc == 4 && std::strcmp(argv[1], "--target") == 0 && std::strcmp(argv[3], "--trace-managed-stadia") == 0;
    const bool managed_stadia = trace_managed_control || trace_managed_stadia || discover_stadia || (argc == 4 && std::strcmp(argv[1], "--target") == 0 && std::strcmp(argv[3], "--managed-stadia") == 0);
    const bool status_only = argc == 2 && std::strcmp(argv[1], "--status-only") == 0;
    const bool event_observation = argc == 2 && std::strcmp(argv[1], "--event-progress") == 0;
    const bool service_latency = argc == 2 && std::strcmp(argv[1], "--service-latency") == 0;
    const bool inspect_hid = argc == 2 && std::strcmp(argv[1], "--inspect-hid-control") == 0;
    const bool inspect_control = inspect_hid || (argc == 2 && std::strcmp(argv[1], "--inspect-control") == 0);
    const bool bridge_control = argc == 2 && std::strcmp(argv[1], "--bridge-control") == 0;
    const bool hid_control = argc == 2 && std::strcmp(argv[1], "--control-hid16") == 0;
    const bool list_control = argc == 2 && std::strcmp(argv[1], "--control-hid16-lists") == 0;
    const bool control = bridge_control || hid_control || list_control || (argc == 2 && std::strcmp(argv[1], "--control") == 0);
    const bool connect_file = argc == 2 && std::strcmp(argv[1], "--connect") == 0;
    const bool cancel_control = argc == 2 && std::strcmp(argv[1], "--cancel-control") == 0;
    const bool connect_control = cancel_control || (argc == 2 && std::strcmp(argv[1], "--connect-control") == 0);
    Log("Stadia BLE diagnostic probe 0.6.33, HOS=0x%08x\n", hosversionGet());
    if (canonical_control) Log("CANONICAL-ONLY CONTROL: Stadia OFF, Mac Bluetooth OFF, Android connectable advertiser ON. One12sec canonical UUID128 scan; no little-endian repeat. Fresh second A gates ONE managed connection. No lease/cancel/GATT access.\n");
    if (trace_managed_control && !canonical_control) Log("ANDROID CONTROL: Stadia OFF, Mac Bluetooth OFF, Android connectable advertiser ON. Up to two scoped UUID128 byte-order scans, verified stop, RAM target + fresh A, then ONE managed connection. No lease/cancel/GATT access.\n");
    if (trace_managed_stadia) Log("STADIA SETTLED TRACE: BOTH Mac and Android Bluetooth OFF. Initial A, SDK initialization +12s idle(NO scan), then fresh second A for ONE connect. Observe actual driver IPC result +callback delay. Only owned+native-correlated success permits read-only managed HID catalogue; no values/subscriptions/pair/lease/cancel. Observer retirement is NOT link cleanup.\n");
    if (discovery_requests) Log("Discovery-only with passive native request counters. No connect on positive discovery. Requires explicit pairing-ready operator and matching module.\n");
    Log("No installation changes. No explicit pairing or GATT writes; native background activity is not excluded.\n");
    if (managed_stadia && !trace_managed_stadia && !trace_managed_control) Log(discover_stadia ?
        "STADIA discovery-assisted: scoped supported UUID16 scan, exact native target cache observation, verified stop, then one managed connect. No aliases/lease/raw cancel. Pending => reboot.\n" :
        "STADIA managed-only: saved target, no scan/lease/raw cancel. Connected target: query cached HID metadata, then managed disconnect. Pending => reboot, no retry.\n");
    if (missioncontrol::probe::ConnectionDiagnosticsPaused)
        Log(managed_stadia ? "Legacy leased/raw-cancel modes remain PAUSED. This dedicated managed-only attempt never arms that worker.\n" :
                            "Connection diagnostics PAUSED locally after system sluggishness; status does not validate cancellation safety.\n");
    Log(managed_stadia ? "Use Joy-Con: A=start, PLUS=stop observation. Unconfirmed pending state requires full reboot, not cancellation/exit.\n" :
                         "Use Joy-Con: A=start, PLUS=cancel/exit.\n");
    if (connect_control) Log(cancel_control ?
        "Android cancellation control: select advertiser into RAM, then STOP advertising before second A. One connection+immediate stop, no retries.\n" :
        "Android control: diagnostic UUID128 in primary data; two physical A gates, one owned connection, no stored target.\n");
    if (inspect_control) Log("Android scan-only batch; keep the advertiser ON. No connection tests.\n");
    if (event_observation) Log("Read-only event progress from diagnostic module cmd11 protocol3 buffered transport. No advertiser needed.\n");
    if (service_latency) Log("Read-only MC/BTM IPC timing. No advertiser needed; BEGIN is logged before every timed request.\n");
    if (!discovery_requests && !managed_stadia && !g_console_mode && !connect_control && !inspect_control && !service_latency && !event_observation) Log(control ? "Synthetic-service control; Stadia is not needed. Up to two 30-second scans.\n" :
                  connect_file ? "Owned connection diagnostic: reserved target only; disconnect Stadia from Mac first.\n" :
                  "Disconnect Stadia from Mac first.\n");
    if (!g_log) Log("Cannot open log file. Use nxlink output or a screenshot instead.\n");
    if (g_console_mode) {
        Log("Foreground diagnostics, guarded SD and approved Bluetooth-code exports. No shell or raw NAND access.\n");
        Log("Press fresh A to authorize this diagnostic/code-export session. PLUS exits locally.\n");
        if (g_network < 0) { Log("No console host connection; no scan will run.\n"); g_running = false; }
    }
    bool start = false;
    missioncontrol::probe::StartGate start_gate;
    while (Pump()) {
        if (start_gate.Update((padGetButtons(&g_pad) & HidNpadButton_A) != 0,
                              (padGetButtonsDown(&g_pad) & HidNpadButton_A) != 0)) {
            start = true;
            break;
        }
    }
    if (discovery_requests && start) RunDiscoveryRequests(argv[2]);
    if (trace_managed_control && start) RunManagedConnection(nullptr,
        canonical_control ? ManagedDiscovery::AndroidControlCanonical : ManagedDiscovery::AndroidControl);
    else if (managed_stadia && start) RunManagedConnection(argv[2],
        discover_stadia ? ManagedDiscovery::SavedStadia : ManagedDiscovery::None, trace_managed_stadia);
    if (g_console_mode && start) RunRemoteSession();
    if (inspect_control && start) RunControlInspection(inspect_hid);
    if (service_latency && start) RunServiceLatency();
    if (event_observation && start) RunEventObservation();
    bool connection_requested = connect_file || connect_control;
    for (unsigned pass = 0; !discovery_requests && !g_console_mode && !inspect_control && !service_latency && !event_observation && !managed_stadia && start && g_running && g_scan_cleanup_confirmed && pass < (control ? 2u : 1u); ++pass) {
        Probe probe;
        probe.require_bridge = bridge_control || list_control;
        probe.require_learned_format = list_control;
        probe.require_fresh_hid_control = list_control;
        probe.status_only = status_only;
        if (list_control) {
            probe.mode = pass == 0 ? ScanMode::ControlHid16BigEndian : ScanMode::ControlHid16IncompleteBigEndian;
            Log("HID list control pass %u/2: %s. Same Mac HID16-only advertiser for both.\n",
                pass + 1, pass == 0 ? "COMPLETE 0x03 baseline" : "INCOMPLETE 0x02 comparison");
        }
        else if (bridge_control) probe.mode = pass == 0 ? ScanMode::ControlLittleEndian : ScanMode::ControlHid16BigEndian;
        else if (hid_control) probe.mode = pass == 0 ? ScanMode::ControlHid16LittleEndian : ScanMode::ControlHid16BigEndian;
        else if (control) probe.mode = pass == 0 ? ScanMode::ControlLittleEndian : ScanMode::ControlCanonical;
        bool arguments_ok = status_only || control || connection_requested || argc <= 1;
        if ((argc == 3 || argc == 4) && std::strcmp(argv[1], "--target") == 0) {
            std::array<std::uint8_t, 6> target{};
            arguments_ok = missioncontrol::probe::ParseTargetAddress(argv[2], std::strlen(argv[2]), &target);
            if (arguments_ok) {
                std::memcpy(probe.target.address, target.data(), target.size());
                probe.target_from_args = true;
                if (argc == 4) {
                    if (std::strcmp(argv[3], "--patched-be") == 0) probe.mode = ScanMode::PatchedBigEndian;
                    else if (std::strcmp(argv[3], "--patched-le") == 0) probe.mode = ScanMode::PatchedLittleEndian;
                    else if (std::strcmp(argv[3], "--patched-incomplete-be") == 0) probe.mode = ScanMode::PatchedIncompleteBigEndian;
                    else if (std::strcmp(argv[3], "--connect") == 0) connection_requested = true;
                    else arguments_ok = false;
                }
            }
        }
        if (arguments_ok && connection_requested) {
            // One bounded owned-connection attempt; the scan Probe is unused.
            ConnectionProbe connection;
            connection.target = probe.target;
            connection.target_from_args = probe.target_from_args;
            connection.select_control = connect_control;
            connection.cancel_control = cancel_control;
            connection.Run();
        } else if (arguments_ok) {
            probe.Run();
        } else {
            Log("Invalid launch arguments. No Bluetooth commands sent.\n");
        }
        // Calibration must succeed before testing the bridge. Each destructor
        // stops its scan before another pass; unconfirmed cleanup prevents reuse.
        if ((bridge_control || list_control) ? !probe.discovered : probe.discovered) break;
    }
    Log("Finished. Log: %s\n", LogPath);
    if (g_running) {
        Log("Press PLUS to exit.\n");
        while (Pump()) { }
    }
    if (g_log) std::fclose(g_log);
    if (g_network >= 0) close(g_network);
    if (sockets) socketExit();
    consoleExit(nullptr);
    return 0;
}
