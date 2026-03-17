#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Checking build command for aes_amd64.o ===" >&2

nix develop --command bash -c 'make -n build/release.x64/external/openzfs/module/icp/asm-x86_64/aes/aes_amd64.o 2>&1 | grep -E "g\+\+|gcc"'
