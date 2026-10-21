/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace missioncontrol::probe {
    // Public code identities extracted from archived PFS0 main/NSO0 headers,
    // not git-version strings or incrementally cached compile timestamps.
    constexpr std::array<std::uint8_t, 32> NativeHidBuildId{
        0x03,0xff,0x63,0xe3,0x18,0x7d,0x2c,0xf0,0xde,0xc6,
        0xd8,0x0b,0x68,0xfe,0x2c,0x85,0xf4,0x4b,0x4a,0x73};
    constexpr std::array<std::uint8_t, 32> RetiredFlagsBuildId{
        0x20,0x05,0x43,0xad,0x64,0x4a,0x02,0x9a,0x43,0xf9,
        0x57,0x40,0x53,0xc8,0xb5,0x60,0x0a,0x1d,0x44,0x8a};
    constexpr std::array<std::uint8_t, 32> PreDiscoveryBuildId{
        0x27,0x48,0xfc,0x6a,0xd1,0x21,0x64,0x3e,0xe5,0x6d,
        0xb5,0x67,0xfa,0xc4,0xc3,0xa3,0xfc,0x9e,0xc4,0x2b};
    constexpr std::array<std::uint8_t, 32> ManagedControlBuildId{
        0xa7,0x2c,0xd2,0x3b,0xee,0x76,0xdb,0x50,0x37,0x73,
        0x89,0x15,0x5d,0x74,0xd0,0x49,0x57,0x3c,0xd2,0x13};
    constexpr std::array<std::uint8_t, 32> NativeReturnBuildId{
        0x1e,0x96,0x89,0xda,0xa0,0x1b,0x91,0x4f,0x06,0x3b,
        0x88,0x4c,0x77,0x3d,0xbb,0x56,0xc6,0xac,0x61,0xdc};
    enum class KnownModuleBuild { Unknown, NativeHidP2, RetiredFlagsP1, PreDiscovery, ManagedControl, NativeReturn };
    inline KnownModuleBuild IdentifyModuleBuild(const std::uint8_t *id) {
        if (!id) return KnownModuleBuild::Unknown;
        if (!std::memcmp(id, NativeHidBuildId.data(), NativeHidBuildId.size())) return KnownModuleBuild::NativeHidP2;
        if (!std::memcmp(id, RetiredFlagsBuildId.data(), RetiredFlagsBuildId.size())) return KnownModuleBuild::RetiredFlagsP1;
        if (!std::memcmp(id, PreDiscoveryBuildId.data(), PreDiscoveryBuildId.size())) return KnownModuleBuild::PreDiscovery;
        if (!std::memcmp(id, ManagedControlBuildId.data(), ManagedControlBuildId.size())) return KnownModuleBuild::ManagedControl;
        if (!std::memcmp(id, NativeReturnBuildId.data(), NativeReturnBuildId.size())) return KnownModuleBuild::NativeReturn;
        return KnownModuleBuild::Unknown;
    }
}
