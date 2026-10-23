#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Prove the offline runtime tests fail when specific safety guards are removed.

Copies only named source files into a temporary tree; never mutates the repository,
opens a device, or uses Bluetooth. Compile failures are NOT counted as killed mutants.
"""
import importlib.util
import os
from pathlib import Path
import resource
import shlex
import shutil
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('gatt_prepare', ROOT/'tools/prepare-gatt-runtime-tests.py')
prepare = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prepare)
HEADERS = ['ble_gatt.hpp','ble_types.hpp','ble_events.hpp','ble_results.hpp','ble_log.hpp',
           'ble_gatt_read.hpp','ble_gatt_cache.hpp','ble_diagnostics.hpp',
           'ble_event_observation.hpp','ble_input_timing.hpp']
CASES = [
    ('read-copy-after-invalidation','ble_gatt.cpp','if (g_cache.Unusable() || !CanRequestGatt(conn_id)) break;','if (!CanRequestGatt(conn_id)) break;'),
    ('read-copy-after-radio-cancel','ble_gatt.cpp','if (g_cache.Unusable() || !CanRequestGatt(conn_id)) break;','if (g_cache.Unusable()) break;'),
    ('pending-read-not-finished','ble_gatt.cpp','g_read.Finish();','(void)0;'),
    ('cross-service-cccd','ble_gatt.cpp','IsWithinService(desc, service) &&',''),
    ('cccd-cache-rechecks','ble_gatt.cpp',None,None),
    ('ignored-notification-ipc-error','ble_gatt.cpp',
     'log::Write("btdrvRegisterGattNotification handle=0x%04x rc=0x%x", attr.handle, rc.GetValue());\n            R_TRY(rc);',
     'log::Write("btdrvRegisterGattNotification handle=0x%04x rc=0x%x", attr.handle, rc.GetValue());'),
    ('missing-host-failure-latch','ble-host-setup.inc','g_setup_failed = true;','(void)0;'),
    ('fallthrough-after-discovery-failure','ble-host-setup.inc',
     'StopAfterSetupFailure("GATT discovery cache");\n                    Disconnect();\n                    continue;',
     'StopAfterSetupFailure("GATT discovery cache");\n                    Disconnect();\n                    (void)0;'),
    ('pump-not-dispatching-sink','ble_events.hpp','sink(s_record);','(void)s_record;'),
]

def run_case(base, name, filename=None, old=None, new=None):
    tree=base/name
    ble=tree/'mc_mitm/source/ble'
    ble.mkdir(parents=True)
    for source in HEADERS+['ble_gatt.cpp']:
        shutil.copyfile(ROOT/'mc_mitm/source/ble'/source,ble/source)
    bluetooth=tree/'mc_mitm/source/bluetooth_mitm/bluetooth'
    bluetooth.mkdir(parents=True)
    for source in ['bluetooth_types.hpp','hid_report_layout.hpp']:
        shutil.copyfile(ROOT/'mc_mitm/source/bluetooth_mitm/bluetooth'/source,bluetooth/source)
    (tree/'tests').mkdir()
    shutil.copyfile(ROOT/'tests/ble_gatt_runtime_tests.cpp',tree/'tests/ble_gatt_runtime_tests.cpp')
    generated=tree/'generated'
    subprocess.run(['python3',str(ROOT/'tools/prepare-gatt-runtime-tests.py'),str(generated)],check=True,stdout=subprocess.DEVNULL)
    if filename:
        target=(generated if filename.endswith('.inc') else ble)/filename
        text=target.read_text()
        if name=='cccd-cache-rechecks':
            original=prepare.block(text,'Result Subscribe(')
            guard='R_UNLESS(!g_cache.Unusable(), svc::ResultNotSupported());'
            assert original.count(guard)>=4
            text=text.replace(original,original.replace(guard,'(void)0;'),1)
        else:
            assert old in text,name
            text=text.replace(old,new)
        target.write_text(text)
    command=shlex.split(os.environ.get('CXX','c++'))+[
        '-std=c++20','-Wall','-Wextra','-Werror','-pedantic','-fsanitize=address,undefined',
        '-fno-omit-frame-pointer','-g','-I',str(ROOT/'tests/gatt-runtime-stubs'),
        '-I',str(generated),'-I',str(ROOT/'lib/libnx/nx/include'),
        str(tree/'tests/ble_gatt_runtime_tests.cpp'),'-o',str(tree/'test')]
    subprocess.run(command,check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    result=subprocess.run([str(tree/'test')],capture_output=True,text=True)
    if filename:
        assert result.returncode==-signal.SIGABRT and 'assert' in result.stderr.lower() and 'fail' in result.stderr.lower(), (name,result.returncode,result.stderr)
        print('killed:',name)
    else:
        assert result.returncode==0,(name,result.stderr)
        print('control: production runtime tests pass')

def main():
    resource.setrlimit(resource.RLIMIT_CORE,(0,0))
    with tempfile.TemporaryDirectory(prefix='mc-gatt-mutations-') as temporary:
        base=Path(temporary)
        run_case(base,'control')
        for case in CASES:
            run_case(base,*case)
    print(f'GATT runtime sensitivity: {len(CASES)} safety regressions rejected; repository unchanged, no devices.')

if __name__=='__main__':
    main()
