#!/usr/bin/env python3
"""Offline only: never invoke --connected-catalog or create a Bluetooth manager."""
from pathlib import Path
import json
import re
import subprocess
import sys
import tempfile

binary, descriptor = sys.argv[1:]


def run(*args):
    return subprocess.run([binary, *map(str, args)], capture_output=True, text=True, timeout=20)


assert run('--help').returncode == 0
assert run().returncode == 2
assert run('--unknown').returncode == 2
result = run('--self-test', descriptor)
assert result.returncode == 0, result.stderr
assert json.loads(result.stdout) == {'event': 'self_test', 'ok': True, 'bluetooth_manager_initialized': False}
assert 'central_state' not in result.stdout
assert 'do_not_echo' not in result.stdout + result.stderr
with tempfile.TemporaryDirectory() as directory:
    bad = Path(directory)/'bad.hex'
    for content in ('', '00', 'xx', '0', 'a'*20000):
        bad.write_text(content)
        assert run('--self-test', bad).returncode == 1
    assert run('--self-test', Path(directory)/'missing').returncode == 1
source = (Path(__file__).parent/'gatt-capability.swift').read_text()
assert source.index('guard args == ["--connected-catalog"]') < source.index('let probe = Capability()')
assert source.index('if args.count == 2 && args[0] == "--self-test"') < source.index('let probe = Capability()')
assert source.count('central.connect(') == 1 and source.count('central.cancelPeripheralConnection(') == 1
calls = set(re.findall(r'\b(?:central|peripheral|peer|selected)\.(\w+)\(', source))
assert calls == {'retrieveConnectedPeripherals', 'connect', 'cancelPeripheralConnection',
                 'discoverServices', 'discoverCharacteristics', 'discoverDescriptors', 'readValue'}, calls
assert not any(term in source for term in ('.identifier', '.uuidString', '.localizedDescription', '.userInfo', '.writeValue(', '.setNotifyValue(', '.scanForPeripherals('))
assert 'ownership.cancelOnce()' in source and 'ownership.terminal()' in source
assert 'p === peer && ownership.owns' in source
assert 'characteristic === map' in source and 'descriptor === pendingReference' in source
assert 'characteristic === reports[index]' in source
assert 'seenReferences.insert(packed).inserted' in source
assert 'mc_hogp_reference_size' in source
assert 'now - startedAt >= 45' in source and 'systemUptime + 8' in source
assert 'cleanup_scope' in source and 'this_app_connection_reference' in source
assert 'input_notifications_verified": false' in source
print('GATT offline: ownership/bridge self-tests, malformed fixtures, privacy and single-owned-call source contracts passed (no Bluetooth manager initialized)')
