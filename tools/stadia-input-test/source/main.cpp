// SPDX-License-Identifier: GPL-2.0-only
#include <switch.h>
#include "ui.hpp"
#include <array>
#include <cstdio>

using namespace input_test;
static_assert(HidNpadButton_A==(1ULL<<0) && HidNpadButton_B==(1ULL<<1));
static_assert(HidNpadButton_X==(1ULL<<2) && HidNpadButton_Y==PhysicalX);
static_assert(HidNpadButton_StickL==(1ULL<<4) && HidNpadButton_StickR==(1ULL<<5));
static_assert(HidNpadButton_L==(1ULL<<6) && HidNpadButton_R==(1ULL<<7));
static_assert(HidNpadButton_ZL==(1ULL<<8) && HidNpadButton_ZR==(1ULL<<9));
static_assert(HidNpadButton_Plus==Menu && HidNpadButton_Minus==Options);
static_assert(HidNpadButton_Left==Left && HidNpadButton_Right==Right);
static_assert(HidNpadButton_Up==(1ULL<<13) && HidNpadButton_Down==(1ULL<<15));

int main(int, char **) {
    padConfigureInput(8,HidNpadStyleSet_NpadStandard);
    std::array<PadState,9> pads{};
    std::array<Tracker,9> trackers{};
    for(unsigned i=0;i<8;++i) padInitializeWithMask(&pads[i],1ULL<<i);
    padInitialize(&pads[8],HidNpadIdType_Handheld);
    Framebuffer fb{};
    Result rc=framebufferCreate(&fb,nwindowGetDefault(),Width,Height,PIXEL_FORMAT_RGBA_8888,2);
    if(R_SUCCEEDED(rc)) rc=framebufferMakeLinear(&fb);
    if(R_FAILED(rc)) {
        framebufferClose(&fb);
        consoleInit(nullptr);
        std::printf("Input Lab: framebuffer unavailable (0x%x).\nPress + to exit.\n",rc);
        PadState any{};padInitializeAny(&any);
        while(appletMainLoop()) {padUpdate(&any);if(padGetButtonsDown(&any)&HidNpadButton_Plus)break;consoleUpdate(nullptr);}
        consoleExit(nullptr);return 1;
    }
    hidInitializeTouchScreen();
    int selected=-1,exit_owner=-1;
    bool previous_touch=false;
    u64 exit_started=0;
    while(appletMainLoop()) {
        // Dequeue can wait for the compositor. Sample input AFTER that wait, not a frame earlier.
        u32 stride=0;
        auto *pixels=static_cast<std::uint32_t *>(framebufferBegin(&fb,&stride));
        if(!pixels)break;
        for(auto &pad:pads) padUpdate(&pad);
        if(selected<0) {
            for(int i=0;i<8;++i) if(padIsConnected(&pads[i])&&(padGetStyleSet(&pads[i])&HidNpadStyleTag_NpadFullKey)){selected=i;break;}
            if(selected<0) for(int i=0;i<9;++i) if(padIsConnected(&pads[i])){selected=i;break;}
        }
        const int current=selected<0?0:selected;
        int next=current,holding_exit=-1;
        bool reset=false,quit=false;
        std::array<bool,9> shortcut{};
        for(int i=0;i<9;++i) {
            const u64 buttons=padGetButtons(&pads[i]),down=padGetButtonsDown(&pads[i]);
            if(!padIsConnected(&pads[i]) || !(buttons&Options))continue;
            shortcut[i]=(buttons&(Left|Right|PhysicalX|Menu))!=0;
            if(down&Left)next=(current+8)%9;
            if(down&Right)next=(current+1)%9;
            if(down&PhysicalX)reset=true;
            if(buttons&Menu)holding_exit=i;
        }
        HidTouchScreenState touch{};
        const bool touched=hidGetTouchScreenStates(&touch,1)>0 && touch.count>0;
        if(touched&&!previous_touch) {
            const auto x=touch.touches[0].x,y=touch.touches[0].y;
            if(y>=103 && y<139) {
                if(x>=28 && x<156) next=(current+8)%9;
                if(x>=156 && x<284) next=(current+1)%9;
                if(x>=918 && x<1069) reset=true;
                if(x>=1090 && x<1250) quit=true;
            }
        }
        previous_touch=touched;
        if(next!=current) {selected=next;exit_owner=-1;exit_started=0;}
        if(reset)trackers[next].Reset();
        for(unsigned i=0;i<pads.size();++i) {
            const auto l=padGetStickPos(&pads[i],0),r=padGetStickPos(&pads[i],1);
            trackers[i].Update(padIsConnected(&pads[i]),padGetButtons(&pads[i]),{{{l.x,l.y},{r.x,r.y}}},!shortcut[i]);
        }
        if(holding_exit>=0) {
            const u64 now=armGetSystemTick();
            if(exit_owner!=holding_exit){exit_owner=holding_exit;exit_started=now;}
            else if(armTicksToNs(now-exit_started)>=1'000'000'000ULL)quit=true;
        } else {exit_owner=-1;exit_started=0;}
        Canvas canvas{pixels,stride/sizeof(std::uint32_t)};
        Render(canvas,trackers[next],next,padGetStyleSet(&pads[next]),holding_exit>=0);
        framebufferEnd(&fb);
        if(quit)break; // Always return the dequeued frame, including the exit path.
    }
    framebufferClose(&fb);
    return 0;
}
