/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace missioncontrol::probe::sd {
    enum class Operation : std::uint32_t { List = 1, Get = 2, Hash = 3, Put = 4, Code = 5, CodeViaFs = 6 };
    enum class Result : std::uint32_t { Ok = 0, Denied = 1, Invalid = 2, NotFound = 3,
        TooLarge = 4, HashMismatch = 5, Io = 6, Conflict = 7, CodeUnavailable = 8, Ready = 100 };
    constexpr std::size_t MaxPath = 240, MaxFile = 4 * 1024 * 1024, MaxList = 64 * 1024;
    constexpr const char *Staging = "/config/MissionControl/remote-staging/";

    inline bool SafePath(const std::string &path) {
        if (path.empty() || path.size() > MaxPath || path[0] != '/' ||
            (path.size() > 1 && path.back() == '/')) return false;
        std::string lower;
        for (unsigned char c : path) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-' || c == ' ')) return false;
            lower += c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        }
        if (path == "/") return true;
        std::size_t begin = 1;
        while (begin < lower.size()) {
            const auto end = lower.find('/', begin);
            const auto part = lower.substr(begin, end == std::string::npos ? end : end - begin);
            if (part.empty() || part == "." || part == ".." || part[0] == '.' ||
                part == "nintendo" || part == "emummc" || part == "backup" || part == "backups" ||
                part == "nand" || part == "boot0" || part.compare(0, 6, "boot0.") == 0 ||
                part == "boot1" || part.compare(0, 6, "boot1.") == 0 ||
                part == "cal0" || part.compare(0, 5, "cal0.") == 0 ||
                part.find("prodinfo") != std::string::npos || part.find(".keys") != std::string::npos ||
                part.find(".key") != std::string::npos || part.find(".pem") != std::string::npos ||
                part.find("credential") != std::string::npos || part.find("token") != std::string::npos ||
                part == "keys" || part.compare(0, 5, "keys.") == 0 || part == "stadia-probe-address.txt") return false;
            if (end == std::string::npos) break;
            begin = end + 1;
        }
        return true;
    }
    inline bool CanReadFile(const std::string &path) {
        if (!SafePath(path)) return false;
        const auto dot = path.find_last_of('.');
        if (dot == std::string::npos) return false;
        auto ext = path.substr(dot);
        for (auto &c : ext) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        for (const auto *allowed : {".ini", ".json", ".lst", ".txt", ".log", ".nro", ".nsp",
                                   ".nso", ".elf", ".ips", ".nacp", ".hex"})
            if (ext == allowed) return true;
        return false;
    }
    inline bool CanWriteFile(const std::string &path) {
        if (!CanReadFile(path)) return false;
        if (path == "/config/MissionControl/missioncontrol.ini") return true;
        const std::size_t prefix = std::strlen(Staging);
        if (path.compare(0, prefix, Staging) != 0 || path.find('/', prefix) != std::string::npos) return false;
        // Staging never activates modules, patches, boot flags or another app.
        // Reject patch files even in staging to make the protection unambiguous.
        auto lower = path;
        for (auto &c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return lower.find(".ips") == std::string::npos && lower.find("boot2.flag") == std::string::npos;
    }
    inline std::uint32_t Load32(const unsigned char *p) {
        return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) | (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
    }
    inline void Store32(unsigned char *p, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (i * 8));
    }
}
