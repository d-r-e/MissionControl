/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include "stadia_ble_scan_filter.hpp"
#include "stadia_ble_advertisement.hpp"

namespace ams::bluetooth::stadia_scan {
    // Pure bookkeeping, externally serialized with driver operations. Counts
    // successful additions per (index, hypothesis), rather than collapsing two
    // conditions at one index to a boolean. Duplicate adds are conservatively
    // reference-counted: uncertainty must never report a false clean state.
    class Tracker {
        Status m_status = [] {
            Status status{};
            status.protocol_version = ProtocolVersion;
            return status;
        }();
        std::uint16_t m_conditions[256][TrackedMarkerCount]{};
        std::uint8_t m_saturated[256]{};

        static constexpr unsigned Slot(ByteOrder order) {
            return order == ByteOrder::BigEndian ? 0 : order == ByteOrder::LittleEndian ? 1 :
                   order == ByteOrder::IncompleteBigEndian ? 2 : order == ByteOrder::NativeHid16 ? 3 :
                   order == ByteOrder::NativeControlLittleEndian ? 4 :
                   order == ByteOrder::NativeControlCanonical ? 5 : TrackedMarkerCount;
        }

        void ClearIndex(std::uint8_t index) {
            for (auto &count : m_conditions[index]) {
                m_status.removed += count;
                m_status.active -= count;
                count = 0;
            }
            m_saturated[index] = 0;
        }

        public:
            Status GetStatus() const { return m_status; }

            void RecordCondition(std::uint8_t index, ByteOrder order, bool add, std::uint32_t result) {
                if (Slot(order) >= TrackedMarkerCount) return;
                m_status.last_result = result;
                if (result != 0) { ++m_status.failed; return; }
                auto &count = m_conditions[index][Slot(order)];
                const unsigned bit = 1u << Slot(order);
                if (add) {
                    ++m_status.added;
                    if (count < 0xffff) { ++count; ++m_status.active; }
                    else m_saturated[index] |= bit;
                } else if (count != 0 && (m_saturated[index] & bit) == 0) {
                    --count;
                    --m_status.active;
                    ++m_status.removed;
                }
                // A saturated/uncertain condition remains active until a
                // successful whole-index deletion, regardless of condition deletes.
            }

            void RecordIndexDeletion(std::uint8_t index, std::uint32_t result) {
                if (!IsTrackedIndex(index)) return;
                m_status.last_result = result;
                if (result == 0) ClearIndex(index);
                else ++m_status.failed;
            }

            bool IsTrackedIndex(std::uint8_t index) const {
                if (m_saturated[index]) return true;
                for (const auto count : m_conditions[index]) if (count) return true;
                return false;
            }

            std::uint8_t GetBridgeMarkers() const {
                if (m_status.active == 0 || m_status.failed != 0) return 0; // Any unresolved private-filter error: fail closed until reboot.
                std::uint8_t markers = 0;
                for (unsigned index = 0; index < 256; ++index) {
                    if (m_saturated[index]) return 0; // Uncertain ownership: no synthesis.
                    for (unsigned slot = 0; slot < PrivateMarkerCount; ++slot) {
                        if (m_conditions[index][slot]) markers |= 1u << slot;
                    }
                }
                return markers & HidBridgeMarkerMask; // Native HID/control slots NEVER alter BTM response copies.
            }

            void LearnSizeConvention(std::uint32_t convention) {
                if (convention != 3 && !ValidAdvertisementFormat(convention)) return;
                if (m_status.ad_size_convention == 0) m_status.ad_size_convention = convention;
                else if (m_status.ad_size_convention != convention) m_status.ad_size_convention = 3;
            }

            void RecordBridge(BridgeResult result) {
                switch (result) {
                    case BridgeResult::Appended: ++m_status.bridged_advertisements; break;
                    case BridgeResult::NoSpace: ++m_status.bridge_no_space; break;
                    case BridgeResult::Unvalidated: ++m_status.bridge_unvalidated; break;
                    case BridgeResult::Malformed: ++m_status.bridge_rejected; break;
                    case BridgeResult::Unchanged: break;
                }
            }

            void ObserveManagedEvent(std::uint32_t type, bool scan, std::uint32_t result,
                                     std::uint8_t status, bool hid) {
                if (m_status.active == 0) return;
                ++m_status.managed_events;
                m_status.last_event_type = type;
                if (!scan) return;
                ++m_status.scan_events;
                m_status.last_scan_status = status;
                m_status.last_scan_result = result;
                if (result != 0) { ++m_status.scan_errors; return; }
                if (status == 2) ObserveAdvertisement(hid);
            }

            void ObserveAdvertisement(bool hid) {
                if (m_status.active == 0) return;
                ++m_status.driver_advertisements;
                if (hid) ++m_status.driver_hid_advertisements;
            }
    };
}
