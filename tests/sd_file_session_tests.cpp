#include "sd_file_session.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
#include <cstdlib>

namespace fs = std::filesystem;
namespace sd = missioncontrol::probe::sd;
struct FakeChannel {
    std::vector<unsigned char> input, output;
    std::size_t position = 0;
    unsigned replies = 0;
    bool fail_reply = false, fail_payload = false;
    bool Transfer(void *buffer, std::size_t size, bool sending) {
        auto *p = static_cast<unsigned char *>(buffer);
        if (sending) {
            if (fail_payload && size != 48) return false;
            output.insert(output.end(), p, p+size); return true;
        }
        if (size > input.size()-position) return false;
        std::memcpy(p, input.data()+position, size); position += size; return true;
    }
    bool Reply(sd::Result result, std::uint32_t size=0, const unsigned char *hash=nullptr) {
        ++replies;
        if (fail_reply) return false;
        unsigned char header[48]{}; std::memcpy(header,"MCSDR001",8);
        sd::Store32(header+8,static_cast<std::uint32_t>(result)); sd::Store32(header+12,size);
        if (hash) std::memcpy(header+16,hash,32);
        return Transfer(header,48,true);
    }
};
FakeChannel Request(sd::Operation op, const std::string &path, const std::string &body="",
                    const std::string *old=nullptr) {
    FakeChannel c; c.input.resize(88);
    auto *p=c.input.data(); std::memcpy(p,"MCSD0001",8);
    sd::Store32(p+8,static_cast<std::uint32_t>(op)); sd::Store32(p+12,path.size());
    sd::Store32(p+16,body.size()); sd::Store32(p+20,old ? 1 : 0);
    if (old) sha256CalculateHash(p+24,old->data(),old->size());
    sha256CalculateHash(p+56,body.data(),body.size());
    c.input.insert(c.input.end(),path.begin(),path.end()); c.input.insert(c.input.end(),body.begin(),body.end());
    return c;
}
std::string Read(const fs::path &path) {
    std::ifstream f(path,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}};
}
void Write(const fs::path &path,const std::string &value) { std::ofstream(path,std::ios::binary)<<value; }
void ResetCode() {
    g_code_init_rc = 0; g_normal_fs_rc = 0; g_code_fail_stage = 0;
    g_code_read_calls = 0; g_code_fail_read_call = 0; g_code_short_read = false;
    g_bad_content_path = false; g_code_file_size = 256; g_old_code_version = false;
    g_code_init_calls = g_code_exit_calls = g_ncm_calls = 0;
    g_code_program = 0; g_code_calls.clear();
    std::memset(g_code_data, 0, sizeof(g_code_data));
    std::memcpy(g_code_data, "NSO0", 4);
}
unsigned Calls(unsigned id) {
    unsigned count = 0; for (const auto call : g_code_calls) count += call == id; return count;
}
void Failure(const FakeChannel &channel, sd::Result result, sd::CodeRoute route,
             sd::CodeStage stage, ::Result ipc) {
    assert(channel.output.size() == 48 && channel.replies == 1);
    const auto *out = channel.output.data();
    assert(sd::Load32(out + 8) == static_cast<unsigned>(result));
    assert(sd::Load32(out + 12) == 0 && sd::Load32(out + 16) == ipc);
    assert(sd::Load32(out + 20) == sd::CodeDetailMagic);
    assert(sd::Load32(out + 24) == 1);
    assert(sd::Load32(out + 28) == static_cast<unsigned>(route));
    assert(sd::Load32(out + 32) == static_cast<unsigned>(stage));
    for (unsigned i = 36; i < 48; ++i) assert(out[i] == 0);
}
int g_alive_calls = 0, g_cancel_at = 0;
bool CodeAlive() { return ++g_alive_calls != g_cancel_at; }
void TestCodeFailures() {
    for (const unsigned stage : {3u, 4u, 5u, 6u, 15u, 16u, 18u}) {
        ResetCode(); g_code_fail_stage = stage;
        auto c = Request(sd::Operation::Code, "/btdrv");
        assert(sd::ProcessFileSession(c) == sd::Result::CodeUnavailable);
        Failure(c, sd::Result::CodeUnavailable, sd::CodeRoute::Loader, static_cast<sd::CodeStage>(stage), 0xbeef);
        assert(g_code_init_calls == 1 && g_ncm_calls == 0);
        assert(Calls(101) == (stage == 3 ? 0u : 1u));
        assert(Calls(102) == (stage >= 15 ? 1u : 0u));
        assert(Calls(103) == (stage >= 16 ? 1u : 0u));
        if (stage >= 15) assert(g_code_calls.back() == 101);
    }
    for (const unsigned stage : {7u, 8u, 9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u, 18u}) {
        ResetCode(); g_code_fail_stage = stage;
        auto c = Request(sd::Operation::CodeViaFs, "/btm");
        assert(sd::ProcessFileSession(c) == sd::Result::CodeUnavailable);
        const auto ipc = stage == 10 ? MAKERESULT(Module_Libnx, LibnxError_BadInput) : 0xbeef;
        Failure(c, sd::Result::CodeUnavailable, sd::CodeRoute::FileSystem, static_cast<sd::CodeStage>(stage), ipc);
        assert(g_code_init_calls == 0 && Calls(101) == 0); // No loader fallback.
        assert(Calls(104) == (stage >= 8 ? 1u : 0u));
        assert(Calls(105) == (stage >= 9 ? 1u : 0u));
        assert(Calls(106) == (stage >= 13 ? 1u : 0u));
        assert(Calls(102) == (stage >= 15 ? 1u : 0u));
        assert(Calls(103) == (stage >= 16 ? 1u : 0u));
    }
    const s64 invalid_sizes[] = {-1, 0, 255, static_cast<s64>(sd::MaxCodeSize + 1)};
    for (const s64 size : invalid_sizes) {
        ResetCode(); g_code_file_size = size;
        auto c = Request(sd::Operation::Code, "/btdrv");
        assert(sd::ProcessFileSession(c) == sd::Result::TooLarge);
        Failure(c, sd::Result::TooLarge, sd::CodeRoute::Loader, sd::CodeStage::SizeBounds, 0);
        assert(g_code_read_calls == 0 && Calls(101) == 1 && Calls(102) == 1 && Calls(103) == 1);
    }
    ResetCode(); g_bad_content_path = true;
    auto path = Request(sd::Operation::CodeViaFs, "/btm");
    assert(sd::ProcessFileSession(path) == sd::Result::CodeUnavailable);
    Failure(path, sd::Result::CodeUnavailable, sd::CodeRoute::FileSystem, sd::CodeStage::MetadataPath,
            MAKERESULT(Module_Libnx, LibnxError_BadInput));
    assert(!Calls(14));
    ResetCode(); g_code_short_read = true;
    auto short_read = Request(sd::Operation::Code, "/btdrv");
    assert(sd::ProcessFileSession(short_read) == sd::Result::CodeUnavailable);
    Failure(short_read, sd::Result::CodeUnavailable, sd::CodeRoute::Loader, sd::CodeStage::HashRead, 0);
    ResetCode(); g_code_data[0] = 'X';
    auto header = Request(sd::Operation::Code, "/btdrv");
    assert(sd::ProcessFileSession(header) == sd::Result::Invalid);
    Failure(header, sd::Result::Invalid, sd::CodeRoute::Loader, sd::CodeStage::NsoHeader, 0);
    for (int cancel_at : {1, 2, 3}) {
        ResetCode(); g_alive_calls = 0; g_cancel_at = cancel_at;
        auto c = Request(sd::Operation::Code, "/btdrv");
        assert(sd::ProcessFileSession(c, CodeAlive) == sd::Result::Io);
        if (cancel_at < 3) Failure(c, sd::Result::Io, sd::CodeRoute::Loader, sd::CodeStage::Cancelled, 0);
        else assert(c.output.size() == 48 && c.replies == 1 && sd::Load32(c.output.data() + 8) == 0);
        assert(Calls(101) == (cancel_at == 1 ? 0u : 1u));
    }
    ResetCode(); g_code_fail_read_call = 2;
    auto stream = Request(sd::Operation::Code, "/btdrv");
    assert(sd::ProcessFileSession(stream) == sd::Result::Io);
    assert(stream.output.size() == 48 && stream.replies == 1 && sd::Load32(stream.output.data() + 8) == 0);
    assert(Calls(101) == 1 && Calls(102) == 1 && Calls(103) == 1);
    for (bool fail_reply : {false, true}) {
        ResetCode(); auto c = Request(sd::Operation::Code, "/btdrv");
        c.fail_reply = fail_reply; c.fail_payload = !fail_reply;
        assert(sd::ProcessFileSession(c) == sd::Result::Io);
        assert(c.replies == 1 && c.output.size() == (fail_reply ? 0u : 48u));
        assert(Calls(101) == 1 && Calls(102) == 1 && Calls(103) == 1);
    }
    ResetCode(); g_code_fail_stage = 4;
    auto unsent_error = Request(sd::Operation::Code, "/btdrv"); unsent_error.fail_reply = true;
    assert(sd::ProcessFileSession(unsent_error) == sd::Result::Io);
    assert(unsent_error.output.empty() && unsent_error.replies == 1 && Calls(101) == 1);
    for (const auto operation : {sd::Operation::Code, sd::Operation::CodeViaFs}) {
        ResetCode(); g_old_code_version = true;
        auto old = Request(operation, "/btdrv");
        assert(sd::ProcessFileSession(old) == sd::Result::CodeUnavailable);
        Failure(old, sd::Result::CodeUnavailable, operation == sd::Operation::Code ?
                sd::CodeRoute::Loader : sd::CodeRoute::FileSystem, sd::CodeStage::Version, 0);
        assert(g_code_calls.empty());
    }
    ResetCode();
    auto success = Request(sd::Operation::Code, "/btdrv");
    assert(sd::ProcessFileSession(success) == sd::Result::Ok);
    assert(g_code_calls.size() >= 3 && g_code_calls[g_code_calls.size()-3] == 103 &&
           g_code_calls[g_code_calls.size()-2] == 102 && g_code_calls.back() == 101);
    auto rejected = Request(sd::Operation::Code, "/prod.keys");
    const auto calls = g_code_calls.size();
    assert(sd::ProcessFileSession(rejected) == sd::Result::Denied && calls == g_code_calls.size());
    Failure(rejected, sd::Result::Denied, sd::CodeRoute::Loader, sd::CodeStage::Request, 0);
    ResetCode();
}
int main() {
    const auto previous=fs::current_path();
    std::string pattern=(fs::temp_directory_path()/"mc-sd-session-XXXXXX").string();
    char *temporary=mkdtemp(pattern.data()); assert(temporary);
    const fs::path root=temporary; fs::current_path(root);
    fs::create_directories("sdmc:/config/MissionControl");
    const std::string config="/config/MissionControl/missioncontrol.ini", native="sdmc:"+config;
    const std::string original="[General]\nexample=true\n", updated="[General]\nexample=false\n";
    Write(native,original);
    auto get=Request(sd::Operation::Get,config);
    assert(sd::ProcessFileSession(get)==sd::Result::Ok);
    assert(get.output.size()==48+original.size());
    assert(std::string(get.output.begin()+48,get.output.end())==original);
    auto hash=Request(sd::Operation::Hash,config);
    assert(sd::ProcessFileSession(hash)==sd::Result::Ok && hash.output.size()==48);
    auto list=Request(sd::Operation::List,"/config/MissionControl");
    assert(sd::ProcessFileSession(list)==sd::Result::Ok);
    g_code_init_calls = 0;
    auto unknown_code=Request(sd::Operation::Code,"/arbitrary");
    assert(sd::ProcessFileSession(unknown_code)==sd::Result::Denied && g_code_init_calls==0);
    auto code_refused=Request(sd::Operation::Code,"/btdrv");
    assert(sd::ProcessFileSession(code_refused)==sd::Result::CodeUnavailable);
    assert(g_code_init_calls==1 && g_code_program==0);
    g_code_init_rc=0;
    auto code=Request(sd::Operation::Code,"/btdrv");
    assert(sd::ProcessFileSession(code)==sd::Result::Ok);
    assert(g_code_program==sd::BluetoothProgram && code.output.size()==48+sizeof(g_code_data));
    auto manager=Request(sd::Operation::Code,"/btm");
    assert(sd::ProcessFileSession(manager)==sd::Result::Ok && g_code_program==sd::BluetoothManagerProgram);
    g_code_data[0]='X';
    auto not_nso=Request(sd::Operation::Code,"/btdrv");
    assert(sd::ProcessFileSession(not_nso)==sd::Result::Invalid && not_nso.output.size()==48);
    g_code_data[0]='N';
    g_old_code_version=true;
    const auto calls_before=g_code_init_calls;
    auto old_version=Request(sd::Operation::Code,"/btdrv");
    assert(sd::ProcessFileSession(old_version)==sd::Result::CodeUnavailable && g_code_init_calls==calls_before);
    g_old_code_version=false;
    const auto loader_calls=g_code_init_calls;
    auto normal_fs=Request(sd::Operation::CodeViaFs,"/btm");
    assert(sd::ProcessFileSession(normal_fs)==sd::Result::Ok);
    assert(g_ncm_calls==1 && g_code_program==sd::BluetoothManagerProgram && g_code_init_calls==loader_calls);
    auto forbidden_fs=Request(sd::Operation::CodeViaFs,"/other");
    assert(sd::ProcessFileSession(forbidden_fs)==sd::Result::Denied && g_ncm_calls==1);
    g_normal_fs_rc=0x1234;
    auto refused_fs=Request(sd::Operation::CodeViaFs,"/btdrv");
    assert(sd::ProcessFileSession(refused_fs)==sd::Result::CodeUnavailable);
    assert(g_code_init_calls==loader_calls); // No automatic loader-session fallback.
    g_normal_fs_rc=0;
    TestCodeFailures();
    auto denied=Request(sd::Operation::Get,"/prod.keys");
    assert(sd::ProcessFileSession(denied)==sd::Result::Denied);
    auto protected_put=Request(sd::Operation::Put,"/atmosphere/exefs_patches/example.ips","PATCH");
    assert(sd::ProcessFileSession(protected_put)==sd::Result::Denied);
    auto no_overwrite=Request(sd::Operation::Put,config,updated);
    assert(sd::ProcessFileSession(no_overwrite)==sd::Result::Conflict && Read(native)==original);
    const std::string stale="stale";
    auto stale_write=Request(sd::Operation::Put,config,updated,&stale);
    assert(sd::ProcessFileSession(stale_write)==sd::Result::HashMismatch && Read(native)==original);
    auto truncated=Request(sd::Operation::Put,config,updated,&original); truncated.input.pop_back();
    assert(sd::ProcessFileSession(truncated)==sd::Result::Io && Read(native)==original);
    auto corrupt=Request(sd::Operation::Put,config,updated,&original); corrupt.input.back()^=1;
    assert(sd::ProcessFileSession(corrupt)==sd::Result::HashMismatch && Read(native)==original);
    g_fail_commit=true;
    auto commit_fail=Request(sd::Operation::Put,config,updated,&original);
    assert(sd::ProcessFileSession(commit_fail)==sd::Result::Io && Read(native)==original);
    g_fail_commit=false;
    g_mock_tick=0xabc;
    const std::string backup=native+".mc-0000000000000abc.ini";
    Write(backup,"preserved backup");
    auto collision=Request(sd::Operation::Put,config,updated,&original);
    assert(sd::ProcessFileSession(collision)==sd::Result::Conflict);
    assert(Read(backup)=="preserved backup" && Read(native)==original);
    auto replacement=Request(sd::Operation::Put,config,updated,&original);
    assert(sd::ProcessFileSession(replacement)==sd::Result::Ok && Read(native)==updated);
    bool found_backup=false;
    for (const auto &entry:fs::directory_iterator("sdmc:/config/MissionControl")) {
        assert(entry.path().extension()!=".stage");
        if (entry.path()!=native && Read(entry.path())==original) found_backup=true;
    }
    assert(found_backup);
    const std::string staged="/config/MissionControl/remote-staging/test.txt";
    auto create=Request(sd::Operation::Put,staged,"new data");
    assert(sd::ProcessFileSession(create)==sd::Result::Ok && Read("sdmc:"+staged)=="new data");
    fs::current_path(previous); fs::remove_all(root);
    std::cout<<"SD transaction get/list/hash/create/backup/precondition/truncation/commit tests passed (SDK mock).\n";
}
