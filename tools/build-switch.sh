#!/usr/bin/env bash
# Build against the repository's pinned libnx, not the image's bundled version.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
image="${MISSIONCONTROL_BUILD_IMAGE:-devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282}"

command -v docker >/dev/null || { echo 'Docker is required.' >&2; exit 1; }
docker info >/dev/null 2>&1 || { echo 'Start Docker before building.' >&2; exit 1; }
git -C "$root" submodule update --init --recursive

docker run --rm -v "$root:/work" -w /work "$image" \
    bash -lc 'set -e; make -C lib/libnx -j4 install; make -j4 "$@"' bash "$@"
