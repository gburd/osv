#!/bin/bash
set -euo pipefail

# update-zfs.sh -- Update OpenZFS submodule and verify OSv integration
#
# Usage:
#   scripts/update-zfs.sh                  # Update to latest stable
#   scripts/update-zfs.sh zfs-2.3.7       # Update to specific tag
#   scripts/update-zfs.sh --check          # Check for updates only
#   scripts/update-zfs.sh --status         # Show current version info

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OPENZFS_DIR="$REPO_ROOT/external/openzfs"
OSV_PLATFORM_DIR="$OPENZFS_DIR/module/os/osv"
OSV_INCLUDE_DIR="$OPENZFS_DIR/include/os/osv"
PATCHES_DIR="$REPO_ROOT/bsd/sys/cddl/osv-patches"
MAPPING_DOC="$REPO_ROOT/bsd/sys/cddl/OPENZFS_MAPPING.md"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

check_prerequisites() {
    if [ ! -d "$OPENZFS_DIR/.git" ]; then
        log_error "OpenZFS submodule not found at $OPENZFS_DIR"
        log_error "Run: git submodule add https://github.com/openzfs/zfs.git external/openzfs"
        exit 1
    fi

    if [ ! -d "$OSV_PLATFORM_DIR" ]; then
        log_error "OSv platform directory not found at $OSV_PLATFORM_DIR"
        exit 1
    fi
}

get_current_version() {
    cd "$OPENZFS_DIR"
    git describe --tags --exact-match 2>/dev/null || git rev-parse --short HEAD
}

get_latest_stable() {
    cd "$OPENZFS_DIR"
    git fetch --tags --quiet
    # Find latest zfs-X.Y.Z tag (exclude -rc and -99 pre-release)
    git tag -l 'zfs-*' | grep -E '^zfs-[0-9]+\.[0-9]+\.[0-9]+$' | sort -V | tail -1
}

show_status() {
    check_prerequisites
    local current
    current=$(get_current_version)
    echo "OpenZFS submodule: $OPENZFS_DIR"
    echo "Current version:   $current"
    echo ""
    echo "OSv platform files:"
    ls "$OSV_PLATFORM_DIR/zfs/" 2>/dev/null | while read -r f; do
        echo "  module/os/osv/zfs/$f"
    done
    echo ""
    echo "OSv headers:"
    ls "$OSV_INCLUDE_DIR/zfs/sys/" 2>/dev/null | while read -r f; do
        echo "  include/os/osv/zfs/sys/$f"
    done
}

check_for_updates() {
    check_prerequisites
    local current latest
    current=$(get_current_version)
    latest=$(get_latest_stable)

    echo "Current version: $current"
    echo "Latest stable:   $latest"

    if [ "$current" = "$latest" ]; then
        log_info "Already on latest stable release"
        return 0
    else
        log_warn "Update available: $current -> $latest"
        return 1
    fi
}

verify_osv_platform_files() {
    log_info "Verifying OSv platform files..."
    local missing=0

    local required_sources=(
        "arc_os.c"
        "dmu_os.c"
        "event_os.c"
        "kmod_core.c"
        "spa_os.c"
        "sysctl_os.c"
        "vdev_disk.c"
        "vdev_label_os.c"
        "zfs_initialize_osv.c"
        "zfs_ioctl_os.c"
        "zfs_vfsops.c"
        "zfs_vnops_os.c"
        "zfs_znode_os.c"
        "zvol_os.c"
    )

    for src in "${required_sources[@]}"; do
        if [ ! -f "$OSV_PLATFORM_DIR/zfs/$src" ]; then
            log_error "Missing: module/os/osv/zfs/$src"
            missing=$((missing + 1))
        fi
    done

    local required_headers=(
        "arc_os.h"
        "zfs_context_os.h"
        "zfs_znode_impl.h"
    )

    for hdr in "${required_headers[@]}"; do
        if [ ! -f "$OSV_INCLUDE_DIR/zfs/sys/$hdr" ]; then
            log_error "Missing: include/os/osv/zfs/sys/$hdr"
            missing=$((missing + 1))
        fi
    done

    if [ "$missing" -gt 0 ]; then
        log_error "$missing platform files missing"
        return 1
    fi

    log_info "All OSv platform files present"
    return 0
}

check_api_compatibility() {
    log_info "Checking API compatibility..."
    local issues=0

    # Check that vdev_ops_t still has the fields we use
    if ! grep -q 'vdev_op_io_start' "$OPENZFS_DIR/include/sys/vdev_impl.h"; then
        log_error "vdev_ops_t missing vdev_op_io_start"
        issues=$((issues + 1))
    fi

    # Check that ABD API exists
    if ! grep -q 'abd_borrow_buf' "$OPENZFS_DIR/include/sys/abd.h"; then
        log_error "ABD API missing abd_borrow_buf"
        issues=$((issues + 1))
    fi

    # Check that arc_os functions are expected
    if ! grep -q 'arc_available_memory' "$OPENZFS_DIR/module/zfs/arc.c"; then
        log_error "arc.c missing arc_available_memory call"
        issues=$((issues + 1))
    fi

    # Check that the platform directory pattern is intact
    if [ ! -d "$OPENZFS_DIR/module/os/freebsd" ]; then
        log_warn "FreeBSD platform directory missing -- OpenZFS structure may have changed"
        issues=$((issues + 1))
    fi

    if [ "$issues" -gt 0 ]; then
        log_error "$issues API compatibility issues found"
        return 1
    fi

    log_info "API compatibility checks passed"
    return 0
}

do_update() {
    local target_version="${1:-}"
    check_prerequisites

    local current
    current=$(get_current_version)

    if [ -z "$target_version" ]; then
        target_version=$(get_latest_stable)
    fi

    echo "=================================="
    echo "OpenZFS Update: $current -> $target_version"
    echo "=================================="
    echo ""

    # Step 1: Checkout target version
    log_info "Checking out $target_version..."
    cd "$OPENZFS_DIR"
    git fetch --tags --quiet
    if ! git rev-parse "$target_version" >/dev/null 2>&1; then
        log_error "Tag $target_version not found"
        exit 1
    fi
    git checkout "$target_version" --quiet

    # Step 2: Verify OSv platform files
    verify_osv_platform_files || {
        log_error "Platform file verification failed"
        exit 1
    }

    # Step 3: Check API compatibility
    check_api_compatibility || {
        log_warn "API compatibility issues detected"
        log_warn "Manual review of OSv platform files may be needed"
    }

    # Step 4: Generate update report
    echo ""
    echo "=================================="
    echo "Update Report"
    echo "=================================="
    echo "Previous version: $current"
    echo "New version:      $target_version"
    echo ""

    # Show what changed in OpenZFS between versions
    local old_rev new_rev
    old_rev=$(git rev-parse "$current" 2>/dev/null || echo "unknown")
    new_rev=$(git rev-parse "$target_version")

    if [ "$old_rev" != "unknown" ]; then
        echo "Changes in platform-relevant files:"
        local relevant_paths=(
            "module/zfs/arc.c"
            "module/zfs/zfs_ioctl.c"
            "module/zfs/zfs_vnops.c"
            "module/zfs/zfs_znode.c"
            "module/zfs/spa.c"
            "module/zfs/zvol.c"
            "module/zfs/zio.c"
            "module/os/freebsd/"
            "include/sys/vdev_impl.h"
            "include/sys/abd.h"
            "include/sys/arc.h"
            "include/sys/arc_impl.h"
            "include/sys/zfs_znode.h"
        )

        for path in "${relevant_paths[@]}"; do
            local count
            count=$(git log --oneline "$old_rev..$new_rev" -- "$path" 2>/dev/null | wc -l | tr -d ' ')
            if [ "$count" -gt 0 ]; then
                echo "  $path: $count commits"
            fi
        done
    fi

    echo ""
    log_info "Update complete. Next steps:"
    echo "  1. Review API compatibility warnings (if any)"
    echo "  2. Update OSv platform files if needed"
    echo "  3. Build: make -j\$(nproc)"
    echo "  4. Test: scripts/test.py -t zfs"
    echo "  5. Commit: git add external/openzfs && git commit"
}

# Parse arguments
case "${1:-}" in
    --status)
        show_status
        ;;
    --check)
        check_for_updates
        ;;
    --help|-h)
        echo "Usage: $0 [OPTIONS] [VERSION]"
        echo ""
        echo "Options:"
        echo "  --status    Show current version and platform files"
        echo "  --check     Check for available updates"
        echo "  --help      Show this help"
        echo ""
        echo "Arguments:"
        echo "  VERSION     Target tag (e.g., zfs-2.3.7). Default: latest stable"
        ;;
    *)
        do_update "${1:-}"
        ;;
esac
