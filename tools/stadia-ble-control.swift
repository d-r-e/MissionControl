// Synthetic macOS BLE advertiser for the Switch managed-scan positive control.
// No connections requested, no GATT services/characteristics installed, no names,
// device identifiers, addresses or payloads captured. Stops after 180 seconds.
import Foundation
import CoreBluetooth

let controlUUID = "9f6dca74-69b7-4a35-9f01-9ab841ab5832"
// Canonical form of MarkerBigEndian's BLE-order bytes. Synthetic advertiser only.
let markerUUID = "9c7bc3ae-3f7a-4f14-92af-6b6d63737461"
func primaryServiceUUID(_ args: [String]) -> String {
    if args == ["--hid16-only"] { return "1812" }
    return args == ["--marker-hid16"] ? markerUUID : controlUUID
}
func emit(_ event: String, _ extra: [String: Any] = [:]) {
    var fields = extra
    fields["event"] = event
    do {
        let data = try JSONSerialization.data(withJSONObject: fields, options: [.sortedKeys])
        FileHandle.standardOutput.write(data)
        FileHandle.standardOutput.write(Data([10]))
    } catch { exit(1) }
}
let arguments = Array(CommandLine.arguments.dropFirst())
if arguments == ["--help"] {
    print("""
    Advertise a synthetic UUID128 for up to 180 seconds, then stop this tool's
    advertisement. Does not enable/disable Bluetooth, pair, request connections,
    publish GATT services, or log device identities. No local name is advertised.
    Run with Switch probe --control; Stadia is not needed. Keep the Mac nearby.
    --hid16 advertises HID UUID16 1812 alongside the UUID128 control (25 bytes
    including flags and AD headers); use probe --control-hid16.
    --hid16-only advertises only 1812 (no UUID128); use after calibration without
    rebooting the Switch, with probe --control-hid16.
    --marker-hid16 substitutes the BE private marker UUID128 alongside HID16.
    Use the same probe --control-hid16 to test a possible BTM-side UUID128 check.
    --check checks peripheral-manager readiness without advertising.
    --self-test checks constants without accessing Bluetooth.
    Compile with stadia-ble-control.Info.plist embedded; see tools/README.md.
    """)
    exit(0)
}
if arguments == ["--self-test"] {
    let uuid = CBUUID(string: controlUUID)
    precondition(uuid.data.count == 16)
    precondition(Array(uuid.data) == [0x9f,0x6d,0xca,0x74,0x69,0xb7,0x4a,0x35,0x9f,0x01,0x9a,0xb8,0x41,0xab,0x58,0x32])
    let marker = CBUUID(string: markerUUID)
    // Copy while the CBUUID owner is alive; temporary.data can outlive its backing storage.
    let markerBytes = withExtendedLifetime(marker) { Array(marker.data) }
    precondition(Array(markerBytes.reversed()) == [0x61,0x74,0x73,0x63,0x6d,0x6b,0xaf,0x92,0x14,0x4f,0x7a,0x3f,0xae,0xc3,0x7b,0x9c])
    let hid = CBUUID(string: "1812")
    precondition(withExtendedLifetime(hid) { hid.data.count } == 2)
    precondition(primaryServiceUUID(["--hid16-only"]) == "1812")
    precondition(primaryServiceUUID(["--hid16"]) == controlUUID)
    precondition(primaryServiceUUID(["--marker-hid16"]) == markerUUID)
    precondition(3 + 2 + uuid.data.count + 2 + 2 <= 31) // Flags plus both service ADs, without a name.
    emit("self_test", ["ok": true])
    exit(0)
}
guard arguments.isEmpty || arguments == ["--check"] || arguments == ["--hid16"] || arguments == ["--marker-hid16"] || arguments == ["--hid16-only"] else { emit("invalid_arguments"); exit(2) }

final class ControlAdvertiser: NSObject, CBPeripheralManagerDelegate {
    var manager: CBPeripheralManager!
    let checkOnly: Bool
    let includeHid16: Bool
    let primaryUUID: String
    var requested = false
    var confirmed = false
    var finished = false
    var failed = false

    init(checkOnly: Bool, includeHid16: Bool, primaryUUID: String) {
        self.checkOnly = checkOnly; self.includeHid16 = includeHid16
        self.primaryUUID = primaryUUID; super.init()
    }
    func start() {
        manager = CBPeripheralManager(delegate: self, queue: .main,
            options: [CBPeripheralManagerOptionShowPowerAlertKey: false])
    }
    func peripheralManagerDidUpdateState(_ peripheral: CBPeripheralManager) {
        emit("peripheral_state", ["state": peripheral.state.rawValue])
        switch peripheral.state {
        case .poweredOn:
            if checkOnly { emit("ready_without_advertising"); finished = true }
            else if !requested {
                requested = true
                // UUID only: avoid putting services in overflow due to a name.
                var services = [CBUUID(string: primaryUUID)]
                if includeHid16 { services.append(CBUUID(string: "1812")) }
                peripheral.startAdvertising([CBAdvertisementDataServiceUUIDsKey: services])
            }
        case .unauthorized, .unsupported, .poweredOff:
            failed = true
            finished = true
        case .resetting:
            if requested { failed = true; finished = true }
        default: break
        }
    }
    func peripheralManagerDidStartAdvertising(_ peripheral: CBPeripheralManager, error: Error?) {
        if let error {
            emit("advertising_failed", ["code": (error as NSError).code])
            failed = true
            finished = true
        } else {
            confirmed = true
            emit("advertising_start_accepted", ["service_uuid": primaryUUID, "hid16_requested": includeHid16 || primaryUUID == "1812", "is_advertising": peripheral.isAdvertising])
        }
    }
    func stop() {
        if requested {
            manager.stopAdvertising()
            emit("advertising_stop_requested")
        }
    }
}
let markerControl = arguments == ["--marker-hid16"]
let advertiser = ControlAdvertiser(checkOnly: arguments == ["--check"],
    includeHid16: markerControl || arguments == ["--hid16"],
    primaryUUID: primaryServiceUUID(arguments))
advertiser.start()
let deadline = Date(timeIntervalSinceNow: advertiser.checkOnly ? 10 : 180)
while !advertiser.finished && Date() < deadline {
    _ = RunLoop.current.run(mode: .default, before: Date(timeIntervalSinceNow: 0.2))
}
if !advertiser.finished && (advertiser.checkOnly || !advertiser.confirmed) {
    advertiser.failed = true
    emit("readiness_or_start_unconfirmed")
}
advertiser.stop()
exit(advertiser.failed ? 1 : 0)
