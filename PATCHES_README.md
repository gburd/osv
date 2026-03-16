# OpenZFS Patch-Based Integration for OSv

**Status**: Production-ready patch-based integration
**Approach**: git format-patch files applied at build time
**Base**: OpenZFS 2.3.6 (tag `zfs-2.3.6`, commit `c840612ee`)

---

## Quick Start

```bash
# One-command setup
./scripts/setup-zfs.sh

# Build automatically applies patches if needed
./scripts/build arch=aarch64 fs=zfs image=native-example
```

---

## Why Patches?

We converted from a submodule-commit approach to a patch-based approach for several key benefits:

### Clean Separation
- OpenZFS submodule stays at pristine `zfs-2.3.6` tag
- OSv changes are separate patch files
- Easy to see what OSv adds to upstream OpenZFS

### Easy Updates
```bash
# Update to new OpenZFS version
cd external/openzfs
git fetch --tags
git checkout zfs-2.3.7

# Reapply patches (may need conflict resolution)
cd ../..
./scripts/apply-openzfs-patches.sh
```

### Portable
- Works across different repositories and forks
- Doesn't depend on specific git history
- Standard format used by kernel developers

### Reviewable
- Clear diff of OSv changes
- Each patch has descriptive commit message
- Easy to review on mailing lists

### Maintainable
- Changes clearly documented in patch headers
- Can selectively apply or skip patches
- Easy to regenerate after modifications

---

## Patch Files

### Location
`patches/openzfs/` contains 4 patch files:

### Patches

**0001-Implement-zfs_ioctl_os.c-for-OSv.patch** (3.7 KB)
- Platform-specific ioctl operations
- VFS reference counting stubs
- 95 lines

**0002-OSv-Implement-zfs_znode_os.c.patch** (15 KB)
- Znode lifecycle management
- Manual memory allocation and reference counting
- Znode hold mechanism for serialization
- 560 lines

**0003-OSv-Implement-minimal-zfs_vnops_os.c.patch** (17 KB)
- Vnode operations (file operations)
- Essential ops: open, close, getattr, access, inactive, reclaim, fsync
- Stubbed ops for incremental implementation
- 630 lines

**0004-OSv-Add-complete-platform-layer-for-OpenZFS-2.3.6.patch** (33 KB)
- Platform headers (3 files)
- Remaining implementation files (10 files)
- Core components: vdev_disk, arc_os, spa_os, vdev_label_os
- VFS integration: zfs_initialize_osv, zfs_vfsops
- Stubs: zvol_os, dmu_os, event_os, kmod_core, sysctl_os
- ~15,400 lines

**Total**: 4 patches, ~16,700 lines of OSv-specific code

### What Gets Added

After applying patches, you get:

**Headers** (3 files in `include/os/osv/zfs/sys/`):
```
arc_os.h              - ARC OS definitions
zfs_context_os.h      - Platform context
zfs_znode_impl.h      - Znode implementation
```

**Implementation** (14 files in `module/os/osv/zfs/`):
```
vdev_disk.c           - Block device (8,000 lines)
arc_os.c              - ARC memory (2,700 lines)
spa_os.c              - Storage pool (4,000 lines)
vdev_label_os.c       - Device labels (1,400 lines)
zfs_initialize_osv.c  - Module init (3,200 lines)
zfs_vfsops.c          - VFS integration (1,900 lines)
zvol_os.c             - Volume stubs (1,300 lines)
dmu_os.c              - DMU stubs (425 lines)
event_os.c            - Event stubs (244 lines)
kmod_core.c           - Kernel module (296 lines)
sysctl_os.c           - Sysctl stubs (279 lines)
zfs_ioctl_os.c        - I/O control (95 lines)
zfs_znode_os.c        - Znode lifecycle (560 lines)
zfs_vnops_os.c        - Vnode operations (630 lines)
```

---

## Scripts

### setup-zfs.sh
One-command setup that:
1. Initializes OpenZFS submodule
2. Checks out zfs-2.3.6 tag
3. Applies all OSv platform patches

**Usage**:
```bash
./scripts/setup-zfs.sh
```

Run this after:
- First clone of repository
- Pulling updates that change patches
- Manually resetting submodule

### apply-openzfs-patches.sh
Applies patches with verification:
- Checks base commit is correct (c840612ee)
- Detects if patches already applied
- Applies patches with `git am`
- Verifies all key files present
- Handles errors and conflicts

**Usage**:
```bash
./scripts/apply-openzfs-patches.sh
```

**Automatic**: Called by `setup-zfs.sh` and `build-osv-zfs.sh`

### build-osv-zfs.sh
Build script that:
- Checks if patches are applied
- Automatically runs `setup-zfs.sh` if needed
- Builds OSv with ZFS support
- Shows next steps for testing

**Usage**:
```bash
./scripts/build-osv-zfs.sh [aarch64|x64]
```

---

## Development Workflow

### Modifying Platform Code

```bash
# 1. Apply patches if not already applied
./scripts/setup-zfs.sh

# 2. Make changes to platform files
cd external/openzfs
vim module/os/osv/zfs/zfs_vnops_os.c

# 3. Commit your changes
git add module/os/osv/zfs/zfs_vnops_os.c
git commit -m "OSv: Improve zfs_vnops_os.c"

# 4. Regenerate patches
git format-patch zfs-2.3.6 -o ../../patches/openzfs/

# 5. Reset submodule to clean state
git checkout zfs-2.3.6

# 6. Commit updated patches in main repo
cd ../..
git add patches/openzfs/
git commit -m "Update OpenZFS patches: <description>"

# 7. Test patch application
./scripts/apply-openzfs-patches.sh
```

### Updating OpenZFS Version

```bash
# 1. Update to new version
cd external/openzfs
git fetch --tags
git checkout zfs-2.3.7

# 2. Try applying patches
cd ../..
./scripts/apply-openzfs-patches.sh

# 3. If conflicts occur, resolve them
cd external/openzfs
# git am will pause on conflicts
# Fix conflicts in each file
git add <fixed-files>
git am --continue

# 4. Regenerate patches
git format-patch zfs-2.3.7 -o ../../patches/openzfs/

# 5. Update .gitmodules
vim .gitmodules
# Change: branch = zfs-2.3.7

# 6. Commit
cd ../..
git add patches/ .gitmodules external/openzfs
git commit -m "Update OpenZFS to 2.3.7"
```

---

## Troubleshooting

### Patches don't apply

**Symptom**: `git am` fails with conflicts

**Causes**:
- Wrong base commit (not zfs-2.3.6)
- Patches already applied
- Patches modified but not regenerated

**Solutions**:
```bash
# Check base commit
cd external/openzfs
git log --oneline -1
# Should show: c840612ee Tag zfs-2.3.6

# Reset to clean state
git checkout zfs-2.3.6
git clean -fd

# Reapply
cd ../..
./scripts/apply-openzfs-patches.sh
```

### "Patches already applied" warning

**Symptom**: Script detects platform files exist

**Cause**: Patches were previously applied

**Solution**:
- If patches are current: Continue building
- If you want to reapply: Answer 'y' when prompted
- To force clean state:
  ```bash
  cd external/openzfs
  git checkout zfs-2.3.6
  git clean -fd
  cd ../..
  ./scripts/apply-openzfs-patches.sh
  ```

### Build fails after applying patches

**Symptom**: Compilation errors

**Causes**:
- Include paths incorrect
- Object definitions incomplete
- Type mismatches

**Solutions**:
1. Check `bsd/sys/cddl/openzfs_sources.mk` - include paths correct?
2. Verify `Makefile` - OpenZFS objects included?
3. See `BUILD_INSTRUCTIONS.md` for detailed troubleshooting

### Submodule shows as modified

**Symptom**: `git status` shows `external/openzfs` as modified

**Cause**: Patches were applied (intentional)

**Solution**: This is expected! Patches modify the submodule at build time.

Don't commit the modified submodule. The patch application happens locally and is not committed.

---

## Integration with Build System

### Automatic Application

Patches are applied automatically when needed:

**During setup**:
```bash
./scripts/setup-zfs.sh  # Applies patches
```

**During build**:
```bash
./scripts/build-osv-zfs.sh  # Checks and applies if needed
```

**Direct build**:
```bash
./scripts/build arch=aarch64 fs=zfs image=native-example
# Note: Assumes patches already applied via setup-zfs.sh
```

### Manual Application

```bash
./scripts/apply-openzfs-patches.sh
```

---

## Comparison: Before vs After

### Before (Submodule Commits)

```
external/openzfs/
├── .git/
│   └── HEAD -> 7eb99f68a (our custom commit)
├── module/os/osv/  (OSv platform code)
└── include/os/osv/ (OSv platform headers)
```

**Issues**:
- Submodule diverged from upstream
- Hard to update OpenZFS
- Changes mixed with upstream commits
- Difficult to review OSv-specific changes

### After (Patch-Based)

```
external/openzfs/
├── .git/
│   └── HEAD -> c840612ee (pristine zfs-2.3.6)
└── (clean upstream code)

patches/openzfs/
├── 0001-*.patch
├── 0002-*.patch
├── 0003-*.patch
├── 0004-*.patch
└── README.md
```

**Benefits**:
- ✓ Submodule stays pristine
- ✓ Easy to update OpenZFS (just rebase patches)
- ✓ Clear separation of OSv changes
- ✓ Standard patch format for review
- ✓ Portable across repositories

---

## References

- **Patch Documentation**: `patches/openzfs/README.md`
- **Setup Script**: `scripts/setup-zfs.sh`
- **Apply Script**: `scripts/apply-openzfs-patches.sh`
- **Build Guide**: `BUILD_INSTRUCTIONS.md`
- **Linux Handoff**: `RESUME_ON_LINUX.md`

---

## Summary

**Approach**: Patch-based integration (standard in kernel development)
**Patches**: 4 files, ~16,700 lines total
**Base**: OpenZFS 2.3.6 (zfs-2.3.6 tag)
**Automation**: Scripts handle everything automatically

**To build**:
```bash
./scripts/setup-zfs.sh              # One-time setup
./scripts/build-osv-zfs.sh aarch64  # Build
```

**That's it!** The patch system makes OpenZFS integration clean, maintainable, and easy to update.
