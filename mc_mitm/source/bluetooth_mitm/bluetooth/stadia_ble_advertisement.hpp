/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "stadia_ble_scan_filter.hpp"
#include <algorithm>
#include <cstring>
#include <span>
#include <type_traits>

namespace ams::bluetooth::stadia_scan {
    struct Advertisement {
        std::uint8_t size, type, data[29];
    };
    static_assert(sizeof(Advertisement) == 31);
    enum class BridgeResult { Unchanged, Appended, NoSpace, Unvalidated, Malformed };

    template<class Ad>
    bool UuidEquals(const Ad &ad, const std::array<std::uint8_t, 16> &uuid) {
        return std::equal(uuid.begin(), uuid.end(), ad.data);
    }

    constexpr bool ValidAdvertisementFormat(std::uint32_t format) {
        return format == 1 || format == 2 || format == 5 || format == 6;
    }

    template<class Ad>
    bool NativeUuidEquals(const Ad &ad, const std::array<std::uint8_t, 16> &uuid, bool canonical) {
        for (unsigned i = 0; i < 16; ++i) if (ad.data[i] != uuid[canonical ? 15 - i : i]) return false;
        return true;
    }

    template<class Ad>
    std::uint32_t FindSizeConvention(const Ad (&ads)[10], std::uint8_t count) {
        if (count > 10) return 0;
        std::uint32_t convention = 0;
        for (unsigned i = 0; i < count; ++i) {
            const auto &ad = ads[i];
            if ((ad.type != 6 && ad.type != 7) || (ad.size != 16 && ad.size != 17)) continue;
            const bool canonical = UuidEquals(ad, ControlUuidCanonical);
            if (!UuidEquals(ad, ControlUuidLittleEndian) && !canonical) continue;
            const std::uint32_t candidate = (ad.size == 16 ? 1u : 2u) | (canonical ? 4u : 0u);
            if (convention != 0 && convention != candidate) return 3;
            convention = candidate;
        }
        return convention;
    }

    // Read one 31-byte record at a time, not a whole scan-result/310-byte list.
    // memcpy permits unaligned IPC storage without type-punning/lifetime UB.
    struct ByteAdvertisements {
        std::span<std::uint8_t> bytes;
        bool Valid() const { return bytes.data() && bytes.size() >= 10 * sizeof(Advertisement); }
        Advertisement Read(unsigned index) const {
            Advertisement ad;
            std::memcpy(&ad, bytes.data() + index * sizeof(ad), sizeof(ad));
            return ad;
        }
        void Write(unsigned index, const Advertisement &ad) const {
            std::memcpy(bytes.data() + index * sizeof(ad), &ad, sizeof(ad));
        }
    };

    template<class Ad>
    struct ArrayAdvertisements {
        Ad (&entries)[10];
        static_assert(sizeof(Ad) == sizeof(Advertisement) && std::is_trivially_copyable_v<Ad>);
        static_assert(sizeof(entries[0].data) == 29);
        bool Valid() const { return true; }
        Advertisement Read(unsigned index) const {
            Advertisement ad;
            std::memcpy(&ad, &entries[index], sizeof(ad));
            return ad;
        }
        void Write(unsigned index, const Advertisement &ad) const {
            std::memcpy(&entries[index], &ad, sizeof(ad));
        }
    };

    template<class View>
    BridgeResult BridgeAdvertisementView(View view, std::uint8_t &count,
                                        std::uint8_t markers, std::uint32_t convention,
                                        bool enabled, bool btm_client, bool advertisement) {
        if (!enabled || !btm_client || !advertisement || markers == 0) return BridgeResult::Unchanged;
        if (!view.Valid() || count > 10 || (markers & ~HidBridgeMarkerMask) != 0) return BridgeResult::Malformed;
        if (!ValidAdvertisementFormat(convention)) return BridgeResult::Unvalidated;
        const unsigned bias = (convention & 3u) == 2 ? 1 : 0;
        const bool canonical = (convention & 4u) != 0;
        bool hid = false;
        unsigned missing = markers;
        for (unsigned i = 0; i < count; ++i) {
            const auto ad = view.Read(i);
            if (ad.size < bias || ad.size - bias > sizeof(ad.data)) return BridgeResult::Malformed;
            const unsigned length = ad.size - bias;
            if (ad.type == 2 || ad.type == 3) {
                if (length % 2 != 0) return BridgeResult::Malformed;
                for (unsigned off = 0; off < length; off += 2) {
                    if (ad.data[off] == 0x12 && ad.data[off + 1] == 0x18) hid = true;
                }
            }
            if (ad.type == 6 || ad.type == 7) {
                if (length % 16 != 0) return BridgeResult::Malformed;
                if (length == 16) {
                    for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) {
                        if (NativeUuidEquals(ad, MarkerForSlot(slot), canonical)) missing &= ~(1u << slot);
                    }
                }
            }
        }
        if (!hid || missing == 0) return BridgeResult::Unchanged;
        unsigned needed = 0;
        for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) needed += bool(missing & (1u << slot));
        if (needed > 10u - count) return BridgeResult::NoSpace;
        // All validation precedes writes. Preserve every existing record/order.
        for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) {
            if ((missing & (1u << slot)) == 0) continue;
            Advertisement entry{};
            entry.size = 16 + bias;
            entry.type = 7;
            const auto &marker = MarkerForSlot(slot);
            if (canonical) std::copy(marker.rbegin(), marker.rend(), entry.data);
            else std::copy(marker.begin(), marker.end(), entry.data);
            view.Write(count++, entry);
        }
        return BridgeResult::Appended;
    }

    template<class Ad>
    BridgeResult BridgeAdvertisements(Ad (&ads)[10], std::uint8_t &count,
                                      std::uint8_t markers, std::uint32_t convention,
                                      bool enabled, bool btm_client, bool advertisement) {
        return BridgeAdvertisementView(ArrayAdvertisements<Ad>{ads}, count, markers, convention,
                                       enabled, btm_client, advertisement);
    }
}
