/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <switch.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include "sd_file_policy.hpp"

namespace missioncontrol::probe::sd {
    constexpr std::size_t MaxCodeSize = 16 * 1024 * 1024;
    constexpr std::uint64_t BluetoothProgram = 0x010000000000000bull;
    constexpr std::uint64_t BluetoothManagerProgram = 0x010000000000002aull;
    inline std::uint64_t CodeProgram(const std::string &alias) {
        if (alias == "/btdrv") return BluetoothProgram;
        if (alias == "/btm") return BluetoothManagerProgram;
        return 0;
    }

    // Failure-only extension inside the existing32-byte response detail field.
    // Old readers still see the raw IPC result at byte0. No paths, identifiers,
    // verification data or executable bytes are included in failure responses.
    enum class CodeRoute : std::uint32_t { Loader = 1, FileSystem = 2 };
    enum class CodeStage : std::uint32_t {
        Request = 1, Version, LoaderService, LoaderDomain, LoaderProcess,
        LoaderOpen, MetadataInit, MetadataDatabase, MetadataKey, MetadataIdentity,
        MetadataContent, MetadataStorage, MetadataPath, FileSystemOpen,
        MainOpen, MainSize, SizeBounds, HashRead, NsoHeader, Cancelled
    };
    constexpr std::uint32_t CodeDetailMagic = 0x5845434d; // MCEX, little endian
    constexpr std::uint32_t CodeDetailVersion = 1;
    inline void CodeFailureDetail(unsigned char (&detail)[32], ::Result ipc,
                                  CodeRoute route, CodeStage stage) {
        std::memset(detail, 0, sizeof(detail));
        Store32(detail, ipc);
        Store32(detail + 4, CodeDetailMagic);
        Store32(detail + 8, CodeDetailVersion);
        Store32(detail + 12, static_cast<std::uint32_t>(route));
        Store32(detail + 16, static_cast<std::uint32_t>(stage));
    }

    // Own one normally acquired loader session, never a borrowed/stolen handle.
    // These are the same three initialization steps and HOS20+ request as pinned
    // libnx fsldr.c, separated ONLY to identify the first failure. No retries,
    // alternative ports, PID spoofing, session eviction or version fallback.
    struct CodeLoaderSession {
        Service service{};
        bool acquired = false;
        ~CodeLoaderSession() { if (acquired) serviceClose(&service); }
    };
    inline ::Result OpenCodeViaLoader(FsFileSystem *out, std::uint64_t program,
                                      CodeLoaderSession &loader, CodeStage &stage) {
        stage = CodeStage::LoaderService;
        ::Result rc = smGetService(&loader.service, "fsp-ldr");
        if (R_FAILED(rc)) return rc;
        loader.acquired = true;
        stage = CodeStage::LoaderDomain;
        rc = serviceConvertToDomain(&loader.service);
        if (R_FAILED(rc)) return rc;
        stage = CodeStage::LoaderProcess;
        const u64 pid_placeholder = 0;
        serviceAssumeDomain(&loader.service);
        rc = serviceDispatchIn(&loader.service, 2, pid_placeholder, .in_send_pid = true);
        if (R_FAILED(rc)) return rc;
        stage = CodeStage::LoaderOpen;
        struct Request { std::uint8_t attr, storage_id; std::uint64_t program; };
        static_assert(sizeof(Request) == 16 && offsetof(Request, program) == 8);
        Request request{};
        request.attr = FsContentAttributes_None;
        request.storage_id = NcmStorageId_BuiltInSystem;
        request.program = program;
        FsCodeInfo verification{};
        serviceAssumeDomain(&loader.service);
        return serviceDispatchIn(&loader.service, 0, request,
            .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
            .buffers = { { &verification, sizeof(verification) } },
            .out_num_objects = 1, .out_objects = &out->s);
    }

    // Existing normal-FS route: fixed system metadata lookup and Code0 open.
    // Its availability is NOT established by the enum. This route is explicitly
    // selected by the operator, never tried automatically after loader failure.
    inline ::Result OpenCodeViaFileSystem(FsFileSystem *out, std::uint64_t program, CodeStage &stage) {
        struct Metadata {
            bool initialized = false, db_open = false, storage_open = false;
            NcmContentMetaDatabase db{}; NcmContentStorage storage{};
            ~Metadata() {
                if (storage_open) ncmContentStorageClose(&storage);
                if (db_open) ncmContentMetaDatabaseClose(&db);
                if (initialized) ncmExit();
            }
        } metadata;
        stage = CodeStage::MetadataInit;
        ::Result rc = ncmInitialize();
        if (R_FAILED(rc)) return rc;
        metadata.initialized = true;
        stage = CodeStage::MetadataDatabase;
        rc = ncmOpenContentMetaDatabase(&metadata.db, NcmStorageId_BuiltInSystem);
        if (R_FAILED(rc)) return rc;
        metadata.db_open = true;
        NcmContentMetaKey key{};
        stage = CodeStage::MetadataKey;
        rc = ncmContentMetaDatabaseGetLatestContentMetaKey(&metadata.db, &key, program);
        if (R_FAILED(rc)) return rc;
        stage = CodeStage::MetadataIdentity;
        if (key.id != program) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        NcmContentId content{};
        stage = CodeStage::MetadataContent;
        rc = ncmContentMetaDatabaseGetContentIdByType(&metadata.db, &content, &key, NcmContentType_Program);
        if (R_FAILED(rc)) return rc;
        stage = CodeStage::MetadataStorage;
        rc = ncmOpenContentStorage(&metadata.storage, NcmStorageId_BuiltInSystem);
        if (R_FAILED(rc)) return rc;
        metadata.storage_open = true;
        char path[FS_MAX_PATH]{};
        stage = CodeStage::MetadataPath;
        rc = ncmContentStorageGetPath(&metadata.storage, path, sizeof(path), &content);
        if (R_FAILED(rc)) return rc;
        if (!path[0] || !std::memchr(path, 0, sizeof(path)))
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        stage = CodeStage::FileSystemOpen;
        return fsOpenFileSystemWithId(out, program, static_cast<FsFileSystemType>(0), path, FsContentAttributes_None);
    }

    template<typename TransferChannel>
    Result ExportBluetoothCode(TransferChannel &channel, const std::string &alias, bool (*alive)(), bool normal_fs = false) {
        const auto route = normal_fs ? CodeRoute::FileSystem : CodeRoute::Loader;
        CodeStage stage = CodeStage::Request;
        auto fail = [&](Result result, ::Result ipc = 0) {
            unsigned char detail[32]{};
            CodeFailureDetail(detail, ipc, route, stage);
            return channel.Reply(result, 0, detail) ? result : Result::Io;
        };
        const auto program = CodeProgram(alias);
        if (!program) return fail(Result::Denied);
        stage = CodeStage::Version;
        if (hosversionBefore(20, 0, 0)) return fail(Result::CodeUnavailable);
        if (alive && !alive()) { stage = CodeStage::Cancelled; return fail(Result::Io); }
        struct Handles {
            // Domain children share the parent session; close file/FS first.
            CodeLoaderSession loader;
            bool fs_open = false, file_open = false;
            FsFileSystem fs{}; FsFile file{};
            ~Handles() {
                if (file_open) fsFileClose(&file);
                if (fs_open) fsFsClose(&fs);
            }
        } handles;
        const ::Result opened = normal_fs ? OpenCodeViaFileSystem(&handles.fs, program, stage) :
                                           OpenCodeViaLoader(&handles.fs, program, handles.loader, stage);
        if (R_FAILED(opened)) return fail(Result::CodeUnavailable, opened);
        handles.fs_open = true;
        stage = CodeStage::MainOpen;
        // fsFsOpenFile advertises FS_MAX_PATH input bytes; unlike the higher-
        // level mount wrapper it does not pad a short string for the caller.
        char main_path[FS_MAX_PATH] = "/main";
        ::Result rc = fsFsOpenFile(&handles.fs, main_path, FsOpenMode_Read, &handles.file);
        if (R_FAILED(rc)) return fail(Result::CodeUnavailable, rc);
        handles.file_open = true;
        stage = CodeStage::MainSize;
        s64 size = 0;
        rc = fsFileGetSize(&handles.file, &size);
        if (R_FAILED(rc)) return fail(Result::CodeUnavailable, rc);
        stage = CodeStage::SizeBounds;
        if (size < 0x100 || static_cast<std::uint64_t>(size) > MaxCodeSize) return fail(Result::TooLarge);
        unsigned char data[4096];
        Sha256Context hash; sha256ContextCreate(&hash);
        for (s64 offset = 0; offset < size;) {
            if ((offset % 65536) == 0 && alive && !alive()) { stage = CodeStage::Cancelled; return fail(Result::Io); }
            const auto count = static_cast<std::uint64_t>(size - offset) < sizeof(data) ? size - offset : sizeof(data);
            u64 read = 0;
            stage = CodeStage::HashRead;
            rc = fsFileRead(&handles.file, offset, data, count, FsReadOption_None, &read);
            if (R_FAILED(rc) || read != static_cast<u64>(count)) return fail(Result::CodeUnavailable, rc);
            stage = CodeStage::NsoHeader;
            if (offset == 0 && std::memcmp(data, "NSO0", 4)) return fail(Result::Invalid);
            sha256ContextUpdate(&hash, data, read); offset += read;
        }
        unsigned char digest[32]; sha256ContextGetHash(&hash, digest);
        if (!channel.Reply(Result::Ok, static_cast<std::uint32_t>(size), digest)) return Result::Io;
        // After the success header, errors close the transfer: never insert a
        // second response frame into executable bytes. Host verifies full SHA256
        // before creating its exclusive output file. No partial export is saved.
        for (s64 offset = 0; offset < size;) {
            if ((offset % 65536) == 0 && alive && !alive()) return Result::Io;
            const auto count = static_cast<std::uint64_t>(size - offset) < sizeof(data) ? size - offset : sizeof(data);
            u64 read = 0;
            rc = fsFileRead(&handles.file, offset, data, count, FsReadOption_None, &read);
            if (R_FAILED(rc) || read != static_cast<u64>(count) || !channel.Transfer(data, read, true)) return Result::Io;
            offset += read;
        }
        return Result::Ok;
    }
}
