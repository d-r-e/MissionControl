#!/usr/bin/env python3
"""Swap the Mission Control sysmodule on a mounted SD card for a Bluetooth LE test.

Everything about this script exists so that one file changes and nothing else does:

- the module (`atmosphere/contents/010000000000bd00/exefs.nsp`) and the INI are
  copied aside with sizes and hashes before anything is written;
- the whole of `atmosphere/` and `switch/` is inventoried before and after, and the
  transaction fails loudly if a file other than the module and the INI differs;
- the INI is edited by key, so the owner's other settings survive;
- `--restore` puts back exactly what the receipt says was there.

Keys are never read, hashed or printed: anything under `switch/` whose name ends in
`.keys` is skipped by the inventory, and INI values are never echoed (a Bluetooth
address in `[bluetooth]` is a device identifier).
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time

MODULE = Path("atmosphere/contents/010000000000bd00/exefs.nsp")
INI = Path("config/MissionControl/missioncontrol.ini")
BLE_KEYS = ("enable", "log", "radio_cycle")

# Never read these. A keyset is not this script's business, in any mode.
SKIP_SUFFIXES = (".keys",)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inventory(sd):
    """Every regular file we could plausibly disturb, keyed by SD-relative path."""
    found = {}
    for root_name in ("atmosphere", "switch", "config/MissionControl"):
        root = sd / root_name
        if not root.is_dir():
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if not d.startswith(".")]
            for name in sorted(filenames):
                if name.startswith(".") or name.lower().endswith(SKIP_SUFFIXES):
                    continue
                path = Path(dirpath) / name
                if not path.is_file():
                    continue
                found[str(path.relative_to(sd))] = {
                    "bytes": path.stat().st_size,
                    "sha256": sha256(path),
                }
    return found


def read_ini_keys(sd):
    """Which [ble] keys the INI sets, without printing any value."""
    ini = sd / INI
    if not ini.is_file():
        return None
    section = None
    present = {}
    for line in ini.read_text(errors="replace").splitlines():
        text = line.strip()
        if text.startswith("[") and text.endswith("]"):
            section = text[1:-1].strip().lower()
        elif section == "ble" and "=" in text and not text.startswith((";", "#")):
            name, value = text.split("=", 1)
            present[name.strip().lower()] = value.strip().lower()
    return present


def set_ini_keys(sd, values):
    """Set [ble] keys in place. Unknown keys in other sections are left alone: the
    module ignores what it does not know, and rewriting the file would lose comments."""
    ini = sd / INI
    lines = ini.read_text(errors="replace").splitlines() if ini.is_file() else []

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
        if "ble" not in {l.strip()[1:-1].strip().lower() for l in out if l.strip().startswith("[")}:
            out.append("[ble]")
        out.extend(f"{name}={value}" for name, value in remaining.items())

    ini.parent.mkdir(parents=True, exist_ok=True)
    ini.write_text("\n".join(out) + "\n")


def stamp():
    return time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())


def inspect(sd):
    module = sd / MODULE
    print(f"sd root            {sd}")
    print(f"module             {'missing' if not module.is_file() else f'{module.stat().st_size} bytes sha256={sha256(module)}'}")
    keys = read_ini_keys(sd)
    if keys is None:
        print("ini                missing")
    else:
        shown = ", ".join(f"{k}={v}" for k, v in sorted(keys.items()) if k in BLE_KEYS) or "no [ble] keys set"
        print(f"ini                present, [ble]: {shown}")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sd", type=Path, required=True, help="mounted SD card root")
    parser.add_argument("--module", type=Path, help="mc_mitm.nsp to install")
    parser.add_argument("--ble", choices=("on", "off"), help="set [ble] enable/log/radio_cycle")
    parser.add_argument("--backup-dir", type=Path, help="where to keep what was replaced")
    parser.add_argument("--restore", action="store_true", help="put back the files named in --backup-dir/receipt.json")
    parser.add_argument("--expect-module", help="refuse to touch the card unless the live module has this sha256")
    parser.add_argument("--apply", action="store_true", help="actually write; otherwise report only")
    args = parser.parse_args()

    sd = args.sd.resolve()
    if not sd.is_dir():
        sys.exit(f"not a directory: {sd}")

    if args.restore:
        receipt = args.backup_dir / "receipt.json"
        if not receipt.is_file():
            sys.exit(f"no receipt at {receipt}")
        record = json.loads(receipt.read_text())
        for rel in (str(MODULE), str(INI)):
            stored = args.backup_dir / rel.replace("/", "_")
            if not stored.is_file():
                sys.exit(f"receipt names {rel} but {stored} is missing")
            target = sd / rel
            if not args.apply:
                print(f"would restore {rel} ({stored.stat().st_size} bytes)")
                continue
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(stored, target)
            if sha256(target) != record["replaced"][rel]["sha256"]:
                sys.exit(f"restore of {rel} did not match the receipt hash")
            print(f"restored {rel}")
        return 0

    inspect(sd)
    live = sd / MODULE
    if args.expect_module:
        if not live.is_file():
            sys.exit(f"{MODULE} is missing, refusing to guess where the module lives")
        actual = sha256(live)
        if actual != args.expect_module:
            sys.exit(f"live module is {actual}, expected {args.expect_module}; nothing written")

    if not args.apply:
        print("nothing written (pass --apply)")
        return 0

    if not args.module and not args.ble:
        sys.exit("nothing to do: pass --module and/or --ble")
    if args.module and not args.module.is_file():
        sys.exit(f"no such module build: {args.module}")
    if not args.backup_dir:
        sys.exit("--backup-dir is required when writing")

    before = inventory(sd)
    args.backup_dir.mkdir(parents=True, exist_ok=True)

    replaced = {}
    for rel in (str(MODULE), str(INI)):
        path = sd / rel
        if not path.is_file():
            continue
        stored = args.backup_dir / rel.replace("/", "_")
        shutil.copyfile(path, stored)
        replaced[rel] = {"bytes": path.stat().st_size, "sha256": sha256(stored)}

    receipt = {
        "stamp": stamp(),
        "sd": str(sd),
        "replaced": replaced,
        "install": None,
        "inventory_before": before,
    }

    if args.module:
        target = sd / MODULE
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(args.module, target)
        if sha256(target) != sha256(args.module):
            sys.exit("module did not read back identical after writing; restore with --restore")

    if args.ble:
        want = "true" if args.ble == "on" else "false"
        set_ini_keys(sd, {key: want for key in BLE_KEYS})

    after = inventory(sd)
    receipt["install"] = {
        "module": {"path": str(args.module), "bytes": args.module.stat().st_size, "sha256": sha256(args.module)} if args.module else None,
        "ble": args.ble,
    }
    receipt["inventory_after"] = after

    changed = {p for p in set(before) | set(after) if before.get(p) != after.get(p)}
    allowed = {str(MODULE), str(INI)}
    unexpected = sorted(changed - allowed)
    missing = sorted(set(before) - set(after) - allowed)
    receipt["changed"] = sorted(changed)
    args.backup_dir.joinpath("receipt.json").write_text(json.dumps(receipt, indent=1) + "\n")

    if unexpected or missing:
        sys.exit(f"UNEXPECTED CHANGES (restore with --restore): changed={unexpected} missing={missing}")

    print(f"changed exactly: {sorted(changed) or 'nothing'}")
    print(f"receipt        : {args.backup_dir / 'receipt.json'}")
    inspect(sd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
