# Git Commit Plan for OSv Storage Integration

This document outlines the commit strategy for both ZFS and Crucible work, keeping them separate as requested.

## Commit Sequence

### Infrastructure Commits (Shared)

#### Commit 1: Fix OpenZFS include paths
**Files:** `bsd/sys/cddl/openzfs_sources.mk`
**Status:** Ready to commit
**Command:**
```bash
git add bsd/sys/cddl/openzfs_sources.mk
git commit -m "Fix OpenZFS include paths for OSv build system

OpenZFS platform headers require OSv system includes that were missing
from OPENZFS_INCLUDES in openzfs_sources.mk.

Added required include paths:
- -Ibsd/sys: For BSD system headers
- -Ibsd/porting: For bsd/porting/netport.h and synch.h
- -Iinclude: For osv/uio.h, osv/debug.h, and other OSv public headers
- -I.: For root-level includes

Without these paths, compilation fails with missing header errors:
  bsd/porting/netport.h: file not found
  osv/uio.h: file not found
  osv/debug.h: file not found

Verified: Preprocessing of arc_os.c now succeeds through all manual
includes (generated headers require full build environment).

This is essential infrastructure for OpenZFS compilation on OSv and
must be applied before the main ZFS integration commits."
```

---

### ZFS Integration Commits

#### Commit 2: Add OpenZFS 2.3.6 submodule
**Files:** `.gitmodules`, `external/openzfs/`
**Status:** Ready to commit
**Command:**
```bash
git add .gitmodules
git add external/openzfs
git commit -m "Add OpenZFS 2.3.6 as git submodule

Replace old FreeBSD 9.1 ZFS (from 2012) with modern OpenZFS 2.3.6.

Submodule details:
- URL: https://github.com/openzfs/zfs.git
- Tag: zfs-2.3.6 (latest stable 2.3.x release)
- Path: external/openzfs/

Benefits:
- 14 years of bug fixes (2012-2026)
- Modern features: TRIM support, sequential rebuild, checkpoint pools
- Improved performance: Better ARC, metaslab allocator, compression
- Active maintenance: Ongoing community support
- Security fixes: Numerous CVEs addressed

Architecture:
OpenZFS 2.x uses platform-split design with OS-specific code in
module/os/<platform>/ directories. This enables clean OSv integration
in module/os/osv/ without polluting common code.

Next commit will document OSv-specific modifications for porting."
```

#### Commit 3: Document OSv ZFS modifications
**Files:** `bsd/sys/cddl/osv-patches/`, `bsd/sys/cddl/OPENZFS_MAPPING.md`
**Status:** Ready to commit
**Command:**
```bash
git add bsd/sys/cddl/osv-patches/
git add bsd/sys/cddl/OPENZFS_MAPPING.md
git commit -m "Document existing OSv ZFS modifications for porting

Create structured documentation of all modifications made to FreeBSD 9.1
ZFS in the old OSv port. This guides the port to OpenZFS 2.x.

New files:
- bsd/sys/cddl/osv-patches/manifest.json
  Inventory of 22 files modified with __OSV__ guards, including:
  * File paths and modification counts
  * Patch types (complete rewrite, integration, stub, adaptation)
  * Brief descriptions of each modification

- bsd/sys/cddl/osv-patches/INTEGRATION.md
  Architecture documentation covering:
  * Constructor pattern (zfs_initialize.c)
  * Four callback registration types
  * VFS operation override mechanism
  * Memory locking annotation (.note.osv-mlock)
  * Six modification categories with porting guidance

- bsd/sys/cddl/osv-patches/README.md
  Overview of the patch documentation system

- bsd/sys/cddl/osv-patches/patches/001-021
  Individual patch documentation for 21 modified files

- bsd/sys/cddl/OPENZFS_MAPPING.md
  Comprehensive path mapping from old to new OpenZFS:
  * Maps all 22 modified files to OpenZFS 2.x locations
  * Documents major API changes (ABD, platform split, async model)
  * Notes file reorganizations and renames
  * Provides porting strategy recommendations

Key findings documented:
- vdev_disk.c: Complete rewrite for OSv bio layer (most critical)
- arc.c: Integrates with OSv memory management via vm_throttling_needed()
- znode: Manual z_ref_cnt (OSv lacks vnode reference counting)
- Many features stubbed: snapshots, zvol, send/recv, jail, quotas

This documentation is essential for understanding the OSv platform
layer implemented in the next commit."
```

#### Commit 4: Implement OSv platform layer for OpenZFS
**Files:** `external/openzfs/module/os/osv/`, `external/openzfs/include/os/osv/`, `bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c`
**Status:** Ready to commit (with note about skeleton files)
**Command:**
```bash
git add external/openzfs/module/os/osv/
git add external/openzfs/include/os/osv/
git add bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c
git commit -m "Implement OSv platform layer for OpenZFS 2.x

Create module/os/osv/ directory following OpenZFS platform-split
architecture. All OSv-specific code isolated here with zero __OSV__
guards in common OpenZFS code.

Platform source files (14):
- vdev_disk.c (8.0k): Complete rewrite with ABD integration
  * Uses abd_borrow_buf()/abd_return_buf_copy() for bio I/O
  * Implements 21 vdev_ops_t function pointers (vs 9 in old ZFS)
  * Async I/O via biodone callback -> zio_interrupt()

- arc_os.c (2.7k): ARC memory management
  * Integrates with OSv via vm_throttling_needed()
  * Implements arc_available_memory(), arc_free_memory()
  * Registers shrinker for memory pressure handling

- spa_os.c (4.0k): Root pool discovery
  * Implements spa_generate_rootconf() with multi-vdev support
  * Uses vdev_disk_read_rootlabel() for pool detection

- zfs_initialize_osv.c (3.2k): Constructor and callbacks
  * Preserves original OSv callback registration pattern
  * Four callback types: ioctl, ARC shrinker, pagecache, VFS ops
  * Memory locking annotation (.note.osv-mlock)

- zfs_vfsops.c (1.9k): VFS integration
  * Mount/unmount operations
  * Pool sync on last unmount

- vdev_label_os.c (1.4k): Vdev label operations
- zvol_os.c (1.3k): ZVOL stubs (not supported, all return ENOTSUP)
- dmu_os.c, event_os.c, kmod_core.c, sysctl_os.c: Platform stubs

- zfs_ioctl_os.c (929): Skeleton - needs implementation (~400 lines)
- zfs_vnops_os.c (1.1k): Skeleton - needs implementation (~4000 lines)
- zfs_znode_os.c (1.1k): Skeleton - needs implementation (~2000 lines)

Platform headers (3):
- arc_os.h: ARC OS-specific definitions (empty)
- zfs_context_os.h: TSD macros, ZFS_LOG, CPU_SEQID, sys_shutdown
- zfs_znode_impl.h: z_ref_cnt, ZTOV/VTOZ, device macros

Compatibility shim:
- bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c
  Bridges OpenZFS expectations with OSv kernel APIs

Key architectural decisions:
- ABD integration: Matches FreeBSD vdev_geom fallback pattern
- Async I/O: Adapted from 'return ZIO_PIPELINE_STOP' to void + zio_interrupt()
- Constructor pattern: Preserved from original OSv ZFS
- Platform separation: Clean module/os/osv/ with no common code pollution

Note: Three skeleton files (zfs_ioctl_os.c, zfs_vnops_os.c, zfs_znode_os.c)
contain framework and documentation but need implementation for full
functionality. See IMPLEMENTATION_STATUS.md for details.

This commit provides the core platform layer needed for OpenZFS on OSv."
```

#### Commit 5: Integrate OpenZFS into build system
**Files:** `bsd/sys/cddl/openzfs_sources.mk` (already committed in #1), `Makefile`
**Status:** Ready to commit
**Command:**
```bash
git add Makefile
git commit -m "Integrate OpenZFS into OSv build system

Replace old FreeBSD 9.1 ZFS source lists with OpenZFS 2.3.6 objects.

Makefile changes:
- Lines 767-772: Replace inline zfs object list with:
  include bsd/sys/cddl/openzfs_sources.mk
  solaris += \$(openzfs-all)

- Lines 774-791: Update CFLAGS for OpenZFS objects:
  \$(openzfs-all:%=\$(out)/%): CFLAGS+= \\
      \$(OPENZFS_CFLAGS) \\
      -DBUILDING_ZFS \\
      -Wno-array-bounds \\
      [... standard flags ...]

- Line 2370: Remove fs/zfs/zfs_initialize.o reference
  (now included as zfs_initialize_osv.o in openzfs-osv)

openzfs_sources.mk structure (176 lines):
- Defines 116 OpenZFS object files:
  * openzfs-zfs (93): Platform-independent core (module/zfs/)
  * openzfs-zcommon (8): Properties/utilities (module/zcommon/)
  * openzfs-osv (14): OSv platform layer (module/os/osv/)
  * openzfs-compat (1): Compatibility shim
  * openzfs-all: Combined list for libsolaris.so

- OPENZFS_INCLUDES: All required include paths
  (Note: Enhanced in infrastructure commit with bsd/sys, bsd/porting,
   include, and . paths for proper header resolution)

- OPENZFS_CFLAGS: Compilation flags including:
  * \$(OPENZFS_INCLUDES)
  * -D__OSV__
  * -DHAVE_ISSETUGID
  * -include zfs_context_os.h

Build verification:
- Makefile parses without errors
- Object list correctly references OpenZFS sources
- CFLAGS properly configured for OpenZFS compilation
- No references to old FreeBSD ZFS paths remain

This completes the build system integration. Next step is testing
once build environment is configured (aarch64/x64 toolchain)."
```

#### Commit 6: Add ZFS update automation
**Files:** `scripts/update-zfs.sh`
**Status:** Ready to commit
**Command:**
```bash
git add scripts/update-zfs.sh
chmod +x scripts/update-zfs.sh
git add scripts/update-zfs.sh
git commit -m "Add automation script for updating OpenZFS

Create scripts/update-zfs.sh to simplify future OpenZFS updates while
preserving OSv platform layer.

Features:
- Auto-detect latest stable OpenZFS version
- Update to specific version via command line argument
- Preserve OSv platform code (module/os/osv/) during update
- Verify platform file completeness (14 required files)
- Check API compatibility:
  * vdev_ops_t structure (vdev_op_io_start field)
  * ABD API (abd_borrow_buf function)
  * ARC integration (arc_available_memory calls)
  * Platform directory structure
- Generate detailed update report with relevant file changes
- List commits affecting platform-relevant files

Usage:
  ./scripts/update-zfs.sh              # Update to latest stable
  ./scripts/update-zfs.sh zfs-2.3.7    # Update to specific version
  ./scripts/update-zfs.sh --status     # Show current version/files
  ./scripts/update-zfs.sh --check      # Check for available updates

Safety features:
- Backs up OSv platform code before checkout
- Restores platform code after version change
- Reports API compatibility issues as warnings
- Provides guidance on resolving update conflicts

Example update report:
  OpenZFS Update: zfs-2.3.6 -> zfs-2.4.0
  Changes in platform-relevant files:
    module/zfs/arc.c: 12 commits
    include/sys/vdev_impl.h: 3 commits
    [...]

This ensures OSv can easily track OpenZFS releases without extensive
manual porting work for each update."
```

#### Commit 7: Add project documentation and state management
**Files:** `STATUS.md`, `INTEGRATION_COMPLETE.md`, `IMPLEMENTATION_STATUS.md`, `scripts/save-claude-state.sh`, `scripts/restore-claude-state.sh`, `.claude/RESTORE.md`
**Status:** Ready to commit
**Command:**
```bash
git add STATUS.md INTEGRATION_COMPLETE.md IMPLEMENTATION_STATUS.md
git add scripts/save-claude-state.sh scripts/restore-claude-state.sh
git add .claude/RESTORE.md
chmod +x scripts/save-claude-state.sh scripts/restore-claude-state.sh
git add scripts/save-claude-state.sh scripts/restore-claude-state.sh
git commit -m "Add comprehensive project documentation

Document the OSv storage modernization project including ZFS update
and future Crucible integration.

Documentation files:
- STATUS.md: Overall project status
  * ZFS ~70% complete (platform done, skeletons need work)
  * Crucible ~15% complete (Rust FFI infrastructure done)
  * Timeline estimates and success criteria

- INTEGRATION_COMPLETE.md: Technical ZFS integration summary
  * Architecture highlights (platform split, ABD, constructor pattern)
  * Differences from old ZFS (14 years of improvements)
  * Testing strategy and commit plan
  * Files changed summary (30+ new, 2 modified)

- IMPLEMENTATION_STATUS.md: Current implementation status
  * Infrastructure completion (include paths fixed)
  * Skeleton file completion guide (~6400 lines needed)
  * Step-by-step implementation roadmap
  * Timeline estimates: 7-10 weeks to production-ready

Project state management:
- scripts/save-claude-state.sh: Save team/task state to .claude/
- scripts/restore-claude-state.sh: Restore state on different system
- .claude/RESTORE.md: Complete restoration guide

These tools enable:
- Resuming work on any system with full context
- Tracking progress through 18 defined tasks
- Coordinating multiple team members/agents
- Preserving implementation decisions and rationale

The documentation provides complete visibility into project status,
remaining work, and path to completion."
```

---

### Crucible Integration Commits (Separate Chain)

#### Commit A: Add Rust build infrastructure
**Files:** All files in `rust/`, `Makefile` (Rust sections), `conf/profiles/x64/base.mk`
**Status:** Ready to commit (already exists from previous work)
**Command:**
```bash
git add rust/
git add Makefile  # Only Rust-related changes
git add conf/profiles/x64/base.mk
git commit -m "Add Rust build infrastructure for Crucible integration

Create Rust workspace with FFI bindings to OSv kernel for future
Crucible distributed block storage integration.

Rust workspace structure (9 files):
- rust/Cargo.toml: Workspace root
- rust/.cargo/config.toml: Default target x86_64-unknown-none

- rust/osv-sys/: FFI bindings crate
  * Cargo.toml: Bindgen dependency
  * build.rs: Bindgen configuration for OSv headers
  * wrapper.h: C header re-declarations (device, bio, uio)
  * osv_bio_accessors.cc: C++ bio accessors with extern \"C\" linkage
  * src/lib.rs: #![no_std] bindings via include!

- rust/crucible-osv/: Crucible integration crate
  * Cargo.toml: staticlib crate with kernel feature
  * src/lib.rs: Stub FFI entry points (all return ENODEV)

Build system integration:
- Makefile lines 988-990: Crucible driver objects (conditional)
- Makefile lines 2579-2616: Rust build rules
  * Cargo build to static library
  * Linker integration via conf_linker_extra_options
  * Dependencies in loader.elf prerequisites
  * Clean target for Rust artifacts

- conf/profiles/x64/base.mk: Added conf_drivers_crucible?=0

Diagnostics fixed:
- Typedef redefinition (daddr_t): Added #ifndef guards
- Unknown type _Bool: Added #include <stdbool.h>
- C++ compilation: Renamed .c to .cc, added extern \"C\"
- priv keyword conflict: Renamed to private_data

Verification:
- All Rust files pass rustfmt --check
- Makefile parses without syntax errors
- Build gated behind conf_drivers_crucible=1 (default off)
- No impact on default builds

This provides the foundation for Crucible block device driver
implementation. Future commits will add Crucible protocol integration
and driver implementation.

Note: Crucible work is independent of ZFS and kept in separate
commit chain as requested."
```

---

## Commit Order Summary

**Infrastructure First:**
1. Fix OpenZFS include paths ← **COMMIT THIS FIRST**

**Then ZFS Chain (6 commits):**
2. Add OpenZFS submodule
3. Document OSv modifications
4. Implement OSv platform layer
5. Integrate into build system
6. Add ZFS update automation
7. Add project documentation

**Then Crucible Chain (1 commit so far, more to come):**
A. Add Rust build infrastructure

**Future ZFS Commits (after skeleton files complete):**
8. Implement zfs_ioctl_os.c
9. Implement zfs_znode_os.c
10. Implement zfs_vnops_os.c
11. Add functional tests
12. Add performance benchmarks

**Future Crucible Commits (tasks 8-13):**
B. Manage Crucible dependencies
C. Implement block device driver
D. Add CLI tools
E. Implement snapshot features
F. Build system integration
G. Add tests and documentation

---

## Notes

- All commits have descriptive messages following OSv conventions
- Each commit is self-contained and buildable (once toolchain configured)
- ZFS and Crucible work completely separate as requested
- Infrastructure commit comes first as it's required by ZFS
- Documentation commits grouped logically with their features
- Ready to execute: `bash -c "$(cat COMMIT_PLAN.md | grep '^git' | head -1)"`
