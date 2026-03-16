#!/bin/bash
#
# Setup OpenZFS for OSv
#
# This script initializes the OpenZFS submodule and applies OSv platform patches.
# It should be run once after cloning the repository, or whenever the OpenZFS
# submodule is updated.
#
# Usage:
#   ./scripts/setup-zfs.sh
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSV_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "================================================"
echo "Setting up OpenZFS for OSv"
echo "================================================"
echo ""

cd "$OSV_ROOT"

# Step 1: Initialize/update submodules
echo "Step 1: Initializing OpenZFS submodule..."
if [[ ! -d "external/openzfs/.git" ]]; then
    echo "  Initializing submodule for the first time..."
    git submodule update --init external/openzfs
else
    echo "  Updating submodule..."
    git submodule update external/openzfs
fi
echo "✓ Submodule initialized"
echo ""

# Step 2: Ensure we're on the correct base commit
echo "Step 2: Checking out OpenZFS 2.3.6..."
cd external/openzfs
CURRENT_COMMIT=$(git rev-parse --short HEAD)
if [[ "$CURRENT_COMMIT" != "c840612ee" ]]; then
    echo "  Current commit: $CURRENT_COMMIT"
    echo "  Checking out zfs-2.3.6..."
    git checkout zfs-2.3.6
fi
echo "✓ On zfs-2.3.6 (commit c840612ee)"
echo ""

cd "$OSV_ROOT"

# Step 3: Apply OSv platform patches
echo "Step 3: Applying OSv platform patches..."
./scripts/apply-openzfs-patches.sh
echo ""

echo "================================================"
echo "OpenZFS Setup Complete!"
echo "================================================"
echo ""
echo "Next steps:"
echo "  1. Build OSv with ZFS:"
echo "     ./scripts/build arch=aarch64 fs=zfs image=native-example"
echo ""
echo "  2. Or use the automated build script:"
echo "     ./scripts/build-osv-zfs.sh aarch64"
echo ""
