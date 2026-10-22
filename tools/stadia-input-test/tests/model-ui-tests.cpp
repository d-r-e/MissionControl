// SPDX-License-Identifier: GPL-2.0-only
#include "../source/ui.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace input_test;
int main(int argc,char **argv) {
    const std::array<Stick,2> zero{};
    Tracker t;
    t.Update(false,0xffff,{{{32767,32767},{32767,32767}}});
    assert(t.Progress()==0 && t.live==0);
    assert(Buttons[0].mask==(1ULL<<1) && Buttons[1].mask==(1ULL<<0));
    assert(Buttons[2].mask==(1ULL<<3) && Buttons[3].mask==(1ULL<<2));
    std::uint64_t unique=0;
    for(const auto &b:Buttons){assert(!(unique&b.mask));unique|=b.mask;}
    assert(unique==0xffff);
    t.Update(true,0xffff,zero);assert(t.Progress()==0 && t.pressed==0xffff);
    t.Update(true,0,zero);assert(t.Progress()==16);
    t.Reset();assert(t.Progress()==0);
    t.Update(true,Options|PhysicalX,zero,false);
    t.Update(true,0,zero);assert(t.Progress()==0);
    t.Update(true,1,zero);assert(!t.Done(1));
    t.Update(false,0,zero);assert(!t.Done(1)); // Disconnect is not an observed release.
    t.Update(true,0,zero);assert(t.Progress()==0); // Reconnection starts a new test.
    Tracker other;other.Update(true,2,zero);other.Update(true,0,zero);
    assert(other.Progress()==1 && t.Progress()==0);
    t.Update(true,0xffff,zero);t.Update(true,0,zero);
    t.Update(true,0,{{{24575,0},{-24575,0}}});assert(t.Progress()==16);
    for(Stick axis:std::array<Stick,4>{{{32767,0},{-32768,0},{0,32767},{0,-32768}}})
        t.Update(true,0,{{axis,axis}});
    assert(t.Progress()==24 && !t.sticks[0].centered);
    t.Update(true,0,{{{4097,0},{-4097,0}}});assert(t.Progress()==24);
    t.Update(true,0,{{{4096,0},{-4096,0}}});assert(t.Progress()==26);
    assert(t.sticks[0].minimum.x==-32768 && t.sticks[0].maximum.y==32767);
    t.Update(true,~0ULL,{{{std::numeric_limits<int>::max(),std::numeric_limits<int>::min()},{0,0}}});
    assert(t.Progress()==TotalChecks && t.live==0xffff);
    assert(t.sticks[0].live.x==32767 && t.sticks[0].live.y==-32768);
    constexpr std::size_t pitch=Width+16;
    constexpr std::uint32_t guard=0x1256abcd;
    std::vector<std::uint32_t> frame(pitch*Height+128,guard);
    Canvas canvas{frame.data()+64,pitch};
    Render(canvas,t,0,1);
    for(int y=0;y<Height;++y)for(std::size_t x=Width;x<pitch;++x)assert(frame[64+y*pitch+x]==guard);
    for(unsigned i=0;i<64;++i){assert(frame[i]==guard);assert(frame[64+pitch*Height+i]==guard);}
    canvas.Rect(-100,-100,150,150,Mint);canvas.Circle(Width-1,Height-1,30,Mint);
    assert(frame[64]==Mint);
    // Useful in disconnected, reconnecting, handheld and fully complete states.
    Tracker fresh;
    for(int slot=0;slot<9;++slot)Render(canvas,fresh,slot,0,slot==8);
    if(argc>1) {
        t.Update(true,Buttons[0].mask|Buttons[4].mask,{{{19000,14000},{0,0}}});
        Render(canvas,t,0,1);
        std::ofstream out(argv[1],std::ios::binary);
        for(int y=0;y<Height;++y)out.write(reinterpret_cast<const char *>(frame.data()+64+y*pitch),Width*4);
        assert(out.good());
    }
    std::cout<<"Input Lab: mappings, lifecycle, all26 checks and production UI bounds passed.\n";
}
