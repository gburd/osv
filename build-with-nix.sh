#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Building OSv with Crucible driver and assembly fix ===" >&2

# Run build within nix develop environment
nix develop --command bash -c './scripts/build conf_drivers_profile=crucible image=crucible-basic-test'
