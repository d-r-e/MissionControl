// Passive macOS BLE advertisement inspection; never connects to a peripheral.
import Foundation
import CoreBluetooth

func diagnostic(_ message: String) {
    FileHandle.standardError.write(Data((message + "\n").utf8))
}

func emit(_ fields: [String: Any]) {
    do {
        let data = try JSONSerialization.data(withJSONObject: fields, options: [.sortedKeys])
        FileHandle.standardOutput.write(data)
        FileHandle.standardOutput.write(Data([10]))
    } catch {
        diagnostic("Cannot encode advertisement summary: \(error)")
        exit(1)
    }
}

func summary(localName: String?, cachedName: String?, services: [CBUUID],
             manufacturer: Data?, connectable: NSNumber?) -> [String: Any]? {
    guard [localName, cachedName].compactMap({ $0 }).contains(where: {
        $0.lowercased().hasPrefix("stadia")
    }) else { return nil }
    var fields: [String: Any] = [
        "event": "stadia_advertisement", "name_matches_stadia": true,
        "advertised_service_uuids": services.map { $0.uuidString }.sorted(),
        "service_uuid_byte_lengths": services.map { $0.data.count },
        "manufacturer_data_length": manufacturer?.count ?? 0
    ]
    if let connectable { fields["connectable"] = connectable.boolValue }
    if let manufacturer, manufacturer.count >= 2 {
        let bytes = Array(manufacturer.prefix(2))
        fields["company_id"] = Int(bytes[0]) | (Int(bytes[1]) << 8)
    }
    // No peripheral UUID, address, full name or manufacturer payload is recorded.
    return fields
}

let arguments = Array(CommandLine.arguments.dropFirst())
if arguments == ["--help"] {
    print("""
    Passive Stadia BLE advertisement capture. Does not pair or connect.
    Compile with stadia-advertisement.Info.plist embedded (see tools/README.md).
    Run while Stadia is disconnected and blinking orange; grant Bluetooth access
    if macOS asks. Capture stops after 45 seconds. No addresses, full names,
    peripheral identifiers or raw manufacturer data are recorded.
    --self-test runs formatting/filtering tests without accessing Bluetooth.
    """)
    exit(0)
}
if arguments == ["--self-test"] {
    precondition(summary(localName: "Keyboard", cachedName: nil, services: [], manufacturer: nil, connectable: nil) == nil)
    let fields = summary(localName: "Stadia test", cachedName: nil, services: [CBUUID(string: "1812")],
                         manufacturer: Data([0xe0, 0, 42]), connectable: true)!
    precondition(fields["company_id"] as? Int == 224)
    precondition(fields["advertised_service_uuids"] as? [String] == ["1812"])
    precondition(fields["manufacturer_data_length"] as? Int == 3)
    precondition(fields["connectable"] as? Bool == true)
    precondition(summary(localName: nil, cachedName: "STADIA cached", services: [], manufacturer: Data([1]), connectable: nil)?["company_id"] == nil)
    emit(["event": "self_test", "ok": true])
    exit(0)
}
guard arguments.isEmpty else { diagnostic("Unknown arguments. Use --help."); exit(2) }

final class AdvertisementCapture: NSObject, CBCentralManagerDelegate {
    var manager: CBCentralManager!
    var seen = Set<Data>()
    var failed = false
    var scanning = false

    func start() {
        manager = CBCentralManager(delegate: self, queue: .main)
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        emit(["event": "central_state", "state": central.state.rawValue])
        switch central.state {
        case .poweredOn:
            scanning = true
            central.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
            diagnostic("Scanning passively for Stadia. Leave it disconnected and blinking orange.")
        case .unauthorized:
            failed = true
            diagnostic("Bluetooth access denied. Check Privacy & Security > Bluetooth for Terminal/the capture app.")
        case .unsupported:
            failed = true
            diagnostic("CoreBluetooth is not supported on this Mac.")
        case .poweredOff:
            failed = true
            diagnostic("Mac Bluetooth is switched off.")
        default:
            break
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        guard var fields = summary(
            localName: advertisementData[CBAdvertisementDataLocalNameKey] as? String,
            cachedName: peripheral.name,
            services: advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? [],
            manufacturer: advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
            connectable: advertisementData[CBAdvertisementDataIsConnectable] as? NSNumber
        ) else { return }
        let overflow = advertisementData[CBAdvertisementDataOverflowServiceUUIDsKey] as? [CBUUID] ?? []
        fields["overflow_service_uuids"] = overflow.map { $0.uuidString }.sorted()
        let serviceData = advertisementData[CBAdvertisementDataServiceDataKey] as? [CBUUID: Data] ?? [:]
        fields["service_data_uuids"] = serviceData.keys.map { $0.uuidString }.sorted()
        guard let encoded = try? JSONSerialization.data(withJSONObject: fields, options: [.sortedKeys]),
              seen.insert(encoded).inserted else { return }
        emit(fields)
        diagnostic("Stadia advertisement observed; service summary recorded.")
    }
}

let capture = AdvertisementCapture()
diagnostic("Passive BLE capture (45 seconds). This tool will NOT connect to the controller.")
capture.start()
let deadline = Date(timeIntervalSinceNow: 45)
while !capture.failed && deadline.timeIntervalSinceNow > 0 {
    RunLoop.current.run(until: min(deadline, Date(timeIntervalSinceNow: 0.1)))
}
if capture.scanning { capture.manager.stopScan() }
emit(["event": "capture_end", "distinct_summaries": capture.seen.count, "failed": capture.failed])
if capture.failed || capture.seen.isEmpty {
    diagnostic("No confirmed Stadia advertising summary. Check pairing mode and Bluetooth permission.")
    exit(1)
}
diagnostic("Advertisement capture complete. No connection was attempted.")
