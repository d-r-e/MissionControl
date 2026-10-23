// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Minimal host OS/result boundary. No production include paths or compiler defines select this shim.
#include <switch.h>
#include <algorithm>
#include <compare>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <mutex>
namespace gatt_test { inline s64 now_ms = 0; }
namespace ams {
    class Result {
        u32 m_value;
    public:
        constexpr Result(u32 value=0):m_value(value){}
        constexpr u32 GetValue() const{return m_value;}
        constexpr operator u32() const{return m_value;}
    };
    constexpr Result ResultSuccess(){return {};}
    class TimeSpan {
        s64 m_ms;
    public:
        explicit constexpr TimeSpan(s64 ms=0):m_ms(ms){}
        static constexpr TimeSpan FromMilliSeconds(s64 ms){return TimeSpan(ms);}
        static constexpr TimeSpan FromSeconds(s64 seconds){return TimeSpan(seconds*1000);}
        constexpr s64 GetMilliSeconds() const{return m_ms;}
        auto operator<=>(const TimeSpan&)const=default;
        friend constexpr TimeSpan operator+(TimeSpan a,TimeSpan b){return TimeSpan(a.m_ms+b.m_ms);}
        friend constexpr TimeSpan operator-(TimeSpan a,TimeSpan b){return TimeSpan(a.m_ms-b.m_ms);}
    };
    namespace os {
        using SdkMutex=std::mutex;
        struct Tick {TimeSpan ToTimeSpan()const{return TimeSpan::FromMilliSeconds(gatt_test::now_ms);}};
        inline Tick GetSystemTick(){return {};}
        inline void SleepThread(TimeSpan duration){gatt_test::now_ms+=duration.GetMilliSeconds();}
    }
    namespace svc {
        constexpr Result ResultNotSupported(){return Result(1);}
        constexpr Result ResultNotFound(){return Result(2);}
        constexpr Result ResultTimedOut(){return Result(3);}
    }
    namespace util {template<class T,std::size_t N> constexpr std::size_t size(const T(&)[N]){return N;}}
}
#define R_FAILED(r) (static_cast<u32>(r)!=0)
#define R_SUCCEEDED(r) (!R_FAILED(r))
#define R_THROW(r) return (r)
#define R_RETURN(r) return (r)
#define R_UNLESS(condition,r) do {if(!(condition)){R_THROW(r);}}while(false)
#define R_TRY(r) do {const auto test_result=(r);if(R_FAILED(test_result)){R_THROW(test_result);}}while(false)
#define R_SUCCEED() return ams::ResultSuccess()
#define R_DEFINE_NAMESPACE_RESULT_MODULE(ns,id) static_assert(true)
#define R_DEFINE_ERROR_RESULT(name,id) constexpr Result Result##name(){return Result(id);}
