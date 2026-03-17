#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Building OSv with Crucible driver (no ZFS) ===" >&2

nix develop --command bash -c './scripts/build conf_drivers_profile=crucible image=crucible-basic-test'
