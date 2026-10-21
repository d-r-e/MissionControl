#!/usr/bin/env python3
"""Broker unit tests; no Bluetooth, sockets, Docker, or hardware access."""
import importlib.util
import io
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
spec = importlib.util.spec_from_file_location('console', Path(__file__).with_name('stadia-console.py'))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for command in module.COMMANDS:
    assert module.decode_command(command.encode()+b'\n') == command
for bad in (b'', b'sh\n', b'status \n', b'status\nquit\n', b'status\x00\n', b'\xff\n', b'x'*200+b'\n'):
    try:
        module.decode_command(bad)
    except (ValueError, UnicodeError):
        pass
    else:
        raise AssertionError('Invalid frame accepted')
for command in module.COMMANDS:
    with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess([], 0)) as run:
        module.check_radio(command)
        if command in ('status', 'cancel', 'quit'):
            run.assert_not_called()
        else:
            assert run.call_args.args[0][-1] == ('off' if command.startswith('real-') else 'on')
with patch.object(module.subprocess, 'run', return_value=subprocess.CompletedProcess([], 1)):
    try:
        module.check_radio('real-complete')
    except ValueError:
        pass
    else:
        raise AssertionError('Failed preflight accepted')
sink = io.StringIO(); log = module.SafeLog(sink)
log.feed(b'REMOTE '); assert sink.getvalue() == ''
log.feed(b'READY\n'); assert sink.getvalue() == 'REMOTE READY\n'
for chunks in ((b'01:23:', b'45:67:89:ab\n'), (b'x'*512,), (b'\xff\n',)):
    private_sink = io.StringIO(); guarded = module.SafeLog(private_sink)
    try:
        for chunk in chunks:
            guarded.feed(chunk)
    except (ValueError, UnicodeError):
        pass
    else:
        raise AssertionError('Unsafe log accepted')
    assert private_sink.getvalue() == ''
print('Console broker allowlist/preflight/log-privacy tests passed (no hardware accessed).')
