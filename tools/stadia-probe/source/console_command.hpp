/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <cstddef>
#include <cstring>

namespace missioncontrol::probe {
    enum class ConsoleCommand { None, Invalid, Status, NativeControl, HidComplete,
        HidIncomplete, RealComplete, RealIncomplete, Cancel, Quit, FileTransfer };

    // Exact allowlist. No arguments, addresses, filenames, shell or GATT commands.
    class ConsoleCommandParser {
        char m_line[32]{};
        std::size_t m_size = 0;
        bool m_invalid = false;
    public:
        ConsoleCommand Feed(unsigned char byte) {
            if (byte != '\n') {
                if (byte < 0x20 || byte > 0x7e || m_size == sizeof(m_line) - 1) m_invalid = true;
                else if (!m_invalid) m_line[m_size++] = static_cast<char>(byte);
                return ConsoleCommand::None;
            }
            m_line[m_size] = 0;
            ConsoleCommand result = ConsoleCommand::Invalid;
            if (!m_invalid) {
                struct Entry { const char *name; ConsoleCommand command; };
                constexpr Entry entries[] = {
                    {"status", ConsoleCommand::Status}, {"control", ConsoleCommand::NativeControl},
                    {"hid-complete", ConsoleCommand::HidComplete}, {"hid-incomplete", ConsoleCommand::HidIncomplete},
                    {"real-complete", ConsoleCommand::RealComplete}, {"real-incomplete", ConsoleCommand::RealIncomplete},
                    {"cancel", ConsoleCommand::Cancel}, {"quit", ConsoleCommand::Quit},
                    {"file", ConsoleCommand::FileTransfer}
                };
                for (const auto &entry : entries)
                    if (std::strcmp(m_line, entry.name) == 0) result = entry.command;
            }
            m_size = 0;
            m_invalid = false;
            return result;
        }
    };
}
