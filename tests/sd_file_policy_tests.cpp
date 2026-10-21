/* SPDX-License-Identifier: GPL-2.0-only */
#include "sd_file_policy.hpp"
#include <cassert>
#include <iostream>
#include <string>

using namespace missioncontrol::probe::sd;

int main() {
    // SafePath: root, normal relative roots, sensitive keywords, directory traversal.
    assert(SafePath("/"));
    assert(SafePath("/config/MissionControl/missioncontrol.ini"));
    assert(SafePath("/config/MissionControl/remote-staging/test.txt"));
    assert(SafePath("/switch/stadia-ble-probe.nro"));
    assert(SafePath("/atmosphere/contents/010000000000bd00/exefs.nsp"));
    for (const std::string &bad : {
        std::string(""), std::string("relative/path"), std::string("/trailing/slash/"), std::string("/.."), std::string("/../escape"),
        std::string("/config/../escape"), std::string("/./dot"), std::string("/nintendo/album"), std::string("/emummc/raw_based"),
        std::string("/backup/restore"), std::string("/backups/2026"), std::string("/nand/user"), std::string("/boot0.bin"),
        std::string("/boot1.bin"), std::string("/cal0.bin"), std::string("/prodinfo.bin"), std::string("/prodinfo_backup.bin"),
        std::string("/keys/prod.keys"), std::string("/prod.keys"), std::string("/title.keys"), std::string("/switch/keys.dat"),
        std::string("/cert.pem"), std::string("/my_credential.json"), std::string("/auth_token.txt"),
        std::string("/config/MissionControl/stadia-probe-address.txt"),
        std::string("/zero\0byte", 10), std::string(MaxPath + 1, 'a')
    }) {
        assert(!SafePath(bad));
        assert(!CanReadFile(bad));
        assert(!CanWriteFile(bad));
    }

    // CanReadFile: only known extensions; no executable scripts, sensitive dot-files.
    assert(CanReadFile("/config/MissionControl/missioncontrol.ini"));
    assert(CanReadFile("/atmosphere/contents/010000000000bd00/toolbox.json"));
    assert(CanReadFile("/atmosphere/contents/010000000000bd00/mitm.lst"));
    assert(CanReadFile("/config/MissionControl/stadia-ble-probe.log"));
    assert(CanReadFile("/switch/stadia-ble-probe.nro"));
    assert(CanReadFile("/atmosphere/contents/010000000000bd00/exefs.nsp"));
    assert(CanReadFile("/atmosphere/exefs_patches/bluetooth_patches/test.ips"));
    for (const std::string bad_read : {
        "/config/MissionControl/no_ext", "/config/MissionControl/script.sh",
        "/config/MissionControl/python.py", "/atmosphere/flags/boot2.flag",
        "/switch/.hidden.nro"
    }) {
        assert(!CanReadFile(bad_read));
        assert(!CanWriteFile(bad_read));
    }

    // CanWriteFile: ONLY /config/MissionControl/missioncontrol.ini and /config/MissionControl/remote-staging/<file>
    // Never patches, boot flags, exefs, switch/nro direct, nested staging, or sensitive files.
    assert(CanWriteFile("/config/MissionControl/missioncontrol.ini"));
    assert(CanWriteFile("/config/MissionControl/remote-staging/candidate.ini"));
    assert(CanWriteFile("/config/MissionControl/remote-staging/test.txt"));
    assert(CanWriteFile("/config/MissionControl/remote-staging/exefs.nsp")); // Staging only; not active
    for (const std::string bad_write : {
        "/atmosphere/contents/010000000000bd00/exefs.nsp",
        "/atmosphere/contents/010000000000bd00/flags/boot2.flag",
        "/atmosphere/exefs_patches/bluetooth_patches/patch.ips",
        "/config/MissionControl/remote-staging/patch.ips", // Rejected even in staging!
        "/config/MissionControl/remote-staging/boot2.flag",
        "/config/MissionControl/remote-staging/nested/test.txt",
        "/config/MissionControl/other.ini",
        "/switch/stadia-ble-probe.nro",
        "/switch/remote-staging/test.txt"
    }) {
        assert(!CanWriteFile(bad_write));
    }

    std::cout << "SD file security policy tests passed (no hardware accessed).\n";
}
