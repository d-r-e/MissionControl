/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "ble_connection_state.hpp"

// SDK-free helpers for the one-shot, owned BLE connection diagnostic. The
// module-side lease owns the native command-65 tuple; this client only arms,
// waits and performs the managed disconnect it is told to perform. No device
// address, ARUID or key ever appears in output.
namespace missioncontrol::probe {

    // Safety gate after the real pending-cancel trial caused reported system
    // sluggishness. Deliberately no CLI/env override; native BTM completion and
    // system-wide effects must be understood before another deployment/trial.
    constexpr bool ConnectionDiagnosticsPaused = true;

    // Documented btdrv BleConnectionInfo wire layout: u32 connection_handle,
    // 6-byte address, 2 padding bytes. Byte fields only: no packed structs or
    // unaligned integer accesses are used.
    constexpr std::size_t ConnectionInfoSize = 12;
    constexpr std::uint32_t InvalidConnectionHandle = 0xffffffffu;

    // Resolves the BTM connection handle for exactly one non-invalid record
    // whose address matches the reserved target. Returns false rather than
    // guessing if no record matches; a missing handle is never substituted.
    inline bool FindConnectionHandle(const std::uint8_t *records, std::size_t size, unsigned count,
                                     const std::array<std::uint8_t, 6> &target,
                                     std::uint32_t *out_handle) {
        if (!records || !out_handle || count > size / ConnectionInfoSize) return false;
        bool found = false;
        std::uint32_t candidate = InvalidConnectionHandle;
        for (unsigned i = 0; i < count; ++i) {
            const std::uint8_t *record = records + i * ConnectionInfoSize;
            std::uint32_t handle = 0;
            std::memcpy(&handle, record, sizeof(handle));
            if (handle == InvalidConnectionHandle) continue;
            if (std::memcmp(record + sizeof(handle), target.data(), target.size()) != 0) continue;
            if (found) return false; // Even duplicate identical handles are ambiguous.
            candidate = handle;
            found = true;
        }
        if (found) *out_handle = candidate;
        return found;
    }

    enum class TargetPresence { Unknown, Absent, Present };
    // A full batch may be truncated. Absence is only a managed-handle snapshot,
    // NOT proof that BTM has forgotten every pending native request.
    inline TargetPresence InspectTargetConnections(const std::uint8_t *records, std::size_t size,
                                                   unsigned count, const std::array<std::uint8_t, 6> &target) {
        if (!records || count >= size / ConnectionInfoSize) return TargetPresence::Unknown;
        for (unsigned i = 0; i < count; ++i) {
            const auto *record = records + i * ConnectionInfoSize;
            std::uint32_t handle = 0;
            std::memcpy(&handle, record, sizeof(handle));
            if (handle != InvalidConnectionHandle &&
                std::memcmp(record + sizeof(handle), target.data(), target.size()) == 0)
                return TargetPresence::Present;
        }
        return TargetPresence::Absent;
    }

    // RAM-only selection for the explicitly requested native UUID128 control.
    // Observe the whole bounded scan, not just the first result. Rotation,
    // multiple peers, invalid addresses or truncated batches invalidate it.
    class SingleControlTarget {
        std::array<std::uint8_t, 6> target_{};
        bool seen_ = false;
        std::uint32_t issues_ = 0;
    public:
        enum Issue : std::uint32_t { InvalidAddress = 1, MultipleAddresses = 2, UntrustedBatch = 4 };
        void Invalidate() { issues_ |= UntrustedBatch; }
        bool Seen() const { return seen_; }
        std::uint32_t Issues() const { return issues_; }
        void Observe(const std::array<std::uint8_t, 6> &address) {
            bool zero = true, ff = true;
            for (auto byte : address) { zero &= byte == 0; ff &= byte == 0xff; }
            if (zero || ff) { issues_ |= InvalidAddress; return; }
            if (seen_ && address != target_) { issues_ |= MultipleAddresses; return; }
            target_ = address;
            seen_ = true;
        }
        bool Resolve(std::array<std::uint8_t, 6> *out) const {
            if (!out || !seen_ || issues_) return false;
            *out = target_;
            return true;
        }
    };

    // A zero token is reserved; the module rejects it. The value only has to be
    // nonzero and stable for one lease, not unpredictable.
    inline std::uint64_t MakeLeaseToken(std::uint64_t ticks) {
        const std::uint64_t token = ticks ^ (ticks >> 32);
        return token ? token : 1;
    }

    // Classification is diagnostic, not permission to abandon cleanup:
    // quarantine may still produce a late owned connection requiring disconnect.
    // Observe for a bounded interval unless ambiguity forbids further actions.
    enum class LeaseOutcome { Running, Connected, Retired, Quarantined };

    inline LeaseOutcome ClassifyLease(std::uint32_t connection_state,
                                      std::uint32_t terminal, std::uint32_t ambiguous) {
        using State = ams::bluetooth::ble_connection::State;
        const auto state = static_cast<State>(connection_state);
        if (ambiguous || state == State::Quarantined) return LeaseOutcome::Quarantined;
        if (terminal || state == State::Terminal) return LeaseOutcome::Retired;
        if (state == State::Connected) return LeaseOutcome::Connected;
        return LeaseOutcome::Running;
    }

    struct CancellationEvidence {
        std::uint32_t captured{}, cancels{}, disconnects{}, connected{}, disconnected{}, reason{}, last_result{};
        bool terminal{}, ambiguous{}, owned_handle_absent{};
    };
    enum class CancellationOutcome { NotExercised, Unconfirmed, TerminalAfterCancel, LateConnectionCleaned };
    inline CancellationOutcome ClassifyCancellation(const CancellationEvidence &e) {
        if (!e.captured || !e.cancels) return CancellationOutcome::NotExercised;
        using Reason = ams::bluetooth::ble_connection::Reason;
        const auto reason = static_cast<Reason>(e.reason);
        if (e.captured != 1 || e.cancels != 1 || !e.terminal || e.ambiguous ||
            e.last_result || !e.owned_handle_absent ||
            (reason != Reason::Stopped && reason != Reason::TimedOut))
            return CancellationOutcome::Unconfirmed;
        if (e.connected == 0 && e.disconnected == 1 && e.disconnects == 0)
            return CancellationOutcome::TerminalAfterCancel;
        if (e.connected == 1 && e.disconnected == 1 && e.disconnects == 1)
            return CancellationOutcome::LateConnectionCleaned;
        return CancellationOutcome::Unconfirmed;
    }

} // namespace missioncontrol::probe
