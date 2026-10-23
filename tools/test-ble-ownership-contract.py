#!/usr/bin/env python3
"""Source contracts for passive instrumentation; not a firmware/runtime IPC test."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "mc_mitm/source"


def source(path):
    return (SOURCE / path).read_text()


def body(text, name):
    start = text.index("{", text.index(name))
    depth = 1
    for pos in range(start + 1, len(text)):
        depth += (text[pos] == "{") - (text[pos] == "}")
        if depth == 0:
            return text[start + 1:pos]
    raise AssertionError(f"unclosed body: {name}")


class OwnershipContracts(unittest.TestCase):
    def test_command_96_keeps_pinned_pointer_abi_and_version_gate(self):
        interface = source("bluetooth_mitm/btdrv_mitm_service.hpp")
        line = next(line for line in interface.splitlines()
                    if "AMS_SF_METHOD_INFO(C, H, 96," in line)
        self.assertIn("ObserveLeHidEventInfo", line)
        self.assertIn("sf::Out<ams::bluetooth::BleEventType>", line)
        self.assertIn("const sf::OutPointerBuffer &out_buffer", line)
        self.assertIn("hos::Version_5_1_0", line)

    def test_observer_never_consumes_slot_or_writes_response(self):
        observer = body(source("bluetooth_mitm/btdrv_mitm_service.cpp"),
                        "BtdrvMitmService::ObserveLeHidEventInfo(")
        # Deliberately restrict this callback to observation plus the forwarding sentinel.
        calls = re.findall(r"([\w:]+)\s*\(", observer)
        self.assertEqual(calls.count("ams::ble::diagnostics::Note"), 2)
        for name in calls:
            self.assertIn(name, {"AMS_UNUSED", "if", "ams::ble::diagnostics::Note",
                                 "R_THROW", "sm::mitm::ResultShouldForwardToSession"})
        self.assertIn("R_THROW(sm::mitm::ResultShouldForwardToSession());", observer)
        self.assertNotIn("GetPointer", observer)
        self.assertNotIn("SetValue", observer)

    def test_registration_retains_placeholder_and_disabled_forwarding(self):
        registration = body(source("bluetooth_mitm/btdrv_mitm_service.cpp"),
                            "BtdrvMitmService::RegisterBleHidEvent(")
        self.assertIn("if (!ams::ble::IsEnabled())", registration)
        self.assertIn("R_THROW(sm::mitm::ResultShouldForwardToSession());", registration)
        self.assertIn("out_handle.SetValue(g_le_hid_placeholder_event.GetReadableHandle(), false);",
                      registration)
        self.assertNotIn("btdrvRegisterBleHidEvent(", registration)
        self.assertNotIn("log::Write", registration)

    def test_production_recording_is_gated_without_logger_readiness(self):
        diagnostics = source("ble/ble_diagnostics.cpp")
        self.assertIn("constinit Counters g_counters;", diagnostics)
        self.assertIn("ams::ble::IsEnabled() && mitm::GetGlobalConfig()->ble.log",
                      body(diagnostics, "bool Enabled("))
        note = body(diagnostics, "void Note(")
        self.assertEqual(note.strip(), "g_counters.Record(metric, Enabled());")
        self.assertNotIn("log::", note)

    def test_input_timing_is_observation_only(self):
        host = source("ble/ble_hid_host.cpp")
        listener = body(host, "void LeHidThreadFunc(")
        self.assertEqual(listener.count("btdrvGetLeHidEventInfo("), 1)
        self.assertNotIn("DumpInputTiming(", listener)
        self.assertIn("if (events::IsDisconnected() && events::IsRadioEnabled() && !events::IsShuttingDown()) diagnostics::DumpInputTiming();", host)
        self.assertNotIn("DumpInputTiming(", body(host, "Result DeliverInput("))
        timing = source("ble/ble_diagnostics.cpp")
        record = body(timing, "void RecordFifoWrite(")
        self.assertNotIn("log::", record)
        self.assertNotIn("log::", body(timing, "void RecordInputDuration("))
        bank = source("ble/ble_input_timing.hpp")
        for forbidden in ["log::", "mutex", "new ", "malloc", "sleep", "fstream"]:
            self.assertNotIn(forbidden, bank)
        writer = body(source("bluetooth_mitm/bluetooth/bluetooth_hid_report.cpp"), "void WriteHidDataReport(")
        self.assertEqual(writer.count("g_fake_buffer->Write("), 1)
        self.assertEqual(writer.count("g_system_event_fwd.Signal()"), 1)
        self.assertNotIn("DiscardOldPackets(", writer)
        self.assertNotIn("->Read(", writer)
        self.assertIn("constexpr s32 ThreadPriority = 9;", host)

    def test_stadia_menu_mapping_and_tester_sampling_order(self):
        apply = body(source("controllers/stadia_controller.cpp"), "void StadiaController::ApplyInput(")
        self.assertIn("SwitchButton::Plus,    report.IsPressed(StadiaInputReport::Menu)", apply)
        self.assertIn("SwitchButton::Minus,   report.IsPressed(StadiaInputReport::Options)", apply)
        tester = (ROOT / "tools/stadia-input-test/source/main.cpp").read_text()
        self.assertLess(tester.index("framebufferBegin(&fb,"), tester.index("padUpdate(&pad)"))
        self.assertLess(tester.index("framebufferEnd(&fb)"), tester.index("if(quit)break"))

    def test_parked_shoulder_check_uses_identified_pad_kind(self):
        listener = body(source("ble/ble_hid_host.cpp"), "void LeHidThreadFunc(")
        self.assertIn("pad::HasLrShoulders(kind, notify.data, size)", listener)
        self.assertNotIn("ReportHasLbRb(", listener)
        self.assertIn("if (parked)", listener)

    def test_security_first_order_retains_only_the_current_links_cache(self):
        host = body(source("ble/ble_hid_host.cpp"), "void HostThreadFunc(")
        order = [
            "events::SetClaimedAddress(&address)",
            "gatt::PrepareDiscovery(address)",
            "if (!Pair(address))",
            "gatt::Discover(conn_id, address)",
            "btdrvRegisterGattHidDataPath(&hid_uuid)",
            "gatt::Subscribe(conn_id, &hid)",
            "gatt::Describe(conn_id, hid, &facts)",
            "Present(conn_id, address, hid, policy)",
        ]
        offsets = [host.index(statement) for statement in order]
        self.assertEqual(offsets, sorted(offsets))
        self.assertEqual(host.count("Pair(address)"), 1)
        self.assertEqual(host.count("gatt::PrepareDiscovery(address)"), 1)
        # The extracted retry gate must precede connection attempts in the full host as well.
        self.assertLess(host.index("if (g_setup_failed)"), host.index("Connect(address,"))
        prepare = body(source("ble/ble_gatt.cpp"), "void PrepareDiscovery(")
        self.assertIn("g_cache.Prepare(g_table, address);", prepare)
        self.assertNotIn("btdrv", prepare)
        cache = source("ble/ble_gatt_cache.hpp")
        self.assertIn("table.count = 0;", body(cache, "void Prepare("))
        self.assertIn("table.address = address;", body(cache, "void Prepare("))
        discover = body(source("ble/ble_gatt.cpp"), "Result Discover(")
        self.assertIn("if (!cached_during_bond)", discover)
        self.assertNotIn("g_table.count = 0", discover)
        self.assertNotIn("g_replace_pending = true", discover)
        sink = body(source("ble/ble_gatt.cpp"), "void OnEvent(")
        self.assertIn("if (e.type != BtdrvBleEventType_ClientCacheSave)", sink)
        self.assertIn("g_cache.Append(g_table, e.info.client_cache_save.conn_id, events::GetConnection(),", sink)
        self.assertIn("g_cache.Complete();", discover)
        subscribe = body(source("ble/ble_gatt.cpp"), "Result Subscribe(")
        self.assertIn("if (g_cache.Unusable())", subscribe)
        setup = host[host.index("Result rc = gatt::Discover("):]
        guard = body(setup, "if (R_FAILED(rc))")
        self.assertLess(setup.index("if (R_FAILED(rc))"), setup.index("btdrvRegisterGattHidDataPath("))
        self.assertIn('StopAfterSetupFailure("GATT discovery cache")', guard)
        self.assertIn("Disconnect();", guard)
        self.assertIn("continue;", guard)
        self.assertIn("e.info.client_cache_save.result", sink)
        self.assertGreaterEqual(subscribe.count("!g_cache.Unusable()"), 4)
        self.assertIn("if (g_cache.Unusable() || !CanRequestGatt(conn_id)) break;", body(source("ble/ble_gatt.cpp"), "Result Read("))

    def test_listener_and_read_do_not_add_requests(self):
        listener = body(source("ble/ble_hid_host.cpp"), "void LeHidThreadFunc(")
        self.assertEqual(listener.count("btdrvGetLeHidEventInfo(&"), 1)
        self.assertEqual(listener.count("btdrvRegisterBleHidEvent(&"), 1)
        read = body(source("ble/ble_gatt.cpp"), "Result Read(")
        self.assertEqual(read.count("btdrvReadGattCharacteristic("), 1)
        self.assertIn("BtdrvGattAuthReqType_NoMitm", read)
        self.assertIn("const ReadDeadline deadline(", read)
        self.assertIn("if (deadline.Expired(", read)
        self.assertIn("CanRequestGatt(conn_id)", read)
        available = body(source("ble/ble_gatt.cpp"), "bool CanRequestGatt(")
        for check in ["events::IsRadioEnabled()", "events::IsDisconnected()", "events::IsShuttingDown()", "events::GetConnection() == conn_id"]:
            self.assertIn(check, available)
        self.assertIn('diagnostics::Dump("read-begin")', read)
        self.assertIn('diagnostics::Dump("read-incomplete")', read)


if __name__ == "__main__":
    unittest.main()
