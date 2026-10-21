// SPDX-License-Identifier: GPL-2.0-only
// Metadata-only CoreBluetooth capability test. No scans, pairing APIs, writes
// or subscriptions. Own one app connection reference, not the OS HID link.
import Foundation
import CoreBluetooth

func emit(_ fields: [String: Any]) {
    guard let data = try? JSONSerialization.data(withJSONObject: fields, options: [.sortedKeys]) else { exit(2) }
    FileHandle.standardOutput.write(data); FileHandle.standardOutput.write(Data([10]))
}
func diagnostic(_ text: String) { FileHandle.standardError.write(Data((text + "\n").utf8)) }
func stadiaHint(_ name: String?) -> Bool {
    guard let name, name.utf8.count <= 248 else { return false }
    return name.lowercased().hasPrefix("stadia")
}
func safeError(_ error: Error?) -> [String: Any] {
    guard let error else { return [:] }
    let e = error as NSError
    let allowed = ["CBErrorDomain", "CBATTErrorDomain", "NSPOSIXErrorDomain", "NSCocoaErrorDomain"]
    return ["error_domain": allowed.contains(e.domain) ? e.domain : "other", "error_code": e.code]
}
struct Ownership {
    private(set) var attempted = false
    private(set) var owns = false
    private(set) var cancelIssued = false
    private(set) var retired = false
    mutating func begin() -> Bool {
        guard !attempted && !retired else { return false }
        attempted = true; owns = true; return true
    }
    mutating func cancelOnce() -> Bool {
        guard owns && !cancelIssued && !retired else { return false }
        cancelIssued = true; return true
    }
    mutating func terminal() -> Bool {
        guard owns && !retired else { return false }
        owns = false; retired = true; return true
    }
}
enum Phase: Equatable { case waiting, connecting, services, characteristics, map, descriptors(Int), reference(Int), closing, done }

final class Capability: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    var central: CBCentralManager!
    var peer: CBPeripheral?
    var hid: CBService?
    var map: CBCharacteristic?
    var reports: [CBCharacteristic] = []
    var pendingReference: CBDescriptor?
    var ownership = Ownership()
    var phase = Phase.waiting
    var started = false
    var finishing = false
    var done = false
    var exitCode: Int32 = 1
    let startedAt = ProcessInfo.processInfo.systemUptime
    var deadline = ProcessInfo.processInfo.systemUptime + 15
    var reason = "not_started"
    var success = false
    var mapValidated = false
    var mapBytes: Data?
    var seenReferences = Set<Int32>()
    var referenceCount = 0
    var inputCount = 0

    func start() { central = CBCentralManager(delegate: self, queue: .main, options: [CBCentralManagerOptionShowPowerAlertKey: false]) }
    func own(_ p: CBPeripheral) -> Bool { p === peer && ownership.owns && !done }
    func errorEvent(_ event: String, _ error: Error?) {
        var fields = safeError(error); fields["event"] = event; emit(fields)
    }
    func complete(_ cleanup: Bool) {
        guard !done else { return }
        done = true; phase = .done
        emit(["event": "capability_complete", "reason": reason,
              "raw_gatt_metadata_verified": success && mapValidated && inputCount > 0,
              "map_matches_stadia_layout": mapValidated, "references_read": referenceCount,
              "input_references": inputCount, "input_notifications_verified": false,
              "cleanup_confirmed": cleanup, "cleanup_scope": "this_app_connection_reference",
              "connect_requests": ownership.attempted ? 1 : 0, "cancel_requests": ownership.cancelIssued ? 1 : 0])
        exitCode = success && cleanup ? 0 : 1
        peer?.delegate = nil; central?.delegate = nil
        pendingReference = nil; reports = []; map = nil; hid = nil; peer = nil
        mapBytes = nil; seenReferences = []
    }
    func finish(_ why: String, success ok: Bool = false) {
        guard !done && !finishing else { return }
        finishing = true; reason = why; success = ok; phase = .closing
        emit(["event": "finishing", "reason": why])
        if !ownership.owns { complete(true); return }
        deadline = ProcessInfo.processInfo.systemUptime + 8
        // Only our submitted CB connection can be cancelled, exactly once.
        // Do not infer retirement from CBPeripheral.state or request acceptance.
        if central.state == .poweredOn, let peer, ownership.cancelOnce() {
            central.cancelPeripheralConnection(peer)
        }
    }
    func checkDeadline() {
        guard !done else { return }
        let now = ProcessInfo.processInfo.systemUptime
        if finishing {
            if now >= deadline { complete(false) }
        } else if now >= deadline || now - startedAt >= 45 {
            finish("operation_timeout")
        }
    }
    func nextReference(_ index: Int) {
        guard let peer, !finishing else { return }
        if index == reports.count {
            finish(inputCount > 0 ? "metadata_reads_complete" : "no_valid_stadia_input_reference", success: inputCount > 0)
            return
        }
        pendingReference = nil; phase = .descriptors(index)
        deadline = ProcessInfo.processInfo.systemUptime + 8
        peer.discoverDescriptors(for: reports[index])
    }
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        emit(["event": "central_state", "state": central.state.rawValue])
        guard !done else { return }
        guard central.state == .poweredOn else {
            if [.unauthorized, .unsupported, .poweredOff].contains(central.state) { finish("bluetooth_unavailable") }
            return
        }
        guard !started && !finishing else { return }
        started = true
        // No automatic discovery scan or cached-peer/MAC substitution. Selection
        // is one Stadia-named system-connected peer, not authenticated VID/PID.
        let retrieved = central.retrieveConnectedPeripherals(withServices: [CBUUID(string: "1812"), CBUUID(string: "180A"), CBUUID(string: "180F")])
        guard retrieved.count < 64 else { finish("connected_list_capacity_exceeded"); return }
        var unique: [ObjectIdentifier: CBPeripheral] = [:]
        for p in retrieved where stadiaHint(p.name) { unique[ObjectIdentifier(p)] = p }
        emit(["event": "candidate_selection", "matching_candidates": unique.count,
              "selection": "single_stadia_named_system_peer", "identity_authenticated": false])
        guard unique.count == 1, let selected = unique.values.first else {
            finish(unique.isEmpty ? "connected_candidate_unavailable_inconclusive" : "ambiguous_stadia_peers"); return
        }
        guard ownership.begin() else { finish("duplicate_connect_refused"); return }
        peer = selected; selected.delegate = self; phase = .connecting
        deadline = ProcessInfo.processInfo.systemUptime + 12
        central.connect(selected, options: nil)
    }
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard own(peripheral), !finishing, phase == .connecting else { return }
        emit(["event": "app_connection_acknowledged"])
        phase = .services; deadline = ProcessInfo.processInfo.systemUptime + 8
        peripheral.discoverServices(nil)
    }
    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        guard own(peripheral), ownership.terminal() else { return }
        errorEvent("app_connection_failed", error)
        if !finishing { reason = "app_connection_failed"; success = false }
        complete(true)
    }
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        guard own(peripheral), ownership.terminal() else { return }
        errorEvent("app_connection_reference_ended", error)
        if !finishing { reason = "link_ended_before_metadata_complete"; success = false }
        complete(true)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard own(peripheral), !finishing, phase == .services else { return }
        if error != nil { errorEvent("service_discovery_failed", error); finish("service_discovery_failed"); return }
        let services = peripheral.services ?? []
        guard services.count < 64 else { finish("service_capacity_exceeded"); return }
        let hids = services.filter { $0.uuid == CBUUID(string: "1812") && $0.peripheral === peripheral }
        emit(["event": "services", "count": services.count, "hid_service_visible": !hids.isEmpty,
              "visibility_scope": "this_corebluetooth_client_only"])
        guard hids.count == 1 else { finish(hids.isEmpty ? "hid_service_not_exposed_to_client" : "ambiguous_hid_services"); return }
        hid = hids[0]; phase = .characteristics; deadline = ProcessInfo.processInfo.systemUptime + 8
        peripheral.discoverCharacteristics(nil, for: hids[0])
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard own(peripheral), !finishing, phase == .characteristics, service === hid else { return }
        if error != nil { errorEvent("characteristic_discovery_failed", error); finish("characteristic_discovery_failed"); return }
        let chars = service.characteristics ?? []
        guard chars.count <= 32 else { finish("characteristic_capacity_exceeded"); return }
        let maps = chars.filter { $0.uuid == CBUUID(string: "2A4B") && $0.service === service }
        reports = chars.filter { $0.uuid == CBUUID(string: "2A4D") && $0.service === service }
        emit(["event": "hid_catalog", "map_instances": maps.count, "report_instances": reports.count])
        guard maps.count == 1, !reports.isEmpty, reports.count <= 16, maps[0].properties.contains(.read) else {
            finish("required_hid_metadata_unavailable"); return
        }
        map = maps[0]; phase = .map; deadline = ProcessInfo.processInfo.systemUptime + 8
        peripheral.readValue(for: maps[0])
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard own(peripheral), !finishing, phase == .map, characteristic === map else { return }
        if error != nil { errorEvent("report_map_read_failed", error); finish("report_map_read_failed"); return }
        guard let data = characteristic.value, !data.isEmpty, data.count <= Int(mc_stadia_max_map_bytes()) else {
            finish("report_map_size_invalid"); return
        }
        mapValidated = data.withUnsafeBytes { mc_stadia_validate_map($0.bindMemory(to: UInt8.self).baseAddress, data.count) == 1 }
        emit(["event": "report_map", "bytes": data.count, "matches_stadia_input_layout": mapValidated])
        guard mapValidated else { finish("report_map_not_stadia_input_layout"); return }
        mapBytes = data
        nextReference(0)
    }
    func peripheral(_ peripheral: CBPeripheral, didDiscoverDescriptorsFor characteristic: CBCharacteristic, error: Error?) {
        guard own(peripheral), !finishing, case let .descriptors(index) = phase,
              index < reports.count, characteristic === reports[index] else { return }
        if error != nil { errorEvent("descriptor_discovery_failed", error); finish("descriptor_discovery_failed"); return }
        let descriptors = characteristic.descriptors ?? []
        guard descriptors.count <= 16 else { finish("descriptor_capacity_exceeded"); return }
        let refs = descriptors.filter { $0.uuid == CBUUID(string: "2908") && $0.characteristic === characteristic }
        guard refs.count == 1 else { finish("report_reference_missing_or_ambiguous"); return }
        pendingReference = refs[0]; phase = .reference(index); deadline = ProcessInfo.processInfo.systemUptime + 8
        peripheral.readValue(for: refs[0])
    }
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor descriptor: CBDescriptor, error: Error?) {
        guard own(peripheral), !finishing, case let .reference(index) = phase,
              index < reports.count, descriptor === pendingReference else { return }
        if error != nil { errorEvent("reference_read_failed", error); finish("reference_read_failed"); return }
        guard let data = descriptor.value as? Data, data.count == 2 else { finish("reference_value_format_invalid"); return }
        let packed = data.withUnsafeBytes { mc_hogp_parse_reference($0.bindMemory(to: UInt8.self).baseAddress, data.count) }
        guard packed >= 0, let mapData = mapBytes else { finish("reference_value_invalid"); return }
        let size = mapData.withUnsafeBytes { mapBuffer in
            data.withUnsafeBytes { refBuffer in
                mc_hogp_reference_size(mapBuffer.bindMemory(to: UInt8.self).baseAddress, mapData.count,
                                       refBuffer.bindMemory(to: UInt8.self).baseAddress, data.count)
            }
        }
        guard size > 0, seenReferences.insert(packed).inserted else {
            finish("reference_missing_from_map_or_duplicate"); return
        }
        let id = Int(packed & 255), type = Int(packed >> 8)
        let notify = reports[index].properties.contains(.notify)
        referenceCount += 1
        if id == 3 && type == 1 && notify { inputCount += 1 }
        emit(["event": "report_reference", "instance_ordinal": index, "report_id": id,
              "report_type": type, "expected_value_bytes": size, "notify_property": notify,
              "subscription_requested": false])
        nextReference(index + 1)
    }
}

let args = Array(CommandLine.arguments.dropFirst())
if args == ["--help"] {
    print("Stadia GATT metadata capability: --connected-catalog (max45s+8s cleanup).\nSelects one already system-connected Stadia-named peer; not an authenticated identity.\nOne app-owned connect reference; read services/map/report references, then cancel it once.\nNo scan, subscriptions, writes, explicit pairing, OS link reset or device identifiers logged.\n--self-test MAP_HEX validates model/production bridge WITHOUT a Bluetooth manager.")
    exit(0)
}
if args.count == 2 && args[0] == "--self-test" {
    var policy = Ownership()
    precondition(!policy.cancelOnce() && !policy.terminal())
    precondition(policy.begin() && !policy.begin())
    precondition(policy.cancelOnce() && !policy.cancelOnce())
    precondition(policy.terminal() && !policy.terminal() && !policy.owns && !policy.begin())
    var failed = Ownership(); precondition(failed.begin() && failed.terminal() && !failed.cancelOnce())
    precondition(stadiaHint("Stadia test") && !stadiaHint("Keyboard") && !stadiaHint(nil))
    precondition(!stadiaHint(String(repeating: "s", count: 249)))
    let safe = safeError(NSError(domain: "sensitive_private_domain", code: 7, userInfo: [NSLocalizedDescriptionKey: "do_not_echo"]))
    precondition(safe["error_domain"] as? String == "other" && safe["error_code"] as? Int == 7)
    let url = URL(fileURLWithPath: args[1])
    guard let attrs = try? FileManager.default.attributesOfItem(atPath: url.path),
          let count = attrs[.size] as? NSNumber, count.intValue <= 2 * Int(mc_stadia_max_map_bytes()) + 64,
          let contents = try? String(contentsOf: url, encoding: .utf8) else { exit(1) }
    let hex = contents.trimmingCharacters(in: .whitespacesAndNewlines)
    guard hex.count % 2 == 0, hex.count <= 2 * Int(mc_stadia_max_map_bytes()) else { exit(1) }
    var data = Data(); var pos = hex.startIndex
    while pos < hex.endIndex {
        let end = hex.index(pos, offsetBy: 2)
        guard let value = UInt8(hex[pos..<end], radix: 16) else { exit(1) }
        data.append(value); pos = end
    }
    guard data.withUnsafeBytes({ mc_stadia_validate_map($0.bindMemory(to: UInt8.self).baseAddress, data.count) }) == 1 else {
        diagnostic("Self-test map fixture invalid (contents not echoed)."); exit(1)
    }
    let ref = Data([3,1]) // MODELLED self-test only, not a discovered descriptor.
    precondition(ref.withUnsafeBytes { mc_hogp_parse_reference($0.bindMemory(to: UInt8.self).baseAddress, ref.count) } == 259)
    emit(["event": "self_test", "ok": true, "bluetooth_manager_initialized": false]); exit(0)
}
guard args == ["--connected-catalog"] else { diagnostic("Use --help; no Bluetooth manager initialized."); exit(2) }
diagnostic("METADATA ONLY: one app-owned connection reference to one named system-connected Stadia.\nNo scan, GATT writes, subscriptions or explicit pairing. macOS manages security/background activity.")
let probe = Capability()
probe.start()
while !probe.done {
    RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.05))
    probe.checkDeadline()
}
exit(probe.exitCode)
