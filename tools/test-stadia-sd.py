#!/usr/bin/env python3
"""Binary file protocol tests using fake sockets; no device or network access."""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
import stadia_sd as sd

class FakeSocket:
    def __init__(self, incoming):
        self.incoming = incoming
        self.sent = bytearray()
    def settimeout(self, timeout):
        assert timeout > 0
    def recv(self, size):
        count = min(size, 7, len(self.incoming))
        result, self.incoming = self.incoming[:count], self.incoming[count:]
        return result
    def sendall(self, data):
        self.sent.extend(data)

def header(status=0, payload=b'', digest=None):
    return sd.RESPONSE.pack(b'MCSDR001', status, len(payload),
                            digest if digest is not None else hashlib.sha256(payload).digest())

def refused(callback):
    try:
        callback()
    except (ValueError, OSError):
        return
    raise AssertionError('Invalid transaction accepted')

with tempfile.TemporaryDirectory(prefix='mc-sd-wire-') as temporary:
    root = Path(temporary); artifacts = root/'receipts'
    value = b'local test data\x00\xff'
    target = root/'download.txt'
    sock = FakeSocket(header(payload=value)+value)
    result = sd.exchange(sock, {'operation':'get','path':'/config/MissionControl/example.txt',
                                'destination':str(target)}, artifacts)
    assert target.read_bytes() == value and result['sha256'] == hashlib.sha256(value).hexdigest()
    refused(lambda: sd.exchange(FakeSocket(header(payload=value)+value),
                                {'operation':'get','path':'/example.txt','destination':str(target)},artifacts))
    assert target.read_bytes() == value
    missing = root/'must-not-exist.txt'
    refused(lambda: sd.exchange(FakeSocket(header(payload=value, digest=bytes(32))+value),
                                {'operation':'get','path':'/example.txt','destination':str(missing)},artifacts))
    assert not missing.exists()
    refused(lambda: sd.exchange(FakeSocket(header(payload=value)+value[:-1]),
                                {'operation':'get','path':'/example.txt','destination':str(missing)},artifacts))
    assert not missing.exists()
    digest = hashlib.sha256(value).digest()
    result = sd.exchange(FakeSocket(header(digest=digest)), {'operation':'hash','path':'/example.txt'},artifacts)
    assert result['sha256'] == digest.hex()
    source = root/'source.txt'; source.write_bytes(value)
    request = {'operation':'put','path':'/config/MissionControl/remote-staging/example.txt','source':str(source)}
    sock = FakeSocket(header(status=100)+header(digest=digest))
    result = sd.exchange(sock, request, artifacts)
    assert result['sha256'] == digest.hex() and result['backup'] is None
    assert sock.sent.endswith(value)
    denied = FakeSocket(header(status=5))
    refused(lambda: sd.exchange(denied, request, artifacts))
    assert len(denied.sent) == sd.REQUEST.size+len(request['path'].encode()) # No payload before READY.
    refused(lambda: sd.exchange(FakeSocket(header(status=100)+header(digest=bytes(32))),request,artifacts))
    refused(lambda: sd.validate({'operation':'put','path':'/example.txt','source':str(source),'expected':'xyz'}))
    refused(lambda: sd.validate({'operation':'get','path':'/../escape.txt','destination':str(missing)}))
    refused(lambda: sd.validate({'operation':'unknown','path':'/example.txt'}))
    nso = b'NSO0' + bytes(252)
    code_target = root/'btdrv.nso'
    result = sd.exchange(FakeSocket(header(payload=nso)+nso),
                         {'operation':'code','path':'/btdrv','destination':str(code_target)},artifacts)
    assert code_target.read_bytes()==nso and result['bytes']==len(nso)
    refused(lambda: sd.validate({'operation':'code','path':'/other','destination':str(root/'other.nso')}))
    fs_target=root/'btm.nso'
    fs_result=sd.exchange(FakeSocket(header(payload=nso)+nso),
                          {'operation':'code-fs','path':'/btm','destination':str(fs_target)},artifacts)
    assert fs_target.read_bytes()==nso and fs_result['operation']=='code-fs'
    refused(lambda: sd.validate({'operation':'code-fs','path':'/prod.keys','destination':str(root/'no.nso')}))
    refused(lambda: sd.exchange(FakeSocket(header(payload=value)+value),
                                {'operation':'code','path':'/btm','destination':str(root/'invalid.nso')},artifacts))
    assert not (root/'invalid.nso').exists()
    refused(lambda: sd.exchange(FakeSocket(header(status=8,digest=struct.pack('<I',0x1234)+bytes(28))),
                                {'operation':'code','path':'/btm','destination':str(root/'refused.nso')},artifacts))
    assert not (root/'refused.nso').exists()
    # New failure detail is a bounded extension of the existing header; legacy
    # failures remain stage-unknown. No error creates an executable or receipt.
    for stage in sd.CODE_STAGES:
        for route in sd.CODE_ROUTES:
            detail = struct.pack('<5I', 0x615, int.from_bytes(b'MCEX', 'little'), 1, route, stage) + bytes(12)
            incoming = sd.RESPONSE.pack(b'MCSDR001', 8, 0, detail)
            try:
                sd.exchange(FakeSocket(incoming), {'operation': 'code', 'path': '/btdrv',
                            'destination': str(root/'stage-error.nso')}, artifacts)
            except sd.CodeExportError as error:
                assert (error.status, error.ipc, error.route, error.stage) == (8, 0x615, route, stage)
                assert sd.CODE_STAGES[stage] in str(error) and 'no fallback' in str(error)
            else:
                raise AssertionError('Code failure accepted')
    assert not (root/'stage-error.nso').exists()
    for result in (1, 2, 4, 6):
        detail = struct.pack('<5I', 0, int.from_bytes(b'MCEX', 'little'), 1, 1, 17) + bytes(12)
        try:
            sd.response(FakeSocket(sd.RESPONSE.pack(b'MCSDR001', result, 0, detail)), sd.time.monotonic()+10)
        except sd.CodeExportError as error:
            assert error.status == result and error.stage == 17 and error.ipc == 0
        else:
            raise AssertionError('Structured validation failure accepted')
    try:
        sd.response(FakeSocket(header(status=8, digest=struct.pack('<I', 0x1234)+bytes(28))), sd.time.monotonic()+10)
    except sd.CodeExportError as error:
        assert error.stage is None and error.route is None and error.ipc == 0x1234
    else:
        raise AssertionError('Legacy failure accepted')
    for version, route, stage, reserved, size in ((2, 1, 3, bytes(12), 0),
            (1, 0, 3, bytes(12), 0), (1, 1, 21, bytes(12), 0),
            (1, 1, 3, b'X'+bytes(11), 0), (1, 1, 3, bytes(12), 1)):
        detail = struct.pack('<5I', 0x615, int.from_bytes(b'MCEX', 'little'), version, route, stage)+reserved
        refused(lambda: sd.response(FakeSocket(sd.RESPONSE.pack(b'MCSDR001', 8, size, detail)), sd.time.monotonic()+10))
    # A stream interrupted after success must not be mistaken for a diagnostic
    # frame, nor saved partially. A mutated second pass is caught by full hash.
    for body in (nso[:-1], b'NSO0'+b'X'*252):
        refused(lambda: sd.exchange(FakeSocket(header(payload=nso)+body),
                                    {'operation':'code','path':'/btm','destination':str(root/'partial.nso')}, artifacts))
        assert not (root/'partial.nso').exists()
    # Receipts contain metadata only, never transferred file values.
    for receipt in artifacts.glob('*.json'):
        assert 'local test data' not in receipt.read_text()
print('SD binary transfer framing/hash/precondition/truncation/no-overwrite tests passed (fake sockets).')
