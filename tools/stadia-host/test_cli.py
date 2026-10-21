#!/usr/bin/env python3
"""Offline CLI/privacy/source-contract checks. Never invokes valid --hid."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

binary, reports, descriptor = sys.argv[1:]


def run(*args):
    return subprocess.run([binary, *map(str, args)], capture_output=True, text=True, timeout=15)


assert run('--help').returncode == 0
assert run().returncode == 2
for args in [('--hid', '--seconds', x) for x in ('0', '181', '-1', 'nan', '1x', '4294967296')]:
    assert run(*args).returncode == 2
assert run('--unknown').returncode == 2
result = run('--replay', reports, '--map', descriptor)
assert result.returncode == 0, result.stderr
lines = [json.loads(line) for line in result.stdout.splitlines()]
assert len(lines) == 31
neutral = {'dpad': 8, 'buttons': 0, 'left_x': 2047, 'left_y': 2047,
           'right_x': 2047, 'right_y': 2047, 'left_trigger': 0,
           'right_trigger': 0, 'consumer_buttons': 0}
assert lines[-1] == {'event': 'replay_complete', 'reports': 30,
                     'modelled_gatt_matches': 30, 'live_gatt_verified': False,
                     'state': neutral}
assert [line['row'] for line in lines[:-1]] == list(range(1, 31))
assert all(line['source'] == 'fixture_hid' for line in lines[:-1])
assert not re.search(r'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}', result.stdout)
assert not any(word in result.stdout for word in ('"hex"', 'address', 'serial', 'identifier', 'report_descriptor_hex'))
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    bad = directory/'bad.txt'
    for value in ('', 'broken row\n', 'unsafe\"name 00\n', 'x 0300\n', 'x gg\n', 'a'*300+' 00\n'):
        bad.write_text(value)
        failure = run('--replay', bad, '--map', descriptor)
        assert failure.returncode == 1
        assert 'unsafe"name' not in failure.stderr
    rows = [row.split() for row in Path(reports).read_text().splitlines() if row and not row.startswith('#')]
    held = next(value for _, value in rows if int(value[4:8], 16) != 0)
    # User-supplied labels can contain identifiers or packet hex: never emit them.
    private_label = 'user_secret_serial_abc123'
    bad.write_text(private_label + ' ' + held + '\n')
    single = run('--replay', bad, '--map', descriptor)
    assert single.returncode == 0
    assert private_label not in single.stdout and held not in single.stdout
    single_rows = [json.loads(line) for line in single.stdout.splitlines()]
    assert single_rows[0]['state']['buttons'] != 0 and single_rows[-1]['state'] == neutral
    bad.write_text(private_label + ' ' + held + '\nmalformed\n')
    aborted = run('--replay', bad, '--map', descriptor)
    assert aborted.returncode == 1 and private_label not in aborted.stdout
    assert json.loads(aborted.stdout.splitlines()[-1]) == {'event': 'replay_aborted', 'state': neutral}
    bad.write_text('00')
    assert run('--replay', reports, '--map', bad).returncode == 1
    assert run('--replay', directory/'missing', '--map', descriptor).returncode == 1
source = (Path(__file__).parent/'source/native_hid.cpp').read_text()
assert 'kIOHIDOptionsTypeNone' in source and 'IOHIDManagerSetDeviceMatchingMultiple' in source
assert 'kHIDUsage_GD_GamePad' in source and '0x18d1' in source and '0x9400' in source
assert 'CFSTR("Bluetooth")' in source and 'CFSTR("Bluetooth Low Energy")' in source
assert not any(term in source for term in ('IOHIDDeviceSetReport', 'kIOHIDOptionsTypeSeizeDevice', 'kIOHIDSerialNumberKey', 'CBCentralManager', 'IOHIDRequestAccess'))
assert source.index('int RunNativeHid') < source.index('IOHIDManagerCreate(')
print('Host CLI: offline replay/argument/privacy and non-seizing scoped HID source checks passed (no device opened)')
