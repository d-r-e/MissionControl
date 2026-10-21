/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "decoder.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace stadia_host {
inline bool Unhex(const std::string &hex, std::size_t max_size, std::vector<std::uint8_t> *out) {
    if (!out) return false;
    out->clear();
    if (hex.empty() || hex.size() % 2 || hex.size() / 2 > max_size) return false;
    auto digit = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int a = digit(hex[i]), b = digit(hex[i + 1]);
        if (a < 0 || b < 0) { out->clear(); return false; }
        out->push_back(static_cast<std::uint8_t>((a << 4) | b));
    }
    return true;
}
inline bool ReadBounded(const char *path, std::size_t limit, std::string *out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size < 0 || static_cast<std::uint64_t>(size) > limit) return false;
    out->assign(static_cast<std::size_t>(size), '\0');
    file.seekg(0);
    if (!out->empty()) file.read(out->data(), static_cast<std::streamsize>(out->size()));
    return static_cast<bool>(file);
}
inline bool SafeLabel(const std::string &s) {
    if (s.empty() || s.size() > 64) return false;
    for (const unsigned char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
inline int Replay(const char *reports_path, const char *map_path, std::ostream &out, std::ostream &error) {
    std::string map_text, report_text;
    if (!ReadBounded(map_path, 2 * hogp::ReportLayout::MaxDescriptorSize + 64, &map_text) ||
        !ReadBounded(reports_path, 4 * 1024 * 1024, &report_text)) {
        error << "Cannot read bounded fixture files.\n"; return 1;
    }
    std::istringstream map_stream(map_text);
    std::string hex, extra;
    std::vector<std::uint8_t> descriptor;
    if (!(map_stream >> hex) || (map_stream >> extra) ||
        !Unhex(hex, hogp::ReportLayout::MaxDescriptorSize, &descriptor)) {
        error << "Invalid descriptor fixture.\n"; return 1;
    }
    HidDecoder hid;
    GattFixtureModel model;
    if (!hid.Open(descriptor.data(), descriptor.size()) || !model.Open(descriptor.data(), descriptor.size())) {
        error << "Descriptor does not validate the Stadia input layout.\n"; return 1;
    }
    auto fail = [&](const char *message) {
        error << message << '\n';
        out << "{\"event\":\"replay_aborted\",\"state\":";
        PrintState(out, State{}); out << "}\n";
        return 1;
    };
    std::istringstream lines(report_text);
    std::string line;
    unsigned count = 0;
    while (std::getline(lines, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string label, report_hex;
        std::vector<std::uint8_t> report;
        if (++count > 10000 || line.size() > 256 || !(row >> label >> report_hex) ||
            (row >> extra) || !SafeLabel(label) || !Unhex(report_hex, Report::Size, &report)) {
            return fail("Invalid fixture row (input contents not echoed).");
        }
        State a, b;
        if (!hid.Decode(Report::ReportId, report.data(), report.size(), &a) ||
            !model.Decode(report.data() + 1, report.size() - 1, &b) || a != b) {
            return fail("HID/modelled-GATT framing or state mismatch.");
        }
        // Input labels are not trusted identifiers; emit our own ordinal only.
        out << "{\"event\":\"state\",\"source\":\"fixture_hid\",\"row\":" << count << ",\"state\":";
        PrintState(out, a); out << "}\n";
    }
    hid.Close(); model.Close();
    if (!count) return fail("No fixture reports.");
    out << "{\"event\":\"replay_complete\",\"reports\":" << count
        << ",\"modelled_gatt_matches\":" << count << ",\"live_gatt_verified\":false,\"state\":";
    PrintState(out, State{}); out << "}\n";
    return 0;
}
}
