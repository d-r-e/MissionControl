#!/usr/bin/env python3
"""Foreground diagnostics and guarded SD transfers; no shell or Switch listener."""
import argparse
import ipaddress
import json
import os
from pathlib import Path
import re
import selectors
import socket
import subprocess
import sys
import stadia_sd

ROOT = Path(__file__).resolve().parent.parent
STATE = Path.home() / '.cache/missioncontrol/stadia-console'
SOCKET = STATE / 'control.sock'
COMMANDS = frozenset(('status', 'control', 'hid-complete', 'hid-incomplete',
                      'real-complete', 'real-incomplete', 'cancel', 'quit'))
IMAGE = 'devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282'
ADDRESS = re.compile(rb'(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}')


def decode_command(data):
    if len(data) > 32 or not data.endswith(b'\n') or data.count(b'\n') != 1:
        raise ValueError('Invalid command frame')
    command = data[:-1].decode('ascii')
    if command not in COMMANDS:
        raise ValueError('Command not allowed')
    return command


def check_radio(command):
    if command in ('status', 'cancel', 'quit', 'file'):
        return
    expected = 'off' if command.startswith('real-') else 'on'
    result = subprocess.run([sys.executable, str(ROOT/'tools/check-mac-bluetooth.py'),
                             '--expect', expected], capture_output=True, timeout=15)
    if result.returncode:
        raise ValueError('Mac Bluetooth preflight refused command; no request sent')


class SafeLog:
    """Only this diagnostic's bounded text; refuse device-address output."""
    def __init__(self, stream, on_line=None):
        self.stream = stream
        self.on_line = on_line
        self.pending = b''
        self.total = 0

    def feed(self, data):
        self.total += len(data)
        if self.total > 4 * 1024 * 1024:
            raise ValueError('Session log capacity exceeded')
        self.pending += data
        while b'\n' in self.pending:
            line, self.pending = self.pending.split(b'\n', 1)
            if len(line) > 511 or ADDRESS.search(line):
                raise ValueError('Diagnostic log privacy/length guard refused output')
            decoded = line.decode('utf-8', errors='strict')
            self.stream.write(decoded+'\n')
            self.stream.flush()
            if self.on_line is not None:
                self.on_line(decoded)
        if len(self.pending) > 511:
            raise ValueError('Diagnostic line capacity exceeded')


def serve(address, upload):
    if not ipaddress.IPv4Address(address).is_private:
        raise ValueError('A private LAN Switch address is required')
    STATE.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(STATE, 0o700)
    if SOCKET.exists() or (STATE / 'file.sock').exists():
        raise ValueError('A console socket already exists; do not overwrite an existing session')
    network = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    file_listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    local = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    file_local = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    selector = selectors.DefaultSelector()
    peer = None
    uploader = None
    local_bound = False
    file_local_bound = False
    artifacts = STATE / 'artifacts'
    file_sock_path = STATE / 'file.sock'
    try:
        network.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        network.bind(('0.0.0.0', 28771))
        network.listen(2)
        file_listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        file_listener.bind(('0.0.0.0', 28772))
        file_listener.listen(2)
        local.bind(str(SOCKET))
        local_bound = True
        os.chmod(SOCKET, 0o600)
        local.listen(2)
        file_local.bind(str(file_sock_path))
        file_local_bound = True
        os.chmod(file_sock_path, 0o600)
        file_local.listen(2)
        selector.register(network, selectors.EVENT_READ, 'network')
        selector.register(local, selectors.EVENT_READ, 'local')
        selector.register(file_local, selectors.EVENT_READ, 'file_local')
        print('Console listener ready. Switch must connect and authorize with physical A.', flush=True)
        if upload:
            # No -s: this broker, not nxlink's log-only server, owns port 28771.
            uploader = subprocess.Popen(['docker', 'run', '--rm', '-v', str(ROOT)+':/work:ro',
                '-w', '/work', IMAGE, 'nxlink', '-a', address, '-r', '2', '--args', '--console',
                'tools/stadia-probe/package/switch/stadia-ble-probe.nro'])
        state = {'idle': False, 'file_access': False}
        def observe(line):
            if line.startswith('REMOTE READY:'):
                state['file_access'] = '/file/' in line
                state['idle'] = True
            elif line.startswith('REMOTE IDLE:'):
                state['idle'] = True
            elif line == 'REMOTE BEGIN' or line == 'REMOTE FILE SESSION START':
                state['idle'] = False
        log = SafeLog(sys.stdout, observe)
        while True:
            if uploader is not None and uploader.poll() is not None:
                if uploader.returncode:
                    raise ValueError('Netload failed; no diagnostic session established')
                uploader = None
            for key, _ in selector.select(timeout=1):
                if key.data == 'network':
                    candidate, remote = network.accept()
                    if peer is not None or remote[0] != address:
                        candidate.close()
                        continue
                    peer = candidate
                    peer.setblocking(False)
                    selector.register(peer, selectors.EVENT_READ, 'peer')
                    print('Switch diagnostic stream connected.', flush=True)
                elif key.data == 'peer':
                    data = peer.recv(4096)
                    if not data:
                        print('Switch disconnected. Session closed.', flush=True)
                        return
                    log.feed(data)
                elif key.data == 'file_local':
                    request_sock, _ = file_local.accept()
                    with request_sock:
                        request_sock.settimeout(5)
                        try:
                            buf = bytearray()
                            while b'\n' not in buf and len(buf) <= 8192:
                                chunk = request_sock.recv(8193 - len(buf))
                                if not chunk:
                                    break
                                buf.extend(chunk)
                            if len(buf) > 8192 or not buf.endswith(b'\n') or buf.count(b'\n') != 1:
                                raise ValueError('Invalid file request frame')
                            parsed = json.loads(buf.decode('utf-8'))
                            if peer is None:
                                raise ValueError('Switch not connected')
                            if not state['file_access']:
                                raise ValueError('This probe has not authorized file access')
                            if not state['idle']:
                                raise ValueError('Console is not authorized and idle')
                            stadia_sd.validate(parsed)
                            state['idle'] = False
                            receipt = stadia_sd.perform(file_listener, address, peer, parsed, artifacts)
                            request_sock.sendall((json.dumps(receipt) + '\n').encode('utf-8'))
                        except (ValueError, UnicodeError, OSError, subprocess.SubprocessError) as error:
                            err_msg = json.dumps({'status': 'error', 'error': str(error)}) + '\n'
                            request_sock.sendall(err_msg.encode('utf-8'))
                else:
                    request, _ = local.accept()
                    with request:
                        request.settimeout(1)
                        try:
                            data = bytearray()
                            while b'\n' not in data and len(data) <= 32:
                                chunk = request.recv(33 - len(data))
                                if not chunk:
                                    break
                                data.extend(chunk)
                            command = decode_command(bytes(data))
                            if peer is None:
                                raise ValueError('Switch not connected')
                            check_radio(command)
                            if command not in ('cancel', 'quit'):
                                state['idle'] = False
                            peer.sendall(command.encode('ascii')+b'\n')
                            request.sendall(b'QUEUED: acceptance/completion must be checked in the session log\n')
                        except (ValueError, UnicodeError, OSError, subprocess.SubprocessError) as error:
                            request.sendall(('REFUSED: '+str(error)+'\n').encode('utf-8'))
    finally:
        # Closing the outbound stream makes the NRO cancel/clean up and exit.
        selector.close()
        if peer is not None:
            peer.close()
        network.close()
        local.close()
        if local_bound:
            SOCKET.unlink(missing_ok=True)
        if file_local_bound:
            file_sock_path.unlink(missing_ok=True)
        file_listener.close()
        file_local.close()
        if uploader is not None and uploader.poll() is None:
            uploader.terminate()
            uploader.wait(timeout=10)


def send_command(command):
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
        client.settimeout(20)
        client.connect(str(SOCKET))
        client.sendall(command.encode('ascii')+b'\n')
        reply = client.recv(1024).decode('utf-8')
        print(reply, end='')
        return 0 if reply.startswith('QUEUED:') else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    launch = sub.add_parser('serve')
    launch.add_argument('address')
    launch.add_argument('--upload', action='store_true')
    command = sub.add_parser('send')
    command.add_argument('command', choices=sorted(COMMANDS))
    file_cmd = sub.add_parser('file')
    file_sub = file_cmd.add_subparsers(dest='file_action', required=True)
    f_list = file_sub.add_parser('list')
    f_list.add_argument('path')
    f_get = file_sub.add_parser('get')
    f_get.add_argument('path')
    f_get.add_argument('destination')
    f_code = file_sub.add_parser('code')
    f_code.add_argument('module', choices=('btdrv', 'btm'))
    f_code.add_argument('destination')
    f_code.add_argument('--route', choices=('loader', 'filesystem'), default='loader')
    f_hash = file_sub.add_parser('hash')
    f_hash.add_argument('path')
    f_put = file_sub.add_parser('put')
    f_put.add_argument('source')
    f_put.add_argument('path')
    f_put.add_argument('--expected', default=None)
    args = parser.parse_args()
    if args.action == 'serve':
        serve(args.address, args.upload)
        return 0
    if args.action == 'file':
        req = {'operation': args.file_action, 'path': getattr(args, 'path', None)}
        if args.file_action in ('get', 'code'):
            req['destination'] = args.destination
            if args.file_action == 'code':
                req['path'] = '/' + args.module
                if args.route == 'filesystem':
                    req['operation'] = 'code-fs'
        elif args.file_action == 'put':
            req['source'] = args.source
            if args.expected:
                req['expected'] = args.expected
        import json
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
            client.settimeout(65)
            client.connect(str(STATE / 'file.sock'))
            client.sendall((json.dumps(req) + '\n').encode('utf-8'))
            reply = bytearray()
            while b'\n' not in reply:
                chunk = client.recv(4096)
                if not chunk:
                    break
                reply.extend(chunk)
            res = json.loads(reply.decode('utf-8'))
            print(json.dumps(res, indent=2))
            return 0 if res.get('status') == 'ok' else 1
    return send_command(args.command)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, OSError, subprocess.SubprocessError) as error:
        print('Console stopped:', error, file=sys.stderr)
        sys.exit(1)
