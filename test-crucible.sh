#!/bin/bash
#
# Test script for OSv + Crucible + ZFS integration
#

set -e

echo "=== OSv Crucible Integration Test ==="
echo ""
echo "Configuration:"
echo "  - Crucible downstairs: localhost:8810, localhost:8820, localhost:8830"
echo "  - Region UUID: test-region-uuid"
echo "  - Block size: 4096"
echo "  - VirtioFS: ./tmp -> /data"
echo "  - Image: crucible-zfs-test"
echo ""

# Ensure tmp directory exists
mkdir -p tmp
echo "Created/verified ./tmp directory for logs"
echo ""

# Check if downstairs servers are running
echo "Checking downstairs servers..."
for port in 8810 8820 8830; do
    if nc -z localhost $port 2>/dev/null; then
        echo "  ✓ localhost:$port is reachable"
    else
        echo "  ✗ localhost:$port is NOT reachable"
        echo "     Please ensure downstairs server is running on port $port"
    fi
done
echo ""

echo "Starting OSv with Crucible..."
echo ""

./scripts/run.py \
    --crucible=localhost:8810,localhost:8820,localhost:8830 \
    --crucible-uuid=test-region-uuid \
    --crucible-block-size=4096 \
    --virtio-fs-dir=tmp:/data \
    --memsize=2G \
    --verbose \
    --execute='/crucible-test'

echo ""
echo "=== Test Complete ==="
echo ""
echo "Check logs in: ./tmp/crucible-test.log"
