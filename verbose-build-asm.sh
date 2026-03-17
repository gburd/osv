#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Building aes_amd64.o with V=1 ===" >&2

# Remove the object file first
rm -f build/release.x64/external/openzfs/module/icp/asm-x86_64/aes/aes_amd64.o

nix develop --command bash -c 'make V=1 build/release.x64/external/openzfs/module/icp/asm-x86_64/aes/aes_amd64.o 2>&1' | grep -E "^g\+\+|Error:"
