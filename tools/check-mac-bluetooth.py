#!/usr/bin/env python3
"""Read-only, fail-closed Mac radio preflight; never print device identities."""
import argparse
import json
import subprocess


def controller_states(value):
    states = []
    if isinstance(value, dict):
        for key, child in value.items():
            if key == "controller_state" and isinstance(child, str):
                states.append(child)
            elif isinstance(child, (dict, list)):
                states.extend(controller_states(child))
    elif isinstance(value, list):
        for child in value:
            states.extend(controller_states(child))
    return states


def matches(states, expected):
    return bool(states) and all(state == "attrib_" + expected for state in states)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--expect", choices=("on", "off"))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        assert matches(["attrib_off"], "off")
        assert matches(["attrib_on"], "on")
        assert not matches(["attrib_on"], "off")
        assert not matches([], "off")
        assert not matches(["attrib_off", "attrib_on"], "off")
        assert not matches(["unknown"], "off")
        assert controller_states({"x": [{"controller_state": "attrib_off"}]}) == ["attrib_off"]
        print("Mac Bluetooth preflight classifier tests passed (no hardware accessed).")
        return 0
    if not args.expect:
        parser.error("--expect is required unless --self-test is selected")
    try:
        result = subprocess.run(["system_profiler", "-json", "SPBluetoothDataType"],
                                check=True, capture_output=True, text=True, timeout=20)
        states = controller_states(json.loads(result.stdout))
    except (OSError, subprocess.SubprocessError, ValueError):
        print("Mac Bluetooth state unavailable; launch blocked.")
        return 1
    if not matches(states, args.expect):
        print(f"Mac Bluetooth is not confirmed {args.expect.upper()}; launch blocked.")
        return 1
    print(f"Mac Bluetooth confirmed {args.expect.upper()}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
