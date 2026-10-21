#include "../tools/stadia-probe/source/start_gate.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {
    enum class Scenario { Confirm, Held, NoPress, AbortWait, AbortConfirm } scenario;
    std::uint64_t now;
    unsigned frames;
    bool prompt, g_running;
    int g_pad;
    constexpr unsigned HidNpadButton_A = 1;
    template<typename... Args> void Log(const char *text, Args...) {
        if (std::strstr(text, "Release A, then press fresh A")) prompt = true;
    }
    std::uint64_t armGetSystemTick() { return now; }
    std::uint64_t armTicksToNs(std::uint64_t ticks) { return ticks; }
    void svcSleepThread(std::uint64_t ns) { now += ns; }
    bool Pump() {
        if ((scenario == Scenario::AbortWait && now >= 1'000'000'000ULL) ||
            (scenario == Scenario::AbortConfirm && prompt)) g_running = false;
        if (prompt) ++frames;
        return g_running;
    }
    unsigned padGetButtons(int *) {
        if (scenario == Scenario::Held) return HidNpadButton_A;
        return scenario == Scenario::Confirm && frames >= 2 ? HidNpadButton_A : 0;
    }
    unsigned padGetButtonsDown(int *) {
        if (scenario == Scenario::Held) return HidNpadButton_A;
        return scenario == Scenario::Confirm && frames == 2 ? HidNpadButton_A : 0;
    }
    #include "../tools/stadia-probe/source/settled_stadia.inc"
    void Reset(Scenario next) { scenario = next; now = 0; frames = 0; prompt = false; g_running = true; }
}
int main() {
    Reset(Scenario::Confirm);
    assert(ConfirmSettledStadia() && prompt && frames == 2);
    assert(now >= 12'000'000'000ULL && now < 13'000'000'000ULL);
    for (auto next : {Scenario::Held, Scenario::NoPress}) {
        Reset(next); assert(!ConfirmSettledStadia());
        assert(prompt && g_running && now == 42'000'000'000ULL);
    }
    Reset(Scenario::AbortWait); assert(!ConfirmSettledStadia());
    assert(!g_running && !prompt && now < 12'000'000'000ULL);
    Reset(Scenario::AbortConfirm); assert(!ConfirmSettledStadia());
    assert(!g_running && prompt);
    std::puts("Settled Stadia: actual inert12s helper, fresh released-A, timeout and abort gates passed (no radio APIs).");
}
