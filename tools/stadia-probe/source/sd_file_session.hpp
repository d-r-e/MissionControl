/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <switch.h>
#include <array>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sd_file_policy.hpp"
#include "bluetooth_code_export.hpp"

namespace missioncontrol::probe::sd {
    class Channel {
        int m_fd = -1;
        std::uint64_t m_deadline = armGetSystemTick() + armNsToTicks(60'000'000'000ull);
        bool (*m_alive)();
    public:
        explicit Channel(bool (*alive)()) : m_alive(alive) {}
        ~Channel() { if (m_fd >= 0) close(m_fd); }
        bool Wait(short events) {
            while (armGetSystemTick() < m_deadline) {
                pollfd p{m_fd, events, 0};
                const int result = poll(&p, 1, 100);
                if (result > 0) return (p.revents & events) != 0;
                if (result < 0 && errno != EINTR) return false;
                if (!m_alive()) return false;
            }
            return false;
        }
        bool Connect(in_addr host) {
            m_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (m_fd < 0) return false;
            const int flags = fcntl(m_fd, F_GETFL, 0);
            if (flags < 0 || fcntl(m_fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_port = htons(28772);
            address.sin_addr = host;
            const int result = connect(m_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address));
            if (result < 0 && errno != EINPROGRESS) return false;
            if (result < 0) {
                if (!Wait(POLLOUT)) return false;
                int error = 0; socklen_t size = sizeof(error);
                if (getsockopt(m_fd, SOL_SOCKET, SO_ERROR, &error, &size) < 0 || error != 0) return false;
            }
            return true;
        }
        bool Transfer(void *buffer, std::size_t size, bool sending) {
            auto *p = static_cast<unsigned char *>(buffer);
            while (size) {
                if (!Wait(sending ? POLLOUT : POLLIN)) return false;
                const auto count = sending ? send(m_fd, p, size, MSG_NOSIGNAL) : recv(m_fd, p, size, 0);
                if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
                if (count <= 0) return false;
                p += count; size -= count;
            }
            return true;
        }
        bool Reply(Result result, std::uint32_t size = 0, const unsigned char *hash = nullptr) {
            unsigned char header[48]{};
            std::memcpy(header, "MCSDR001", 8);
            Store32(header + 8, static_cast<std::uint32_t>(result));
            Store32(header + 12, size);
            if (hash) std::memcpy(header + 16, hash, 32);
            return Transfer(header, sizeof(header), true);
        }
    };

    inline Result HashFile(const std::string &path, unsigned char *hash, std::size_t *size) {
        struct stat info{};
        if (lstat(path.c_str(), &info) != 0) return errno == ENOENT ? Result::NotFound : Result::Io;
        if (!S_ISREG(info.st_mode)) return Result::Denied;
        if (info.st_size < 0 || static_cast<std::uint64_t>(info.st_size) > MaxFile) return Result::TooLarge;
        FILE *file = std::fopen(path.c_str(), "rb");
        if (!file) return Result::Io;
        Sha256Context context; sha256ContextCreate(&context);
        unsigned char data[4096]; std::size_t total = 0, count;
        while ((count = std::fread(data, 1, sizeof(data), file)) != 0) {
            total += count;
            if (total > MaxFile) { std::fclose(file); return Result::TooLarge; }
            sha256ContextUpdate(&context, data, count);
        }
        const bool ok = !std::ferror(file) && total == static_cast<std::size_t>(info.st_size);
        const int closed = std::fclose(file);
        if (!ok || closed != 0) return Result::Io;
        sha256ContextGetHash(&context, hash); *size = total;
        return Result::Ok;
    }

    template<typename TransferChannel>
    Result ProcessFileSession(TransferChannel &channel, bool (*alive)() = nullptr) {
        unsigned char request[88];
        if (!channel.Transfer(request, sizeof(request), false)) return Result::Io;
        const auto operation = static_cast<Operation>(Load32(request + 8));
        const auto path_size = Load32(request + 12), data_size = Load32(request + 16), flags = Load32(request + 20);
        auto fail = [&](Result result) { channel.Reply(result); return result; };
        if (std::memcmp(request, "MCSD0001", 8) || path_size == 0 || path_size > MaxPath ||
            data_size > MaxFile || flags > 1 || operation < Operation::List || operation > Operation::CodeViaFs ||
            (operation != Operation::Put && (data_size != 0 || flags != 0)))
            return fail(Result::Invalid);
        std::string path(path_size, '\0');
        if (!channel.Transfer(path.data(), path.size(), false)) return Result::Io;
        if (operation == Operation::Code || operation == Operation::CodeViaFs)
            return ExportBluetoothCode(channel, path, alive, operation == Operation::CodeViaFs);
        if (!SafePath(path)) return fail(Result::Denied);
        const std::string native = "sdmc:" + path;
        if (operation == Operation::List) {
            DIR *directory = opendir(native.c_str());
            if (!directory) return fail(Result::NotFound);
            std::string listing;
            bool overflow = false; unsigned count = 0;
            while (auto *entry = readdir(directory)) {
                const std::string name = entry->d_name;
                const std::string child = (path == "/" ? "/" : path + "/") + name;
                if (!SafePath(child)) continue;
                struct stat info{};
                if (lstat(("sdmc:" + child).c_str(), &info) != 0 ||
                    (!S_ISDIR(info.st_mode) && !S_ISREG(info.st_mode))) continue;
                if (S_ISREG(info.st_mode) && !CanReadFile(child)) continue;
                const auto line = std::string(S_ISDIR(info.st_mode) ? "D\t" : "F\t") + name + "\n";
                if (++count > 512 || listing.size() + line.size() > MaxList) { overflow = true; break; }
                listing += line;
            }
            closedir(directory);
            if (overflow) return fail(Result::TooLarge); // Never silently truncate.
            unsigned char hash[32]; sha256CalculateHash(hash, listing.data(), listing.size());
            if (!channel.Reply(Result::Ok, listing.size(), hash) ||
                !channel.Transfer(listing.data(), listing.size(), true)) return Result::Io;
            return Result::Ok;
        }
        if (!CanReadFile(path)) return fail(Result::Denied);
        unsigned char old_hash[32]{}; std::size_t size = 0;
        const auto previous = HashFile(native, old_hash, &size);
        if (operation != Operation::Put) {
            if (previous != Result::Ok) return fail(previous);
            if (!channel.Reply(Result::Ok, operation == Operation::Get ? size : 0, old_hash)) return Result::Io;
            if (operation == Operation::Hash) return Result::Ok;
            FILE *file = std::fopen(native.c_str(), "rb");
            if (!file) return Result::Io;
            unsigned char data[4096]; bool ok = true;
            while (size) {
                const auto chunk = size < sizeof(data) ? size : sizeof(data);
                if (std::fread(data, 1, chunk, file) != chunk || !channel.Transfer(data, chunk, true)) { ok = false; break; }
                size -= chunk;
            }
            std::fclose(file);
            return ok ? Result::Ok : Result::Io;
        }
        if (!CanWriteFile(path)) return fail(Result::Denied);
        if (path == "/config/MissionControl/missioncontrol.ini" && data_size > 65536) return fail(Result::TooLarge);
        const bool replacing = previous == Result::Ok;
        if ((flags == 0 && previous != Result::NotFound) || (flags == 1 && !replacing)) return fail(Result::Conflict);
        if (flags == 1 && std::memcmp(old_hash, request + 24, 32)) return fail(Result::HashMismatch);
        if (path.compare(0, std::strlen(Staging), Staging) == 0) {
            if (mkdir("sdmc:/config/MissionControl/remote-staging", 0777) != 0 && errno != EEXIST) return fail(Result::Io);
        }
        char suffix[40]; std::snprintf(suffix, sizeof(suffix), ".mc-%016llx", static_cast<unsigned long long>(armGetSystemTick()));
        const auto stage = native + suffix + ".stage";
        const auto backup = native + suffix + path.substr(path.find_last_of('.'));
        const int descriptor = open(stage.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (descriptor < 0) return fail(Result::Conflict);
        FILE *output = fdopen(descriptor, "wb");
        if (!output) { close(descriptor); unlink(stage.c_str()); return fail(Result::Io); }
        if (!channel.Reply(Result::Ready, 0, old_hash)) { std::fclose(output); unlink(stage.c_str()); return Result::Io; }
        Sha256Context context; sha256ContextCreate(&context);
        unsigned char data[4096]; std::size_t remaining = data_size; bool ok = true;
        while (remaining) {
            const auto chunk = remaining < sizeof(data) ? remaining : sizeof(data);
            if (!channel.Transfer(data, chunk, false) || std::fwrite(data, 1, chunk, output) != chunk) { ok = false; break; }
            sha256ContextUpdate(&context, data, chunk); remaining -= chunk;
        }
        if (std::fclose(output) != 0) ok = false;
        unsigned char hash[32]; sha256ContextGetHash(&context, hash);
        if (!ok || std::memcmp(hash, request + 56, 32)) {
            unlink(stage.c_str()); return fail(ok ? Result::HashMismatch : Result::Io);
        }
        unsigned char staged_hash[32]{}; std::size_t staged_size = 0;
        if (R_FAILED(fsdevCommitDevice("sdmc")) || HashFile(stage, staged_hash, &staged_size) != Result::Ok ||
            staged_size != data_size || std::memcmp(staged_hash, hash, 32)) {
            unlink(stage.c_str()); return fail(Result::Io);
        }
        struct stat backup_info{};
        if (lstat(backup.c_str(), &backup_info) == 0 || errno != ENOENT) {
            unlink(stage.c_str()); return fail(Result::Conflict);
        }
        // Recheck immediately before activation; do not overwrite intervening edits.
        unsigned char current_hash[32]{}; std::size_t current_size = 0;
        const auto current = HashFile(native, current_hash, &current_size);
        if ((replacing && (current != Result::Ok || std::memcmp(current_hash, old_hash, 32))) ||
            (!replacing && current != Result::NotFound)) {
            unlink(stage.c_str()); return fail(Result::Conflict);
        }
        if (replacing && rename(native.c_str(), backup.c_str()) != 0) {
            unlink(stage.c_str()); return fail(Result::Io);
        }
        if (rename(stage.c_str(), native.c_str()) != 0) {
            if (replacing) rename(backup.c_str(), native.c_str()); // Best-effort rollback; backup never deleted.
            unlink(stage.c_str()); return fail(Result::Io);
        }
        if (R_FAILED(fsdevCommitDevice("sdmc"))) return fail(Result::Io);
        // Backup is retained. These two renames are recoverable, NOT a power-loss
        // atomic swap. No boot/module/patch activation is permitted by policy.
        std::string receipt = replacing ? backup.substr(5) : std::string{};
        if (!channel.Reply(Result::Ok, receipt.size(), hash) ||
            !channel.Transfer(receipt.data(), receipt.size(), true)) return Result::Io;
        return Result::Ok;
    }

    inline Result RunFileSession(in_addr host, bool (*alive)()) {
        Channel channel(alive);
        if (!channel.Connect(host)) return Result::Io;
        return ProcessFileSession(channel, alive);
    }
}
