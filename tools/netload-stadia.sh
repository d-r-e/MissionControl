#!/usr/bin/env bash
# Guarded launcher. Never send a real-controller probe after a failed preflight.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ $# -ne 2 ]]; then
    echo "Usage: bash tools/netload-stadia.sh SWITCH_IP MODE" >&2
    exit 2
fi
address="$1"
mode="$2"
case "$mode" in
    --status-only|--service-latency|--event-progress) expected='' ;;
    --managed-stadia|--discover-stadia|--discovery-requests|--trace-managed-stadia|--trace-managed-control|--trace-managed-control-canonical) expected=off ;;
    --control|--control-hid16|--control-hid16-lists|--bridge-control|--patched-be|--patched-le|--patched-incomplete-be|--inspect-control|--inspect-hid-control)
        echo 'Scan diagnostics PAUSED after scan-only sidebar/power-menu slowdown. No radio preflight or upload performed. Read-only status/service timing only. See HANDOFF.md.' >&2
        exit 1
        ;;
    --connect|--connect-control|--cancel-control)
        echo 'Connection diagnostics PAUSED after system sluggishness. No radio preflight or upload performed. See HANDOFF.md and tools/stadia-probe/REGRESSION.md.' >&2
        exit 1
        ;;
    *) echo 'Unsupported probe mode.' >&2; exit 2 ;;
esac
if [[ ! -f "$root/tools/stadia-probe/package/switch/stadia-ble-probe.nro" ]]; then
    echo 'Build the standalone probe first.' >&2
    exit 1
fi
if [[ -n "$expected" ]]; then
    python3 "$root/tools/check-mac-bluetooth.py" --expect "$expected"
fi
# Target is read inside the container, not interpolated into Docker's stored
# command metadata or printed. The NRO separately validates launch arguments.
name="missioncontrol-stadia-$(date -u +%Y%m%dT%H%M%SZ)-$$"
exec docker run -d --name "$name" -p 28771:28771 -v "$root:/work:ro" -w /work \
    devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282 \
    bash -lc '
        set -euo pipefail
        arguments="$2"
        case "$2" in
            --patched-be|--patched-le|--patched-incomplete-be|--connect|--managed-stadia|--discover-stadia|--discovery-requests|--trace-managed-stadia)
                target=$(tr -d "\r\n" < tools/stadia-probe/package/config/MissionControl/stadia-probe-address.txt)
                arguments="--target $target $2"
                ;;
        esac
        exec stdbuf -oL -eL nxlink -a "$1" -r 2 -s --args "$arguments" tools/stadia-probe/package/switch/stadia-ble-probe.nro
    ' -- "$address" "$mode"
