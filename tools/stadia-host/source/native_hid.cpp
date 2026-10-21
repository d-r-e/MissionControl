/* SPDX-License-Identifier: GPL-2.0-only */
#include "native_hid.hpp"
#include "decoder.hpp"
#include <chrono>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>

namespace stadia_host {
namespace {
struct Capture {
    HidDecoder decoder;
    IOHIDDeviceRef active = nullptr;
    std::ostream &out, &error;
    State last{};
    unsigned reports = 0, changes = 0, rejected = 0;
    bool failed = false, removed = false;
};
bool Number(CFMutableDictionaryRef dict, CFStringRef key, int value) {
    const auto number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    if (!number) return false;
    CFDictionarySetValue(dict, key, number);
    CFRelease(number);
    return true;
}
CFMutableDictionaryRef Match(CFStringRef transport) {
    auto dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (!dict) return nullptr;
    if (!Number(dict, CFSTR(kIOHIDVendorIDKey), 0x18d1) ||
        !Number(dict, CFSTR(kIOHIDProductIDKey), 0x9400) ||
        !Number(dict, CFSTR(kIOHIDPrimaryUsagePageKey), kHIDPage_GenericDesktop) ||
        !Number(dict, CFSTR(kIOHIDPrimaryUsageKey), kHIDUsage_GD_GamePad)) {
        CFRelease(dict); return nullptr;
    }
    CFDictionarySetValue(dict, CFSTR(kIOHIDTransportKey), transport);
    return dict;
}
void Attached(void *context, IOReturn result, void *, IOHIDDeviceRef device) {
    auto &c = *static_cast<Capture *>(context);
    if (c.failed || c.removed || result != kIOReturnSuccess) return;
    if (c.active && c.active != device) {
        c.failed = true; c.decoder.Close();
        c.error << "Multiple matching Stadia Bluetooth gamepads; refusing ambiguous input.\n";
        return;
    }
    if (c.active == device) return;
    const auto property = IOHIDDeviceGetProperty(device, CFSTR(kIOHIDReportDescriptorKey));
    if (!property || CFGetTypeID(property) != CFDataGetTypeID()) {
        c.failed = true; c.error << "OS HID descriptor unavailable; no input accepted.\n"; return;
    }
    const auto descriptor = static_cast<CFDataRef>(property);
    const auto length = CFDataGetLength(descriptor);
    if (length <= 0 || !c.decoder.Open(CFDataGetBytePtr(descriptor), static_cast<std::size_t>(length))) {
        c.failed = true; c.error << "OS HID descriptor does not validate Stadia framing.\n"; return;
    }
    c.active = device;
    c.out << "{\"event\":\"attached\",\"source\":\"macos_iohid\",\"raw_gatt_verified\":false}\n";
}
void Removed(void *context, IOReturn, void *, IOHIDDeviceRef device) {
    auto &c = *static_cast<Capture *>(context);
    if (device != c.active) return;
    c.decoder.Close(); c.active = nullptr; c.last = {}; c.removed = true;
    c.out << "{\"event\":\"removed\",\"source\":\"macos_iohid\",\"state\":";
    PrintState(c.out, c.last); c.out << "}\n";
}
void Input(void *context, IOReturn result, void *, IOHIDReportType type,
           std::uint32_t id, std::uint8_t *bytes, CFIndex count) {
    auto &c = *static_cast<Capture *>(context);
    if (!c.active || c.failed || c.removed || result != kIOReturnSuccess || type != kIOHIDReportTypeInput) return;
    if (id != Report::ReportId) return; // Other report IDs are not this input layout.
    State state;
    if (count <= 0 || !c.decoder.Decode(id, bytes, static_cast<std::size_t>(count), &state)) {
        ++c.rejected; return; // No raw payload in diagnostics.
    }
    if (++c.reports == 1 || state != c.last) {
        ++c.changes; c.last = state;
        c.out << "{\"event\":\"state\",\"source\":\"macos_iohid\",\"state\":";
        PrintState(c.out, state); c.out << "}\n";
        c.out.flush();
    }
}
}
int RunNativeHid(unsigned seconds, std::ostream &out, std::ostream &error) {
    if (seconds < 1 || seconds > 180) return 2;
    error << "Native read-only IOHID input: ONE already OS-connected Stadia Bluetooth gamepad.\n"
          << "No pairing/connection request, seizure, output report or Bluetooth setting change.\n"
          << "If denied, check Terminal Input Monitoring; this tool will not change permissions.\n";
    Capture capture{.out = out, .error = error};
    const auto manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) { error << "Cannot create scoped HID manager.\n"; return 1; }
    const auto matches = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    if (!matches) { CFRelease(manager); return 1; }
    for (const auto transport : {CFSTR("Bluetooth"), CFSTR("Bluetooth Low Energy")}) {
        const auto match = Match(transport);
        if (!match) { CFRelease(matches); CFRelease(manager); return 1; }
        CFArrayAppendValue(matches, match); CFRelease(match);
    }
    IOHIDManagerSetDeviceMatchingMultiple(manager, matches); CFRelease(matches);
    IOHIDManagerRegisterDeviceMatchingCallback(manager, Attached, &capture);
    IOHIDManagerRegisterDeviceRemovalCallback(manager, Removed, &capture);
    IOHIDManagerRegisterInputReportCallback(manager, Input, &capture);
    const auto loop = CFRunLoopGetCurrent();
    IOHIDManagerScheduleWithRunLoop(manager, loop, kCFRunLoopDefaultMode);
    const auto opened = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (opened == kIOReturnSuccess) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        while (!capture.failed && !capture.removed && std::chrono::steady_clock::now() < end)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, false);
    } else {
        error << "Cannot open scoped HID manager:0x" << std::hex << static_cast<std::uint32_t>(opened)
              << std::dec << " (access/device error). No retry.\n";
        capture.failed = true;
    }
    capture.decoder.Close();
    if (opened == kIOReturnSuccess) IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    IOHIDManagerUnscheduleFromRunLoop(manager, loop, kCFRunLoopDefaultMode);
    CFRelease(manager);
    out << "{\"event\":\"capture_complete\",\"source\":\"macos_iohid\",\"reports\":" << capture.reports
        << ",\"state_changes\":" << capture.changes << ",\"rejected\":" << capture.rejected
        << ",\"removed\":" << (capture.removed ? "true" : "false") << ",\"state\":";
    PrintState(out, State{}); out << ",\"raw_gatt_verified\":false}\n";
    if (!capture.reports) error << "No verified Stadia input reports; not a successful live test.\n";
    return capture.failed || capture.removed || !capture.reports || capture.rejected ? 1 : 0;
}
}
#else
namespace stadia_host {
int RunNativeHid(unsigned, std::ostream &, std::ostream &error) {
    error << "Native HID backend requires macOS. Docker/Linux can run replay, not this host's Bluetooth.\n";
    return 1;
}
}
#endif
