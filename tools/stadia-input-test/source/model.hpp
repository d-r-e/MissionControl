// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace input_test {
// Switch HID positions, labelled with the corresponding physical Stadia buttons.
struct Button { const char *physical; const char *mapped; std::uint64_t mask; };
inline constexpr std::array<Button, 16> Buttons{{
    {"A", "SW B", 1ULL<<1}, {"B", "SW A", 1ULL<<0},
    {"X", "SW Y", 1ULL<<3}, {"Y", "SW X", 1ULL<<2},
    {"L1", "L", 1ULL<<6}, {"R1", "R", 1ULL<<7},
    {"L2", "ZL", 1ULL<<8}, {"R2", "ZR", 1ULL<<9},
    {"L3", "CLICK", 1ULL<<4}, {"R3", "CLICK", 1ULL<<5},
    {"OPTIONS", "MINUS", 1ULL<<11}, {"MENU", "PLUS", 1ULL<<10},
    {"UP", "D-PAD", 1ULL<<13}, {"DOWN", "D-PAD", 1ULL<<15},
    {"LEFT", "D-PAD", 1ULL<<12}, {"RIGHT", "D-PAD", 1ULL<<14},
}};
inline constexpr std::uint64_t Options = 1ULL<<11, Menu = 1ULL<<10;
inline constexpr std::uint64_t PhysicalX = 1ULL<<3, Left = 1ULL<<12, Right = 1ULL<<14;
struct Stick { int x{}, y{}; };
struct Sweep {
    Stick live{}, minimum{}, maximum{};
    unsigned directions{};
    bool centered{};
    void Update(Stick input, bool track) {
        live = {std::clamp(input.x,-32768,32767), std::clamp(input.y,-32768,32767)};
        if (!track) return;
        minimum.x=std::min(minimum.x,live.x); minimum.y=std::min(minimum.y,live.y);
        maximum.x=std::max(maximum.x,live.x); maximum.y=std::max(maximum.y,live.y);
        constexpr int edge=24575;
        if (live.x < -edge) directions|=1;
        if (live.x > edge) directions|=2;
        if (live.y < -edge) directions|=4;
        if (live.y > edge) directions|=8;
        if (directions==15 && live.x>=-4096 && live.x<=4096 && live.y>=-4096 && live.y<=4096)
            centered=true;
    }
};
inline int Bits(std::uint64_t value) {
    int count=0; while(value) { value&=value-1; ++count; } return count;
}
struct Tracker {
    bool connected{};
    std::uint64_t live{}, pressed{}, released{};
    std::array<Sweep,2> sticks{};
    void Reset() { pressed=released=0; sticks={}; }
    void Update(bool active, std::uint64_t buttons, std::array<Stick,2> axes, bool track=true) {
        if (active && !connected) Reset(); // A newly connected device must not inherit another one's passes.
        connected=active;
        live=active ? buttons & 0xffffULL : 0;
        if (active && track) { pressed|=live; released|=pressed & ~live; }
        for (unsigned i=0;i<2;++i) sticks[i].Update(active ? axes[i] : Stick{},active && track);
    }
    bool Done(std::uint64_t mask) const { return (released & mask)==mask; }
    int Progress() const {
        int count=Bits(released & 0xffffULL);
        for (const auto &stick:sticks) count+=Bits(stick.directions)+(stick.centered?1:0);
        return count;
    }
};
inline constexpr int TotalChecks=26;
}
