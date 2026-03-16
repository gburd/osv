# ZFS Automatic Pool Upgrade

## Overview

OSv now includes automatic ZFS pool upgrade functionality. When enabled (default), OSv will automatically upgrade legacy ZFS pools to OpenZFS 2.3.6 format (version 5000 with feature flags) on first mount.

## Features

- **Automatic Detection**: Detects legacy pool versions (1-28) and automatically upgrades to version 5000
- **Safety Checks**: Performs comprehensive safety validation before upgrade
- **Non-Destructive**: Pool remains functional if upgrade fails
- **Configurable**: Can be disabled via boot option
- **Logging**: Detailed logging of upgrade process and any issues

## Usage

### Default Behavior (Auto-Upgrade Enabled)

By default, auto-upgrade is **enabled**. Simply boot OSv with an existing ZFS image:

```bash
./scripts/run.py
```

On first boot, OSv will:
1. Detect the pool version
2. Check if upgrade is needed
3. Perform safety checks
4. Automatically upgrade the pool to version 5000
5. Log the upgrade process

### Disabling Auto-Upgrade

To disable automatic pool upgrade, use the `--no-zfs-auto-upgrade` boot option:

```bash
./scripts/run.py -e '--no-zfs-auto-upgrade /your_app'
```

This is useful for:
- Testing with legacy pools
- Maintaining compatibility with older systems
- Manual upgrade control

### Manual Pool Upgrade

If auto-upgrade is disabled, you can manually upgrade pools from within OSv:

```bash
# Check current pool version
zpool get version mypool

# Upgrade specific pool to version 5000
zpool upgrade -V 5000 mypool

# Upgrade pool and enable all supported features
zpool upgrade mypool

# List all pools and their versions
zpool upgrade
```

## Safety Mechanisms

The auto-upgrade feature includes multiple safety checks:

### 1. Read-Only Pool Detection
- Pools mounted read-only are **not** upgraded
- Prevents upgrade attempts on immutable storage

### 2. Pool State Validation
- Only upgrades pools in ACTIVE state
- Skips pools that are degraded or unavailable

### 3. Free Space Check
- Requires at least 1% free space in the pool
- Prevents upgrade on nearly-full pools

### 4. Version Support Validation
- Verifies target version (5000) is supported
- Ensures compatibility with OSv's ZFS implementation

### 5. Non-Destructive Failure
- If upgrade fails, pool remains at current version
- Original functionality preserved
- Clear error messages logged

## Upgrade Process

### What Gets Upgraded

When upgrading from legacy versions (1-28) to version 5000:

1. **Pool Version Number**: Updated from legacy (e.g., 28) to 5000
2. **Feature Flags System**: Enabled for the pool
3. **Metadata Structures**: On-disk format updated to support features
4. **Uberblock**: Pool's root metadata block updated

### What Doesn't Change

- **Data Blocks**: Your actual file data is unchanged
- **Filesystem Datasets**: Dataset names and mountpoints preserved
- **Snapshots**: All snapshots remain intact
- **Properties**: Pool and dataset properties preserved

### Upgrade is One-Way

**Important**: Pool upgrades are **irreversible** without restoring from backup.

- Old systems (FreeBSD 9.1, old OSv versions) **cannot** import upgraded pools
- Always backup critical data before upgrading
- Test the upgrade on a copy first if possible

## Compatibility

### Supported Pool Versions

- **Legacy Versions**: 1 through 28 (pre-feature-flags)
- **Target Version**: 5000 (OpenZFS feature flags era)
- **All Supported**: 1-28 and 5000 are supported

### Feature Flags

After upgrading to version 5000, the pool uses OpenZFS feature flags:

**Key Features Available** (43 total in OpenZFS 2.3.6):
- `async_destroy` - Asynchronous dataset destruction
- `lz4_compress` - LZ4 compression algorithm
- `multi_vdev_crash_dump` - Improved crash dump support
- `encryption` - Native ZFS encryption
- `block_cloning` - Fast file cloning
- `blake3` - BLAKE3 checksum algorithm
- `raidz_expansion` - RAIDZ expansion capability
- ... and 36 more

Features are **disabled by default** and activate automatically when used.

## Logging and Monitoring

### Boot-Time Logging

Auto-upgrade logs are visible during boot:

```
[ZFS] Pool 'mypool' detected at version 28, auto-upgrading enabled
[ZFS] Upgrading pool 'mypool' from version 28 to 5000
[ZFS] Pool 'mypool' upgraded successfully to version 5000
```

### Checking Pool Version

After boot, verify the upgrade:

```bash
# Get pool version
zpool get version mypool

# Should show:
# mypool  version  5000  default

# List enabled features
zpool get all mypool | grep feature
```

### Upgrade History

ZFS logs all upgrades to pool history:

```bash
# View pool history (if zpool history command is available)
zpool history mypool | grep upgrade
```

## Testing

### Test Scenarios

#### 1. Upgrade Legacy Pool

```bash
# Create a test image with old ZFS pool (if available)
# Or use existing FreeBSD 9.1 ZFS image

# Boot with auto-upgrade (default)
./scripts/run.py --disk old-pool.img

# Verify upgrade in logs
# Check with: zpool get version testpool
```

#### 2. Fresh Pool (No Upgrade Needed)

```bash
# Create pool with current OSv
./scripts/run.py -e 'zpool create testpool /dev/vblk0'

# Reboot - should see "Pool is up-to-date" message
./scripts/run.py
```

#### 3. Disabled Auto-Upgrade

```bash
# Boot with auto-upgrade disabled
./scripts/run.py -e '--no-zfs-auto-upgrade /hello'

# Pool remains at current version
# Can manually upgrade if desired
```

#### 4. Read-Only Pool

```bash
# Import pool read-only
./scripts/run.py -e 'zpool import -o readonly=on testpool'

# Auto-upgrade should skip (logged)
```

## Troubleshooting

### Upgrade Failed

If automatic upgrade fails:

```
[ZFS] Pool upgrade failed for 'mypool': error 28
[ZFS] Pool can still be used at current version 28
[ZFS] To disable auto-upgrade, use --no-zfs-auto-upgrade boot option
```

**Resolution**:
1. Pool remains functional at current version
2. Check pool health: `zpool status`
3. Verify free space: `zpool list`
4. Try manual upgrade: `zpool upgrade mypool`
5. If persistent, disable auto-upgrade and investigate

### Pool Won't Import

If pool won't import after upgrade:

1. **Version mismatch**: Ensure OSv has OpenZFS 2.3.6
2. **Corruption**: Run `zpool scrub` from working system
3. **Missing features**: Check for unsupported feature flags

### Performance Issues After Upgrade

New features may change I/O patterns:

1. **Disable auto-activated features** if problematic
2. **Run scrub**: `zpool scrub mypool` to rebuild metadata
3. **Check fragmentation**: `zpool list -v mypool`

## Implementation Details

### Architecture

```
Boot Process
    ↓
ZFS Driver Init (libsolaris.so loaded)
    ↓
Dataset Mount (zfsvfs_create)
    ↓
Extract Pool Name from Dataset
    ↓
zfs_post_import_hook(poolname) ← Auto-upgrade hook
    ↓
Check opt_zfs_auto_upgrade flag
    ↓
pool_needs_upgrade() - Check version
    ↓
check_upgrade_safety() - Safety checks
    ↓
auto_upgrade_pool() - spa_prop_set(ZPOOL_PROP_VERSION)
    ↓
Pool Upgraded (or skipped)
```

### Key Files

- `/loader.cc` - Boot option parsing (`opt_zfs_auto_upgrade`)
- `/fs/zfs/zfs_auto_upgrade.cc` - Auto-upgrade implementation
- `/fs/zfs/zfs_auto_upgrade.hh` - Header with hook declaration
- `/external/openzfs/module/os/osv/zfs/zfs_vfsops.c` - Hook integration
- `/Makefile` - Build system integration

### APIs Used

- `spa_open()` / `spa_close()` - Open/close pool
- `spa_version()` - Get current pool version
- `spa_writeable()` - Check if pool is writable
- `spa_prop_set()` - Set pool properties (including version)
- `spa_state()` - Get pool state
- `spa_get_space()` - Get pool space information

## Best Practices

### Before Production Use

1. **Test on Copy**: Test upgrade on a copy of your pool first
2. **Backup Data**: Always have recent backups before upgrading
3. **Verify Version**: Confirm OSv supports version 5000
4. **Check Space**: Ensure adequate free space (>5% recommended)
5. **Test Restore**: Verify backup restore procedure works

### For Development

1. **Use --no-zfs-auto-upgrade**: Disable for testing with legacy pools
2. **Manual Control**: Upgrade manually when ready
3. **Monitor Logs**: Check debug messages for upgrade status
4. **Version Testing**: Test with various pool versions

### For Production

1. **Enable Auto-Upgrade**: Use default enabled setting
2. **Monitor First Boot**: Watch logs on first boot after OSv update
3. **Verify Success**: Check pool version after upgrade
4. **Run Scrub**: Schedule scrub after upgrade
5. **Document Changes**: Record pool version changes

## FAQ

### Q: Will my data be lost during upgrade?

**A**: No. The upgrade only modifies metadata structures, not your data blocks. However, always maintain backups as a best practice.

### Q: Can I downgrade after upgrading?

**A**: No. Pool upgrades are one-way. The only way to "downgrade" is to restore from a backup taken before the upgrade.

### Q: What if I need to use the pool on an old system?

**A**: Disable auto-upgrade (`--no-zfs-auto-upgrade`) to keep the pool at its current version. Or export data before upgrading.

### Q: Does upgrade affect performance?

**A**: The upgrade itself is fast (<1 second). Performance after upgrade should be similar or better due to improved features.

### Q: What if upgrade is interrupted?

**A**: ZFS uses transactions. If the upgrade transaction doesn't complete, the pool remains at the old version. Your data is safe.

### Q: Can I upgrade specific pools only?

**A**: Currently, auto-upgrade applies to all pools. Use `--no-zfs-auto-upgrade` and manually upgrade specific pools with `zpool upgrade`.

### Q: What features are enabled after upgrade?

**A**: Upgrading to version 5000 enables the feature flags system. Individual features activate automatically when first used (e.g., lz4_compress activates when you create a compressed dataset).

## Related Documentation

- [ZFS Wiki](https://github.com/cloudius-systems/osv/wiki/ZFS)
- [OpenZFS Documentation](https://openzfs.org/wiki/Features)
- [Pool Upgrade](https://openzfs.github.io/openzfs-docs/man/8/zpool-upgrade.8.html)
- [OSv Storage Architecture](docs/storage-architecture.md)

## Version History

- **OSv 0.XX** (2026-03): Initial implementation of automatic pool upgrade
- **Target**: OpenZFS 2.3.6 (version 5000)
- **Supported Legacy Versions**: ZFS pool versions 1-28
