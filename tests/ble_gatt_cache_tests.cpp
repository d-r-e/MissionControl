// SPDX-License-Identifier: GPL-2.0-only
#include "ble_gatt_cache.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>
#include <tuple>
#include <vector>
using namespace ams::ble::gatt;
using Address=std::array<std::uint8_t,6>;
struct Uuid { std::uint32_t size=2; std::array<std::uint8_t,16> uuid{}; bool operator==(const Uuid&)const=default; };
struct Id { std::uint8_t instance_id{}; std::array<std::uint8_t,3> pad{}; Uuid uuid{}; bool operator==(const Id&)const=default; };
struct Attribute {
    Id id{};std::uint16_t type=1,handle{},group_end_handle{};std::uint8_t property=0x12;bool is_primary{};
    bool operator==(const Attribute&)const=default;
};
// Match the production assertions against the pinned SDK, while remaining SDK-free on the host.
static_assert(sizeof(Uuid)==20 && offsetof(Uuid,uuid)==4);
static_assert(sizeof(Id)==24 && offsetof(Id,uuid)==4);
static_assert(sizeof(Attribute)==32 && offsetof(Attribute,id)==0 && offsetof(Attribute,type)==24 &&
    offsetof(Attribute,handle)==26 && offsetof(Attribute,group_end_handle)==28 &&
    offsetof(Attribute,property)==30 && offsetof(Attribute,is_primary)==31);
struct Table {Address address{};std::uint32_t count{};std::array<Attribute,64> attributes{};bool operator==(const Table&)const=default;};
struct Batch {Attribute attributes[10]{};};
static Attribute Make(unsigned handle,unsigned type=1) {
    Attribute a;a.handle=handle;a.type=type;
    a.id.uuid.uuid[0]=static_cast<std::uint8_t>(handle);a.id.uuid.uuid[1]=0x2a;
    if(type==3){a.group_end_handle=handle+8;a.is_primary=true;}
    return a;
}
static Batch MakeBatch(unsigned first) {Batch b;for(unsigned i=0;i<10;++i)b.attributes[i]=Make(first+i);return b;}

// Independent semantic reference: canonical keys + transactional map, not the production
// comparator or insertion code. Original bytes of first occurrences are retained in input order.
using Key=std::tuple<unsigned,unsigned,unsigned,unsigned,std::vector<std::uint8_t>,unsigned,bool>;
static Key KeyOf(const Attribute&a) {
    return {a.type,a.group_end_handle,a.id.instance_id,a.id.uuid.size,
        {a.id.uuid.uuid.begin(),a.id.uuid.uuid.begin()+a.id.uuid.size},
        a.type==1?static_cast<unsigned>(a.property):0u,a.type==3?a.is_primary:false};
}
struct Reference {
    CacheState state{};std::vector<Attribute> items;std::map<unsigned,Key> definitions;
    void Prepare(){state={};state.replace_pending=true;items.clear();definitions.clear();}
    void Complete(){state.discovery_complete=true;state.replace_pending=false;}
    bool Take(){bool result=state.rediscovered;state.rediscovered=false;return result;}
    CacheAppendResult Fail(CacheFailure f) {
        state.failure=f;state.rediscovered=state.discovery_complete;
        switch(f){
            case CacheFailure::Overflow:return CacheAppendResult::Overflow;
            case CacheFailure::Conflict:return CacheAppendResult::Conflict;
            case CacheFailure::Changed:return CacheAppendResult::Changed;
            case CacheFailure::DriverError:return CacheAppendResult::DriverError;
            default:return CacheAppendResult::Malformed;
        }
    }
    CacheAppendResult Append(unsigned event,const Batch&batch,std::uint32_t count,unsigned status) {
        if(event!=6)return CacheAppendResult::Ignored;
        if(state.failure!=CacheFailure::None)return CacheAppendResult::Rejected;
        if(status)return Fail(CacheFailure::DriverError);
        auto trial=definitions;std::vector<Attribute> new_items;
        for(unsigned i=0;i<std::min<std::uint32_t>(count,10);++i){
            const auto&a=batch.attributes[i];unsigned n=a.id.uuid.size;
            if(!a.handle || a.type>3 || (n!=2&&n!=4&&n!=16) || (a.type==3&&a.group_end_handle<a.handle))
                return Fail(CacheFailure::Malformed);
            auto key=KeyOf(a);auto found=trial.find(a.handle);
            if(found!=trial.end()){if(found->second!=key)return Fail(CacheFailure::Conflict);continue;}
            if(state.discovery_complete)return Fail(CacheFailure::Changed);
            trial.emplace(a.handle,key);
            if(trial.size()>64)return Fail(CacheFailure::Overflow);
            new_items.push_back(a);
        }
        definitions=trial;items.insert(items.end(),new_items.begin(),new_items.end());state.replace_pending=false;
        return new_items.empty()?CacheAppendResult::Unchanged:CacheAppendResult::Appended;
    }
};

int main() {
    constexpr Address a{1,2,3,4,5,6},b{7,8,9,10,11,12};
    auto first=MakeBatch(1),second=MakeBatch(11),third=MakeBatch(21);
    // Semantic identity ignores padding/tails and only the documented inapplicable fields.
    auto x=Make(1),y=x;y.id.pad.fill(0xff);y.id.uuid.uuid[15]=0xff;y.is_primary=true;
    assert(SameCacheAttribute(x,y));y.property^=1;assert(!SameCacheAttribute(x,y));
    y=x;y.id.instance_id=1;assert(!SameCacheAttribute(x,y));
    y=x;y.handle=2;assert(!SameCacheAttribute(x,y));
    y=x;y.type=2;assert(!SameCacheAttribute(x,y));
    y=x;y.id.uuid.size=4;assert(!SameCacheAttribute(x,y));
    x.id.uuid.size=16;y=x;y.id.uuid.uuid[15]=1;assert(!SameCacheAttribute(x,y));
    x=Make(1);
    y=x;y.id.uuid.uuid[0]^=1;assert(!SameCacheAttribute(x,y));
    y=x;y.group_end_handle=7;assert(!SameCacheAttribute(x,y));
    for(unsigned n:{2,4,16}){x.id.uuid.size=n;assert(ValidCacheAttribute(x));assert(SameCacheAttribute(x,x));}
    for(unsigned n:std::array<std::uint32_t,5>{0,1,3,17,UINT32_MAX}){x.id.uuid.size=n;assert(!ValidCacheAttribute(x));assert(!SameCacheAttribute(x,x));}
    x=Make(1,2);y=x;y.property^=1;y.is_primary=true;assert(SameCacheAttribute(x,y));
    x=Make(1,3);y=x;y.property^=1;assert(SameCacheAttribute(x,y));y.is_primary=false;assert(!SameCacheAttribute(x,y));
    x.group_end_handle=0;assert(!ValidCacheAttribute(x));assert(!ValidCacheAttribute(Make(0)));assert(!ValidCacheAttribute(Make(1,4)));
    assert(ValidCacheAttribute(Make(1,0))); // Included service type is legitimate.

    DiscoveryCache cache;Table table;
    auto append=[&](const Batch&batch,unsigned count,unsigned event=6,unsigned status=0){return cache.Append(table,event,6,batch.attributes,count,status);};
    cache.Prepare(table,a);const auto prepared=cache.State();
    assert(append(first,10,5,1)==CacheAppendResult::Ignored && cache.State()==prepared && table.count==0);
    assert(append(first,10)==CacheAppendResult::Appended);
    assert(append(second,10)==CacheAppendResult::Appended);
    assert(append(third,7)==CacheAppendResult::Appended);assert(table.count==27);
    for(unsigned i=0;i<27;++i)assert(table.attributes[i].handle==i+1);
    const auto paired=table;
    assert(append(first,10)==CacheAppendResult::Unchanged && table==paired);
    cache.Complete();
    assert(append(first,10)==CacheAppendResult::Unchanged && !cache.TakeRediscovered());
    assert(append(first,0)==CacheAppendResult::Unchanged && !cache.TakeRediscovered());
    assert(append(first,10,7)==CacheAppendResult::Ignored && table==paired);
    auto novel=MakeBatch(40);
    assert(append(novel,1)==CacheAppendResult::Changed && table==paired && cache.Unusable());
    assert(cache.TakeRediscovered() && !cache.TakeRediscovered());
    assert(append(first,10)==CacheAppendResult::Rejected);

    // Conflicts and malformed tails never append a valid-looking prefix.
    cache.Prepare(table,a);append(first,1);const auto before=table;
    auto conflict=MakeBatch(40);conflict.attributes[1]=first.attributes[0];conflict.attributes[1].property^=1;
    assert(append(conflict,2)==CacheAppendResult::Conflict && table==before);
    assert(append(first,0,6,1)==CacheAppendResult::Rejected && cache.State().failure==CacheFailure::Conflict);
    cache.Complete();assert(cache.Unusable());
    cache.Prepare(table,b);assert(table.address==b && !cache.Unusable() && !cache.TakeRediscovered());
    conflict=first;conflict.attributes[1]=first.attributes[0];conflict.attributes[1].id.instance_id++;
    assert(append(conflict,2)==CacheAppendResult::Conflict && table.count==0);
    cache.Prepare(table,a);auto malformed=first;malformed.attributes[1].id.uuid.size=99;
    assert(append(malformed,2)==CacheAppendResult::Malformed && table.count==0);
    cache.Prepare(table,a);assert(append(first,0,6,1)==CacheAppendResult::DriverError && table.count==0);
    cache.Prepare(table,a);table.count=65;cache.Complete();
    assert(append(first,1)==CacheAppendResult::Malformed);

    // Exact replay works at capacity; an overflowing batch is atomic and failure is sticky.
    cache.Prepare(table,a);
    for(unsigned i=0;i<6;++i){auto batch=MakeBatch(1+i*10);assert(append(batch,10)==CacheAppendResult::Appended);}
    auto last=MakeBatch(61);assert(append(last,3)==CacheAppendResult::Appended && table.count==63);
    const auto nearly_full=table;
    auto extra=MakeBatch(64);assert(append(extra,2)==CacheAppendResult::Overflow && table==nearly_full);
    assert(cache.Overflowed() && cache.Unusable());
    cache.Prepare(table,a);
    for(unsigned i=0;i<6;++i){auto batch=MakeBatch(1+i*10);append(batch,10);}
    append(last,4);assert(table.count==64 && !cache.Unusable());const auto full=table;
    assert(append(first,10)==CacheAppendResult::Unchanged && table==full);
    cache.Complete();assert(append(last,4)==CacheAppendResult::Unchanged && !cache.TakeRediscovered());

    // Native count clamp and physical extent are retained; duplicates within a batch collapse.
    cache.Prepare(table,a);assert(append(first,UINT32_MAX)==CacheAppendResult::Appended && table.count==10);
    cache.Prepare(table,a);auto dup=first;dup.attributes[1]=dup.attributes[0];
    assert(append(dup,2)==CacheAppendResult::Appended && table.count==1);
    cache.Prepare(table,a);Attribute small[2]{Make(1),Make(2)};
    cache.Append(table,6,6,small,99);assert(table.count==2);
    cache.Prepare(table,a);Attribute large[12];for(unsigned i=0;i<12;++i)large[i]=Make(i+1);
    cache.Append(table,6,6,large,12);assert(table.count==10);
    // Same-address/recycled-ID Prepare clears local state, not the driver's queue. A stale
    // record carrying the same ID is still indistinguishable, and is NOT claimed solved here.
    cache.Prepare(table,a);assert(append(first,1)==CacheAppendResult::Appended);

    // Retain the old literal append oracle for the domain whose behavior must not change:
    // healthy fresh discovery with unique attributes (including foreign and empty batches).
    std::uint32_t random=0x91a2b3c4;
    auto next=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return random;};
    for(unsigned run=0;run<1000;++run){
        cache.Prepare(table,a);Table legacy;legacy.address=a;unsigned next_handle=1;
        for(unsigned event=0;event<16;++event){
            auto r=next();unsigned n=std::min<unsigned>((r>>8)%11,64-legacy.count);
            auto batch=MakeBatch(next_handle);unsigned connection=(r&1)?6:7;
            append(batch,n,connection);
            if(connection==6){for(unsigned i=0;i<std::min<unsigned>(n,10);++i)legacy.attributes[legacy.count++]=batch.attributes[i];next_handle+=n;}
            assert(table.address==legacy.address && table.count==legacy.count);
            for(unsigned i=0;i<table.count;++i)assert(table.attributes[i]==legacy.attributes[i]);
        }
        cache.Complete();assert(!cache.Unusable() && !cache.TakeRediscovered());
    }

    // Differential replay against an independently implemented transactional semantic model.
    DiscoveryCache actual;Table actual_table;Reference reference;
    for(unsigned step=0;step<50000;++step){
        auto r=next();
        switch(r%9){
            case 0:actual.Prepare(actual_table,a);reference.Prepare();break;
            case 1:actual.Complete();reference.Complete();break;
            case 2:assert(actual.TakeRediscovered()==reference.Take());break;
            default:{
                auto batch=MakeBatch(1+(r>>12)%70);
                if(r&0x100)batch.attributes[1]=batch.attributes[0];
                if(r&0x200)batch.attributes[1].property^=1;
                if(r&0x400)batch.attributes[2].id.uuid.size=3;
                unsigned connection=(r&0x800)?6:7,status=(r&0x1000)?1:0;
                auto count=(r&0x2000)?UINT32_MAX:(r>>20)%11;
                assert(actual.Append(actual_table,connection,6,batch.attributes,count,status)==reference.Append(connection,batch,count,status));
                break;
            }
        }
        assert(actual.State()==reference.state && actual_table.count==reference.items.size());
        for(unsigned i=0;i<actual_table.count;++i)assert(actual_table.attributes[i]==reference.items[i]);
        assert(actual.Unusable()==(reference.state.failure!=CacheFailure::None));
    }
    std::puts("Production cache: semantic equality, transactional dedup/conflict/overflow, late-change rejection, legacy healthy replay and 50000 independent-model events passed");
}
