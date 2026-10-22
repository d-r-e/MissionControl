// SPDX-License-Identifier: GPL-2.0-only
#include "ble_input_timing.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
using namespace ams::ble::diagnostics;
int main() {
    InputTimings disabled;
    disabled.Record(TimingMetric::SlotIpc,123,false);
    disabled.Record(TimingMetric::Count,123,true);
    disabled.Record(static_cast<TimingMetric>(999),123,true);
    disabled.NoteFifoFailure(false);
    for(const auto &v:disabled.Read()){assert(v.count==0 && v.sum==0);for(auto n:v.buckets)assert(n==0);}
    assert(disabled.Failures()==0);
    for(auto metric:{TimingMetric::InputWork,TimingMetric::FifoUsedBytes}) {
        const auto &limits=metric==TimingMetric::FifoUsedBytes?QueueLimitsBytes:DurationLimitsUs;
        for(std::size_t i=0;i<limits.size();++i){assert(BucketFor(metric,limits[i])==i);assert(BucketFor(metric,limits[i]+1)==i+1);}
    }
    assert(BucketFor(TimingMetric::SlotIpc,0)==0);
    assert(BucketFor(TimingMetric::SlotIpc,UINT64_MAX)==7);
    InputTimings concurrent;
    constexpr unsigned iterations=10000;
    auto writer=[&]{for(unsigned i=0;i<iterations;++i){concurrent.Record(TimingMetric::InputWork,1234,true);concurrent.NoteFifoFailure(true);}};
    std::thread a(writer),b(writer),c(writer),d(writer);
    std::uint64_t previous=0;
    for(unsigned i=0;i<iterations;++i){const auto n=concurrent.Read()[static_cast<std::size_t>(TimingMetric::InputWork)].count;assert(n>=previous && n<=4*iterations);previous=n;}
    a.join();b.join();c.join();d.join();
    const auto v=concurrent.Read()[static_cast<std::size_t>(TimingMetric::InputWork)];
    assert(v.count==4*iterations && v.sum==4ULL*iterations*1234);
    for(std::size_t i=0;i<TimingBuckets;++i)assert(v.buckets[i]==(i==2?4*iterations:0));
    assert(concurrent.Failures()==4*iterations);
    std::puts("Input timing: disabled/invalid guards, histogram boundaries, fixed-memory concurrent atomic recording passed");
}
