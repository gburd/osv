#!/bin/bash
set -euo pipefail

cd /home/gburd/ws/osv

echo "=== Testing Crucible Driver with 3 Downstairs Servers ==="
echo ""
echo "This test will:"
echo "  1. Boot OSv with Crucible driver"
echo "  2. Connect to localhost:8810,8820,8830"
echo "  3. Run basic block device tests"
echo "  4. Log output to ./tmp/crucible-test.log"
echo ""

mkdir -p tmp

./scripts/run.py \
    --crucible=localhost:8810,localhost:8820,localhost:8830 \
    --crucible-uuid=test-region-uuid \
    --crucible-block-size=4096 \
    --virtio-fs-dir=tmp:/data \
    --memsize=2G \
    --execute='/crucible-test'
