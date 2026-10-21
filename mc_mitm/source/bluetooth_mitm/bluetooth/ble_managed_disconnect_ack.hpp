/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once

namespace ams::bluetooth::ble_connection {
    // Externally serialized acknowledgement bookkeeping for one diagnostic
    // managed disconnect. Token/session validation remains the service's job.
    // Native completion may win the race with the NRO's IPC-result report.
    class ManagedDisconnectAck {
    public:
        enum class Disposition { Reject, CompletePending, AlreadyTerminal };
        bool Issue() {
            if (issued_) return false;
            issued_ = true;
            return true;
        }
        Disposition Record(bool pending, bool terminal) {
            if (!issued_ || recorded_ || (!pending && !terminal)) return Disposition::Reject;
            recorded_ = true;
            return pending ? Disposition::CompletePending : Disposition::AlreadyTerminal;
        }
    private:
        bool issued_ = false;
        bool recorded_ = false;
    };
}
