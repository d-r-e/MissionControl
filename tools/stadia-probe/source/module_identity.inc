/* SPDX-License-Identifier: GPL-2.0-only */
// Read-only process/module metadata ONLY. No process-memory reads, debug attach,
// process start/stop, program argument changes or Bluetooth service requests.
bool ReadRunningMissionControlIdentity(const std::uint8_t *expected) {
    if (!Status("Read-only PM metadata service", pmdmntInitialize())) return false;
    u64 process_id = 0;
    const Result lookup = pmdmntGetProcessId(&process_id, 0x010000000000bd00ULL);
    pmdmntExit();
    if (!Status("Read-only MissionControl process lookup", lookup)) return false;
    if (!Status("Read-only loader metadata service", ldrDmntInitialize())) return false;
    std::array<LoaderModuleInfo, 8> modules{};
    s32 count = 0;
    const Result rc = ldrDmntGetProcessModuleInfo(process_id, modules.data(), modules.size(), &count);
    ldrDmntExit();
    if (!Status("Read-only loaded module build IDs", rc)) return false;
    if (count < 0 || static_cast<std::size_t>(count) > modules.size()) {
        Log("Invalid module metadata count; no identity claim.\n");
        return false;
    }
    Log("LOADED MC MODULE METADATA: returned=%d capacity=%zu. No PID, memory addresses or code dumped.\n", count, modules.size());
    if (static_cast<std::size_t>(count) == modules.size())
        Log("Module metadata capacity reached; completeness NOT assumed.\n");
    unsigned matches = 0;
    for (s32 i = 0; i < count; ++i) {
        const auto *id = modules[i].build_id;
        if (expected && !std::memcmp(id, expected, sizeof(modules[i].build_id))) ++matches;
        Log("  Code build ID[%d]: ", i);
        for (unsigned j = 0; j < sizeof(modules[i].build_id); ++j) Log("%02x", id[j]);
        const auto known = missioncontrol::probe::IdentifyModuleBuild(id);
        const char *label = known == missioncontrol::probe::KnownModuleBuild::NativeHidP2 ? "corrected native-HID protocol2 archive" :
            known == missioncontrol::probe::KnownModuleBuild::RetiredFlagsP1 ? "retired Flags protocol1 archive" :
            known == missioncontrol::probe::KnownModuleBuild::PreDiscovery ? "pre-discovery protocol3 diagnostic archive" :
            known == missioncontrol::probe::KnownModuleBuild::ManagedControl ? "tracked native-control protocol6 archive" :
            known == missioncontrol::probe::KnownModuleBuild::NativeReturn ? "native-driver-return trace2 archive" :
            "not one of the pinned archives";
        Log(" (%s).\n", label);
    }
    const bool complete = count > 0 && static_cast<std::size_t>(count) < modules.size();
    if (expected) Log("Loaded-code gate: exact expected module matches=%u metadata_complete=%u.\n", matches, complete ? 1u : 0u);
    return complete && (!expected || matches == 1);
}
