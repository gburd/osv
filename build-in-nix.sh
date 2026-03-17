#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

# Source the nix environment
if [ -f /etc/profile.d/nix.sh ]; then
    source /etc/profile.d/nix.sh
fi

# Enter nix develop and build
nix develop --command bash -c './scripts/build conf_drivers_profile=crucible fs=zfs image=crucible-basic-test 2>&1' | \
  grep -E "(AS external|Error|Error:|make failed|Build complete)" | tail -40
