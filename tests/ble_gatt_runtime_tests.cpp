// SPDX-License-Identifier: GPL-2.0-only
// Compile the real production translation unit, not a reimplementation of its state machine.
#include "../mc_mitm/source/ble/ble_gatt.cpp"
#include <cassert>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace gatt_test {
    using namespace ams::ble;
    enum class Fault {None,BeforeDiscovery,DuringMtu,ReadConflict,CccdConflict,ReadTimeout,
        ReadRadioOff,ReadDisconnected,ReadShutdown,ReadRadioOffReply,ReadDisconnectedReply,ReadShutdownReply,
        ReadRejected,ReadError,ReadZero,ReadOversize,
        WrongThenRight,ReadBeforeIpc,CccdTimeout,CccdRejected,CccdError,CccdRadioOffReply,
        CccdShutdownReply,NotifyRejected,NotifyRadioOff};
    Fault fault=Fault::None;
    bool radio=true,disconnected=false,shutdown=false;
    u32 results=0,failure=0;
    unsigned reads=0,writes=0,disconnects=0;
    std::vector<std::string> calls;
    std::vector<std::pair<unsigned,unsigned>> bindings;
    std::deque<EventRecord> ready;
    std::multimap<s64,std::function<void()>> jobs;
    std::vector<BtdrvGattAttribute> attributes;
    constexpr u32 connection=6;
    constexpr ams::bluetooth::Address address{{1,2,3,4,5,6}};
    // Small valid input report map; captured Stadia map/controls have their own fixture tests.
    const std::vector<u8> report_map{0x05,0x01,0x09,0x05,0xa1,0x01,0x85,0x03,0x75,0x08,0x95,0x0a,0x81,0x02,0xc0};
    u16 Uuid16Value(const BtdrvGattAttributeUuid &u){return u.uuid[0]|(static_cast<u16>(u.uuid[1])<<8);}
    BtdrvGattAttribute Attr(u16 handle,u16 type,u16 uuid,u8 instance=0){
        BtdrvGattAttribute a{};a.handle=handle;a.type=type;a.id.uuid=MakeUuid16(uuid);a.id.instance_id=instance;
        if(type==BtdrvGattAttributeType_Service){a.group_end_handle=20;a.is_primary=true;}
        if(type==BtdrvGattAttributeType_Characteristic)a.property=BtdrvGattCharacteristicProperty_Read;
        return a;
    }
    std::vector<BtdrvGattAttribute> Base(){
        auto first=Attr(6,1,GattCharReport),second=Attr(10,1,GattCharReport,1);
        first.property=second.property=BtdrvGattCharacteristicProperty_Notify;
        return {Attr(1,3,GattServiceHid),Attr(2,1,GattCharHidInformation),Attr(4,1,GattCharReportMap),
            first,Attr(7,2,GattDescClientCharConfig),second,Attr(11,2,GattDescClientCharConfig,1)};
    }
    EventRecord Cache(const std::vector<BtdrvGattAttribute>&values){
        assert(values.size()<=10);EventRecord e{};e.type=BtdrvBleEventType_ClientCacheSave;
        auto &c=e.info.client_cache_save;c.conn_id=connection;c.count=values.size();
        std::copy(values.begin(),values.end(),c.attr_list);return e;
    }
    void Later(s64 delay,std::function<void()>fn){jobs.emplace(now_ms+delay,std::move(fn));}
    void Queue(s64 delay,EventRecord e){Later(delay,[e]{ready.push_back(e);});}
    void BadCache(s64 delay){auto a=attributes[1];a.property^=BtdrvGattCharacteristicProperty_Write;Queue(delay,Cache({a}));}
    EventRecord Reply(u16 uuid,const std::vector<u8>&data,u32 conn=connection,u32 rc=0){
        EventRecord e{};e.type=BtdrvBleEventType_ClientNotify;auto&n=e.info.client_notify;
        n.conn_id=conn;n.result=rc;n.char_uuid=MakeUuid16(uuid);n.size=data.size();
        assert(data.size()<=sizeof(n.data));std::copy(data.begin(),data.end(),n.data);return e;
    }
    unsigned Count(const std::string &call){return std::count(calls.begin(),calls.end(),call);}
}

namespace ams::ble::events {
    u32 GetConnection(){return gatt_test::connection;}
    bool IsDisconnected(){return gatt_test::disconnected;}
    bool IsRadioEnabled(){return gatt_test::radio;}
    bool IsShuttingDown(){return gatt_test::shutdown;}
    u32 GetGattResultCount(){return gatt_test::results;}
    u32 GetGattFailure(){return gatt_test::failure;}
    void ClearGattFailure(){gatt_test::failure=0;}
    void NoteGattResult(u32 result){++gatt_test::results;if(result)gatt_test::failure=result;}
    Sink GetSink(){return gatt::OnEvent;}
    void Log(const EventRecord&){}
    bool Pop(EventRecord*out){
        using namespace gatt_test;
        while(!jobs.empty()&&jobs.begin()->first<=now_ms){auto fn=std::move(jobs.begin()->second);jobs.erase(jobs.begin());fn();}
        if(ready.empty())return false;
        *out=ready.front();ready.pop_front();return true;
    }
    void WaitForQueue(TimeSpan duration){
        using namespace gatt_test;
        auto end=now_ms+duration.GetMilliSeconds();
        now_ms=jobs.empty()?end:std::min(end,std::max(now_ms,jobs.begin()->first));
    }
    // Verbatim production implementation, using the real WaitFor/Pump above.
#include "ble-wait-result.inc"
}
namespace ams::ble::log {void Write(const char*,...){} bool IsEnabled(){return false;}}
namespace ams::ble::diagnostics {
    void Note(Metric){}
    void Dump(const char*point){
        if(gatt_test::fault==gatt_test::Fault::ReadBeforeIpc&&std::strcmp(point,"read-begin")==0)gatt_test::radio=false;
    }
}

// These are the only driver operations linked into the host test. No SDK service implementation,
// network, filesystems, pairing, radio cycling, output characteristics or controller is available.
::Result btdrvGetGattAttribute(BtdrvAddress,u32){gatt_test::calls.push_back("attributes");return 0;}
::Result btdrvConfigureAttMtu(u32 conn,u16 mtu){
    using namespace gatt_test;assert(conn==connection&&mtu==0x100);calls.push_back("mtu");
    if(fault==Fault::DuringMtu)BadCache(1);
    EventRecord e{};e.type=BtdrvBleEventType_ClientConfigureMtu;e.info.client_configure_mtu.conn_id=conn;
    Queue(2,e);return 0;
}
::Result btdrvRegisterGattHidDataPath(const BtdrvGattAttributeUuid*u){
    assert(gatt_test::Uuid16Value(*u)==ams::ble::GattServiceHid);gatt_test::calls.push_back("hid-path");return 0;
}
::Result btdrvReadGattCharacteristic(u32 conn,bool primary,const BtdrvGattId*service,const BtdrvGattId*characteristic,u8 auth){
    using namespace gatt_test;
    assert(conn==connection&&primary&&Uuid16Value(service->uuid)==GattServiceHid&&auth==BtdrvGattAuthReqType_NoMitm);
    calls.push_back("read");++reads;auto uuid=Uuid16Value(characteristic->uuid);
    auto data=uuid==GattCharReportMap?report_map:std::vector<u8>{0x11,0x01,0,0x02};
    if(reads==1){
        switch(fault){
            case Fault::ReadConflict:BadCache(1);break;
            case Fault::ReadTimeout:return 0;
            case Fault::ReadRadioOff:Later(1,[]{radio=false;});return 0;
            case Fault::ReadDisconnected:Later(1,[]{disconnected=true;});return 0;
            case Fault::ReadShutdown:Later(1,[]{shutdown=true;});return 0;
            case Fault::ReadRadioOffReply:Later(1,[]{radio=false;});break;
            case Fault::ReadDisconnectedReply:Later(1,[]{disconnected=true;});break;
            case Fault::ReadShutdownReply:Later(1,[]{shutdown=true;});break;
            case Fault::ReadRejected:return 0xdead;
            case Fault::ReadError:Queue(2,Reply(uuid,{},connection,26));return 0;
            case Fault::ReadZero:Queue(2,Reply(uuid,{}));return 0;
            case Fault::ReadOversize:{auto e=Reply(uuid,{});e.info.client_notify.size=513;Queue(2,e);return 0;}
            case Fault::WrongThenRight:
                Queue(1,Reply(uuid,{0xee},connection+1));Queue(1,Reply(uuid+1,{0xdd}));
                Queue(3,Reply(uuid,{0xbb}));break;
            default:break;
        }
    }
    // For WrongThenRight, the genuine response precedes a duplicate in the same Pump.
    Queue(2,Reply(uuid,data));return 0;
}
::Result btdrvRegisterGattNotification(u32 conn,bool primary,const BtdrvGattId*service,const BtdrvGattId*characteristic){
    using namespace gatt_test;
    assert(conn==connection&&primary&&Uuid16Value(service->uuid)==GattServiceHid&&Uuid16Value(characteristic->uuid)==GattCharReport);
    calls.push_back("notify");if(fault==Fault::NotifyRadioOff)radio=false;
    return fault==Fault::NotifyRejected?0xdead:0;
}
::Result btdrvWriteGattDescriptor(u32 conn,bool primary,const BtdrvGattId*service,const BtdrvGattId*characteristic,
                                  const BtdrvGattId*descriptor,const void*data,size_t size,u8 auth){
    using namespace gatt_test;
    assert(conn==connection&&primary&&Uuid16Value(service->uuid)==GattServiceHid&&Uuid16Value(characteristic->uuid)==GattCharReport);
    assert(Uuid16Value(descriptor->uuid)==GattDescClientCharConfig&&size==2&&auth==BtdrvGattAuthReqType_NoMitm);
    const auto*bytes=static_cast<const u8*>(data);assert(bytes[0]==1&&bytes[1]==0);
    calls.push_back("cccd");++writes;bindings.emplace_back(characteristic->instance_id,descriptor->instance_id);
    if(fault==Fault::CccdRejected)return 0xdead;
    if(fault==Fault::CccdTimeout)return 0;
    if(fault==Fault::CccdConflict&&writes==1)BadCache(1);
    if(fault==Fault::CccdRadioOffReply)Later(1,[]{radio=false;});
    if(fault==Fault::CccdShutdownReply)Later(1,[]{shutdown=true;});
    Queue(2,Reply(GattCharReport,{},connection,fault==Fault::CccdError?26:0));return 0;
}
namespace ams::ble::host_fixture {
    void Disconnect(){++gatt_test::disconnects;gatt_test::disconnected=true;}
    // Exact post-pair host setup, StopAfterSetupFailure and retry gate. Not the whole HostThreadFunc.
#include "ble-host-setup.inc"
}

namespace gatt_test {
    namespace host=ams::ble::host_fixture;
    void Reset(Fault f=Fault::None,std::vector<BtdrvGattAttribute>table=Base()){
        // Do not hide a leaked production pending read between test cases.
        assert(!gatt::g_read.pending);
        now_ms=0;fault=f;radio=true;disconnected=shutdown=false;results=failure=0;
        reads=writes=disconnects=0;calls.clear();bindings.clear();ready.clear();jobs.clear();attributes=std::move(table);
        host::g_setup_failed=false;gatt::PrepareDiscovery(address);gatt::OnEvent(Cache(attributes));
        if(fault==Fault::BeforeDiscovery)BadCache(0);
    }
    void Failed(Fault f,unsigned expected_reads,unsigned expected_writes){
        Reset(f);gatt::HidService hid{};
        assert(!host::SetupOnce(connection,address,&hid));
        assert(host::g_setup_failed&&disconnects==1&&!gatt::g_read.pending);
        assert(reads==expected_reads&&writes==expected_writes);
        auto before=calls;assert(!host::SetupOnce(connection,address,&hid));
        assert(calls==before&&disconnects==1); // Actual sticky host retry gate makes no further setup calls.
        auto size=gatt::g_read.size;
        Queue(0,Reply(GattCharHidInformation,{0xfa}));events::Pump(ams::TimeSpan::FromMilliSeconds(1));
        assert(!gatt::g_read.pending&&gatt::g_read.size==size&&calls==before);
    }
    void BadOwnership(std::vector<BtdrvGattAttribute>table){
        Reset(Fault::None,std::move(table));gatt::HidService hid{};
        assert(!host::SetupOnce(connection,address,&hid));
        assert(host::g_setup_failed&&disconnects==1&&reads==0&&writes==0&&Count("notify")==0);
    }
}
int main(){
    using namespace gatt_test;
    // Healthy setup executes real discovery, read completion and two CCCD waits, with retained map.
    Reset();ams::ble::gatt::HidService hid{};assert(host::SetupOnce(connection,address,&hid));
    assert(reads==2&&writes==2&&!host::g_setup_failed&&!gatt::g_read.pending);
    assert((bindings==std::vector<std::pair<unsigned,unsigned>>{{0,0},{1,1}}));
    gatt::ReportFacts facts{};assert(R_SUCCEEDED(gatt::Describe(connection,hid,&facts)));
    assert(facts.report_map_parsed&&facts.input_report_id==3&&facts.input_report_size==10&&reads==2);
    auto unchanged=calls;Queue(0,Cache(attributes));events::Pump(ams::TimeSpan::FromMilliSeconds(1));
    assert(!gatt::TakeRediscovered()&&calls==unchanged);
    // Actual connected-host rediscovery branch rejects a partial replacement without new requests.
    Queue(0,Cache({Attr(18,1,0x2a19)}));events::Pump(ams::TimeSpan::FromMilliSeconds(1));
    host::RediscoverOnce(connection);assert(host::g_setup_failed&&calls==unchanged);
    assert(!host::SetupOnce(connection,address,&hid)&&calls==unchanged);

    Failed(Fault::BeforeDiscovery,0,0);assert(calls.empty());
    Failed(Fault::DuringMtu,0,0);assert(Count("hid-path")==0&&Count("mtu")==1);
    Failed(Fault::ReadConflict,1,0);assert(Count("notify")==0);
    Failed(Fault::CccdConflict,2,1);assert(Count("notify")==1);
    Failed(Fault::ReadTimeout,1,0);assert(now_ms>=35'000);
    for(auto f:{Fault::ReadRadioOff,Fault::ReadDisconnected,Fault::ReadShutdown,Fault::ReadRejected,
                Fault::ReadError,Fault::ReadZero,Fault::ReadOversize,Fault::ReadRadioOffReply,
                Fault::ReadDisconnectedReply,Fault::ReadShutdownReply})Failed(f,1,0);
    Failed(Fault::CccdTimeout,2,1);Failed(Fault::CccdRejected,2,1);Failed(Fault::CccdError,2,1);
    Failed(Fault::NotifyRejected,2,0);assert(Count("notify")==1);
    Failed(Fault::ReadBeforeIpc,0,0);Failed(Fault::NotifyRadioOff,2,0);
    Failed(Fault::CccdRadioOffReply,2,1);Failed(Fault::CccdShutdownReply,2,1);

    // Copy-out remains untouched even when invalidation AND a valid response arrive in one Pump.
    Reset(Fault::ReadConflict);assert(R_SUCCEEDED(gatt::Discover(connection,address)));
    hid.service=attributes.front();u8 out[16];std::fill(std::begin(out),std::end(out),0xa5);size_t length=777;
    assert(R_FAILED(gatt::Read(connection,hid,ams::ble::GattCharHidInformation,out,sizeof(out),&length)));
    assert(!gatt::g_read.pending&&length==777&&std::all_of(std::begin(out),std::end(out),[](u8 b){return b==0xa5;}));
    for(auto f:{Fault::ReadRadioOffReply,Fault::ReadDisconnectedReply,Fault::ReadShutdownReply}){
        Reset(f);assert(R_SUCCEEDED(gatt::Discover(connection,address)));hid.service=attributes.front();
        std::fill(std::begin(out),std::end(out),0xa5);length=777;
        assert(R_FAILED(gatt::Read(connection,hid,ams::ble::GattCharHidInformation,out,sizeof(out),&length)));
        assert(!gatt::g_read.pending&&length==777&&std::all_of(std::begin(out),std::end(out),[](u8 b){return b==0xa5;}));
    }
    Reset(Fault::WrongThenRight);assert(R_SUCCEEDED(gatt::Discover(connection,address)));hid.service=attributes.front();
    assert(R_SUCCEEDED(gatt::Read(connection,hid,ams::ble::GattCharHidInformation,out,sizeof(out),&length)));
    assert(length==4&&out[0]==0x11&&!gatt::g_read.pending);

    // Already-unavailable links cannot start discovery, reads or subscription IPC.
    for(unsigned state=0;state<3;++state){
        Reset();assert(R_SUCCEEDED(gatt::Discover(connection,address)));hid.service=attributes.front();calls.clear();
        if(state==0)radio=false;else if(state==1)disconnected=true;else shutdown=true;
        assert(R_FAILED(gatt::Read(connection,hid,ams::ble::GattCharHidInformation,out,sizeof(out),&length)));
        assert(R_FAILED(gatt::Subscribe(connection,&hid)));
        assert(R_FAILED(gatt::Discover(connection,address)));
        assert(calls.empty()&&!gatt::g_read.pending);
    }

    // Unsorted rows still bind by handles; included-service references do not create owners.
    auto reversed=Base();std::reverse(reversed.begin(),reversed.end());
    reversed.push_back(Attr(3,0,0x180f));Reset(Fault::None,reversed);
    assert(host::SetupOnce(connection,address,&hid)&&writes==2);
    std::sort(bindings.begin(),bindings.end());assert((bindings==std::vector<std::pair<unsigned,unsigned>>{{0,0},{1,1}}));

    auto service=Attr(1,3,ams::ble::GattServiceHid),report=Attr(6,1,ams::ble::GattCharReport);
    report.property=BtdrvGattCharacteristicProperty_Notify;service.group_end_handle=8;
    auto other=Attr(9,3,0x180f);other.group_end_handle=20;
    BadOwnership({service,report,other,Attr(11,2,ams::ble::GattDescClientCharConfig)}); // Adjacent orphan CCCD.
    service.group_end_handle=20;
    BadOwnership({service,report,Attr(8,1,0x2a19),Attr(9,2,ams::ble::GattDescClientCharConfig)}); // Intervening char.
    BadOwnership({service,report,Attr(7,2,ams::ble::GattDescClientCharConfig),Attr(8,2,ams::ble::GattDescClientCharConfig,1)});
    auto overlap=Attr(5,3,0x180f);overlap.group_end_handle=18;
    BadOwnership({service,report,Attr(7,2,ams::ble::GattDescClientCharConfig),overlap});
    auto nested=Attr(9,3,0x180f);nested.group_end_handle=15;
    BadOwnership({service,report,Attr(11,2,ams::ble::GattDescClientCharConfig),nested});
    service.group_end_handle=8;
    BadOwnership({service,report,Attr(10,2,ams::ble::GattDescClientCharConfig)}); // Outside end.
    Reset(Fault::None,{service,report,Attr(8,2,ams::ble::GattDescClientCharConfig),other});
    assert(host::SetupOnce(connection,address,&hid)&&writes==1); // Inclusive end and adjacent valid service.
    service.group_end_handle=1;BadOwnership({service,report,Attr(7,2,ams::ble::GattDescClientCharConfig)});
    std::puts("Production GATT runtime: real event pumps, discovery/read/CCCD invalidation, pending cleanup, host setup latch/retry gate and same-service descriptor ownership passed (mock SDK/clock only)");
}
