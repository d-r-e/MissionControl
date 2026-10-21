/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace ams::bluetooth::event_progress {
    // v2 introduced buffered transport; v3 adds passive native callback fields.
    // Still exact224byte OutMapAliasBuffer, NOT inline sf::Out<Status>.
    constexpr std::uint32_t ProtocolVersion = 3;
    constexpr std::uint32_t ConnectionDetailsValid = 1u << 31;
    constexpr std::uint32_t StatusCommand = 11;
    enum class Lane : std::uint32_t { Core, Hid, Ble, Count };
    enum class Phase : std::uint32_t { Idle, NativeRead, Processing, ConsumerWait };
    constexpr std::size_t LaneCount = static_cast<std::size_t>(Lane::Count);

    // Diagnostic observations only. Counts are not connection ownership or proof
    // of native completion. No address, ARUID, handle, payload or key is stored.
    struct LaneStatus {
        std::uint32_t phase, last_event_type;
        std::uint64_t phase_since_tick;
        std::uint32_t begun, finished, forward_waits, consumer_reads;
        std::uint32_t wait_returns, unstable;
        std::uint64_t last_wait_ticks, max_wait_ticks;
        std::uint32_t last_connection_result, connection_details;
    };
    struct Status {
        std::uint32_t protocol_version, lane_count;
        std::uint64_t sample_tick;
        std::uint32_t connect_hook_attempts, btm_connect_attempts, passthrough_attempts, connection_events;
        std::array<LaneStatus, LaneCount> lanes;
    };
    static_assert(sizeof(LaneStatus) == 64 && sizeof(Status) == 224);
    static_assert(offsetof(Status, lanes) == 32);
    static_assert(std::is_trivial_v<Status> && std::is_trivially_copyable_v<Status> && std::is_standard_layout_v<Status>);
    // Pinned SF PrepareForReply adds16 bytes padding +16 CMIF header;
    // a simple HIPC response header is8 bytes.224 inline bytes encode264,
    // exceeding the256-byte TLS message buffer even though the POD fits alone.
    constexpr std::size_t InlineReplyBytes(std::size_t payload) {
        return 8 + 16 + 16 + ((payload + 3) & ~std::size_t{3});
    }
    static_assert(InlineReplyBytes(sizeof(Status)) > 0x100);
    static_assert(InlineReplyBytes(0) <= 0x100); // Buffered reply carries no POD inline.
    inline bool CopyStatus(const Status &status, void *destination, std::size_t size) {
        if (!destination || size != sizeof(Status)) return false;
        std::memcpy(destination, &status, sizeof(status));
        return true;
    }
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);

    class Progress {
        struct AtomicLane {
            std::atomic<std::uint32_t> sequence{}, phase{}, event_type{};
            std::atomic<std::uint64_t> since{};
            std::atomic<std::uint32_t> begun{}, finished{}, waits{}, reads{}, returns{};
            std::atomic<std::uint64_t> last_wait{}, max_wait{};
            std::atomic<std::uint32_t> connection_result{}, connection_details{};
        };
        std::array<AtomicLane, LaneCount> m_lanes{};
        std::atomic<std::uint32_t> m_connects{}, m_btm_connects{}, m_passthrough{}, m_connection_events{};
        AtomicLane *Find(Lane lane) {
            const auto i = static_cast<std::size_t>(lane);
            return i < LaneCount ? &m_lanes[i] : nullptr;
        }
        // One producer (the shared event thread) changes phase for each lane.
        // Consumer reads increment only 'reads', never clear/wake/advance phase.
        static void StorePhaseFields(AtomicLane &lane, Phase phase, std::uint64_t tick) {
            lane.since.store(tick);
            lane.phase.store(static_cast<std::uint32_t>(phase));
        }
        static void SetPhase(AtomicLane &lane, Phase phase, std::uint64_t tick) {
            lane.sequence.fetch_add(1);
            StorePhaseFields(lane, phase, tick);
            lane.sequence.fetch_add(1);
        }
    public:
        void Begin(Lane kind, std::uint64_t tick) {
            if (auto *lane = Find(kind)) {
                lane->begun.fetch_add(1);
                SetPhase(*lane, Phase::NativeRead, tick);
            }
        }
        void NativeRead(Lane kind, std::uint32_t type, std::uint64_t tick) {
            if (auto *lane = Find(kind)) {
                // Type, phase and timestamp belong to one snapshot generation.
                lane->sequence.fetch_add(1);
                lane->event_type.store(type);
                StorePhaseFields(*lane, Phase::Processing, tick);
                lane->sequence.fetch_add(1);
            }
        }
        void BeginWait(Lane kind, std::uint64_t tick) {
            if (auto *lane = Find(kind)) {
                lane->waits.fetch_add(1);
                SetPhase(*lane, Phase::ConsumerWait, tick);
            }
        }
        void ConsumerRead(Lane kind) {
            if (auto *lane = Find(kind)) lane->reads.fetch_add(1);
        }
        void EndWait(Lane kind, std::uint64_t tick) {
            if (auto *lane = Find(kind)) {
                const auto start = lane->since.load();
                const auto elapsed = tick >= start ? tick - start : 0;
                lane->last_wait.store(elapsed);
                if (elapsed > lane->max_wait.load()) lane->max_wait.store(elapsed);
                lane->returns.fetch_add(1);
                SetPhase(*lane, Phase::Processing, tick);
            }
        }
        void Finish(Lane kind, std::uint64_t tick) {
            if (auto *lane = Find(kind)) {
                lane->finished.fetch_add(1);
                SetPhase(*lane, Phase::Idle, tick);
            }
        }
        void ConnectHook(bool btm_client, bool passthrough) {
            m_connects.fetch_add(1);
            if (btm_client) m_btm_connects.fetch_add(1);
            if (passthrough) m_passthrough.fetch_add(1);
        }
        // Shared event thread only. Retain the LAST ClientConnection's scalar
        // fields across unrelated events. Never store its address/IDs/payload.
        void ClientConnection(std::uint32_t result, std::uint8_t status, std::uint16_t reason) {
            auto &lane = m_lanes[static_cast<std::size_t>(Lane::Ble)];
            lane.sequence.fetch_add(1);
            lane.connection_result.store(result);
            lane.connection_details.store(ConnectionDetailsValid | status | (std::uint32_t{reason} << 8));
            m_connection_events.fetch_add(1);
            lane.sequence.fetch_add(1);
        }
        Status Snapshot(std::uint64_t tick) const {
            Status out{};
            out.protocol_version = ProtocolVersion;
            out.lane_count = LaneCount;
            out.sample_tick = tick;
            out.connect_hook_attempts = m_connects.load();
            out.btm_connect_attempts = m_btm_connects.load();
            out.passthrough_attempts = m_passthrough.load();
            out.connection_events = m_connection_events.load();
            for (std::size_t i = 0; i < LaneCount; ++i) {
                const auto &lane = m_lanes[i];
                auto &copy = out.lanes[i];
                // Bounded, lock-free phase snapshot. On instability age/type are
                // not authoritative. Other counters are individually atomic,
                // not a transaction or equality invariant under concurrency.
                copy.unstable = 1;
                for (unsigned attempt = 0; attempt < 3; ++attempt) {
                    const auto before = lane.sequence.load();
                    copy.phase = lane.phase.load();
                    copy.phase_since_tick = lane.since.load();
                    copy.last_event_type = lane.event_type.load();
                    copy.last_connection_result = lane.connection_result.load();
                    copy.connection_details = lane.connection_details.load();
                    const auto after = lane.sequence.load();
                    if (!(before & 1) && before == after) {
                        copy.unstable = 0;
                        break;
                    }
                }
                copy.begun = lane.begun.load();
                copy.finished = lane.finished.load();
                copy.forward_waits = lane.waits.load();
                copy.consumer_reads = lane.reads.load();
                copy.wait_returns = lane.returns.load();
                copy.last_wait_ticks = lane.last_wait.load();
                copy.max_wait_ticks = lane.max_wait.load();
            }
            return out;
        }
    };
}
