// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "model.hpp"
#include <cstdio>
#include <cstring>
#include <cstddef>

namespace input_test {
inline constexpr int Width=1280, Height=720;
constexpr std::uint32_t Color(unsigned r,unsigned g,unsigned b) {return r|(g<<8)|(b<<16)|0xff000000U;}
inline constexpr auto Bg=Color(13,20,30), Card=Color(23,33,46), Body=Color(42,55,72);
inline constexpr auto White=Color(235,242,248), Muted=Color(140,161,181), Mint=Color(70,222,172);
inline constexpr auto Orange=Color(255,171,82), Line=Color(58,76,95), Dark=Color(12,22,33);
struct Glyph {char c; std::uint8_t rows[7];};
inline constexpr Glyph Font[]={
 {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
 {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
 {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
 {'G',{14,17,16,23,17,17,15}}, {'H',{17,17,17,31,17,17,17}},
 {'I',{31,4,4,4,4,4,31}}, {'J',{7,2,2,2,2,18,12}},
 {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
 {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
 {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
 {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
 {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
 {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
 {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
 {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
 {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
 {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
 {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
 {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
 {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
 {'-',{0,0,0,31,0,0,0}}, {'+',{0,4,4,31,4,4,0}},
 {'/',{1,2,2,4,8,8,16}}, {':',{0,4,4,0,4,4,0}},
 {'.',{0,0,0,0,0,12,12}}, {'<',{2,4,8,16,8,4,2}},
 {'>',{8,4,2,1,2,4,8}}, {'(',{2,4,8,8,8,4,2}},
 {')',{8,4,2,2,2,4,8}}, {'=',{0,0,31,0,31,0,0}},
 {'%',{17,2,4,8,16,17,0}},
};
struct Canvas {
    std::uint32_t *pixels;
    std::size_t pitch; // pixels, not bytes; framebuffer stride can exceed visible width.
    void Rect(int x,int y,int w,int h,std::uint32_t c) {
        const int end_x=std::clamp(x+w,0,Width), end_y=std::clamp(y+h,0,Height);
        for (int row=std::max(y,0);row<end_y;++row)
            for (int col=std::max(x,0);col<end_x;++col) pixels[row*pitch+col]=c;
    }
    void Circle(int x,int y,int radius,std::uint32_t c) {
        for(int row=-radius;row<=radius;++row) for(int col=-radius;col<=radius;++col)
            if(col*col+row*row<=radius*radius) Rect(x+col,y+row,1,1,c);
    }
    void Round(int x,int y,int w,int h,int r,std::uint32_t c) {
        Rect(x+r,y,w-2*r,h,c); Rect(x,y+r,w,h-2*r,c);
        Circle(x+r,y+r,r,c); Circle(x+w-r-1,y+r,r,c);
        Circle(x+r,y+h-r-1,r,c); Circle(x+w-r-1,y+h-r-1,r,c);
    }
    void Text(int x,int y,const char *s,std::uint32_t color=White,int scale=2) {
        for(;*s;++s,x+=6*scale) {
            const char c=(*s>='a' && *s<='z') ? *s-'a'+'A' : *s;
            for(const auto &glyph:Font) if(glyph.c==c) {
                for(int row=0;row<7;++row) for(int col=0;col<5;++col)
                    if(glyph.rows[row] & (1U<<(4-col))) Rect(x+col*scale,y+row*scale,scale,scale,color);
                break;
            }
        }
    }
    void Center(int x,int y,const char *s,std::uint32_t color=White,int scale=2) {
        Text(x-static_cast<int>(std::strlen(s))*3*scale,y-7*scale/2,s,color,scale);
    }
};
inline void ButtonCircle(Canvas &c,const Tracker &t,int x,int y,const Button &b,int radius=24) {
    c.Circle(x,y,radius+2,t.Done(b.mask)?Mint:Line);
    const bool held=t.connected && (t.live & b.mask);
    c.Circle(x,y,radius,held?Mint:Dark);
    c.Center(x,y,b.physical,held?Dark:White,radius<=18?1:2);
}
inline void ButtonBar(Canvas &c,const Tracker &t,int x,int y,int w,const Button &b,int h=34) {
    const bool held=t.connected && (t.live & b.mask);
    c.Round(x,y,w,h,7,held?Mint:(t.Done(b.mask)?Color(27,80,72):Dark));
    c.Center(x+w/2,y+h/2,b.physical,held?Dark:White,2);
}
inline void DrawStick(Canvas &c,const Tracker &t,unsigned i,int x,int y) {
    const auto &s=t.sticks[i];
    c.Circle(x,y,55,s.centered?Mint:Line);c.Circle(x,y,52,Dark);
    c.Rect(x-45,y,91,1,Line);c.Rect(x,y-45,1,91,Line);
    const auto mask=Buttons[8+i].mask;
    const int dx=s.live.x*36/32768,dy=-s.live.y*36/32768;
    c.Circle(x+dx,y+dy,19,(t.connected && (t.live&mask))?Mint:Body);
    c.Center(x+dx,y+dy,i==0?"L3":"R3",White,2);
    for(unsigned direction=0;direction<4;++direction) {
        const int ox[]={-64,64,0,0},oy[]={0,0,64,-64};
        c.Circle(x+ox[direction],y+oy[direction],4,(s.directions&(1U<<direction))?Mint:Line);
    }
}
inline void Render(Canvas &c,const Tracker &t,int selected,unsigned style,bool exiting=false) {
    c.Rect(0,0,Width,Height,Bg);
    c.Text(28,25,"STADIA",White,4);c.Text(187,34,"/ INPUT LAB",Mint,3);
    c.Text(30,73,"PRESS AND RELEASE EVERY BUTTON. SWEEP BOTH STICKS, THEN LET GO.",Muted);
    c.Round(1040,22,210,42,9,t.connected?Color(26,76,66):Body);
    c.Center(1145,43,t.connected?"CONNECTED":"NO CONTROLLER",t.connected?Mint:Orange,2);
    char label[100];
    if(selected==8) std::snprintf(label,sizeof(label),"<  HANDHELD  >");
    else std::snprintf(label,sizeof(label),"<  PLAYER %d  >",selected+1);
    c.Round(28,103,256,36,7,Body);c.Center(156,121,label,White,2);
    c.Text(306,115,(style&1U)?"PRO CONTROLLER INPUT":"SWITCH HID INPUT",Muted);
    c.Round(918,103,151,36,7,Body);c.Center(993,121,"RESET",White);
    c.Round(1090,103,160,36,7,Body);c.Center(1170,121,"EXIT",White);
    c.Round(28,154,786,390,18,Card);c.Round(834,154,418,390,18,Card);
    c.Text(52,172,"LIVE CONTROLLER",Muted);c.Text(858,172,"PRESS + RELEASE",Muted);
    // Symmetric Stadia layout. Button glyphs are physical Stadia labels, not Switch face labels.
    c.Round(164,269,514,170,56,Body);c.Circle(208,410,73,Body);c.Circle(637,410,73,Body);
    ButtonBar(c,t,182,194,130,Buttons[6],25);ButtonBar(c,t,182,227,130,Buttons[4],25);
    ButtonBar(c,t,540,194,130,Buttons[7],25);ButtonBar(c,t,540,227,130,Buttons[5],25);
    ButtonCircle(c,t,621,359,Buttons[0]);ButtonCircle(c,t,660,320,Buttons[1]);
    ButtonCircle(c,t,582,320,Buttons[2]);ButtonCircle(c,t,621,281,Buttons[3]);
    ButtonCircle(c,t,226,286,Buttons[12],18);ButtonCircle(c,t,226,354,Buttons[13],18);
    ButtonCircle(c,t,192,320,Buttons[14],18);ButtonCircle(c,t,260,320,Buttons[15],18);
    ButtonBar(c,t,327,294,93,Buttons[10]);ButtonBar(c,t,436,294,72,Buttons[11]);
    c.Center(422,340,"HOME / CAPTURE",Muted);c.Center(422,360,"HANDLED BY OS",Muted);
    DrawStick(c,t,0,326,430);DrawStick(c,t,1,514,430);
    c.Center(422,518,"FILLED = HELD   MINT EDGE = TESTED",Muted,2);
    for(unsigned i=0;i<Buttons.size();++i) {
        const auto &b=Buttons[i]; const int x=860+static_cast<int>(i%2)*197,y=207+static_cast<int>(i/2)*39;
        const bool held=t.connected&&(t.live&b.mask);
        c.Circle(x+6,y+6,5,t.Done(b.mask)?Mint:((t.pressed&b.mask)?Orange:Line));
        c.Text(x+20,y,b.physical,held?Mint:White,2);
        c.Text(x+20,y+17,b.mapped,Muted,1);
    }
    c.Round(28,558,1224,86,12,Card);
    for(unsigned i=0;i<2;++i) {
        const auto &s=t.sticks[i];const int x=49+static_cast<int>(i)*404;
        c.Text(x,573,i==0?"LEFT STICK":"RIGHT STICK",White,2);
        std::snprintf(label,sizeof(label),"X %+6d  Y %+6d",s.live.x,s.live.y);c.Text(x,596,label,Muted,2);
        std::snprintf(label,sizeof(label),"RANGE X %d..%d  Y %d..%d",s.minimum.x,s.maximum.x,s.minimum.y,s.maximum.y);
        c.Text(x,621,label,Muted,1);
    }
    const int count=t.Progress();
    std::snprintf(label,sizeof(label),"%02d / %d CHECKS",count,TotalChecks);c.Text(899,574,label,count==TotalChecks?Mint:White,2);
    c.Round(899,599,322,10,4,Line);if(count)c.Rect(899,599,count*322/TotalChecks,10,Mint);
    c.Text(899,622,count==TotalChecks?"ALL CHECKS PASSED":"16 BUTTONS + 10 STICK CHECKS",count==TotalChecks?Mint:Muted,1);
    c.Text(30,658,"TOUCH CONTROLS OR: OPTIONS + LEFT/RIGHT = SLOT   OPTIONS + X = RESET",Muted,2);
    c.Text(30,680,exiting?"KEEP HOLDING MENU + OPTIONS TO EXIT":"HOLD MENU + OPTIONS 1S TO EXIT. TRIGGERS ARE DIGITAL; NO RUMBLE TEST.",exiting?Orange:Muted,2);
    // Home/Capture are intercepted by the OS. Assistant/media have no Switch Pro mapping.
}
}
