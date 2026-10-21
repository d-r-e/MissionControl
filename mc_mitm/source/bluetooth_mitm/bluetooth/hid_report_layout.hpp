/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace ams::bluetooth::hogp {

    enum class ReportType : std::uint8_t { Input = 1, Output = 2, Feature = 3 };
    enum class LayoutResult { Ok, Malformed, Unsupported, LimitExceeded };

    // Layout only, not a usage-to-gamepad mapper. Padding contributes to size.
    // HOGP values exclude the Report ID even when the Report Map declares IDs.
    // Keep this ~1.6 KiB object in device-owned storage, not the BLE event stack.
    class ReportLayout {
        static constexpr std::uint32_t MaxBits = 512 * 8;
        std::array<std::array<std::uint16_t, 3>, 256> m_bits{};
        bool m_valid = false;
        bool m_uses_ids = false;

    public:
        static constexpr std::size_t MaxDescriptorSize = 4096;
        static constexpr std::size_t MaxPayloadSize = MaxBits / 8;

        LayoutResult Parse(const std::uint8_t *data, std::size_t size) {
            m_valid = false;
            m_uses_ids = false;
            for (auto &bits : m_bits) bits.fill(0);
            if (!data || size == 0) return LayoutResult::Malformed;
            if (size > MaxDescriptorSize) return LayoutResult::LimitExceeded;

            struct Globals { std::uint32_t size = 0, count = 0; std::uint8_t id = 0; } globals;
            std::array<Globals, 8> stack{};
            std::size_t depth = 0, collections = 0;
            bool implicit_report = false, has_report = false;
            for (std::size_t pos = 0; pos < size;) {
                const auto prefix = data[pos++];
                if (prefix == 0xfe) return LayoutResult::Unsupported;
                const unsigned length_code = prefix & 3;
                const std::size_t length = length_code == 3 ? 4 : length_code;
                if (length > size - pos) return LayoutResult::Malformed;
                std::uint32_t value = 0;
                for (std::size_t i = 0; i < length; ++i)
                    value |= std::uint32_t(data[pos++]) << (8 * i);
                const unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
                if (type == 3) return LayoutResult::Unsupported;
                if (type == 2) continue; // Local usages do not change bit counts.
                if (type == 1) {
                    switch (tag) {
                        case 7: globals.size = value; break;
                        case 8:
                            if (length != 1 || value == 0 || implicit_report)
                                return LayoutResult::Malformed;
                            globals.id = static_cast<std::uint8_t>(value);
                            m_uses_ids = true;
                            break;
                        case 9: globals.count = value; break;
                        case 10:
                            if (length != 0) return LayoutResult::Malformed;
                            if (depth == stack.size()) return LayoutResult::LimitExceeded;
                            stack[depth++] = globals;
                            break;
                        case 11:
                            if (length != 0 || depth == 0) return LayoutResult::Malformed;
                            globals = stack[--depth];
                            break;
                        default:
                            if (tag > 6) return LayoutResult::Unsupported;
                            break; // Usage page, logical/physical bounds, units.
                    }
                    continue;
                }
                if (tag == 10) {
                    if (length != 1) return LayoutResult::Malformed;
                    if (++collections > 16) return LayoutResult::LimitExceeded;
                } else if (tag == 12) {
                    if (length != 0 || collections == 0) return LayoutResult::Malformed;
                    --collections;
                } else if (tag == 8 || tag == 9 || tag == 11) {
                    if (length == 0 || collections == 0 || globals.size == 0 || globals.count == 0)
                        return LayoutResult::Malformed;
                    if (globals.id == 0) {
                        if (m_uses_ids) return LayoutResult::Malformed;
                        implicit_report = true;
                    }
                    if (globals.size > MaxBits || globals.count > MaxBits)
                        return LayoutResult::LimitExceeded;
                    const auto bits = globals.size * globals.count;
                    const unsigned index = tag == 8 ? 0 : tag == 9 ? 1 : 2;
                    auto &total = m_bits[globals.id][index];
                    if (bits > MaxBits - total) return LayoutResult::LimitExceeded;
                    total = static_cast<std::uint16_t>(total + bits);
                    has_report = true;
                } else {
                    return LayoutResult::Unsupported;
                }
            }
            if (collections != 0 || depth != 0 || !has_report) return LayoutResult::Malformed;
            m_valid = true;
            return LayoutResult::Ok;
        }

        bool IsValid() const { return m_valid; }
        bool UsesReportIds() const { return m_valid && m_uses_ids; }
        // Which ids a report map declares for a type. A device is told apart from another by these plus their
        // sizes, which is why the list is exposed rather than only a lookup by a caller-guessed id.
        std::size_t ListReports(ReportType type, std::uint8_t *ids, std::size_t max) const {
            if (!m_valid || !ids) return 0;
            const auto index = static_cast<unsigned>(type) - 1u;
            std::size_t count = 0;
            for (std::size_t id = 0; id < m_bits.size() && count < max; ++id) {
                if (m_bits[id][index] != 0) ids[count++] = static_cast<std::uint8_t>(id);
            }
            return count;
        }

        std::size_t PayloadSize(std::uint8_t id, ReportType type) const {
            const auto index = static_cast<unsigned>(type);
            if (!m_valid || index < 1 || index > 3) return 0;
            return (m_bits[id][index - 1] + 7u) / 8u;
        }
    };
}
