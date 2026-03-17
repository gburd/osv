#!/bin/bash
set -e

cd /home/gburd/ws/osv

echo "=== Building OSv with OpenZFS assembly fix ===" >&2

./scripts/build conf_drivers_profile=crucible fs=zfs image=crucible-basic-test 2>&1 | \
  grep -E "(AS external|Error|Error:|make\[|Build complete|make failed)" | tail -40
