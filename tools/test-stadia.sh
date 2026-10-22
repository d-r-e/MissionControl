#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="$(mktemp -d)"
trap 'rm -rf "$build"' EXIT

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$root/tools/stadia-input-test/tests/model-ui-tests.cpp" -o "$build/stadia-input-test-tests"
"$build/stadia-input-test-tests"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/controllers" \
    "$root/tests/stadia_report_tests.cpp" -o "$build/stadia-report-tests"
"$build/stadia-report-tests" "$root/tests/fixtures/stadia_bluetooth_reports.txt"

"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/tools/stadia-probe/source" \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    "$root/tests/stadia_probe_tests.cpp" -o "$build/stadia-probe-tests"
"$build/stadia-probe-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/tools/stadia-probe/source" \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    "$root/tests/managed_control_tests.cpp" -o "$build/managed-control-tests"
"$build/managed-control-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$root/tests/settled_stadia_tests.cpp" -o "$build/settled-stadia-tests"
"$build/settled-stadia-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    "$root/tests/stadia_advertisement_tests.cpp" -o "$build/stadia-advertisement-tests"
"$build/stadia-advertisement-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    -I "$root/mc_mitm/source/controllers" \
    "$root/tests/hogp_report_tests.cpp" -o "$build/hogp-report-tests"
"$build/hogp-report-tests" "$root/tests/fixtures/stadia_bluetooth_report_map.hex" \
    "$root/tests/fixtures/stadia_bluetooth_reports.txt"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/ble" \
    "$root/tests/ble_pad_tests.cpp" -o "$build/ble-pad-tests"
"$build/ble-pad-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/ble" \
    "$root/tests/ble_gatt_read_tests.cpp" -o "$build/ble-gatt-read-tests"
"$build/ble-gatt-read-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/ble" \
    "$root/tests/ble_event_observation_tests.cpp" -o "$build/ble-event-observation-tests"
"$build/ble-event-observation-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/ble" \
    "$root/tests/ble_input_timing_tests.cpp" -o "$build/ble-input-timing-tests"
"$build/ble-input-timing-tests"
python3 "$root/tools/test-ble-ownership-contract.py"
python3 "$root/tools/stadia-probe/prepare-target.py" --self-test
# tools/check-stadia-scope.py guarded the module-side probe/trace instrumentation, which now
# lives on branch diag/ble-probe and is intentionally absent from this branch.
python3 "$root/tools/check-mac-bluetooth.py" --self-test
python3 "$root/tools/test-stadia-launcher.py"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/tools/stadia-probe/source" \
    "$root/tests/stadia_console_tests.cpp" -o "$build/stadia-console-tests"
"$build/stadia-console-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/tools/stadia-probe/source" \
    "$root/tests/sd_file_policy_tests.cpp" -o "$build/sd-file-policy-tests"
"$build/sd-file-policy-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/tests/stubs" -I "$root/tools/stadia-probe/source" \
    "$root/tests/sd_file_session_tests.cpp" -o "$build/sd-file-session-tests"
"$build/sd-file-session-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$root/tests/ble_connection_lease_tests.cpp" -o "$build/ble-connection-lease-tests"
"$build/ble-connection-lease-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$root/tests/ble_event_progress_tests.cpp" -o "$build/ble-event-progress-tests"
"$build/ble-event-progress-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    "$root/tests/ble_connection_trace_tests.cpp" -o "$build/ble-connection-trace-tests"
"$build/ble-connection-trace-tests"
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    -I "$root/mc_mitm/source/bluetooth_mitm/bluetooth" \
    "$root/tests/connection_trace_client_tests.cpp" -o "$build/connection-trace-client-tests"
"$build/connection-trace-client-tests"
python3 "$root/tools/test-stadia-console.py"
python3 "$root/tools/test-stadia-sd.py"

# BLE pad identification (advertisement records, endpoint fingerprints) and the SD module swap
ble_pad_bin="$(mktemp -d)/ble-pad"
clang++ -std=c++20 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined \
    -I "$root/mc_mitm/source/ble" "$root/tests/ble_pad_tests.cpp" -o "$ble_pad_bin"
"$ble_pad_bin"
python3 "$root/tools/test-ble-deploy.py"

bash -n "$root/tools/netload-stadia.sh"
bash "$root/tools/stadia-host/test.sh"
