#!/usr/bin/env python3
"""Test launch abort semantics using fake USB/radio tools; no hardware/network."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
with tempfile.TemporaryDirectory(prefix="stadia-launch-tests-") as name:
    temp = Path(name)
    tools = temp / "project/tools"
    tools.mkdir(parents=True)
    for filename in ("netload-stadia.sh", "check-mac-bluetooth.py"):
        shutil.copyfile(root / "tools" / filename, tools / filename)
    nro = tools / "stadia-probe/package/switch/stadia-ble-probe.nro"
    nro.parent.mkdir(parents=True)
    nro.write_bytes(b"")  # Only an existence fixture; never executed.
    bin_dir = temp / "bin"
    bin_dir.mkdir()
    docker_marker = temp / "docker-called"
    profiler_marker = temp / "profiler-called"
    profiler = bin_dir / "system_profiler"
    profiler.write_text('#!/bin/sh\nprintf x > "$TEST_PROFILER_MARKER"\nprintf "%s\\n" "$TEST_BLUETOOTH_JSON"\nexit "${TEST_PROFILER_RC:-0}"\n')
    docker = bin_dir / "docker"
    docker.write_text('#!/bin/sh\nprintf x > "$TEST_DOCKER_MARKER"\nprintf "fake-container-id\\n"\n')
    profiler.chmod(0o755)
    docker.chmod(0o755)
    cases = [
        ("--patched-incomplete-be", ["attrib_on"], False),
        ("--patched-incomplete-be", ["attrib_off"], False),
        ("--patched-be", [], False),
        ("--patched-be", ["unknown"], False),
        ("--patched-be", ["attrib_off", "attrib_on"], False),
        ("--control", ["attrib_off"], False),
        ("--control", ["attrib_on"], False),
        ("--control-hid16-lists", ["attrib_off"], False),
        ("--control-hid16-lists", ["attrib_on"], False),
        ("--control-hid16-lists", ["unknown"], False),
        ("--connect", ["attrib_off"], False),
        ("--connect", ["attrib_on"], False),
        ("--connect-control", ["attrib_off"], False),
        ("--connect-control", ["attrib_on"], False),
        ("--cancel-control", ["attrib_off"], False),
        ("--cancel-control", ["attrib_on"], False),
        ("--cancel-control", ["unknown"], False),
        ("--inspect-control", ["attrib_off"], False),
        ("--inspect-control", ["attrib_on"], False),
        ("--inspect-control", ["unknown"], False),
        ("--inspect-hid-control", ["attrib_off"], False),
        ("--inspect-hid-control", ["attrib_on"], False),
        ("--inspect-hid-control", ["unknown"], False),
        ("--status-only", [], True),
        ("--managed-stadia", ["attrib_off"], True),
        ("--managed-stadia", ["attrib_on"], False),
        ("--managed-stadia", ["unknown"], False),
        ("--trace-managed-stadia", ["attrib_off"], True),
        ("--trace-managed-stadia", ["attrib_on"], False),
        ("--trace-managed-stadia", ["unknown"], False),
        ("--trace-managed-control", ["attrib_off"], True),
        ("--trace-managed-control", ["attrib_on"], False),
        ("--trace-managed-control", ["unknown"], False),
        ("--trace-managed-control-canonical", ["attrib_off"], True),
        ("--trace-managed-control-canonical", ["attrib_on"], False),
        ("--trace-managed-control-canonical", ["unknown"], False),
        ("--discovery-requests", ["attrib_off"], True),
        ("--discovery-requests", ["attrib_on"], False),
        ("--discovery-requests", ["unknown"], False),
        ("--discover-stadia", ["attrib_off"], True),
        ("--discover-stadia", ["attrib_on"], False),
        ("--discover-stadia", ["unknown"], False),
        ("--event-progress", [], True),
        ("--event-progress", ["attrib_on"], True),
        ("--event-progress", ["unknown"], True),
        ("--service-latency", [], True),
        ("--service-latency", ["attrib_off"], True),
        ("--service-latency", ["attrib_on"], True),
        ("--service-latency", ["unknown"], True),
    ]
    for mode, states, expected_launch in cases:
        for marker in (docker_marker, profiler_marker):
            marker.unlink(missing_ok=True)
        env = os.environ.copy()
        env.update(PATH=str(bin_dir) + os.pathsep + env["PATH"],
                   TEST_DOCKER_MARKER=str(docker_marker),
                   TEST_PROFILER_MARKER=str(profiler_marker),
                   TEST_BLUETOOTH_JSON=json.dumps({"controllers": [{"controller_state": x} for x in states]}))
        result = subprocess.run(["bash", str(tools / "netload-stadia.sh"), "192.0.2.1", mode],
                                capture_output=True, text=True, env=env, timeout=10)
        assert docker_marker.exists() == expected_launch, (mode, states, result.stdout, result.stderr)
        assert (result.returncode == 0) == expected_launch
        paused = mode not in {"--status-only", "--service-latency", "--event-progress", "--managed-stadia", "--discover-stadia", "--trace-managed-stadia", "--trace-managed-control", "--trace-managed-control-canonical", "--discovery-requests"}
        assert profiler_marker.exists() == (mode in {"--managed-stadia", "--discover-stadia", "--trace-managed-stadia", "--trace-managed-control", "--trace-managed-control-canonical", "--discovery-requests"})
        if paused:
            assert "PAUSED" in result.stderr
            assert not docker_marker.exists() and not profiler_marker.exists()
    for invalid_json, profiler_rc in (("not-json", "0"), ("{}", "7")):
        docker_marker.unlink(missing_ok=True)
        env.update(TEST_BLUETOOTH_JSON=invalid_json, TEST_PROFILER_RC=profiler_rc)
        result = subprocess.run(["bash", str(tools / "netload-stadia.sh"), "192.0.2.1", "--patched-be"],
                                capture_output=True, text=True, env=env, timeout=10)
        assert result.returncode != 0 and not docker_marker.exists()
print("Guarded launcher abort/pass/status-only tests passed (fake tools; no hardware accessed).")
