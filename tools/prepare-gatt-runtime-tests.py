#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Generate host test includes from pinned SDK/production source; never open a device.

The test compiles the real ble_gatt.cpp and ble_events.hpp WaitFor/Pump. The small
host post-pair setup/retry slice and WaitForGattResult are extracted, not reimplemented.
No generated files enter a Switch build. Structural changes fail generation rather
than silently testing stale copies. This is not a test of the whole host thread.
"""
import argparse
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent

def block(text, marker):
    assert text.count(marker) == 1, f"expected unique production marker: {marker}"
    start = text.index(marker)
    left = text.index('{', start)
    depth = 0
    # Selected blocks have no brace-bearing string literals; reject those if introduced.
    for pos in range(left, len(text)):
        if text[pos] == '{':
            depth += 1
        elif text[pos] == '}':
            depth -= 1
            if not depth:
                return text[start:pos+1]
    raise AssertionError(f"unterminated production block: {marker}")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    sdk = (ROOT/'lib/libnx/nx/include/switch/services/btdrv.h').read_text()
    declarations = ['// Generated from the pinned SDK. Do not edit.\n#pragma once\n// Pinned C SDK uses GNU anonymous nested types; treat only these declarations as system headers.\n#pragma GCC system_header\n#include <switch/services/btdrv_types.h>\n']
    for name in ['BtdrvEventInfo', 'BtdrvHidEventInfo', 'BtdrvHidReportEventInfo', 'BtdrvBleEventInfo']:
        tag = '} '+name+';'
        assert sdk.count(tag) == 1
        end = sdk.index(tag)+len(tag)
        start = sdk.rfind('typedef struct {', 0, end)
        declaration = sdk[start:end]
        assert start >= 0 and declaration.count('typedef ') == 1
        declarations.append(declaration+'\n')
    for function in ['btdrvGetGattAttribute', 'btdrvConfigureAttMtu', 'btdrvRegisterGattHidDataPath',
                     'btdrvReadGattCharacteristic', 'btdrvRegisterGattNotification', 'btdrvWriteGattDescriptor']:
        matches = [line for line in sdk.splitlines() if line.startswith('Result '+function+'(')]
        assert len(matches) == 1 and matches[0].endswith(';'), function
        declarations.append(matches[0]+'\n')
    (args.output/'ble-sdk-declarations.hpp').write_text('\n'.join(declarations))
    events = (ROOT/'mc_mitm/source/ble/ble_events.cpp').read_text()
    (args.output/'ble-wait-result.inc').write_text(block(events, 'bool WaitForGattResult(')+'\n')
    host = (ROOT/'mc_mitm/source/ble/ble_hid_host.cpp').read_text()
    thread = block(host, 'void HostThreadFunc(')
    retry = block(thread, 'if (g_setup_failed)')
    start_marker = 'Result rc = gatt::Discover(conn_id, address);'
    end_marker = '// What the device says about its own reports,'
    assert thread.count(start_marker) == thread.count(end_marker) == 1
    setup = thread[thread.index(start_marker):thread.index(end_marker)]
    assert setup.count('gatt::Subscribe(') == 1 and setup.count('btdrvRegisterGattHidDataPath(') == 1
    assert host.count('bool g_setup_failed = false;') == 1
    output = 'bool g_setup_failed = false;\n'+block(host, 'void StopAfterSetupFailure(')+'\n'
    output += 'bool SetupOnce(u32 conn_id, const bluetooth::Address &address, gatt::HidService *out) {\n'
    output += 'bool completed=false; for(unsigned attempt=0;attempt<1;++attempt) {\n'+retry+'\n'+setup
    output += '*out=hid; completed=true; } return completed; }\n'
    connected = block(host, 'void RunConnected(')
    output += 'void RediscoverOnce(u32 conn_id) {\n'+block(connected, 'if (gatt::TakeRediscovered())')+'\n}\n'
    (args.output/'ble-host-setup.inc').write_text(output)
    print('Generated verbatim SDK declarations, real wait-result body and post-pair host setup/retry slice (no devices).')

if __name__ == '__main__':
    main()
