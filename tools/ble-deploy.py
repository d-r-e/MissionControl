#!/usr/bin/env python3
"""Swap the Mission Control sysmodule for a Bluetooth LE test, over a mount or over FTP.

Everything about this script exists so that one file changes and nothing else does:

- the module (`atmosphere/contents/010000000000bd00/exefs.nsp`) and the INI are copied aside
  with sizes and hashes before anything is written;
- `atmosphere/`, `switch/` and `config/MissionControl/` are inventoried before and after, and
  the transaction fails loudly if any other file differs;
- the INI is edited by key, so the owner's other settings survive;
- `--restore` puts back exactly what the receipt says was there.

The card is reached either as a mounted directory (`--sd`) or as the console's FTP server
(`--ftp host[:port]`). Switch FTP servers only answer `LIST`/`NLST` after a `CWD`, which is
why the FTP transport walks by changing directory rather than listing by path.

Keys are never read, hashed or printed: anything whose name starts with `.` or ends in
`.keys` is outside the inventory in every mode. INI values are never echoed either - a
`host_address` in `[bluetooth]` identifies a device.
"""

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import posixpath
import shutil
import sys
import time
from urllib.parse import unquote

MODULE = "atmosphere/contents/010000000000bd00/exefs.nsp"
INI = "config/MissionControl/missioncontrol.ini"
INVENTORY_ROOTS = ("atmosphere", "switch", "config/MissionControl")
BLE_KEYS = ("enable", "log", "radio_cycle")
BLOCK = 1 << 20


def is_off_limits(name):
    """Dotfiles (macOS writes `._*` next to everything) and keysets are never touched."""
    return name.startswith(".") or name.lower().endswith(".keys")


def digest(data):
    return hashlib.sha256(data).hexdigest()


class LocalCard:
    """A mounted SD card."""

    def __init__(self, root):
        self.root = Path(root)

    def describe(self):
        return f"sd {self.root}"

    def exists(self, rel):
        return (self.root / rel).is_file()

    def size(self, rel):
        return (self.root / rel).stat().st_size

    def read(self, rel):
        return (self.root / rel).read_bytes()

    def write(self, rel, data):
        target = self.root / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)

    def list(self, rel):
        directory = self.root / rel
        if not directory.is_dir():
            return []
        return sorted(p.name for p in directory.iterdir())

    def is_dir(self, rel):
        return (self.root / rel).is_dir()


class FtpCard:
    """The console's FTP server. Paths are absolute and slash-separated on the wire."""

    def __init__(self, host, port=5000, user="anonymous", password="mc@ble-deploy", timeout=60, ftp=None):
        self.host = host
        self.port = port
        if ftp is None:
            from ftplib import FTP     # imported here so local use needs no ftplib at all
            ftp = FTP()
            ftp.connect(host, port, timeout=timeout)
            ftp.login(user, password)
        self.ftp = ftp
        self.ftp.sendcmd("TYPE I")

    def describe(self):
        return f"ftp {self.host}:{self.port}"

    def _abs(self, rel):
        return "/" + rel.lstrip("/")

    def exists(self, rel):
        try:
            if self.ftp.size(self._abs(rel)) is not None:
                return True
        except Exception:
            pass
        # some servers answer SIZE only for some files; the parent listing is the fallback
        parent, _, name = rel.rpartition("/")
        try:
            return name in self.list(parent)
        except Exception:
            return False

    def size(self, rel):
        return self.ftp.size(self._abs(rel))

    def read(self, rel):
        buffer = io.BytesIO()
        self.ftp.retrbinary(f"RETR {self._abs(rel)}", buffer.write, BLOCK)
        return buffer.getvalue()

    def write(self, rel, data):
        # The card's FTP has no usable MKD on this server for deep trees, and both files this
        # tool writes already exist in practice; a missing directory shows up as a failed store.
        self.ftp.storbinary(f"STOR {self._abs(rel)}", io.BytesIO(data), BLOCK)

    def list(self, rel):
        self.ftp.sendcmd("CWD " + (self._abs(rel) if rel else "/"))
        # this server answers with full paths, which is not what callers of list() expect
        return sorted(posixpath.basename(unquote(name)) for name in self.ftp.nlst(""))

    def is_dir(self, rel):
        try:
            self.ftp.sendcmd("CWD " + self._abs(rel))
            return True
        except Exception:
            return False

    def close(self):
        try:
            self.ftp.quit()
        except Exception:
            self.ftp.close()


def inventory(card):
    """Every regular file that could plausibly be disturbed, keyed by card-relative path.

    An entry that can be neither read as a directory nor sized is recorded as unreadable
    rather than skipped, so a card that stops cooperating mid-transaction shows up in the
    receipt instead of looking unchanged.
    """
    found = {}
    unreadable = []

    def walk(rel):
        try:
            names = card.list(rel)
        except Exception as exc:
            unreadable.append({"path": rel, "why": f"list failed: {exc}"})
            return
        for name in names:
            if is_off_limits(name):
                continue
            child = posixpath.join(rel, name) if rel else name
            try:
                if card.is_dir(child):
                    walk(child)
                    continue
                found[child] = {"bytes": card.size(child), "sha256": digest(card.read(child))}
            except Exception as exc:
                unreadable.append({"path": child, "why": str(exc).splitlines()[0][:120]})

    for root in INVENTORY_ROOTS:
        walk(root)
    return found, unreadable


def read_ini_keys(card):
    """Which [ble] keys the INI sets, without printing any value."""
    if not card.exists(INI):
        return None
    section = None
    present = {}
    for line in card.read(INI).decode("utf-8", "replace").splitlines():
        text = line.strip()
        if text.startswith("[") and text.endswith("]"):
            section = text[1:-1].strip().lower()
        elif section == "ble" and "=" in text and not text.startswith((";", "#")):
            name, value = text.split("=", 1)
            present[name.strip().lower()] = value.strip().lower()
    return present


def set_ini_keys(card, values):
    """Set [ble] keys in place. Unknown keys in other sections are left alone: the module
    ignores what it does not know, and rewriting the file would lose the owner's comments."""
    lines = card.read(INI).decode("utf-8", "replace").splitlines() if card.exists(INI) else []

    remaining = dict(values)
    out = []
    section = None
    for line in lines:
        text = line.strip()
        if text.startswith("[") and text.endswith("]"):
            if section == "ble":
                out.extend(f"{name}={value}" for name, value in remaining.items())
                remaining.clear()
            section = text[1:-1].strip().lower()
        elif section == "ble" and "=" in text and not text.startswith((";", "#")):
            name = text.split("=", 1)[0].strip().lower()
            if name in remaining:
                out.append(f"{name}={remaining.pop(name)}")
                continue
            if name in values:  # a duplicate later in the section would win at parse time
                out.append(f"; {name} (superseded above by this tool)")
                continue
        out.append(line)

    if section == "ble":
        out.extend(f"{name}={value}" for name, value in remaining.items())
        remaining.clear()

    if remaining:
        if out and out[-1].strip():
            out.append("")
        sections = {l.strip()[1:-1].strip().lower() for l in out if l.strip().startswith("[")}
        if "ble" not in sections:
            out.append("[ble]")
        out.extend(f"{name}={value}" for name, value in remaining.items())

    card.write(INI, ("\n".join(out) + "\n").encode("utf-8"))


def stamp():
    return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())


def inspect(card):
    print(f"card               {card.describe()}")
    if card.exists(MODULE):
        print(f"module             {card.size(MODULE)} bytes sha256={digest(card.read(MODULE))}")
    else:
        print("module             missing")
    keys = read_ini_keys(card)
    if keys is None:
        print("ini                missing")
    else:
        shown = ", ".join(f"{k}={v}" for k, v in sorted(keys.items()) if k in BLE_KEYS) or "no [ble] keys set"
        print(f"ini                present, [ble]: {shown}")


def backup_path(backup_dir, rel):
    return Path(backup_dir) / rel.replace("/", "_")


def do_restore(card, backup_dir, apply, force_card=False):
    if not backup_dir:
        sys.exit("--backup-dir names the receipt to restore from")
    receipt = Path(backup_dir) / "receipt.json"
    if not receipt.is_file():
        sys.exit(f"no receipt at {receipt}")
    record = json.loads(receipt.read_text())
    if not force_card and record.get("card") and record["card"] != card.describe():
        sys.exit(f"this receipt was taken from {record['card']}, not {card.describe()}; "
                 "pass --force-card if that is really intended")
    for rel in (MODULE, INI):
        stored = backup_path(backup_dir, rel)
        if not stored.is_file():
            sys.exit(f"receipt names {rel} but {stored} is missing")
        if not apply:
            print(f"would restore {rel} ({stored.stat().st_size} bytes)")
            continue
        card.write(rel, stored.read_bytes())
        if digest(card.read(rel)) != record["replaced"][rel]["sha256"]:
            sys.exit(f"restore of {rel} did not match the receipt hash")
        print(f"restored {rel}")


def run(card, args):
    if args.restore:
        do_restore(card, args.backup_dir, args.apply, args.force_card)
        return 0

    inspect(card)
    if args.expect_module:
        if not card.exists(MODULE):
            sys.exit(f"{MODULE} is missing, refusing to guess where the module lives")
        actual = digest(card.read(MODULE))
        if actual != args.expect_module:
            sys.exit(f"live module is {actual}, expected {args.expect_module}; nothing written")

    if not args.apply:
        print("nothing written (pass --apply)")
        return 0

    if not args.module and not args.ble:
        sys.exit("nothing to do: pass --module and/or --ble")
    if args.module and not Path(args.module).is_file():
        sys.exit(f"no such module build: {args.module}")
    if not args.backup_dir:
        sys.exit("--backup-dir is required when writing")

    new_module = Path(args.module).read_bytes() if args.module else None
    backup = Path(args.backup_dir)
    backup.mkdir(parents=True, exist_ok=True)

    # Copy the originals aside and record them before the first write. A transfer that dies half way leaves a
    # damaged module on the card, and a receipt written afterwards is no way back from that.
    replaced = {}
    for rel in (MODULE, INI):
        if not card.exists(rel):
            continue
        data = card.read(rel)
        stored = backup_path(backup, rel)
        stored.write_bytes(data)
        if digest(stored.read_bytes()) != digest(data):
            sys.exit(f"the copy of {rel} in {backup} does not match the card; nothing written")
        replaced[rel] = {"bytes": len(data), "sha256": digest(data)}

    receipt = {
        "stamp": stamp(),
        "card": card.describe(),
        "replaced": replaced,
        "install": None,
        "state": "backed up, nothing written yet",
        "inventory_before": None,
        "unreadable_before": None,
    }
    (backup / "receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")

    before, unreadable_before = inventory(card)
    receipt["inventory_before"] = before
    receipt["unreadable_before"] = unreadable_before
    (backup / "receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")

    if new_module is not None:
        card.write(MODULE, new_module)
        if digest(card.read(MODULE)) != digest(new_module):
            sys.exit(f"the module did not read back identical after writing; restore it with\n"
                     f"  python3 tools/ble-deploy.py <--sd|--ftp ...> --backup-dir {backup} --restore --apply")

    receipt["state"] = "module written"
    (backup / "receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")

    if args.ble:
        want = "true" if args.ble == "on" else "false"
        set_ini_keys(card, {key: want for key in BLE_KEYS})

    after, unreadable_after = inventory(card)
    receipt["install"] = {
        "module": {"bytes": len(new_module), "sha256": digest(new_module)} if new_module is not None else None,
        "ble": args.ble,
    }
    receipt["inventory_after"] = after
    receipt["unreadable_after"] = unreadable_after

    changed = {p for p in set(before) | set(after) if before.get(p) != after.get(p)}
    unexpected = sorted(changed - {MODULE, INI})
    missing = sorted(set(before) - set(after) - {MODULE, INI})
    receipt["changed"] = sorted(changed)
    receipt["state"] = "complete"
    (backup / "receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")

    if unexpected or missing:
        sys.exit(f"UNEXPECTED CHANGES (restore with --restore): changed={unexpected} missing={missing}")

    print(f"changed exactly: {sorted(changed) or 'nothing'}")
    print(f"receipt        : {backup / 'receipt.json'}")
    inspect(card)
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    card_group = parser.add_mutually_exclusive_group(required=True)
    card_group.add_argument("--sd", help="mounted SD card root")
    card_group.add_argument("--ftp", help="console FTP as host[:port], e.g. 192.168.1.146:5000")
    parser.add_argument("--ftp-user", default="anonymous")
    parser.add_argument("--ftp-pass", default="mc@ble-deploy")
    parser.add_argument("--module", help="path to the mc_mitm.nsp to install")
    parser.add_argument("--ble", choices=("on", "off"), help="set [ble] enable/log/radio_cycle")
    parser.add_argument("--backup-dir", help="where to keep what was replaced")
    parser.add_argument("--restore", action="store_true", help="put back the files named in --backup-dir/receipt.json")
    parser.add_argument("--expect-module", help="refuse to touch the card unless the live module has this sha256")
    parser.add_argument("--apply", action="store_true", help="actually write; otherwise report only")
    parser.add_argument("--force-card", action="store_true", help="let --restore use a receipt from another card")
    args = parser.parse_args()

    if args.sd:
        root = Path(args.sd).resolve()
        if not root.is_dir():
            sys.exit(f"not a directory: {root}")
        card = LocalCard(root)
    else:
        host, _, port = args.ftp.partition(":")
        card = FtpCard(host, int(port or 5000), args.ftp_user, args.ftp_pass)

    try:
        return run(card, args)
    finally:
        if hasattr(card, "close"):
            card.close()


if __name__ == "__main__":
    sys.exit(main())
