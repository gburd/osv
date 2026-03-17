#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Checking build command for zstd-in.o ===" >&2

nix develop --command bash -c 'make -n build/release.x64/external/openzfs/module/zstd/zstd-in.o 2>&1 | grep -E "^g\+\+|^gcc"'
