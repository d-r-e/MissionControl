#include "stadia_report.hpp"
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using ams::controller::StadiaInputReport;
using ams::controller::StadiaAxisToSwitch;

std::vector<std::uint8_t> Unhex(const std::string &hex) {
    assert(hex.size() % 2 == 0);
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        bytes.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::map<std::string, std::uint16_t> expected_buttons = {
        {"a", 0x4000}, {"b", 0x2000}, {"x", 0x1000}, {"y", 0x0800},
        {"l1", 0x0400}, {"r1", 0x0200}, {"l3", 0x0100}, {"r3", 0x0080},
        {"menu", 0x0040}, {"options", 0x0020}, {"home", 0x0010},
        {"assistant", 0x0002}, {"capture", 0x0001}
    };
    const std::map<std::string, std::uint8_t> expected_hats = {
        {"dpad_up", 0}, {"dpad_right", 2}, {"dpad_down", 4}, {"dpad_left", 6}
    };
    const std::map<std::string, StadiaInputReport::Button> button_names = {
        {"a", StadiaInputReport::A}, {"b", StadiaInputReport::B},
        {"x", StadiaInputReport::X}, {"y", StadiaInputReport::Y},
        {"l1", StadiaInputReport::L1}, {"r1", StadiaInputReport::R1},
        {"l3", StadiaInputReport::L3}, {"r3", StadiaInputReport::R3},
        {"menu", StadiaInputReport::Menu}, {"options", StadiaInputReport::Options},
        {"home", StadiaInputReport::Home}, {"assistant", StadiaInputReport::Assistant},
        {"capture", StadiaInputReport::Capture}
    };

    std::ifstream fixture(argv[1]);
    assert(fixture.is_open());
    std::string line;
    unsigned count = 0;
    while (std::getline(fixture, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string name, hex;
        assert(row >> name >> hex);
        const auto bytes = Unhex(hex);
        StadiaInputReport report{};
        assert(StadiaInputReport::Parse(bytes.data(), bytes.size(), &report));
        assert(bytes.size() == 11);
        assert(report.left_x == bytes[4] && report.left_y == bytes[5]);
        assert(report.right_x == bytes[6] && report.right_y == bytes[7]);
        assert(report.left_trigger == bytes[8] && report.right_trigger == bytes[9]);
        assert(report.consumer_buttons == 0);
        if (name == "neutral") {
            assert(report.dpad == 8 && report.buttons == 0);
            assert(report.left_x == 128 && report.left_y == 128);
            assert(report.right_x == 128 && report.right_y == 128);
            assert(report.left_trigger == 0 && report.right_trigger == 0);
        } else if (expected_buttons.contains(name)) {
            assert(report.buttons == expected_buttons.at(name));
            assert(report.IsPressed(button_names.at(name)));
            assert(report.dpad == 8);
        } else if (expected_hats.contains(name)) {
            assert(report.dpad == expected_hats.at(name));
            assert(report.buttons == 0);
        } else {
            const bool minimum = name.ends_with("_min");
            unsigned value = 0;
            bool trigger = false;
            if (name.starts_with("left_x_")) value = report.left_x;
            else if (name.starts_with("left_y_")) value = report.left_y;
            else if (name.starts_with("right_x_")) value = report.right_x;
            else if (name.starts_with("right_y_")) value = report.right_y;
            else if (name.starts_with("left_trigger_")) { value = report.left_trigger; trigger = true; }
            else if (name.starts_with("right_trigger_")) { value = report.right_trigger; trigger = true; }
            else assert(false && "Unknown fixture label");
            assert(value == (minimum ? (trigger ? 0u : 1u) : 255u));
        }

        // HOGP's value excludes the Report ID. Explicit framing gives the same result.
        StadiaInputReport gatt{};
        assert(StadiaInputReport::ParseGatt(3, bytes.data() + 1, bytes.size() - 1, &gatt));
        assert(gatt.dpad == report.dpad && gatt.buttons == report.buttons);
        assert(gatt.left_x == report.left_x && gatt.right_y == report.right_y);
        assert(gatt.left_trigger == report.left_trigger && gatt.right_trigger == report.right_trigger);
        ++count;
    }
    assert(count == 30);

    std::array<std::uint8_t, 12> buffer{3, 8, 0, 0, 128, 128, 128, 128, 0, 0, 0, 0};
    StadiaInputReport sentinel{};
    sentinel.buttons = 0x1234;
    for (std::size_t size = 0; size <= buffer.size(); ++size) {
        if (size == 11) continue;
        assert(!StadiaInputReport::Parse(buffer.data(), size, &sentinel));
        assert(sentinel.buttons == 0x1234);
    }
    assert(!StadiaInputReport::Parse(nullptr, 11, &sentinel));
    assert(!StadiaInputReport::Parse(buffer.data(), 11, nullptr));
    buffer[0] = 5; // ID5 is an output report, not an input report.
    assert(!StadiaInputReport::Parse(buffer.data(), 11, &sentinel));
    assert(!StadiaInputReport::ParseGatt(5, buffer.data() + 1, 10, &sentinel));
    assert(!StadiaInputReport::ParseGatt(3, nullptr, 10, &sentinel));
    assert(!StadiaInputReport::ParseGatt(3, buffer.data() + 1, 10, nullptr));
    assert(!StadiaInputReport::ParseGatt(3, buffer.data(), 11, &sentinel));
    assert(sentinel.buttons == 0x1234);

    // A diagonal hat value 3 must not be mistaken for an embedded Report ID.
    buffer[0] = 3;
    buffer[1] = 0xf3;
    buffer[2] = 0xff;
    buffer[3] = 0x7f;
    buffer[10] = 0xff;
    assert(StadiaInputReport::Parse(buffer.data(), 11, &sentinel));
    assert(sentinel.dpad == 3 && sentinel.buttons == 0x7fff && sentinel.consumer_buttons == 7);

    assert(StadiaAxisToSwitch(0) == 0 && StadiaAxisToSwitch(0, true) == 4095);
    assert(StadiaAxisToSwitch(1) == 0 && StadiaAxisToSwitch(1, true) == 4095);
    assert(StadiaAxisToSwitch(128) == 2047 && StadiaAxisToSwitch(128, true) == 2047);
    assert(StadiaAxisToSwitch(255) == 4095 && StadiaAxisToSwitch(255, true) == 0);
    for (unsigned i = 1; i < 255; ++i) {
        assert(StadiaAxisToSwitch(i) < StadiaAxisToSwitch(i + 1));
        assert(StadiaAxisToSwitch(i, true) > StadiaAxisToSwitch(i + 1, true));
    }
    std::cout << count << " captured Stadia fixtures + framing, malformed input and axis tests passed\n";
}
