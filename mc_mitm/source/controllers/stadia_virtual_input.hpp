/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ams::controller {
    // Initial input-only profile: standard full input30 and command replies21.
    // Generic3f serialization in the shared base is incomplete. Do not accept
    // a format switch and then publish an invalid hat/buttons or silently send
    // a different report format. NFC/MCU/attachment output is unsupported too.
    inline bool StadiaVirtualOutputAllowed(const std::uint8_t *data, std::size_t size) {
        if (!data || !size || size > 512) return false;
        if (data[0] == 0x10) return size >= 10; // Virtual rumble command (no physical output).
        if (data[0] != 0x01 || size < 49) return false;
        return data[10] != 0x03 || data[11] == 0x30; // SetDataFormat subcommand.
    }

    // Common publication seam for input AND command-generated replies. Copies
    // before remapping; no caller-owned output buffer or response state changes.
    // Test doubles cover order/bounds/format rejection without device access.
    template<typename Remap, typename Emit>
    bool PublishStadiaVirtualInput(const std::uint8_t *data, std::size_t size,
                                  Remap remap, Emit emit) {
        if (!data || size != 49 || (data[0] != 0x30 && data[0] != 0x21)) return false;
        std::array<std::uint8_t, 49> copy{};
        std::memcpy(copy.data(), data, size);
        remap(copy.data(), size);
        emit(copy.data(), size);
        return true;
    }
}
