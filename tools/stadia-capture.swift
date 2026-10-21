#!/usr/bin/env swift
// Read-only macOS HID capture. Never opens unrelated HID devices or sends output reports.
import Foundation
import IOKit.hid

func diagnostic(_ message: String) {
    FileHandle.standardError.write(Data((message + "\n").utf8))
}

func hex(_ data: Data) -> String {
    data.map { String(format: "%02x", $0) }.joined()
}

func emit(_ fields: [String: Any]) {
    do {
        let data = try JSONSerialization.data(withJSONObject: fields, options: [.sortedKeys])
        FileHandle.standardOutput.write(data)
        FileHandle.standardOutput.write(Data([10]))
    } catch {
        diagnostic("Cannot encode capture record: \(error)")
        exit(1)
    }
}

let arguments = Array(CommandLine.arguments.dropFirst())
if arguments == ["--help"] {
    print("""
    Usage: swift tools/stadia-capture.swift > /tmp/stadia-capture.jsonl

    Pair ONE Stadia controller to this Mac over Bluetooth and unplug its USB cable.
    Follow the timed prompts on stderr. Each control starts and ends at rest.
    Captures only Google Stadia (18d1:9400) Bluetooth HID devices; no keyboard,
    serial number, Bluetooth address, pairing key or output report is recorded.
    Requires Apple's command-line tools. If access is denied, check Terminal's
    Input Monitoring permission in System Settings > Privacy & Security.
    --self-test validates formatting without opening any devices.
    """)
    exit(0)
}
if arguments == ["--self-test"] {
    precondition(hex(Data([0, 1, 15, 128, 255])) == "00010f80ff")
    emit(["event": "self_test", "ok": true])
    exit(0)
}
guard arguments.isEmpty else {
    diagnostic("Unknown arguments. Use --help.")
    exit(2)
}

final class Capture {
    var phase = "waiting"
    var devices = Set<IOHIDDevice>()
    var lastReports: [UInt32: Data] = [:]
    var reportCount = 0
    let start = ProcessInfo.processInfo.systemUptime

    func attached(_ device: IOHIDDevice) {
        devices.insert(device)
        let transport = IOHIDDeviceGetProperty(device, kIOHIDTransportKey as CFString) as? String ?? "unknown"
        var fields: [String: Any] = [
            "event": "device", "vid": 0x18d1, "pid": 0x9400,
            "transport": transport
        ]
        if let descriptor = IOHIDDeviceGetProperty(device, kIOHIDReportDescriptorKey as CFString) as? Data {
            fields["report_descriptor_hex"] = hex(descriptor)
        }
        emit(fields)
        diagnostic("Stadia HID connected (\(transport)).")
    }

    func report(_ result: IOReturn, _ type: IOHIDReportType, _ id: UInt32,
                _ bytes: UnsafeMutablePointer<UInt8>, _ count: Int) {
        guard result == kIOReturnSuccess, type == kIOHIDReportTypeInput,
              count > 0, count <= 65536 else { return }
        let data = Data(bytes: bytes, count: count)
        guard lastReports[id] != data else { return }
        lastReports[id] = data
        reportCount += 1
        emit(["event": "report", "phase": phase, "report_id": id,
              "length": count, "hex": hex(data),
              "seconds": ProcessInfo.processInfo.systemUptime - start])
    }
}

func runFor(_ seconds: TimeInterval) {
    let end = Date(timeIntervalSinceNow: seconds)
    while end.timeIntervalSinceNow > 0 {
        RunLoop.current.run(until: min(end, Date(timeIntervalSinceNow: 0.05)))
    }
}

let capture = Capture()
let context = Unmanaged.passUnretained(capture).toOpaque()
let manager = IOHIDManagerCreate(kCFAllocatorDefault, IOOptionBits(kIOHIDOptionsTypeNone))
// macOS versions expose BLE HID as either Bluetooth or Bluetooth Low Energy.
let matching = ["Bluetooth", "Bluetooth Low Energy"].map { transport in
    [kIOHIDVendorIDKey: 0x18d1, kIOHIDProductIDKey: 0x9400,
     kIOHIDTransportKey: transport] as [String: Any]
}
IOHIDManagerSetDeviceMatchingMultiple(manager, matching as CFArray)
IOHIDManagerRegisterDeviceMatchingCallback(manager, { context, result, _, device in
    guard let context, result == kIOReturnSuccess else { return }
    Unmanaged<Capture>.fromOpaque(context).takeUnretainedValue().attached(device)
}, context)
IOHIDManagerRegisterDeviceRemovalCallback(manager, { context, _, _, device in
    guard let context else { return }
    let capture = Unmanaged<Capture>.fromOpaque(context).takeUnretainedValue()
    capture.devices.remove(device)
    capture.lastReports.removeAll()
    emit(["event": "disconnected", "phase": capture.phase])
    diagnostic("Stadia disconnected. Capture will fail if it does not reconnect.")
}, context)
IOHIDManagerRegisterInputReportCallback(manager, { context, result, _, type, id, report, count in
    guard let context else { return }
    Unmanaged<Capture>.fromOpaque(context).takeUnretainedValue().report(result, type, id, report, count)
}, context)
IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), CFRunLoopMode.defaultMode.rawValue)
let result = IOHIDManagerOpen(manager, IOOptionBits(kIOHIDOptionsTypeNone))
guard result == kIOReturnSuccess else {
    diagnostic(String(format: "Cannot open Stadia HID: 0x%08x. Check Input Monitoring permission.", result))
    exit(1)
}

emit(["event": "capture_start", "format_version": 1, "source": "macOS IOHID (not raw GATT)"])
diagnostic("Waiting up to 30 seconds for a Bluetooth Stadia controller. USB must be unplugged.")
let deadline = Date(timeIntervalSinceNow: 30)
while capture.devices.isEmpty && deadline.timeIntervalSinceNow > 0 { runFor(0.1) }
guard !capture.devices.isEmpty else {
    diagnostic("No Bluetooth Stadia HID found (18d1:9400). Confirm pairing and permissions.")
    IOHIDManagerClose(manager, IOOptionBits(kIOHIDOptionsTypeNone))
    exit(1)
}

let phases: [(String, String, TimeInterval)] = [
    ("neutral", "Leave every control at rest.", 5),
    ("a", "Press and release A only, twice.", 4),
    ("b", "Press and release B only, twice.", 4),
    ("x", "Press and release X only, twice.", 4),
    ("y", "Press and release Y only, twice.", 4),
    ("dpad_up", "Press and release D-pad UP only.", 4),
    ("dpad_right", "Press and release D-pad RIGHT only.", 4),
    ("dpad_down", "Press and release D-pad DOWN only.", 4),
    ("dpad_left", "Press and release D-pad LEFT only.", 4),
    ("l1", "Press and release L1 only.", 4),
    ("r1", "Press and release R1 only.", 4),
    ("l2", "Slowly squeeze L2 fully, then release.", 5),
    ("r2", "Slowly squeeze R2 fully, then release.", 5),
    ("l3", "Click and release the LEFT stick without tilting it.", 4),
    ("r3", "Click and release the RIGHT stick without tilting it.", 4),
    ("menu", "Press and release MENU (three lines) only.", 4),
    ("options", "Press and release OPTIONS (three dots) only.", 4),
    ("assistant", "Press and release ASSISTANT only.", 4),
    ("capture", "Press and release CAPTURE only.", 4),
    ("home", "Briefly press and release STADIA only. Do not hold it.", 4),
    ("left_stick", "LEFT stick: full left, right, up, down, then center.", 8),
    ("right_stick", "RIGHT stick: full left, right, up, down, then center.", 8),
    ("neutral_end", "Release all controls and leave at rest.", 3)
]
for (name, instruction, seconds) in phases {
    capture.phase = name
    capture.lastReports.removeAll()
    emit(["event": "phase", "name": name, "duration_seconds": seconds])
    diagnostic("\n[\(name), \(Int(seconds))s] \(instruction)")
    runFor(seconds)
    guard !capture.devices.isEmpty else {
        diagnostic("Controller disconnected; reconnect and repeat the capture.")
        exit(1)
    }
}
IOHIDManagerClose(manager, IOOptionBits(kIOHIDOptionsTypeNone))
IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(), CFRunLoopMode.defaultMode.rawValue)
emit(["event": "capture_end", "reports": capture.reportCount])
guard capture.reportCount > 0 else {
    diagnostic("No input reports received. Check Input Monitoring permission; capture is incomplete.")
    exit(1)
}
diagnostic("Capture complete. Share the JSONL file; do not share Bluetooth pairing databases or keys.")
