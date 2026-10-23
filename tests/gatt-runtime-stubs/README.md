# Offline production GATT integration

Run `bash tools/test-stadia.sh`. For sensitivity checks, run
`python3 tools/test-gatt-runtime-mutations.py` (Unix host; no device access).

`tests/ble_gatt_runtime_tests.cpp` compiles the actual `ble_gatt.cpp` translation
unit, actual `ble_events.hpp` event-pump templates, and the real report-map parser.
The generator takes SDK data declarations/function signatures verbatim from the
pinned libnx headers, plus `WaitForGattResult` and the host's post-Pair setup,
rediscovery branch, failure latch and retry gate from their production sources.
Unique source markers/compile checks prevent silently exercising stale copies.
Only generated SDK declarations are marked as system headers: the pinned C API
uses GNU nested anonymous types. Production and test code retain strict warnings.

The shim provides a deterministic clock, mutex/result wrappers, an injected event
queue, and recording driver functions. No production test macros or alternate
runtime implementation are introduced. No service initialization, radio,
pairing, output characteristic, network or filesystem APIs are linked into the
runtime test. The mutation runner copies named sources into a temporary tree;
compile errors do not count as detected regressions.

Covered paths include same-pump cache invalidation or link/radio cancellation
winning over a successful completion before copy-out, post-Begin/pre-IPC cleanup,
discovery/read/CCCD invalidation, pending-read cleanup, wrong/duplicate/late
completions, timeout/disconnect/radio-off/shutdown, notification/CCCD IPC errors,
asynchronous read/CCCD errors, the sticky setup/retry gate, retained Report Map,
and same-service descriptor preflight (shuffled, adjacent, overlapping, nested,
intervening-characteristic, ambiguous CCCD and inclusive-boundary fixtures).

This is NOT the whole host thread, Pair, firmware event withholding, actual IPC,
power management or physical hardware. Discovery/MTU IPC failures retain their
inherited log-and-continue behavior and are not claimed covered. Missing native
attributes/intermediate characteristics and old callbacks carrying a reused
connection ID cannot be distinguished here. Bond persistence, normal reconnect,
latency and Xbox hardware regression remain separate gates.
