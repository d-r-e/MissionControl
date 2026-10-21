/* SPDX-License-Identifier: GPL-2.0-only */
// Inert timing comparison with the successful control's12s preamble. This is
// NOT a documented SDK readiness requirement and performs no scan/GATT/action.
bool ConfirmSettledStadia() {
    Log("STADIA TIMING COMPARISON: 12 seconds idle after SDK initialization. NO app-issued scan, connection, pairing or GATT request.\n");
    auto started = armGetSystemTick();
    while (g_running && armTicksToNs(armGetSystemTick() - started) < 12'000'000'000ULL && Pump())
        svcSleepThread(50'000'000);
    if (!g_running) return false;
    Log("Release A, then press fresh A for ONE settled Stadia connection. Keep Stadia orange-flashing; no pairing dialogs. PLUS aborts.\n");
    missioncontrol::probe::StartGate gate;
    started = armGetSystemTick();
    while (g_running && armTicksToNs(armGetSystemTick() - started) < 30'000'000'000ULL && Pump()) {
        if (gate.Update((padGetButtons(&g_pad) & HidNpadButton_A) != 0,
                        (padGetButtonsDown(&g_pad) & HidNpadButton_A) != 0)) return true;
        svcSleepThread(50'000'000);
    }
    return false;
}
