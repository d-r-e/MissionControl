#!/usr/bin/env python3
"""Prepare a local, ignored probe target from macOS's Stadia Bluetooth entry.

Does not access pairing databases or print addresses of any device.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess


ADDRESS = re.compile(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}")


def stadia_addresses(value):
    found = set()
    if isinstance(value, dict):
        for name, entry in value.items():
            if name.lower().startswith("stadia") and isinstance(entry, dict):
                address = entry.get("device_address", "")
                if isinstance(address, str) and ADDRESS.fullmatch(address):
                    normalized = address.lower()
                    if normalized not in {"00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff"}:
                        found.add(normalized)
            found.update(stadia_addresses(entry))
    elif isinstance(value, list):
        for entry in value:
            found.update(stadia_addresses(entry))
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parent /
                        "package/config/MissionControl/stadia-probe-address.txt")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        fixture = {"devices": [{"Stadia ABC": {"device_address": "01:23:45:67:89:Ab"}},
                               {"Stadia duplicate": {"device_address": "01:23:45:67:89:ab"}},
                               {"Keyboard": {"device_address": "98:76:54:32:10:00"}},
                               {"Stadia invalid": {"device_address": "not-an-address"}},
                               {"Stadia null": {"device_address": None}},
                               {"Stadia broadcast": {"device_address": "ff:ff:ff:ff:ff:ff"}}]}
        assert stadia_addresses(fixture) == {"01:23:45:67:89:ab"}
        assert not stadia_addresses({"Stadia empty": {}})
        print("Target extraction tests passed (no hardware accessed).")
        return
    try:
        result = subprocess.run(["system_profiler", "SPBluetoothDataType", "-json"],
                                check=True, capture_output=True, text=True, timeout=30)
        addresses = stadia_addresses(json.loads(result.stdout))
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        parser.exit(1, f"Cannot obtain macOS Bluetooth report: {type(error).__name__}\n")
    if len(addresses) != 1:
        parser.exit(1, f"Found {len(addresses)} distinct Stadia addresses; refusing to guess a target.\n")
    contents = addresses.pop() + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text() != contents:
        parser.exit(1, "Target file already contains another address; refusing to overwrite it.\n")
    args.output.write_text(contents)
    print(f"Wrote {args.output} (address intentionally omitted).")
    print("Keep this local configuration private; copy it only to your own Switch.")


if __name__ == "__main__":
    main()
