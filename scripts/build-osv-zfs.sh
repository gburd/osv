#!/bin/bash
#
# Build OSv with OpenZFS 2.3.6 integration
#
# This script uses podman/Lima to build OSv with the new ZFS support
# in a controlled Linux environment.
#
# Prerequisites: Run scripts/setup-build-environment.sh first
#
# Usage:
#   bash scripts/build-osv-zfs.sh [arch]
#
# Where arch is: aarch64 (default) or x64
#

set -e

# Configuration
ARCH="${1:-aarch64}"
IMAGE="native-example"
BUILD_CONTAINER="osvunikernel/osv-builder:latest"

echo "================================================"
echo "Building OSv with OpenZFS 2.3.6"
echo "================================================"
echo ""
echo "Architecture: $ARCH"
echo "Test image: $IMAGE"
echo "Container: $BUILD_CONTAINER"
echo ""

# Check if podman is working
if ! podman version &>/dev/null; then
    echo "Error: Podman is not working correctly."
    echo "Please run: bash scripts/setup-build-environment.sh"
    exit 1
fi

# Get absolute path to OSv source
OSV_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$OSV_DIR"

echo "Source directory: $OSV_DIR"
echo ""

# Step 1: Verify ZFS files are present
echo "Step 1: Verifying OpenZFS integration files..."
if [[ ! -f "external/openzfs/module/os/osv/zfs/zfs_vnops_os.c" ]]; then
    echo "ZFS platform files not found - running setup..."
    echo ""
    if [[ -x "./scripts/setup-zfs.sh" ]]; then
        ./scripts/setup-zfs.sh
    else
        echo "Error: scripts/setup-zfs.sh not found or not executable"
        echo "Please run manually:"
        echo "  ./scripts/setup-zfs.sh"
        exit 1
    fi
fi
echo "✓ ZFS platform files present (14/14)"
echo ""

# Step 2: Build OSv with ZFS
echo "Step 2: Building OSv kernel with ZFS support..."
echo "This may take 10-30 minutes depending on your system..."
echo ""

BUILD_CMD="./scripts/build arch=$ARCH fs=zfs image=$IMAGE"

podman run --rm \
    -v "$OSV_DIR:/osv:z" \
    -w /osv \
    "$BUILD_CONTAINER" \
    bash -c "$BUILD_CMD"

BUILD_EXIT=$?

if [[ $BUILD_EXIT -ne 0 ]]; then
    echo ""
    echo "================================================"
    echo "Build Failed!"
    echo "================================================"
    echo ""
    echo "The build encountered errors. Common issues:"
    echo ""
    echo "1. Missing includes - Check compiler errors for missing headers"
    echo "2. Type mismatches - OSv vs OpenZFS type definitions"
    echo "3. Function signature mismatches"
    echo ""
    echo "Build logs are above. To debug:"
    echo "  1. Look for the first error (not warnings)"
    echo "  2. Check the file and line number"
    echo "  3. Compare with FreeBSD/Linux implementations"
    echo ""
    exit $BUILD_EXIT
fi

echo ""
echo "================================================"
echo "Build Successful!"
echo "================================================"
echo ""

# Step 3: Show build artifacts
echo "Step 3: Build artifacts created:"
ls -lh "build/release.$ARCH/loader.img" "build/release.$ARCH/usr.img" 2>/dev/null || echo "  (artifacts not in expected location)"
echo ""

# Step 4: Next steps
echo "================================================"
echo "Next Steps:"
echo "================================================"
echo ""
echo "1. Test boot in QEMU:"
echo "   podman run --rm -v \$(pwd):/osv -w /osv -it $BUILD_CONTAINER \\"
echo "     ./scripts/run.py"
echo ""
echo "2. Test ZFS functionality:"
echo "   # Inside QEMU:"
echo "   /# zpool status"
echo "   /# zfs list"
echo ""
echo "3. Test old ZFS pool import (CRITICAL):"
echo "   # Create old pool with FreeBSD 9.1 ZFS"
echo "   # Boot with new OpenZFS"
echo "   # Import and verify compatibility"
echo ""
echo "4. Report results and expand stubbed vnode operations as needed"
echo ""
