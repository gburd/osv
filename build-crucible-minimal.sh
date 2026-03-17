#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Building minimal OSv with Crucible driver (no filesystems) ===" >&2

# Build with Crucible driver profile but NO filesystem (use ramfs)
nix develop --command bash -c './scripts/build conf_drivers_profile=crucible fs=ramfs image=native-example'
