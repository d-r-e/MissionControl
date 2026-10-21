/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace missioncontrol::probe {
    constexpr int HexDigit(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    inline bool ParseTargetAddress(const char *text, std::size_t size,
                                   std::array<std::uint8_t, 6> *out) {
        if (!text || !out || size < 17 || size > 63) return false;
        std::array<std::uint8_t, 6> address{};
        for (unsigned i = 0; i < address.size(); ++i) {
            const int hi = HexDigit(text[i * 3]);
            const int lo = HexDigit(text[i * 3 + 1]);
            if (hi < 0 || lo < 0 || (i < 5 && text[i * 3 + 2] != ':')) return false;
            address[i] = (hi << 4) | lo;
        }
        for (std::size_t i = 17; i < size; ++i) {
            if (text[i] != '\r' && text[i] != '\n' && text[i] != ' ' && text[i] != '\t') return false;
        }
        bool all_zero = true, all_ff = true;
        for (auto byte : address) { all_zero &= byte == 0; all_ff &= byte == 0xff; }
        if (all_zero || all_ff) return false;
        *out = address;
        return true;
    }
}
