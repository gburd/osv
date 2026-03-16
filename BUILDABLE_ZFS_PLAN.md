# Getting OSv ZFS Buildable and Testable - Action Plan

**Goal:** Working build on macOS/aarch64 → Test with QEMU → Ensure ZFS pool compatibility

## Current Status

✅ **Completed:**
- Infrastructure (include paths)
- OpenZFS 2.3.6 submodule
- Build system integration
- Platform layer (11/14 files complete)
- zfs_ioctl_os.c implemented (simple, ~95 lines)

⏳ **Remaining:**
- zfs_znode_os.c (~1800 lines needed)
- zfs_vnops_os.c (~4000 lines needed)
- Build environment setup
- QEMU testing

## Immediate Strategy: Get Buildable Fast

Instead of writing 6000 lines of code, let's leverage the existing work more intelligently:

### Option 1: Use FreeBSD Implementation Directly (FASTEST)
**Timeline:** 1 day to buildable

**Approach:**
1. Copy FreeBSD zfs_znode_os.c and zfs_vnops_os.c wholesale
2. Create OSv compatibility shim layer to map FreeBSD APIs → OSv APIs
3. Stub out FreeBSD-specific features (ACLs, extended attrs, etc.)

**Compatibility Layer Needed:**
```c
// In bsd/sys/cddl/compat/opensolaris/freebsd_osv_compat.h
#define vfs_busy(vfs, flags) 0          // OSv: No vfs_busy
#define vfs_unbusy(vfs) do {} while (0)
#define vrecycle(vp) do {} while (0)    // OSv: No vrecycle
#define vn_has_cached_data(vp) 0        // OSv: No mapped reads
// ... more mappings
```

**Benefits:**
- Builds immediately
- Tested code from FreeBSD
- Incremental refinement possible

**Risks:**
- Some FreeBSD assumptions may not hold
- May need debugging of compat layer

### Option 2: Minimal Stub Implementation (SAFEST)
**Timeline:** 2-3 days to buildable

**Approach:**
1. Implement absolute minimum functions
2. Return ENOTSUP for advanced features
3. Core operations only: open, close, read, write, getattr

**What We Implement:**
- z node_alloc/free/init
- Basic vnode attachment
- Simple read/write operations
- Getattr/setattr (basic)

**What We Stub:**
- ACLs, extended attributes
- Mapped reads/writes
- Advanced locking
- Snapshots, clones

**Benefits:**
- Know exactly what we have
- Easy to debug
- Clear path to full implementation

**Risks:**
- Limited functionality initially
- More work to get full features

## Recommended Path: Hybrid Approach

**Phase 1: Copy & Adapt FreeBSD (1-2 days)**
1. Copy FreeBSD zfs_znode_os.c and zfs_vnops_os.c
2. Create compatibility header
3. Comment out/stub incompatible sections
4. Get it to compile

**Phase 2: Test & Fix (2-3 days)**
1. Build with aarch64 toolchain
2. Boot in QEMU
3. Test basic operations
4. Fix compatibility issues as they arise

**Phase 3: ZFS Pool Compatibility (1-2 days)**
1. Verify old ZFS pools can be imported
2. Test automatic feature flag upgrades
3. Document upgrade process

## ZFS Pool Compatibility Strategy

OpenZFS 2.3.6 is backwards compatible with old pools. Key considerations:

### Feature Flags
OpenZFS uses feature flags to enable new features without breaking old pools:
- Old pools: Can be imported read-write immediately
- New features: Disabled until explicitly enabled
- Upgrade: Automatic on first write (if pool supports it)

### Import Process
```
1. OSv boots with new OpenZFS
2. zpool import detects old pool
3. Pool imported with old feature set
4. New features available but not enabled
5. User can upgrade: zpool upgrade poolname
```

### Making It Automatic (OSv-specific)
We can auto-upgrade in zfs_vfsops.c:
```c
int
zfs_mount(struct mount *mp, const char *path, void *data, int flags)
{
    ...
    // After successful mount
    if (spa_version(spa) < SPA_VERSION) {
        kprintf("ZFS: Automatically upgrading pool %s\n", poolname);
        spa_upgrade(spa, SPA_VERSION);
    }
    ...
}
```

### Safety
- Old pools remain readable/writable
- Upgrade only happens on explicit write or auto-upgrade
- No data loss risk (OpenZFS is mature)

## Build Environment: Practical Solution

### Problem
- aarch64 cross-compile toolchain not available
- Network issues prevent package download
- Native macOS build not supported

### Solution 1: Docker (FASTEST)
```bash
# Use pre-built OSv docker image
docker pull osvunikernel/osv-builder
docker run -v $(pwd):/osv -it osvunikernel/osv-builder bash

# Inside container:
cd /osv
./scripts/build arch=aarch64 fs=zfs image=native-example
```

### Solution 2: Vagrant
```bash
# Create Ubuntu VM with toolchain
vagrant init ubuntu/jammy64
vagrant up
vagrant ssh

# Inside VM:
git clone https://github.com/cloudius-systems/osv.git
cd osv
./scripts/setup.py
./scripts/build arch=aarch64 fs=zfs image=native-example
```

### Solution 3: GitHub Actions (AUTOMATED)
Create `.github/workflows/zfs-build.yml`:
```yaml
name: Build ZFS
on: [push]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Setup
        run: ./scripts/setup.py
      - name: Build
        run: ./scripts/build arch=aarch64 fs=zfs image=native-example
      - name: Upload artifact
        uses: actions/upload-artifact@v3
        with:
          name: osv-zfs-image
          path: build/release.aarch64/usr.img
```

## Testing Strategy

### Level 1: Build Verification (1 hour)
```bash
./scripts/build arch=aarch64 fs=zfs image=native-example
# Success = buildable
```

### Level 2: Boot Test (1 hour)
```bash
./scripts/run.py
# Success = boots, kernel doesn't panic
```

### Level 3: Mount Test (2 hours)
```bash
# Inside OSv:
/# zpool create testpool /dev/vblk0
/# zfs create testpool/data
/# mount | grep zfs
/# echo "test" > /data/file.txt
/# cat /data/file.txt
# Success = filesystem works
```

### Level 4: Compatibility Test (2 hours)
```bash
# Create pool with old ZFS (FreeBSD 9.1)
# Boot with new OpenZFS
/# zpool import testpool
/# zpool status
# Success = old pool works with new ZFS
```

### Level 5: Performance Test (1 day)
```bash
# Sequential I/O
dd if=/dev/zero of=/data/test bs=1M count=1024
# Random I/O
fio --name=test --rw=randrw --bs=4k --size=1G --directory=/data
# Success = performance acceptable
```

## Action Items (Prioritized)

**Today (3-4 hours):**
1. ✅ Complete zfs_ioctl_os.c (DONE)
2. Copy FreeBSD zfs_znode_os.c → adapt for OSv (1 hour)
3. Copy FreeBSD zfs_vnops_os.c → adapt for OSv (2 hours)
4. Create compatibility header (30 min)

**Tomorrow (4-6 hours):**
1. Setup Docker/Vagrant build environment (1 hour)
2. Attempt build, fix compilation errors (2-3 hours)
3. Boot test in QEMU (1 hour)
4. Basic mount/unmount test (1 hour)

**Day 3 (4-6 hours):**
1. Test old pool compatibility (2 hours)
2. Implement auto-upgrade if needed (1 hour)
3. Performance testing (2 hours)
4. Fix any issues discovered (1 hour)

**Total: 11-16 hours to fully working and tested ZFS**

## Success Criteria

✅ **Builds successfully** on aarch64
✅ **Boots in QEMU** without kernel panic
✅ **Mounts ZFS filesystem** and basic I/O works
✅ **Imports old ZFS pools** created with FreeBSD 9.1 ZFS
✅ **Automatic upgrade** (or clear upgrade path) documented
✅ **Performance** within 10% of old ZFS

## Next Immediate Step

Copy FreeBSD implementations and create compatibility layer. This gets us to buildable fastest while maintaining code quality.

Let me start with zfs_znode_os.c...
