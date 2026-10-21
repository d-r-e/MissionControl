/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "ble_event_progress.hpp"

namespace ams::bluetooth::event_progress {
    Status GetStatus();
    void RecordConsumerRead(Lane lane);
    void RecordConnectHook(bool btm_client, bool passthrough);
    void RecordClientConnection(std::uint32_t result, std::uint8_t status, std::uint16_t reason);

    // Observes the existing control flow. Never waits, signals, takes a service
    // mutex or invokes native Bluetooth IPC. Destructor covers existing returns.
    class Dispatch {
        Lane m_lane;
    public:
        explicit Dispatch(Lane lane);
        ~Dispatch();
        Dispatch(const Dispatch &) = delete;
        Dispatch &operator=(const Dispatch &) = delete;
        void NativeRead(std::uint32_t type);
        void BeginWait();
        void EndWait();
    };
}
