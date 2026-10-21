/*
 * Copyright (c) 2026 MissionControl contributors
 * SPDX-License-Identifier: GPL-2.0-only
 */
#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include "ble_connection_state.hpp"

namespace ams::bluetooth::ble_connection {

using Address = std::array<std::uint8_t, 6>;
enum class ConnectDisposition { Unrelated, Captured, Rejected };
enum class ActionType { None, Cancel, Disconnect };

// Command payload, not a diagnostic record. Never log this tuple.
struct Action {
    ActionType type{ActionType::None};
    Address address{};
    std::uint8_t client_if{};
    bool is_direct{};
    std::uint32_t conn_id{0xffffffffu}; // Native GATT connection id, NOT a BTM handle.
};
struct Snapshot {
    State state{State::Idle};
    Reason reason{Reason::None};
    bool captured{}, command_returned{}, command_accepted{}, stop_requested{};
    bool ambiguous{}, action_in_flight{}, connection_known{};
    std::uint32_t actions_issued{}, action_failures{};
};

// Externally serialized, non-copyable, one-shot diagnostic lease. No SDK calls.
// BeforeConnect binds the actual native command-65 tuple BEFORE forwarding.
// Call AfterConnect exactly once, only for a Captured request. No own registration.
// Rejected must NOT be forwarded; Unrelated is not owned by this lease. Foreign
// same-address traffic is Unrelated but permanently disables automatic actions.
// TakeAction reserves an action; execute it once and call CompleteAction exactly
// once. An accepted IPC is not terminal. Callbacks must be native GATT events.
// A successful terminal event retires the instance. A later success is ambiguous
// (native ids may be reused), so it quarantines rather than disconnecting by a stale id.
class ConnectionLease {
public:
    static constexpr std::uint32_t InvalidConnection = 0xffffffffu;
    static constexpr std::uint64_t MaxTimeout = 30000; // Caller ticks must be milliseconds.
    ConnectionLease() = default;
    ConnectionLease(const ConnectionLease &) = delete;
    ConnectionLease &operator=(const ConnectionLease &) = delete;
    ConnectionLease(ConnectionLease &&) = delete;
    ConnectionLease &operator=(ConnectionLease &&) = delete;

    bool Arm(const Address &target, std::uint64_t owner, std::uint64_t now,
             std::uint64_t timeout) {
        bool zero = true, ff = true;
        for (auto byte : target) { zero &= byte == 0; ff &= byte == 0xff; }
        if (armed_ || zero || ff || owner == 0 || timeout == 0 || timeout > MaxTimeout ||
            now > std::numeric_limits<std::uint64_t>::max() - timeout) return false;
        target_ = target;
        owner_ = owner;
        deadline_ = now + timeout;
        armed_ = true;
        return true;
    }

    ConnectDisposition BeforeConnect(std::uint64_t owner, const Address &address,
                                     std::uint8_t client_if, bool direct) {
        if (!armed_ || address != target_) return ConnectDisposition::Unrelated;
        if (owner != owner_) {
            // Observe foreign traffic, never claim it or issue actions against it.
            if (!terminal_) {
                ambiguous_ = true;
                Quarantine(Reason::Ambiguous);
            }
            return ConnectDisposition::Unrelated;
        }
        if (captured_ || terminal_ || quarantined_) return ConnectDisposition::Rejected;
        client_if_ = client_if;
        direct_ = direct;
        captured_ = true; // Stop before capture still permits this single command.
        return ConnectDisposition::Captured;
    }

    void AfterConnect(bool result_success) {
        if (!captured_ || returned_) return;
        returned_ = true;
        accepted_ = result_success;
        if (!result_success) Quarantine(Reason::ConnectFailed);
    }

    void OnConnection(const Address &address, std::uint8_t client_if,
                      bool result_success, std::uint32_t status, std::uint32_t conn_id) {
        if (!captured_ || address != target_ || client_if != client_if_ || ambiguous_ ||
            (status != 0 && status != 2)) return;
        if (terminal_) {
            if (status == 0 && result_success) {
                terminal_ = false;
                ambiguous_ = true;
                Quarantine(Reason::PostTerminalEvent);
            }
            return;
        }
        if (!result_success) {
            Quarantine(Reason::CallbackFailed);
            return;
        }
        if (conn_id == InvalidConnection) {
            unsafe_id_ = true;
            Quarantine(Reason::InvalidConnection);
            return;
        }
        if (status == 2) {
            // A different known id is not our terminal event.
            if (known_ && conn_id != conn_id_) return;
            terminal_ = true;
            return;
        }
        if (known_ && conn_id != conn_id_) {
            ambiguous_ = true;
            Quarantine(Reason::ConflictingConnection);
            return;
        }
        known_ = true;
        conn_id_ = conn_id;
    }

    void Stop() {
        if (!armed_ || terminal_) return;
        stop_ = true;
        if (reason_ == Reason::None) reason_ = Reason::Stopped;
    }

    // Monotonic, non-wrapping caller time; an expired lease keeps its ownership.
    void Tick(std::uint64_t now) {
        if (!armed_ || terminal_ || now < deadline_) return;
        stop_ = true;
        if (!quarantined_) reason_ = Reason::TimedOut;
    }

    Action TakeAction() {
        if (!captured_ || !returned_ || terminal_ || in_flight_ != ActionType::None ||
            ambiguous_ || unsafe_id_ || !stop_) return {};
        ActionType type = ActionType::None;
        // Even in quarantine a late owned success can be disconnected, but only
        // with one unambiguous native id, and never after a failed disconnect.
        if (known_ && !disconnect_issued_) {
            type = ActionType::Disconnect;
            disconnect_issued_ = true;
        } else if (!known_ && accepted_ && !quarantined_ && !cancel_issued_) {
            type = ActionType::Cancel;
            cancel_issued_ = true;
        }
        if (type == ActionType::None) return {};
        in_flight_ = type;
        ++actions_issued_;
        return {type, target_, client_if_, direct_,
                type == ActionType::Disconnect ? conn_id_ : InvalidConnection};
    }

    void CompleteAction(bool success) {
        if (in_flight_ == ActionType::None) return;
        in_flight_ = ActionType::None;
        if (!success) {
            ++action_failures_;
            Quarantine(Reason::ActionFailed);
        }
    }

    Snapshot GetSnapshot() const {
        State state = State::Idle;
        if (armed_) {
            state = terminal_ ? State::Terminal : quarantined_ ? State::Quarantined :
                    stop_ ? State::Stopping : known_ ? State::Connected :
                    captured_ ? State::Pending : State::Armed;
        }
        return {state, reason_, captured_, returned_, accepted_, stop_, ambiguous_,
                in_flight_ != ActionType::None, known_, actions_issued_, action_failures_};
    }

private:
    void Quarantine(Reason reason) {
        quarantined_ = true;
        stop_ = true;
        reason_ = reason;
    }

    Address target_{};
    std::uint64_t owner_{}, deadline_{};
    std::uint8_t client_if_{};
    bool direct_{}, armed_{}, captured_{}, returned_{}, accepted_{};
    bool stop_{}, terminal_{}, quarantined_{}, ambiguous_{}, unsafe_id_{}, known_{};
    bool cancel_issued_{}, disconnect_issued_{};
    std::uint32_t conn_id_{InvalidConnection}, actions_issued_{}, action_failures_{};
    Reason reason_{Reason::None};
    ActionType in_flight_{ActionType::None};
};

} // namespace ams::bluetooth::ble_connection
