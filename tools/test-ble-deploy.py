#!/usr/bin/env python3
"""Offline tests for tools/ble-deploy.py against a synthetic SD card."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parent.parent
TOOL = REPO / "tools" / "ble-deploy.py"
MODULE = Path("atmosphere/contents/010000000000bd00/exefs.nsp")
INI = Path("config/MissionControl/missioncontrol.ini")

LIVE_MODULE = b"OLDMODULE-BYTES"
NEW_MODULE = b"NEWBLEBUILD-BYTES-BYTES"
KEYS_BEFORE = b"must-never-be-read-or-hashed\n"

INI_TEXT = """; Mission Control
[general]
enable_rumble=true

[bluetooth]
host_name="MC_bench"
host_address=AA:BB:CC:DD:EE:FF

[misc]
analog_trigger_activation_threshold=27
experimental_ble_connection_probe=true

[ble]
;enable=false
log=false
radio_cycle=false
auto_connect=true
"""


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_sd(root):
    (root / MODULE.parent).mkdir(parents=True)
    (root / "switch" / "titles").mkdir(parents=True)
    (root / INI.parent / "configs").mkdir(parents=True)
    (root / MODULE).write_bytes(LIVE_MODULE)
    (root / INI).write_text(INI_TEXT)
    (root / "switch" / "prod.keys").write_bytes(KEYS_BEFORE)
    (root / "switch" / "titles" / "some.nca").write_bytes(b"nca")
    (root / INI.parent / "configs" / "18d1_9400.ini").write_text("[buttons]\nswap_ab=true\n")
    (root / "atmosphere" / "exefs_patches" / "layer" / "ips" / "a.ips").parent.mkdir(parents=True, exist_ok=True)
    (root / "atmosphere" / "exefs_patches" / "layer" / "ips" / "a.ips").write_bytes(b"IPS3patch")
    return root


def run(sd, *args, expect_fail=False):
    result = subprocess.run([sys.executable, str(TOOL), "--sd", str(sd), *args],
                            capture_output=True, text=True)
    if expect_fail:
        assert result.returncode != 0, f"expected failure, got:\n{result.stdout}\n{result.stderr}"
    else:
        assert result.returncode == 0, f"unexpected failure:\n{result.stdout}\n{result.stderr}"
    return result


def ini_text(sd):
    return (sd / INI).read_text()


def test_inspect_is_read_only(tmp):
    sd = make_sd(tmp / "inspect")
    out = run(sd).stdout
    assert f"{len(LIVE_MODULE)} bytes" in out, out
    assert sha256(sd / MODULE) in out, out
    assert "log=false" in out and "enable=true" not in out
    assert "host_name" not in out and "MC_bench" not in out      # values are never echoed
    assert "prod.keys" not in out


def test_wrong_live_module_refuses(tmp):
    sd = make_sd(tmp / "wrong")
    out = run(sd, "--module", str(write_build(tmp / "other.nsp")), "--backup-dir", str(tmp / "b0"),
              "--expect-module", "0" * 64, "--apply", expect_fail=True)
    assert "nothing written" in out.stdout + out.stderr
    assert (sd / MODULE).read_bytes() == LIVE_MODULE


def write_build(path):
    path.write_bytes(NEW_MODULE)
    return path


def test_apply_changes_only_two_files(tmp):
    sd = make_sd(tmp / "apply")
    backup = tmp / "backup"
    build = write_build(tmp / "new.nsp")
    run(sd, "--module", str(build), "--ble", "on", "--backup-dir", str(backup),
        "--expect-module", sha256(sd / MODULE), "--apply")

    receipt = json.loads((backup / "receipt.json").read_text())
    assert receipt["changed"] == sorted([str(MODULE), str(INI)]), receipt["changed"]
    assert (sd / MODULE).read_bytes() == NEW_MODULE
    assert sha256(backup / str(MODULE).replace("/", "_")) == hashlib.sha256(LIVE_MODULE).hexdigest()

    # the keyset was never part of the inventory, in either snapshot
    for snapshot in ("inventory_before", "inventory_after"):
        assert not [p for p in receipt[snapshot] if p.endswith(".keys")], snapshot
    assert (sd / "switch" / "prod.keys").read_bytes() == KEYS_BEFORE

    # unrelated files untouched
    assert (sd / "switch" / "titles" / "some.nca").read_bytes() == b"nca"
    assert (sd / "atmosphere" / "exefs_patches" / "layer" / "ips" / "a.ips").read_bytes() == b"IPS3patch"
    assert "swap_ab=true" in (sd / INI.parent / "configs" / "18d1_9400.ini").read_text()

    # the owner's settings and comments survived; only [ble] keys moved
    text = ini_text(sd)
    assert "analog_trigger_activation_threshold=27" in text
    assert "experimental_ble_connection_probe=true" in text   # unknown to this build, harmless
    assert 'host_name="MC_bench"' in text
    assert "auto_connect=true" in text                        # deliberately not one of ours
    assert "enable=true" in text and "log=true" in text and "radio_cycle=true" in text
    assert "; Mission Control" in text


def test_duplicate_keys_cannot_undo_the_setting(tmp):
    sd = make_sd(tmp / "duplicates")
    (sd / INI).write_text("[ble]\nlog=true\nlog=false\nradio_cycle=false\n\n[misc]\nenable_motion=true\n")
    run(sd, "--ble", "on", "--backup-dir", str(tmp / "b1"), "--apply")
    lines = [l.strip() for l in ini_text(sd).splitlines() if l.strip() and not l.strip().startswith(";")]
    assert lines.count("log=true") == 1, lines
    assert "log=false" not in lines, lines
    assert "enable_motion=true" in lines
    # the [ble] block was written where the block is, not dumped at the end of the file
    assert ini_text(sd).index("enable=true") < ini_text(sd).index("enable_motion=true")


def test_ble_off_and_missing_ini(tmp):
    sd = make_sd(tmp / "off")
    run(sd, "--ble", "off", "--backup-dir", str(tmp / "b2"), "--apply")
    text = ini_text(sd)
    assert "enable=false" in text and "log=false" in text and "radio_cycle=false" in text

    bare = tmp / "bare"
    (bare / MODULE.parent).mkdir(parents=True)
    (bare / MODULE).write_bytes(LIVE_MODULE)
    run(bare, "--ble", "on", "--backup-dir", str(tmp / "b3"), "--apply")
    assert "[ble]" in ini_text(bare) and "enable=true" in ini_text(bare)


def test_restore_is_byte_identical(tmp):
    sd = make_sd(tmp / "restore")
    backup = tmp / "b4"
    run(sd, "--module", str(write_build(tmp / "new2.nsp")), "--ble", "on", "--backup-dir", str(backup), "--apply")
    assert (sd / MODULE).read_bytes() == NEW_MODULE

    run(sd, "--backup-dir", str(backup), "--restore")            # dry run writes nothing
    assert (sd / MODULE).read_bytes() == NEW_MODULE
    run(sd, "--backup-dir", str(backup), "--restore", "--apply")
    assert (sd / MODULE).read_bytes() == LIVE_MODULE
    assert ini_text(sd) == INI_TEXT


def load_module():
    import importlib.util
    spec = importlib.util.spec_from_file_location("ble_deploy", str(TOOL))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FailingCard:
    """Wraps a card and fails the module write part way through, like a transfer that dies."""

    def __init__(self, inner):
        self.inner = inner

    def __getattr__(self, name):
        return getattr(self.inner, name)

    def describe(self):
        return self.inner.describe()

    def write(self, rel, data):
        if rel == str(MODULE):
            self.inner.write(rel, data[: len(data) // 3])      # truncated, as a half-finished STOR leaves it
            raise OSError("connection reset during STOR")
        self.inner.write(rel, data)


class FakeFtp:
    """Enough ftplib for FtpCard, with the quirks the Switch server actually has: NLST answers with full
    paths and only after a CWD, SIZE refuses directories and key files, MDTM is useless."""

    def __init__(self, root):
        self.root = root          # {posix path: bytes}
        self.cwd = "/"
        self.stored = []

    def sendcmd(self, command):
        if command.startswith("TYPE"):
            return "200 Type set to I"
        if command.startswith("CWD "):
            target = command[4:].rstrip("/") or "/"
            # the real server refuses CWD on a file, which is how the tool tells files from directories
            if target != "/" and not any(p.startswith(target + "/") for p in self.root):
                raise Exception("550 CWD failed")
            self.cwd = target
            return "250 Directory changed."
        raise Exception("500 unimplemented")

    def _child_names(self):
        prefix = self.cwd.rstrip("/") + "/"
        names, dirs = set(), set()
        for path in self.root:
            if not path.startswith(prefix):
                continue
            rest = path[len(prefix):]
            if "/" in rest:
                dirs.add(prefix + rest.split("/")[0])
            else:
                names.add(path)
        return sorted(names | dirs)

    def nlst(self, argument=""):
        if argument:                      # the server answers nothing when given a path
            return []
        return self._child_names()

    def size(self, path):
        if path in self.root:
            base = path.rsplit("/", 1)[-1]
            if base.endswith(".keys"):
                raise Exception("553 File name not allowed")
            return len(self.root[path])
        raise Exception("550 not a file")

    def retrbinary(self, command, callback, blocksize):
        path = command.split(" ", 1)[1]
        callback(self.root[path])

    def storbinary(self, command, handle, blocksize):
        path = command.split(" ", 1)[1]
        self.root[path] = handle.read()
        self.stored.append(path)

    def quit(self):
        pass

    def close(self):
        pass


def test_a_failed_write_leaves_a_way_back(tmp):
    module = load_module()
    sd = make_sd(tmp / "halfwrite")
    card = FailingCard(module.LocalCard(sd))
    build = write_build(tmp / "new.nsp")
    backup = tmp / "b-half"
    args = module.argparse.Namespace(module=str(build), ble="on", backup_dir=str(backup), restore=False,
                                     expect_module=None, apply=True, force_card=False)
    try:
        module.run(card, args)
        raise AssertionError("the failing write should have stopped the tool")
    except OSError:
        pass

    receipt = json.loads((backup / "receipt.json").read_text())
    assert receipt["replaced"][str(MODULE)]["sha256"] == hashlib.sha256(LIVE_MODULE).hexdigest()
    assert receipt["state"] == "backed up, nothing written yet", receipt["state"]
    assert (backup / str(MODULE).replace("/", "_")).read_bytes() == LIVE_MODULE
    assert (sd / MODULE).read_bytes() != LIVE_MODULE          # the card really is damaged at this point

    module.run(module.LocalCard(sd), module.argparse.Namespace(module=None, ble=None, backup_dir=str(backup),
                                                              restore=True, expect_module=None, apply=True,
                                                              force_card=False))
    assert (sd / MODULE).read_bytes() == LIVE_MODULE


def test_restore_is_bound_to_the_card_it_came_from(tmp):
    module = load_module()
    source = make_sd(tmp / "card-a")
    other = make_sd(tmp / "card-b")
    backup = tmp / "b-card"
    module.run(module.LocalCard(source), module.argparse.Namespace(module=str(write_build(tmp / "n.nsp")),
                                                                  ble="on", backup_dir=str(backup), restore=False,
                                                                  expect_module=None, apply=True, force_card=False))
    for force in (False,):
        try:
            module.run(module.LocalCard(other), module.argparse.Namespace(module=None, ble=None,
                                                                         backup_dir=str(backup), restore=True,
                                                                         expect_module=None, apply=True, force_card=force))
            raise AssertionError("restoring card A's receipt onto card B must be refused")
        except SystemExit as exc:
            assert "another card" in str(exc) or str(exc).startswith("this receipt was taken from"), str(exc)
    assert (other / MODULE).read_bytes() == LIVE_MODULE


def test_ftp_transport_matches_the_real_server(tmp):
    module = load_module()
    tree = {
        "/atmosphere/contents/010000000000bd00/exefs.nsp": LIVE_MODULE,
        "/atmosphere/contents/010000000000bd00/._exefs.nsp": b"appledouble",
        "/atmosphere/contents/010000000000bd00/mitm.lst": b"fs\n",
        "/config/MissionControl/missioncontrol.ini": INI_TEXT.encode(),
        "/switch/prod.keys": KEYS_BEFORE,
        "/switch/hbmenu.nro": b"nro",
    }
    ftp = FakeFtp(tree)
    card = module.FtpCard("switch.test", 5000, ftp=ftp)

    found, unreadable = module.inventory(card)
    assert str(MODULE) in found and str(INI) in found, sorted(found)
    assert "switch/hbmenu.nro" in found
    assert not [p for p in found if p.endswith(".keys") or "/._" in p], sorted(found)

    keys_before = tree["/switch/prod.keys"]
    build = write_build(tmp / "ftp.nsp")
    backup = tmp / "b-ftp"
    module.run(card, module.argparse.Namespace(module=str(build), ble="on", backup_dir=str(backup), restore=False,
                                               expect_module=hashlib.sha256(LIVE_MODULE).hexdigest(), apply=True,
                                               force_card=False))
    assert tree["/atmosphere/contents/010000000000bd00/exefs.nsp"] == NEW_MODULE
    assert b"enable=true" in tree["/config/MissionControl/missioncontrol.ini"]
    assert tree["/switch/prod.keys"] == keys_before          # never read, never written
    receipt = json.loads((backup / "receipt.json").read_text())
    assert receipt["changed"] == sorted([str(MODULE), str(INI)]), receipt["changed"]
    assert (backup / str(MODULE).replace("/", "_")).read_bytes() == LIVE_MODULE


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        with tempfile.TemporaryDirectory() as tmpdir:
            test(Path(tmpdir))
        print(f"  ok  {test.__name__}")
    print(f"BLE SD transaction: {len(tests)} tests passed (module swap, receipt, refusal, restore, keys never read)")


if __name__ == "__main__":
    sys.exit(main())
