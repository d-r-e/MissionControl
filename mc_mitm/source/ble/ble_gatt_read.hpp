// One outstanding characteristic read. Portable so callback matching and bounds are tested offline.
#pragma once
#include <cstddef>
#include <cstdint>

namespace ams::ble::gatt {

    // Observe beyond one 30-second SMP/ATT timeout without issuing a second request. This is a bounded
    // diagnostic window, not proof that authentication succeeded or that every multi-stage procedure fits.
    constexpr std::int64_t ReadTimeoutMilliseconds = 35'000;

    struct ReadDeadline {
        std::int64_t deadline_ms;
        explicit constexpr ReadDeadline(std::int64_t start_ms) : deadline_ms(start_ms + ReadTimeoutMilliseconds) {}
        constexpr bool Expired(std::int64_t now_ms) const { return now_ms >= deadline_ms; }
    };

    struct ReadState {
        bool pending = false;
        bool answered = false;
        bool failed = false;
        std::uint32_t connection = 0;
        std::uint16_t uuid = 0;
        std::size_t size = 0;

        bool Begin(std::uint32_t conn, std::uint16_t characteristic) {
            if (pending) {
                return false;
            }
            pending = true;
            answered = failed = false;
            connection = conn;
            uuid = characteristic;
            size = 0;
            return true;
        }

        bool Accept(std::uint32_t conn, std::uint16_t characteristic, std::uint32_t result,
                    std::size_t length, std::size_t capacity) {
            if (!pending || answered || conn != connection || characteristic != uuid) {
                return false;
            }
            answered = true;
            failed = result != 0 || length == 0 || length > capacity;
            size = failed ? 0 : length;
            return true;
        }

        void Finish() {
            pending = false;
        }
    };

}
