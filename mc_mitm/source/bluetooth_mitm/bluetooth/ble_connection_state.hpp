/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <cstdint>

namespace ams::bluetooth::ble_connection {
    // Values are exposed by the connection-probe protocol; preserve their ABI.
    enum class State : std::uint32_t {
        Idle = 0, Armed = 1, Pending = 2, Connected = 3, Stopping = 4,
        Terminal = 5, Quarantined = 6
    };
    enum class Reason : std::uint32_t {
        None = 0, Stopped = 1, TimedOut = 2, Ambiguous = 3, ConnectFailed = 4,
        ActionFailed = 5, InvalidConnection = 6, ConflictingConnection = 7,
        CallbackFailed = 8, PostTerminalEvent = 9
    };
}
