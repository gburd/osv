# OpenZFS Platform Patches for OSv

This directory contains git format-patch files that add the complete OSv platform layer to OpenZFS 2.3.6.

## Overview

**Base**: OpenZFS 2.3.6 (tag `zfs-2.3.6`, commit `c840612ee`)
**Patches**: 4 patches adding ~16,700 lines of OSv-specific code
**Application**: Automatic via `scripts/apply-openzfs-patches.sh`

## Patch Files

### 0001-Implement-zfs_ioctl_os.c-for-OSv.patch
- Adds `zfs_ioctl_os.c` (95 lines)
- Platform-specific ioctl operations
- VFS reference counting stubs

### 0002-OSv-Implement-zfs_znode_os.c.patch
- Adds `zfs_znode_os.c` (560 lines)
- Znode lifecycle management
- Manual memory allocation (kmem_alloc)
- Manual reference counting (z_ref_cnt)
- Znode hold mechanism for serialization

### 0003-OSv-Implement-minimal-zfs_vnops_os.c.patch
- Adds `zfs_vnops_os.c` (630 lines)
- Vnode operations
- Essential operations implemented: open, close, getattr, access, inactive, reclaim, fsync
- Advanced operations stubbed for incremental implementation

### 0004-OSv-Add-complete-platform-layer-for-OpenZFS-2.3.6.patch
- Adds remaining platform files (~15,400 lines)
- Platform headers: arc_os.h, zfs_context_os.h, zfs_znode_impl.h
- Core implementation: vdev_disk.c, arc_os.c, spa_os.c, vdev_label_os.c
- VFS integration: zfs_initialize_osv.c, zfs_vfsops.c
- Stubs: zvol_os.c, dmu_os.c, event_os.c, kmod_core.c, sysctl_os.c

## How Patches Are Applied

### Automatic Application

The patches are applied automatically during the build process:

1. **During setup**: `./scripts/setup.py` calls `apply-openzfs-patches.sh`
2. **Before build**: Build scripts check and apply if needed
3. **Manual**: `./scripts/apply-openzfs-patches.sh`

### What Gets Added

**Platform Headers** (3 files in `include/os/osv/zfs/sys/`):
- `arc_os.h` - ARC OS-specific definitions
- `zfs_context_os.h` - Platform context (TSD, logging, CPU_SEQID)
- `zfs_znode_impl.h` - Znode implementation details

**Platform Implementation** (14 files in `module/os/osv/zfs/`):
- `vdev_disk.c` (8,000 lines) - Block device integration
- `arc_os.c` (2,700 lines) - ARC memory management
- `spa_os.c` (4,000 lines) - Storage pool support
- `vdev_label_os.c` (1,400 lines) - Device labels
- `zfs_initialize_osv.c` (3,200 lines) - Module initialization
- `zfs_vfsops.c` (1,900 lines) - VFS integration
- `zvol_os.c` (1,300 lines) - Volume stubs
- `dmu_os.c` (425 lines) - DMU stubs
- `event_os.c` (244 lines) - Event stubs
- `kmod_core.c` (296 lines) - Kernel module stubs
- `sysctl_os.c` (279 lines) - Sysctl stubs
- `zfs_ioctl_os.c` (95 lines) - I/O control operations
- `zfs_znode_os.c` (560 lines) - Znode lifecycle
- `zfs_vnops_os.c` (630 lines) - Vnode operations

**Total**: 17 files, ~16,700 lines

## Regenerating Patches

If you modify the OpenZFS platform layer:

```bash
cd external/openzfs

# Make your changes to module/os/osv/ or include/os/osv/

# Commit your changes
git add module/os/osv include/os/osv
git commit -m "OSv: Your change description"

# Regenerate patches
git format-patch zfs-2.3.6 -o ../../patches/openzfs/

# Reset submodule to clean state
git checkout zfs-2.3.6

# Test patch application
cd ../..
./scripts/apply-openzfs-patches.sh
```

## Updating OpenZFS Base Version

To update to a newer OpenZFS version:

1. **Update submodule**:
   ```bash
   cd external/openzfs
   git fetch --tags
   git checkout zfs-2.3.7  # or newer version
   ```

2. **Test patch application**:
   ```bash
   cd ../..
   ./scripts/apply-openzfs-patches.sh
   ```

3. **Fix conflicts** (if any):
   ```bash
   cd external/openzfs
   # Resolve conflicts in each failing patch
   git am --continue  # after fixing each one
   ```

4. **Regenerate patches**:
   ```bash
   git format-patch zfs-2.3.7 -o ../../patches/openzfs/
   ```

5. **Update .gitmodules**:
   ```
   [submodule "external/openzfs"]
       branch = zfs-2.3.7  # update version
   ```

## Architecture

### Platform-Split Model

OpenZFS 2.3.6+ uses a clean platform-split architecture:

```
openzfs/
├── include/
│   └── os/
│       ├── linux/      # Linux platform
│       ├── freebsd/    # FreeBSD platform
│       └── osv/        # OSv platform (our patches)
└── module/
    └── os/
        ├── linux/      # Linux implementation
        ├── freebsd/    # FreeBSD implementation
        └── osv/        # OSv implementation (our patches)
```

**Advantages**:
- No `__OSV__` guards in common code
- Clean separation of concerns
- Easy to update OpenZFS
- Easy to maintain platform code

### Key Adaptations for OSv

**Manual Memory Management**:
- Simple `kmem_alloc()` instead of slab allocators
- No `uma_zone` (FreeBSD) or `kmem_cache` (Linux)

**Manual Reference Counting**:
- `z_ref_cnt` field in znode (OSv lacks vnode refcounting)
- Explicit `zfs_zhold()` / `zfs_zrele()` calls

**ABD Integration**:
- Uses `abd_borrow_buf()` / `abd_return_buf_copy()`
- Matches FreeBSD vdev_geom fallback pattern
- Integrates with OSv bio layer

**Simplified Features**:
- No SMR (Safe Memory Reclamation)
- No extended attributes (xvattr)
- No mandatory file locking
- Simpler vnode model than FreeBSD

## Testing

After applying patches, verify the platform layer:

```bash
# Check files exist
find external/openzfs/module/os/osv -type f
find external/openzfs/include/os/osv -type f

# Count lines
find external/openzfs/module/os/osv -name '*.c' -exec wc -l {} + | tail -1
find external/openzfs/include/os/osv -name '*.h' -exec wc -l {} + | tail -1

# Build OSv with ZFS
./scripts/build arch=aarch64 fs=zfs image=native-example

# Test in QEMU
./scripts/run.py
# Inside OSv:
/# zpool status
/# zfs list
```

## Troubleshooting

### Patches don't apply cleanly

**Cause**: OpenZFS submodule is not at the expected commit

**Solution**:
```bash
cd external/openzfs
git checkout zfs-2.3.6
cd ../..
./scripts/apply-openzfs-patches.sh
```

### "Patches already applied" warning

**Cause**: Platform files already exist in submodule

**Solution**:
- If you want to reapply: Answer 'y' to the prompt
- If patches are already applied: Just continue building

### Build errors after applying patches

**Cause**: Include paths or object definitions may need updating

**Solution**:
1. Check `bsd/sys/cddl/openzfs_sources.mk` for correct include paths
2. Verify `Makefile` includes OpenZFS objects correctly
3. See `BUILD_INSTRUCTIONS.md` for troubleshooting

## References

- **OpenZFS Documentation**: https://openzfs.github.io/openzfs-docs/
- **Platform Integration Guide**: https://github.com/openzfs/zfs/tree/master/module/os
- **FreeBSD ZFS** (closest to OSv): `external/openzfs/module/os/freebsd/`
- **Linux ZFS** (simpler reference): `external/openzfs/module/os/linux/`

## Maintenance

These patches represent the OSv platform layer for OpenZFS 2.3.6. They should be:

- **Updated** when OpenZFS releases new versions
- **Extended** when new ZFS features are needed
- **Maintained** to stay compatible with OSv core changes

For questions or issues, see:
- `BUILD_INSTRUCTIONS.md` - Build troubleshooting
- `RESUME_ON_LINUX.md` - Complete development guide
- OSv mailing list: osv-dev@googlegroups.com
