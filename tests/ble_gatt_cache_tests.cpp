// SPDX-License-Identifier: GPL-2.0-only
#include "ble_gatt_cache.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <limits>

using namespace ams::ble::gatt;
using Address = std::array<std::uint8_t, 6>;
struct Attribute {
    std::uint16_t handle{}, value{};
    bool operator==(const Attribute &) const = default;
};
struct Table {
    Address address{};
    std::uint32_t count{};
    std::array<Attribute, 64> attributes{};
    bool operator==(const Table &) const = default;
};
struct Batch { Attribute attributes[10]{}; };
static Batch MakeBatch(unsigned first) {
    Batch batch;
    for (unsigned i=0;i<10;++i) batch.attributes[i] = {
        static_cast<std::uint16_t>(first+i), static_cast<std::uint16_t>((first+i)*3)};
    return batch;
}

// Literal pre-extraction policy, deliberately independent of DiscoveryCache's implementation.
// Differential replay is a behavior-preservation test, not proof this policy handles all stale events.
struct OldPolicy {
    Table table{};
    CacheState state{};
    void Prepare(const Address &address) {
        state.discovery_complete=false;state.rediscovered=false;state.replace_pending=true;
        state.overflowed=false;table.count=0;table.address=address;
    }
    CacheAppendResult Append(std::uint32_t event, std::uint32_t current, const Batch &batch, std::uint32_t count) {
        if(event!=current)return CacheAppendResult::Ignored;
        if(state.discovery_complete)state.rediscovered=true;
        if(state.replace_pending){state.replace_pending=false;table.count=0;}
        for(std::uint32_t i=0;i<std::min<std::uint32_t>(count,10);++i){
            if(table.count>=64){state.overflowed=true;return CacheAppendResult::Overflow;}
            table.attributes[table.count++]=batch.attributes[i];
        }
        return CacheAppendResult::Appended;
    }
    void Complete(){state.discovery_complete=true;state.replace_pending=false;}
    bool Take(){const bool value=state.rediscovered;state.rediscovered=false;return value;}
};

int main() {
    constexpr Address a{1,2,3,4,5,6}, b{7,8,9,10,11,12};
    const auto first=MakeBatch(1), second=MakeBatch(11), third=MakeBatch(21);
    DiscoveryCache cache;
    Table table;
    assert(cache.State()==CacheState{} && !cache.Overflowed() && !cache.TakeRediscovered());

    // Earlier local contents are erased even for the SAME address and recycled connection ID.
    assert(cache.Append(table,6,6,first.attributes,10)==CacheAppendResult::Appended);
    assert(table.count==10);
    cache.Prepare(table,a);
    assert(table.address==a && table.count==0 && cache.State().replace_pending);
    const auto prepared=cache.State();
    assert(cache.Append(table,5,6,first.attributes,10)==CacheAppendResult::Ignored);
    assert(cache.State()==prepared && table.count==0);

    // Real observed shape: 27 attributes in 10+10+7 fragments received while Pair pumps events.
    assert(cache.Append(table,6,6,first.attributes,10)==CacheAppendResult::Appended);
    assert(cache.Append(table,6,6,second.attributes,10)==CacheAppendResult::Appended);
    assert(cache.Append(table,6,6,third.attributes,7)==CacheAppendResult::Appended);
    assert(table.count==27 && !cache.TakeRediscovered());
    for(unsigned i=0;i<27;++i)assert(table.attributes[i].handle==i+1);
    const Table paired=table;
    cache.Complete();
    assert(table==paired && cache.State().discovery_complete && !cache.State().replace_pending);
    assert(!cache.TakeRediscovered());
    assert(cache.Append(table,7,6,first.attributes,1)==CacheAppendResult::Ignored);
    assert(table==paired && !cache.TakeRediscovered());

    // Characterization, NOT deduplication: post-discovery batches append and raise a coalesced latch.
    assert(cache.Append(table,6,6,first.attributes,1)==CacheAppendResult::Appended);
    assert(cache.Append(table,6,6,first.attributes,1)==CacheAppendResult::Appended);
    assert(table.count==29 && table.attributes[27]==table.attributes[28]);
    assert(cache.TakeRediscovered() && !cache.TakeRediscovered());
    cache.Append(table,6,6,first.attributes,0);
    assert(cache.TakeRediscovered()); // Even empty post-completion events preserve the old flag policy.

    cache.Prepare(table,a);
    assert(table.count==0 && !cache.TakeRediscovered() && !cache.Overflowed());
    cache.Append(table,6,6,first.attributes,0);
    assert(table.count==0 && !cache.State().replace_pending);
    cache.Append(table,6,6,first.attributes,std::numeric_limits<std::uint32_t>::max());
    assert(table.count==10); // Advertised count cannot overrun native ten-slot event.

    // Exact capacity is usable. Only an actual extra attribute latches overflow until Prepare.
    cache.Prepare(table,a);
    for(unsigned i=0;i<6;++i)assert(cache.Append(table,6,6,first.attributes,10)==CacheAppendResult::Appended);
    assert(cache.Append(table,6,6,first.attributes,4)==CacheAppendResult::Appended);
    assert(table.count==64 && !cache.Overflowed());
    assert(cache.Append(table,6,6,first.attributes,0)==CacheAppendResult::Appended);
    const auto full=table;
    assert(cache.Append(table,6,6,first.attributes,1)==CacheAppendResult::Overflow);
    assert(table==full && cache.Overflowed());
    cache.Complete();cache.TakeRediscovered();
    cache.Append(table,6,6,first.attributes,0);
    assert(cache.Overflowed());
    cache.Prepare(table,b);
    assert(table.address==b && table.count==0 && !cache.Overflowed() && !cache.TakeRediscovered());

    // The firmware supplies no epoch here. An old record carrying the SAME reused ID is
    // indistinguishable from current data; this limitation is explicit, not a claimed stale-event fix.
    assert(cache.Append(table,6,6,first.attributes,1)==CacheAppendResult::Appended);
    assert(table.count==1);
    Attribute small[2]{{1,2},{3,4}};
    cache.Prepare(table,a);
    cache.Append(table,6,6,small,99);
    assert(table.count==2); // Generic helper also respects a smaller physical array extent.
    Attribute large[12]{};
    cache.Prepare(table,a);
    cache.Append(table,6,6,large,12);
    assert(table.count==10); // Preserve the original maximum even if a future caller offers more.

    // 50,000 deterministic mixed events through the actual production helper and old implementation.
    DiscoveryCache actual;
    Table actual_table;
    OldPolicy expected;
    std::uint32_t random=0x91a2b3c4;
    for(unsigned step=0;step<50000;++step) {
        random^=random<<13;random^=random>>17;random^=random<<5;
        switch(random%7) {
            case 0: {const auto &address=(random&0x100)?a:b;actual.Prepare(actual_table,address);expected.Prepare(address);break;}
            case 1: actual.Complete();expected.Complete();break;
            case 2: assert(actual.TakeRediscovered()==expected.Take());break;
            default: {
                const auto event=(random&0x200)?6u:7u;
                const auto count=(random&0x400)?std::numeric_limits<std::uint32_t>::max():((random>>12)%12);
                const auto batch=MakeBatch(random&0x3fff);
                assert(actual.Append(actual_table,event,6,batch.attributes,count)==expected.Append(event,6,batch,count));
                break;
            }
        }
        assert(actual_table==expected.table && actual.State()==expected.state);
        assert(actual.Overflowed()==expected.state.overflowed);
    }
    std::puts("Production GATT cache: fragmented pairing cache, filtering, resets, capacity/overflow, rediscovery, explicit limitations and 50000-event old-policy equivalence passed");
}
