#pragma once
// Host-only SDK shim for filesystem transaction tests. Fake digest tests flow,
// NOT cryptography; real transfers use libnx hardware SHA256 and Python hashlib.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>
using Result = std::uint32_t;
inline bool g_fail_commit = false;
inline std::uint64_t g_mock_tick = 0xabc;
inline std::uint64_t armGetSystemTick() { return g_mock_tick++; }
inline std::uint64_t armNsToTicks(std::uint64_t n) { return n; }
#define R_FAILED(r) ((r) != 0)
inline Result fsdevCommitDevice(const char *) { return g_fail_commit ? 1 : 0; }
struct Sha256Context { std::uint32_t value; };
inline void sha256ContextCreate(Sha256Context *c) { c->value = 2166136261u; }
inline void sha256ContextUpdate(Sha256Context *c, const void *p, std::size_t n) {
    const auto *bytes = static_cast<const unsigned char *>(p);
    for (std::size_t i = 0; i < n; ++i) c->value = (c->value ^ bytes[i]) * 16777619u;
}
inline void sha256ContextGetHash(Sha256Context *c, void *out) {
    auto *bytes = static_cast<unsigned char *>(out);
    for (unsigned i = 0; i < 32; ++i) bytes[i] = static_cast<unsigned char>(c->value >> ((i % 4) * 8));
}
inline void sha256CalculateHash(void *out, const void *p, std::size_t n) {
    Sha256Context c; sha256ContextCreate(&c); sha256ContextUpdate(&c,p,n); sha256ContextGetHash(&c,out);
}

// Read-only loader API shim. No code API succeeds unless a test opts in.
using s64 = std::int64_t;
using u64 = std::uint64_t;
struct Service { bool active = false; Service *parent = nullptr; };
struct FsCodeInfo { unsigned char bytes[0x124]{}; };
struct FsFileSystem { Service s; };
struct FsFile { Service *parent = nullptr; };
inline unsigned g_code_fail_stage = 0;
inline unsigned g_code_read_calls = 0, g_code_fail_read_call = 0;
inline bool g_code_short_read = false, g_bad_content_path = false;
inline std::int64_t g_code_file_size = 256;
inline std::vector<unsigned> g_code_calls;
inline Result MockStage(unsigned stage) {
    g_code_calls.push_back(stage);
    return stage == g_code_fail_stage ? 0xbeef : 0;
}
constexpr int NcmStorageId_BuiltInSystem = 3;
constexpr int FsContentAttributes_None = 0, FsOpenMode_Read = 1, FsReadOption_None = 0;
inline Result g_code_init_rc = 0x1234;
inline unsigned g_code_init_calls = 0, g_code_exit_calls = 0;
inline u64 g_code_program = 0;
inline bool g_old_code_version = false;
inline unsigned char g_code_data[256] = {'N','S','O','0'};
inline bool hosversionBefore(int, int, int) { return g_old_code_version; }
inline Result smGetService(Service *service, const char *name) {
    ++g_code_init_calls;
    if (std::strcmp(name, "fsp-ldr")) return 1;
    const auto failure = MockStage(3);
    if (failure || g_code_init_rc) return failure ? failure : g_code_init_rc;
    service->active = true; return 0;
}
inline void serviceClose(Service *service) {
    g_code_calls.push_back(101); ++g_code_exit_calls; service->active = false;
}
inline Result serviceConvertToDomain(Service *service) { return service->active ? MockStage(4) : 1; }
inline void serviceAssumeDomain(Service *) {}
constexpr unsigned SfBufferAttr_HipcMapAlias = 4, SfBufferAttr_Out = 2;
struct MockDispatchParams {
    bool in_send_pid = false;
    unsigned buffer_attrs[1]{};
    struct Buffer { void *ptr; std::size_t size; } buffers[1]{};
    unsigned out_num_objects = 0;
    Service *out_objects = nullptr;
};
template<typename Input>
inline Result MockDispatch(Service *service, unsigned command, const Input &in, MockDispatchParams params) {
    if (!service->active) return 1;
    if constexpr (std::is_integral_v<Input>) {
        if (command != 2 || in != 0 || !params.in_send_pid || params.out_num_objects) return 1;
        return MockStage(5);
    } else {
        if (command != 0 || in.attr != 0 || in.storage_id != 3 ||
            params.in_send_pid || params.out_num_objects != 1 || !params.out_objects ||
            params.buffer_attrs[0] != (SfBufferAttr_HipcMapAlias | SfBufferAttr_Out) ||
            params.buffers[0].size != sizeof(FsCodeInfo)) return 1;
        const auto failure = MockStage(6);
        if (failure) return failure;
        g_code_program = in.program;
        params.out_objects->active = true; params.out_objects->parent = service;
        return 0;
    }
}
#define serviceDispatchIn(service, command, input, ...) MockDispatch(service, command, input, MockDispatchParams{__VA_ARGS__})
inline bool MockParentAlive(Service *s) { return s && s->active && (!s->parent || s->parent->active); }
inline Result fsFsOpenFile(FsFileSystem *fs, const char *path, int mode, FsFile *file) {
    // libnx advertises FS_MAX_PATH bytes without padding the caller's path.
    // Read the entire range so ASAN catches a string-literal-sized argument.
    unsigned char advertised_path[769];
    std::memcpy(advertised_path, path, sizeof(advertised_path));
    for (std::size_t i = sizeof("/main"); i < sizeof(advertised_path); ++i)
        if (advertised_path[i] != 0) return 1;
    const auto failure = MockStage(15);
    if (failure) return failure;
    if (!MockParentAlive(&fs->s) || std::strcmp(path, "/main") || mode != 1) return 1;
    file->parent = &fs->s; return 0;
}
inline Result fsFileGetSize(FsFile *file, s64 *size) {
    const auto failure = MockStage(16);
    if (failure) return failure;
    if (!MockParentAlive(file->parent)) return 1;
    *size = g_code_file_size; return 0;
}
inline Result fsFileRead(FsFile *file, s64 offset, void *data, u64 size, int option, u64 *read) {
    ++g_code_read_calls;
    const auto failure = MockStage(18);
    if (failure || g_code_read_calls == g_code_fail_read_call) return 0xbeef;
    if (!MockParentAlive(file->parent) || offset < 0 || option != 0 ||
        static_cast<u64>(offset) + size > sizeof(g_code_data)) return 1;
    const auto copied = g_code_short_read && size ? size - 1 : size;
    std::memcpy(data, g_code_data + offset, copied); *read = copied; return 0;
}
inline void fsFileClose(FsFile *) { g_code_calls.push_back(103); }
inline void fsFsClose(FsFileSystem *fs) { g_code_calls.push_back(102); fs->s.active = false; }

// Normal-FS code route mock: fixed system metadata lookup, no write APIs.
struct NcmContentMetaDatabase {};
struct NcmContentStorage {};
struct NcmContentMetaKey { u64 id; };
struct NcmContentId {};
using FsFileSystemType = int;
constexpr int FS_MAX_PATH = 769, NcmContentType_Program = 1;
constexpr unsigned Module_Libnx = 345, LibnxError_BadInput = 11;
#define MAKERESULT(module, description) ((module) | ((description) << 9))
inline unsigned g_ncm_calls = 0;
inline Result g_normal_fs_rc = 0;
inline Result ncmInitialize() { ++g_ncm_calls; return MockStage(7); }
inline void ncmExit() { g_code_calls.push_back(104); }
inline Result ncmOpenContentMetaDatabase(NcmContentMetaDatabase *, int storage) { return storage == 3 ? MockStage(8) : 1; }
inline Result ncmOpenContentStorage(NcmContentStorage *, int storage) { return storage == 3 ? MockStage(12) : 1; }
inline void ncmContentStorageClose(NcmContentStorage *) { g_code_calls.push_back(106); }
inline void ncmContentMetaDatabaseClose(NcmContentMetaDatabase *) { g_code_calls.push_back(105); }
inline Result ncmContentMetaDatabaseGetLatestContentMetaKey(NcmContentMetaDatabase *, NcmContentMetaKey *key, u64 id) {
    key->id = g_code_fail_stage == 10 ? id + 1 : id; return MockStage(9);
}
inline Result ncmContentMetaDatabaseGetContentIdByType(NcmContentMetaDatabase *, NcmContentId *, const NcmContentMetaKey *, int type) { return type == 1 ? MockStage(11) : 1; }
inline Result ncmContentStorageGetPath(NcmContentStorage *, char *path, std::size_t size, const NcmContentId *) {
    const auto failure = MockStage(13);
    if (failure) return failure;
    if (g_bad_content_path) { std::memset(path, 'X', size); return 0; }
    constexpr const char expected[] = "@SystemContent:/test.nca";
    if (size < sizeof(expected)) return 1;
    std::memcpy(path, expected, sizeof(expected)); return 0;
}
inline Result fsOpenFileSystemWithId(FsFileSystem *fs, u64 program, FsFileSystemType type, const char *path, int attr) {
    const auto failure = MockStage(14);
    if (failure) return failure;
    if (type != 0 || std::strcmp(path, "@SystemContent:/test.nca") || attr != 0) return 1;
    g_code_program = program;
    if (!g_normal_fs_rc) fs->s.active = true;
    return g_normal_fs_rc;
}
