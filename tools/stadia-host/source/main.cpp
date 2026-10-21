/* SPDX-License-Identifier: GPL-2.0-only */
#include "replay.hpp"
#include "native_hid.hpp"
#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
    const auto usage = [] {
        std::cout << "Stadia portable host runner\n"
            "  --replay REPORT_FIXTURES --map DESCRIPTOR_HEX\n"
            "      Offline HID vs explicitly modelled GATT comparison; no device access.\n"
            "  --hid [--seconds N]\n"
            "      macOS only,1..180sec(default30). Read already OS-connected Stadia\n"
            "      Bluetooth gamepad HID input without seizing, pairing or output writes.\n"
            "      Reports OS-HID evidence only, NOT raw GATT or Switch connectivity.\n"
            "Output contains normalized controller state, never raw packets/device IDs.\n";
    };
    if (argc == 2 && std::string_view(argv[1]) == "--help") { usage(); return 0; }
    if (argc == 5 && std::string_view(argv[1]) == "--replay" && std::string_view(argv[3]) == "--map")
        return stadia_host::Replay(argv[2], argv[4], std::cout, std::cerr);
    if ((argc == 2 || argc == 4) && std::string_view(argv[1]) == "--hid") {
        unsigned seconds = 30;
        if (argc == 4) {
            if (std::string_view(argv[2]) != "--seconds") { usage(); return 2; }
            const std::string_view value(argv[3]);
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), seconds);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || seconds < 1 || seconds > 180) {
                std::cerr << "Duration must be1..180 seconds. No device opened.\n"; return 2;
            }
        }
        return stadia_host::RunNativeHid(seconds, std::cout, std::cerr);
    }
    usage(); return 2;
}
