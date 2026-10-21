"""Bounded binary SD transfers, separate from diagnostic logs. No shell commands."""
import hashlib
import json
import os
from pathlib import Path
import secrets
import socket
import struct
import time

MAX_FILE = 4 * 1024 * 1024
MAX_CODE = 16 * 1024 * 1024
REQUEST = struct.Struct('<8sIIII32s32s')
RESPONSE = struct.Struct('<8sII32s')
OPERATIONS = {'list': 1, 'get': 2, 'hash': 3, 'put': 4, 'code': 5, 'code-fs': 6}
ERRORS = {1: 'path protected', 2: 'invalid request', 3: 'not found', 4: 'capacity exceeded',
          5: 'hash precondition mismatch', 6: 'I/O failure', 7: 'conflict'}


CODE_STAGES = {
    1: 'request validation', 2: 'HOS version gate', 3: 'loader service acquisition',
    4: 'loader domain conversion', 5: 'loader current-process registration',
    6: 'loader code-filesystem open', 7: 'NCM initialization',
    8: 'NCM database open', 9: 'NCM latest metadata lookup',
    10: 'NCM identity validation', 11: 'NCM program content lookup',
    12: 'NCM content-storage open', 13: 'NCM content-path lookup/validation',
    14: 'normal code-filesystem open', 15: 'main-file open', 16: 'main-file size',
    17: 'size bounds', 18: 'pre-transfer hash read', 19: 'NSO header validation',
    20: 'operator cancellation',
}
CODE_ROUTES = {1: 'loader', 2: 'normal-fs'}


class CodeExportError(ValueError):
    def __init__(self, status, ipc, route=None, stage=None):
        self.status, self.ipc, self.route, self.stage = status, ipc, route, stage
        where = (f'{CODE_ROUTES[route]} / {CODE_STAGES[stage]}' if stage is not None
                 else 'legacy reply; failure stage unknown')
        super().__init__(f'Code export stopped at {where} '
                         f'(result {status}, IPC 0x{ipc:08x}); no fallback attempted')


def exact(sock, size, deadline):
    data = bytearray()
    while len(data) < size:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise ValueError('File transfer deadline exceeded')
        sock.settimeout(min(5, remaining))
        chunk = sock.recv(min(65536, size-len(data)))
        if not chunk:
            raise ValueError('File transfer incomplete; do not retry a write without checking its outcome')
        data.extend(chunk)
    return bytes(data)


def response(sock, deadline, limit=MAX_FILE):
    magic, status, size, digest = RESPONSE.unpack(exact(sock, RESPONSE.size, deadline))
    if magic != b'MCSDR001' or size > limit:
        raise ValueError('Invalid file response')
    if status not in (0, 100) and digest[4:8] == b'MCEX':
        ipc, _, version, route, stage = struct.unpack('<5I', digest[:20])
        if (size or version != 1 or route not in CODE_ROUTES or stage not in CODE_STAGES
                or any(digest[20:])):
            raise ValueError('Invalid code-export failure detail')
        raise CodeExportError(status, ipc, route, stage)
    if status == 8:
        if size or any(digest[4:]):
            raise ValueError('Invalid legacy code-export failure detail')
        raise CodeExportError(status, struct.unpack('<I', digest[:4])[0])
    if status not in (0, 100):
        raise ValueError('Device refused file operation: '+ERRORS.get(status, 'unknown result')+
                         '; if a write began, check its hash before retrying')
    return status, size, digest


def validate(request):
    if not isinstance(request, dict) or request.get('operation') not in OPERATIONS:
        raise ValueError('Unsupported file operation')
    path = request.get('path')
    if not isinstance(path, str) or not path.startswith('/') or len(path.encode('utf-8')) > 240:
        raise ValueError('Invalid SD path')
    # The authoritative traversal/secret/write policy is also enforced on Switch.
    if '\x00' in path or any(part in ('.', '..') for part in path.split('/')):
        raise ValueError('Invalid SD path')
    if request['operation'] == 'put':
        expected = request.get('expected')
        if expected is not None:
            if not isinstance(expected, str) or len(expected) != 64:
                raise ValueError('Expected SHA256 must be 64 hex digits')
            try:
                bytes.fromhex(expected)
            except ValueError:
                raise ValueError('Invalid expected SHA256') from None
        source = request.get('source')
        if not isinstance(source, str):
            raise ValueError('Upload source required')
        p = Path(source)
        if p.is_symlink() or not p.is_file() or p.stat().st_size > MAX_FILE:
            raise ValueError('Upload source invalid or oversized')
        if p.name.startswith('.') or any(part in p.name.lower() for part in ('.keys', '.pem', 'credential', 'token')):
            raise ValueError('Sensitive upload source refused')
    if request['operation'] in ('code', 'code-fs') and path not in ('/btdrv', '/btm'):
        raise ValueError('Only the approved btdrv/btm main executables may be exported')
    if request['operation'] in ('get', 'code', 'code-fs') and not isinstance(request.get('destination'), str):
        raise ValueError('Download destination required')


def exchange(sock, request, artifacts):
    validate(request)
    deadline = time.monotonic()+60
    operation = request['operation']
    path = request['path'].encode('utf-8')
    payload = Path(request['source']).read_bytes() if operation == 'put' else b''
    if len(payload) > MAX_FILE:
        raise ValueError('Upload source grew beyond capacity')
    expected = request.get('expected')
    header = REQUEST.pack(b'MCSD0001', OPERATIONS[operation], len(path), len(payload),
                          1 if expected is not None else 0,
                          bytes.fromhex(expected) if expected is not None else bytes(32),
                          hashlib.sha256(payload).digest() if operation == 'put' else bytes(32))
    sock.settimeout(5)
    sock.sendall(header+path)
    status, size, digest = response(sock, deadline, MAX_CODE if operation in ('code', 'code-fs') else MAX_FILE)
    if operation == 'put':
        if status != 100 or size != 0:
            raise ValueError('Write was not authorized by device')
        sock.sendall(payload)
        status, size, digest = response(sock, deadline)
        if status != 0 or digest != hashlib.sha256(payload).digest() or size > 1024:
            raise ValueError('Write outcome unconfirmed; inspect device hash before retrying')
        backup = exact(sock, size, deadline).decode('utf-8')
        receipt = {'status': 'ok', 'operation': operation, 'sha256': digest.hex(),
                   'backup': backup or None, 'power_loss_atomic': False}
    elif operation == 'hash':
        if status != 0 or size != 0:
            raise ValueError('Invalid hash response')
        receipt = {'status': 'ok', 'operation': operation, 'sha256': digest.hex()}
    else:
        if status != 0:
            raise ValueError('Unexpected transfer status')
        data = exact(sock, size, deadline)
        if hashlib.sha256(data).digest() != digest:
            raise ValueError('Downloaded content hash mismatch; no file saved')
        if operation in ('code', 'code-fs') and (len(data) < 0x100 or not data.startswith(b'NSO0')):
            raise ValueError('Code export is not an NSO; no file saved')
        artifacts.mkdir(parents=True, exist_ok=True, mode=0o700)
        target = Path(request['destination']) if operation in ('get', 'code', 'code-fs') else artifacts/('listing-'+secrets.token_hex(6)+'.txt')
        # Never overwrite an existing Mac file or follow an output symlink.
        descriptor = os.open(target, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, 'wb') as output:
            output.write(data)
        receipt = {'status': 'ok', 'operation': operation, 'sha256': digest.hex(),
                   'bytes': len(data), 'local_file': str(target)}
    artifacts.mkdir(parents=True, exist_ok=True, mode=0o700)
    record = artifacts/('receipt-'+secrets.token_hex(6)+'.json')
    descriptor = os.open(record, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, 'w') as out:
        json.dump(receipt, out, indent=2)
    return receipt


def perform(listener, address, peer, request, artifacts):
    validate(request)
    peer.sendall(b'file\n')
    deadline = time.monotonic()+10
    while time.monotonic() < deadline:
        listener.settimeout(max(0.1, deadline-time.monotonic()))
        connection, remote = listener.accept()
        with connection:
            if remote[0] == address:
                return exchange(connection, request, artifacts)
    raise ValueError('Switch did not open the file channel; check console status')
