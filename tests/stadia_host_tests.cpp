#include "replay.hpp"
#include "gatt_bridge.h"
#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
using namespace stadia_host;

int main(int argc, char **argv) {
    assert(argc == 3);
    std::string text, hex;
    assert(ReadBounded(argv[2], 8192, &text));
    std::istringstream map_text(text); assert(map_text >> hex);
    std::vector<std::uint8_t> descriptor;
    assert(Unhex(hex, hogp::ReportLayout::MaxDescriptorSize, &descriptor));
    assert(mc_stadia_max_map_bytes() == hogp::ReportLayout::MaxDescriptorSize);
    assert(mc_stadia_validate_map(descriptor.data(), descriptor.size()) == 1);
    assert(mc_stadia_validate_map(nullptr, 1) == 0);
    const std::uint8_t input_ref[] = {3,1}, output_ref[] = {5,2}, invalid_ref[] = {3,0}, unknown_ref[] = {99,1};
    assert(mc_hogp_parse_reference(input_ref, sizeof(input_ref)) == 259);
    assert(mc_hogp_parse_reference(output_ref, sizeof(output_ref)) == 517);
    assert(mc_hogp_parse_reference(input_ref, 1) == -1 && mc_hogp_parse_reference(nullptr, 2) == -1);
    assert(mc_hogp_parse_reference(invalid_ref, 2) == -1);
    assert(mc_hogp_reference_size(descriptor.data(), descriptor.size(), input_ref, 2) == 10);
    assert(mc_hogp_reference_size(descriptor.data(), descriptor.size(), output_ref, 2) == 4);
    assert(mc_hogp_reference_size(descriptor.data(), descriptor.size(), unknown_ref, 2) == -1);
    assert(mc_hogp_reference_size(nullptr, 1, input_ref, 2) == -1);
    HidDecoder hid;
    GattFixtureModel model;
    assert(hid.Open(descriptor.data(), descriptor.size()));
    assert(model.Open(descriptor.data(), descriptor.size()));
    assert(ReadBounded(argv[1], 65536, &text));
    std::istringstream rows(text);
    std::string line;
    unsigned count = 0;
    while (std::getline(rows, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string label, wire_hex;
        assert(row >> label >> wire_hex);
        std::vector<std::uint8_t> wire;
        assert(Unhex(wire_hex, Report::Size, &wire) && wire.size() == Report::Size);
        State a, b;
        assert(hid.Decode(3, wire.data(), wire.size(), &a));
        assert(model.Decode(wire.data() + 1, wire.size() - 1, &b) && a == b);
        assert(!hid.Decode(4, wire.data(), wire.size(), &a) && a == State{});
        assert(!hid.Decode(3, wire.data() + 1, wire.size() - 1, &a));
        assert(!model.Decode(wire.data(), wire.size(), &b));
        assert(!hid.Decode(3, nullptr, wire.size(), &a));
        assert(!hid.Decode(3, wire.data(), wire.size(), nullptr));
        wire[0] = 4;
        assert(!hid.Decode(3, wire.data(), wire.size(), &a));
        ++count;
    }
    assert(count == 30);
    std::uint8_t diagonal[11] = {3,3,0,0,128,128,128,128,0,0,0};
    State a, b;
    assert(hid.Decode(3, diagonal, sizeof(diagonal), &a) && a.dpad == 3);
    assert(model.Decode(diagonal + 1, sizeof(diagonal) - 1, &b) && a == b); // Payload03 is NOT stripped.
    assert(a.left_x == 2047 && a.left_y == 2047);
    hid.Close(); model.Close();
    assert(!hid.Decode(3, diagonal, sizeof(diagonal), &a) && a == State{});
    assert(!model.Decode(diagonal + 1, sizeof(diagonal) - 1, &b) && b == State{});
    assert(hid.Open(descriptor.data(), descriptor.size()));
    assert(model.Open(descriptor.data(), descriptor.size()));
    assert(!hid.Open(nullptr, 1) && !model.Open(nullptr, 1));
    assert(!hid.Decode(3, diagonal, sizeof(diagonal), &a));
    assert(!model.Decode(diagonal + 1, sizeof(diagonal) - 1, &b));
    for (const auto invalid : {"", "0", "gg", "010203"}) {
        std::vector<std::uint8_t> bytes{42};
        assert(!Unhex(invalid, 2, &bytes) && bytes.empty());
    }
    assert(!Unhex("00", 1, nullptr));
    assert(SafeLabel("left_stick-1") && !SafeLabel("bad\"label") && !SafeLabel("AA:BB:CC:DD:EE:FF"));
    assert(!SafeLabel(std::string(65, 'a')));
    std::ostringstream output, error;
    assert(Replay(argv[1], argv[2], output, error) == 0 && error.str().empty());
    assert(output.str().find("\"modelled_gatt_matches\":30") != std::string::npos);
    assert(output.str().find("\"live_gatt_verified\":false") != std::string::npos);
    assert(output.str().find("\"hex\"") == std::string::npos);
    std::cout << "Portable host:30 HID/modelled-GATT fixture states, framing and close/invalidation tests passed\n";
}
