/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stratosphere.hpp>
#include "ble_connection_trace_protocol.hpp"
#include "bluetooth_types.hpp"
#include "../btdrv_shim.h"

namespace ams::bluetooth::connection_trace {
    Result Begin(const BeginRequest &request);
    Result GetStatus(std::uint64_t token, Status &status);
    Result Retire(std::uint64_t token);
    std::uint64_t ObserveRequest(const BtdrvGattConnectRequest &request, bool btm_client);
    void ObserveDriverReturn(std::uint64_t ticket, Result result);
    void ObserveEvent(bluetooth::BleEventType type, const bluetooth::BleEventInfo &event);
}
