# OSv Storage Integration Project - Current Status

**Date:** 2026-03-05
**Team:** osv-storage-integration (2 specialists)
**Goal:** Modernize OSv storage with OpenZFS 2.3.6 and Crucible integration

## Executive Summary

**Overall Progress:** ~40% complete (ZFS: ~70%, Crucible: ~15%)

Both major components have substantial foundation work complete:
- ✅ ZFS: Platform layer implemented, ready for build testing
- ✅ Crucible: Rust FFI infrastructure ready, needs driver implementation

**Next Critical Path:**
1. Integrate OpenZFS into OSv build system (update Makefile)
2. Test ZFS build and functionality
3. Implement Crucible block device driver
4. Generate patch series for both components

---

## Component 1: ZFS Update to OpenZFS

### Completed Work ✅

#### Task #1: Extract and Document OSv Modifications (COMPLETE)
**Location:** `bsd/sys/cddl/osv-patches/`
**Deliverables:**
- `manifest.json` - Inventory of 22 modified files with metadata
- `INTEGRATION.md` - Architecture documentation:
  - Constructor pattern (`zfs_initialize()`)
  - 4 callback registration mechanisms
  - VFS operation override system
  - Memory locking annotation
- `README.md` - Patch system overview
- `patches/001-021` - Individual patch documentation files

**Key Findings:**
- 22 files with `__OSV__` guards documented
- vdev_disk.c is complete rewrite for OSv bio layer
- ARC integrates with OSv memory management via vm_throttling_needed()
- Manual znode reference counting (OSv lacks vnode refcounting)
- Many features stubbed: snapshots, zvol, send/recv, jail, quotas

#### Task #2: Add OpenZFS as Git Submodule (COMPLETE)
**Location:** `external/openzfs/`
**Status:** Submodule added to .gitmodules, pointing to https://github.com/openzfs/zfs.git
**Current Version:** Development branch (zfs-2.4.99-409)
**Target Version:** Should be set to latest stable (zfs-2.3.6 or newer)

**Key Discovery:** OpenZFS 2.x has platform-split architecture with `module/os/<platform>/` directories. This enables clean OSv platform without polluting common code with `__OSV__` guards.

**Deliverable:**
- `bsd/sys/cddl/OPENZFS_MAPPING.md` - Comprehensive path mapping:
  - Maps all 22 old files to new OpenZFS locations
  - Documents API changes (ABD, platform split)
  - Provides porting strategy notes

#### Task #3: Port OSv Modifications to OpenZFS (COMPLETE)
**Location:** `external/openzfs/module/os/osv/`

**Platform Layer (14 source files):**
```
external/openzfs/module/os/osv/zfs/
├── arc_os.c (2.7k)           # Memory management via vm_throttling_needed()
├── dmu_os.c (425)            # Stubs (no vm_page_t needed)
├── event_os.c (244)          # Stubs (no kqueue)
├── kmod_core.c (296)         # Stubs (no kernel module support)
├── spa_os.c (4.0k)           # Root pool discovery via vdev_disk_read_rootlabel()
├── sysctl_os.c (279)         # Stubs (no sysctl)
├── vdev_disk.c (8.0k)        # **CRITICAL** Complete rewrite with ABD support
├── vdev_label_os.c (1.4k)    # vdev label operations
├── zfs_initialize_osv.c (3.2k)  # Constructor pattern, callback registration
├── zfs_ioctl_os.c (929)      # Skeleton (needs implementation)
├── zfs_vfsops.c (1.9k)       # VFS operations integration
├── zfs_vnops_os.c            # Skeleton (needs implementation)
├── zfs_znode_os.c            # Skeleton (needs implementation)
└── zvol_os.c (1.3k)          # All zvol functions stubbed (ENOTSUP)
```

**Headers (3 files):**
```
external/openzfs/include/os/osv/zfs/sys/
├── arc_os.h                  # Empty (no OS-specific ARC params)
├── zfs_context_os.h          # TSD macros, ZFS_LOG, CPU_SEQID
└── zfs_znode_impl.h          # z_ref_cnt field, ZTOV/VTOZ macros
```

**Compatibility Shim:**
- `bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c` - Bridges OpenZFS expectations with OSv kernel

**Key Architectural Decisions:**
1. **ABD Integration:** vdev_disk uses abd_borrow_buf()/abd_return_buf_copy() for bio I/O
2. **Clean Platform Separation:** All OSv code in module/os/osv/, zero __OSV__ guards in common code
3. **Constructor Pattern Preserved:** zfs_initialize_osv.c maintains same callback architecture
4. **io_start Returns Void:** Adapted from old `return ZIO_PIPELINE_STOP` to new void + zio_interrupt()

#### Task #4: Build System Integration (COMPLETE)
**Location:** `bsd/sys/cddl/openzfs_sources.mk`

**Deliverable:** Makefile fragment defining:
- **openzfs-zfs** (~100 objects) - Platform-independent ZFS code
- **openzfs-zcommon** (8 objects) - Property/utility code
- **openzfs-osv** (14 objects) - OSv platform code
- **openzfs-compat** (1 object) - Compatibility shim
- **OPENZFS_INCLUDES** - Include path flags
- **OPENZFS_CFLAGS** - Compilation flags

**Integration Ready:** Instructions provided to update main Makefile (lines 732-857)

#### Task #5: Update Automation Script (COMPLETE)
**Location:** `scripts/update-zfs.sh`

**Features:**
- Detect latest stable OpenZFS version automatically
- Update to specific version via command line
- Preserve OSv platform code during updates
- Verify platform file completeness
- Check API compatibility (vdev_ops_t, ABD, ARC)
- Generate detailed update report
- Status and check-only modes

**Usage:**
```bash
./scripts/update-zfs.sh              # Update to latest stable
./scripts/update-zfs.sh zfs-2.3.7    # Update to specific version
./scripts/update-zfs.sh --status     # Show current state
./scripts/update-zfs.sh --check      # Check for updates
```

### Remaining Work ⏳

#### Task #6: Test and Validate ZFS Update (NOT STARTED)
**Estimated:** 2-3 weeks

**Requirements:**
1. **Apply Makefile Integration:**
   - Edit main Makefile (lines 732-857) to include openzfs_sources.mk
   - Update CFLAGS with OPENZFS_INCLUDES
   - Replace zfs_initialize.o with zfs_initialize_osv.o

2. **Fix Build Issues:**
   - Resolve any compilation errors
   - Complete skeleton files (zfs_ioctl_os.c, zfs_vnops_os.c, zfs_znode_os.c)
   - Update include paths and dependencies

3. **Functional Testing:**
   - Mount/unmount operations
   - Pool creation and import
   - Read/write I/O operations
   - ARC memory management under pressure

4. **Performance Testing:**
   - Throughput benchmarks (sequential, random)
   - ARC hit ratio and effectiveness
   - Boot time impact
   - Memory usage

5. **Stability Testing:**
   - 24+ hour continuous I/O workload
   - Crash recovery testing
   - Power-loss simulation

**Blockers:**
- Makefile integration not yet applied
- Skeleton platform files need completion
- Test infrastructure may need updates for OpenZFS 2.x

#### Task #17: Generate git format-patch Series (NOT STARTED)
**Estimated:** 1 week (after testing complete)

**Requirements:**
1. **Clean Commit History:**
   - Commit 1: Add OpenZFS submodule
   - Commit 2: Create OSv platform directory structure
   - Commit 3: Implement OSv platform files
   - Commit 4: Update build system
   - Commit 5: Add update automation script
   - Commit 6: Update documentation

2. **Cover Letter (0000-cover-letter.patch):**
   - Summary of changes
   - Testing performed (functional, performance, stability)
   - Performance comparison (old vs new ZFS)
   - Migration notes for users
   - Known limitations

3. **Quality Checks:**
   - Each commit builds successfully
   - Each commit passes existing tests
   - Commit messages follow OSv format
   - No extraneous changes included

**Output:** `patches/zfs-update/` directory ready for mailing list submission

---

## Component 2: Crucible Distributed Block Storage

### Completed Work ✅

#### Task #7: Rust Build Infrastructure (COMPLETE)
**Location:** `rust/`

**Workspace Structure (9 files):**
```
rust/
├── Cargo.toml                     # Workspace root
├── .cargo/config.toml             # Default target: x86_64-unknown-none
├── osv-sys/                       # FFI bindings to OSv
│   ├── Cargo.toml                 # Bindgen dependency
│   ├── build.rs                   # Bindgen configuration
│   ├── wrapper.h                  # C header declarations (device, bio, uio types)
│   ├── osv_bio_accessors.cc       # C++ accessors with extern "C" linkage
│   └── src/lib.rs                 # #![no_std] FFI bindings
└── crucible-osv/                  # Crucible integration
    ├── Cargo.toml                 # staticlib crate, kernel feature
    └── src/lib.rs                 # Stub FFI entry points (return ENODEV)
```

**Build System Integration:**
- **Makefile** (lines 988-990, 2579-2616):
  - Rust build rules for cargo
  - Conditional compilation via conf_drivers_crucible
  - Linker integration via conf_linker_extra_options
  - Clean targets
- **conf/profiles/x64/base.mk:**
  - Added conf_drivers_crucible?=0 (default off)

**Diagnostics Fixed:**
- Typedef redefinition (daddr_t) - Added guards
- Unknown type _Bool - Added stdbool.h
- C++ compilation of bio accessors - Renamed .c to .cc, added extern "C"
- priv keyword conflict - Renamed to private_data

**Build Status:**
- All Rust files pass rustfmt --check
- Makefile parses without syntax errors
- Gated behind conf_drivers_crucible=1 (no impact on default builds)

### Remaining Work ⏳

#### Task #8: Manage Crucible Dependencies (NOT STARTED)
**Estimated:** 2-3 weeks
**Complexity:** HIGH - Most complex Crucible task

**Requirements:**
1. **Fork Crucible Repository:**
   - Create 'osv' branch at https://github.com/oxidecomputer/crucible
   - Strip unnecessary dependencies (keep upstairs client + protocol only)

2. **Replace Async Runtime:**
   - Crucible expects tokio async runtime
   - Need synchronous wrappers using OSv socket API:
     ```rust
     pub struct CrucibleConnection {
         fd: i32,
         target: SocketAddr,
     }

     impl CrucibleConnection {
         pub fn send_sync(&self, data: &[u8]) -> Result<()>
         pub fn recv_sync(&self, buf: &mut [u8]) -> Result<usize>
     }
     ```

3. **Configure for Kernel Mode:**
   - crucible-osv/Cargo.toml: no_std, minimal deps
   - Remove: tokio, async-trait, futures
   - Keep: serde, bincode for protocol

**Blockers:**
- Requires deep knowledge of Crucible architecture
- Async-to-sync conversion is non-trivial
- Network stack integration may reveal OSv limitations

#### Task #9: Implement Crucible Block Device Driver (NOT STARTED)
**Estimated:** 2-3 weeks
**Depends On:** Task #8

**Requirements:**
1. **C++ Driver Wrapper** (`drivers/crucible-blk.cc`, ~300 lines):
   - Pattern from virtio-blk.cc
   - Implements: devops (open, close, ioctl, strategy)
   - FFI calls to Rust for read/write/flush

2. **Rust Driver Implementation** (`rust/crucible-osv/src/driver.rs`):
   - CrucibleDevice struct
   - FFI exports: crucible_create, crucible_read, crucible_write, crucible_flush
   - Upstairs client management

3. **Device Initialization:**
   - Parse kernel cmdline: --crucible=<target1>,<target2>,<target3>
   - Create /dev/crucible0 block device
   - Auto-detect partitions

**Deliverable:** `/dev/crucible0` usable by any filesystem (ZFS, EXT4)

#### Task #10: Create CLI Tools and Configuration (NOT STARTED)
**Estimated:** 1 week
**Depends On:** Task #9

**Requirements:**
1. **Kernel Command-Line Parsing:**
   - --crucible=<targets>
   - --crucible-uuid=<uuid>
   - --crucible-key=<key>

2. **User-Space Utility** (`usr/bin/crucible-admin.cc`, ~200 lines):
   ```bash
   crucible-admin status
   crucible-admin snapshot create <name>
   crucible-admin snapshot list
   crucible-admin snapshot rollback <name>
   ```

3. **Ioctl Interface:**
   ```c
   #define CRUCIBLE_GET_STATUS    _IOR('C', 0, struct crucible_status)
   #define CRUCIBLE_SNAPSHOT      _IOW('C', 1, struct crucible_snapshot_req)
   #define CRUCIBLE_LIST_SNAPS    _IOR('C', 2, struct crucible_snapshot_list)
   #define CRUCIBLE_ROLLBACK      _IOW('C', 3, struct crucible_snapshot_req)
   ```

#### Task #11: Implement Snapshot Features (NOT STARTED)
**Estimated:** 1-2 weeks
**Depends On:** Task #10

**Requirements:**
1. **Rust Implementation:**
   - CrucibleDevice::snapshot(&mut self, name: &str) -> Result<Uuid>
   - CrucibleDevice::list_snapshots(&self) -> Vec<SnapshotInfo>
   - CrucibleDevice::rollback(&mut self, snapshot_id: Uuid) -> Result<()>

2. **CLI Integration:**
   - Snapshot creation/deletion
   - Snapshot listing with metadata
   - Rollback operations

#### Task #12: Build System Integration (NOT STARTED)
**Estimated:** 1 week
**Depends On:** Tasks #9, #10, #11

**Requirements:**
1. **Driver Profile** (`conf/profiles/x64/crucible.mk`):
   ```makefile
   conf_drivers_crucible=1
   ```

2. **Makefile Updates:**
   - Conditional driver compilation
   - Link rust/libcrucible_osv.a

3. **Build Command:**
   ```bash
   ./scripts/build conf_drivers_profile=crucible image=myapp
   ```

#### Task #13: Test and Validate Crucible (NOT STARTED)
**Estimated:** 2-3 weeks
**Depends On:** All previous Crucible tasks

**Requirements:**
1. **Driver Load Test:**
   - Verify /dev/crucible0 appears
   - Check device properties (size, max_io_size)

2. **Block I/O Testing:**
   - dd write/read tests
   - Verify data integrity (checksum comparison)

3. **Filesystem Integration:**
   - ZFS on Crucible: `zpool create pool /dev/crucible0`
   - EXT4 on Crucible: `mount -t ext /dev/crucible0 /data`
   - I/O workload testing (fio)

4. **Snapshot Testing:**
   - Create snapshot, modify data, rollback, verify

5. **Resilience Testing:**
   - One downstairs offline (degraded mode)
   - Automatic rebuild after connectivity restore

#### Task #18: Generate git format-patch Series (NOT STARTED)
**Estimated:** 1 week (after testing complete)

**Requirements:**
1. **Clean Commit History:**
   - Commit 1: Add Rust build infrastructure
   - Commit 2: Add osv-sys FFI bindings
   - Commit 3: Add crucible-osv crate
   - Commit 4: Implement Crucible block device driver
   - Commit 5: Add crucible-admin CLI
   - Commit 6: Implement snapshot features
   - Commit 7: Build system integration
   - Commit 8: Add documentation

2. **Cover Letter:**
   - Architecture overview (upstairs in kernel, downstairs external)
   - Usage examples (with ZFS, EXT4)
   - Testing performed
   - Performance characteristics
   - Known limitations

**Output:** `patches/crucible/` directory ready for mailing list submission

---

## Component 3: Documentation

### Task #14: Create Comprehensive Documentation (NOT STARTED)
**Estimated:** 1 week
**Depends On:** Tasks #6 (ZFS), #13 (Crucible), #17, #18

**Requirements:**
1. **Update README.md:**
   - Document OpenZFS integration
   - Document Crucible integration
   - Build instructions for each configuration
   - Licensing notes (CDDL for ZFS, MPL for Crucible)

2. **Create docs/filesystems.md:**
   - Comprehensive ZFS guide (mounting, commands, tuning, snapshots)
   - Update EXT4 documentation

3. **Create docs/crucible.md:**
   - Architecture overview
   - Setup instructions (downstairs servers)
   - Usage examples (with ZFS, EXT4)
   - Snapshot management
   - Performance tuning
   - Troubleshooting

4. **Create docs/developer/updating-zfs.md:**
   - Update process documentation
   - Patch conflict resolution
   - Testing procedures

5. **Update tests/README.md:**
   - Document new test suites

---

## Timeline and Resource Estimates

### Completed Work: ~8-9 weeks equivalent
- ZFS tasks 1-5: 6-7 weeks
- Crucible task 7: 2 weeks

### Remaining Work: ~12-16 weeks
- ZFS completion (tasks 6, 17): 3-4 weeks
- Crucible completion (tasks 8-13, 18): 8-11 weeks
- Documentation (task 14): 1 week

**Total Project:** ~20-25 weeks with serial execution
**Possible with Parallelization:** ~14-18 weeks (ZFS and Crucible tracks parallel)

### Critical Path
1. **Immediate (1-2 weeks):**
   - Apply ZFS Makefile integration
   - Fix build issues
   - Basic functional testing

2. **Short-term (3-5 weeks):**
   - Complete ZFS validation and testing
   - Begin Crucible dependency work (task #8)
   - Generate ZFS patch series

3. **Medium-term (6-12 weeks):**
   - Complete Crucible driver implementation
   - Integrate and test Crucible with filesystems
   - Complete snapshot features

4. **Final (13-16 weeks):**
   - Generate Crucible patch series
   - Complete documentation
   - Prepare mailing list submissions

---

## Risks and Mitigation

### ZFS Risks
1. **Build failures with OpenZFS 2.x:**
   - Mitigation: OpenZFS platform split architecture makes porting cleaner
   - Fallback: Use OpenZFS 2.2.x (older but more stable API)

2. **Performance regression:**
   - Mitigation: Benchmark before/after, tune ARC parameters
   - Acceptable: Within 10% of old ZFS performance

3. **Stability issues:**
   - Mitigation: Extensive testing (24+ hour workloads)
   - Fallback: Maintain old ZFS branch during transition

### Crucible Risks
1. **Async-to-sync conversion complexity:**
   - Mitigation: Start with minimal synchronous prototype
   - Fallback: Userspace Crucible via NBD (network block device)

2. **Dependency bloat:**
   - Mitigation: Fork Crucible, strip aggressively
   - Fallback: Minimal C implementation of protocol

3. **OSv network stack limitations:**
   - Mitigation: Early testing of socket operations
   - Fallback: Enhanced network stack (separate project)

4. **Performance overhead (Rust/FFI boundary):**
   - Mitigation: Minimize FFI calls, batch operations
   - Acceptable: Within 20% of local block device

---

## Recommendations

### Option 1: Complete ZFS First (Recommended)
**Timeline:** 3-4 weeks
**Deliverable:** Production-ready OpenZFS integration

**Rationale:**
- ZFS is 70% complete, Crucible is 15% complete
- ZFS provides immediate value (modern filesystem with 14 years of improvements)
- Crucible is more experimental and complex
- Can ship ZFS independently while Crucible work continues

**Actions:**
1. Apply Makefile integration
2. Fix build issues
3. Complete platform skeleton files
4. Run comprehensive tests
5. Generate patch series
6. Submit to mailing list

### Option 2: Parallel Completion
**Timeline:** 12-16 weeks
**Deliverable:** Both ZFS and Crucible ready

**Rationale:**
- Maximizes value delivery
- Leverages parallel development

**Actions:**
- ZFS specialist: Tasks 6, 17 (3-4 weeks)
- Crucible specialist: Tasks 8-13, 18 (10-12 weeks)
- Final: Documentation (task 14, 1 week)

### Option 3: Minimum Viable Product
**Timeline:** 1-2 weeks
**Deliverable:** Proof-of-concept patches

**Rationale:**
- Quick feedback from community
- Validates approach before full implementation
- Identifies integration issues early

**Actions:**
1. Apply ZFS Makefile integration (minimal)
2. Fix critical build issues only
3. Create basic Crucible driver stub (shows architecture)
4. Generate RFC-quality patches with "Work in Progress" markers
5. Submit for early feedback

---

## Files Changed (Summary)

### New Files (30+)
```
bsd/sys/cddl/osv-patches/               # ZFS documentation
bsd/sys/cddl/OPENZFS_MAPPING.md
bsd/sys/cddl/openzfs_sources.mk
bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c
external/openzfs/                        # Git submodule
external/openzfs/module/os/osv/zfs/      # 14 platform files
external/openzfs/include/os/osv/zfs/sys/ # 3 headers
rust/                                    # Rust workspace (9 files)
scripts/update-zfs.sh
scripts/save-claude-state.sh
scripts/restore-claude-state.sh
.claude/                                 # Team and task state
```

### Modified Files
```
.gitmodules                              # Added OpenZFS submodule
Makefile                                 # Rust build rules (lines 988-990, 2579-2616)
conf/profiles/x64/base.mk                # Added conf_drivers_crucible
```

### Files Needing Modification (Next Steps)
```
Makefile                                 # Lines 732-857 (ZFS source list replacement)
bootfs.manifest.skel                     # May need updates for ZFS
exported_symbols/osv_libsolaris.so.symbols # Verify symbols after build
```

---

## Team State

**Current Session:** No active specialists (in-process agents from previous session terminated)

**Previous Specialists:**
1. **zfs-specialist** (Claude Opus 4.6)
   - Completed: Tasks 1-5
   - Last worked: Task #5 (update script)
   - Can resume: Task #6 (testing)

2. **crucible-specialist** (Claude Opus 4.6)
   - Completed: Task #7
   - Last worked: Task #8 (dependencies)
   - Can resume: Task #8 (Crucible dependencies)

**To Resume Work:**
- Option A: Spawn new specialists with previous context
- Option B: Work directly (single agent implementation)
- Option C: Hybrid (lead does ZFS, specialists do Crucible)

**Team Files Preserved:** `.claude/teams/`, `.claude/tasks/` (18 task definitions)

---

## Next Actions

**Immediate (Today):**
1. Decide on completion strategy (Option 1, 2, or 3 above)
2. If continuing: Apply ZFS Makefile integration
3. Attempt ZFS build, document any errors

**This Week:**
1. Fix ZFS build issues
2. Basic ZFS functional testing
3. Begin Crucible task #8 or defer Crucible work

**This Month:**
1. Complete ZFS validation
2. Generate ZFS patch series
3. Make progress on Crucible driver

---

## Success Criteria

### ZFS Update (Ready for Submission)
- ✅ Builds successfully with OpenZFS 2.3+
- ✅ Mounts pools created with modern OpenZFS
- ✅ All core features work (no regressions)
- ✅ Performance within 10% of old version
- ✅ Passes 24+ hour stability test
- ✅ Clean git patch series with cover letter
- ✅ Documentation updated

### Crucible Integration (Ready for Submission)
- ✅ `/dev/crucible0` device appears and is usable
- ✅ Any filesystem (ZFS, EXT4) can use Crucible
- ✅ Triple replication functions correctly
- ✅ Handles one downstairs failure gracefully
- ✅ Snapshot operations work
- ✅ Performance within 20% of local block device
- ✅ Clean git patch series with cover letter
- ✅ Documentation updated

---

**Report Generated:** 2026-03-05
**Project Status:** In Progress - Foundations Complete, Integration/Testing Pending
