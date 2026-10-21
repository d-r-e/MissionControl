#include "console_command.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
using namespace missioncontrol::probe;
ConsoleCommand Feed(ConsoleCommandParser &parser, const std::string &data) {
    ConsoleCommand result = ConsoleCommand::None;
    for (unsigned char byte : data) {
        const auto next = parser.Feed(byte);
        if (next != ConsoleCommand::None) result = next;
    }
    return result;
}
int main() {
    ConsoleCommandParser parser;
    assert(Feed(parser, "sta") == ConsoleCommand::None);
    assert(Feed(parser, "tus\n") == ConsoleCommand::Status);
    assert(Feed(parser, "control\n") == ConsoleCommand::NativeControl);
    assert(Feed(parser, "hid-complete\n") == ConsoleCommand::HidComplete);
    assert(Feed(parser, "hid-incomplete\n") == ConsoleCommand::HidIncomplete);
    assert(Feed(parser, "real-complete\n") == ConsoleCommand::RealComplete);
    assert(Feed(parser, "real-incomplete\n") == ConsoleCommand::RealIncomplete);
    assert(Feed(parser, "cancel\n") == ConsoleCommand::Cancel);
    assert(Feed(parser, "quit\n") == ConsoleCommand::Quit);
    assert(Feed(parser, "file\n") == ConsoleCommand::FileTransfer);
    for (const auto &bad : {"\n", "status \n", "status;quit\n", "STATUS\n", "sh\n", "pair\n", "clear\n", "status\r\n"})
        assert(Feed(parser, bad) == ConsoleCommand::Invalid);
    assert(Feed(parser, std::string("status\0quit\n", 12)) == ConsoleCommand::Invalid);
    assert(Feed(parser, std::string(10000, 'x') + "status\n") == ConsoleCommand::Invalid);
    assert(Feed(parser, "status\n") == ConsoleCommand::Status); // Recovers only after line boundary.
    std::cout << "Bounded console command allowlist tests passed\n";
}
