# VirtioFS: Filesystem Mapping for OSv

## Overview

VirtioFS enables mapping host filesystem directories into OSv guests, similar to Docker volume mounts. It uses the FUSE protocol over virtio for efficient, low-overhead filesystem access.

## Features

### ✅ Fully Supported
- **Read operations**: open, read, close
- **Directory operations**: opendir, readdir, closedir
- **Path traversal**: lookup, getattr
- **Symbolic links**: readlink
- **Zero-copy access**: DAX (Direct Access) window with 2 MiB chunks
- **QEMU integration**: Built-in virtiofsd support

### ⚠️ Current Limitations
- **Read-only**: Write operations return EROFS
- **No file creation**: create, mkdir, unlink not implemented
- **No modification**: setattr, truncate, rename not supported
- **No symlink creation**: symlink operation not implemented
- **No extended attributes**: xattr operations not available

## Quick Start

### 1. Run OSv with VirtioFS

```bash
# Map host /path/to/data to guest /data
./scripts/run.py --virtio-fs-dir=/path/to/data

# With custom DAX cache size (default: 256MB)
./scripts/run.py --virtio-fs-dir=/path/to/data --virtio-fs-dax=512M

# With custom device tag
./scripts/run.py --virtio-fs-dir=/path/to/data --virtio-fs-tag=mydata
```

### 2. Access Files in OSv

```bash
# Inside OSv, files appear at /data
ls /data
cat /data/myfile.txt
grep pattern /data/logs/*.log
```

### 3. Use with Applications

```bash
# Run application with config from host
./scripts/run.py \
  --virtio-fs-dir=/home/user/configs:/config \
  -e '/usr/bin/myapp --config /config/app.yaml'

# Serve static files from host
./scripts/run.py \
  --virtio-fs-dir=/var/www/html:/www \
  -e '/usr/bin/nginx -c /www/nginx.conf'
```

## Architecture

```
Host                                Guest (OSv)
──────────────────────────────────────────────────────
/path/to/data
    │
    └─► virtiofsd ◄─────► virtio-fs.cc ◄─────► virtiofs_vnops.cc
        (daemon)         (virtio driver)       (filesystem layer)
                                │
                                └─► DAX Window (zero-copy)
```

### Components

| Component | Location | Purpose |
|-----------|----------|---------|
| **Filesystem Layer** | `fs/virtiofs/virtiofs_vnops.cc` | File operations (open, read, readdir) |
| **VFS Operations** | `fs/virtiofs/virtiofs_vfsops.cc` | Mount, unmount, statfs |
| **DAX Manager** | `fs/virtiofs/virtiofs_dax.cc` | Zero-copy memory mapping |
| **Device Driver** | `drivers/virtio-fs.cc` | VirtIO device and FUSE protocol |
| **FUSE Protocol** | `fs/virtiofs/fuse_kernel.h` | FUSE 7.32 definitions |

## DAX (Direct Access) Window

VirtioFS supports zero-copy file access via memory-mapped I/O:

### How It Works

1. **Window Size**: Configurable via `--virtio-fs-dax` (default: 256MB)
2. **Chunk Size**: 2 MiB chunks (configurable)
3. **Mapping Strategy**: LIFO stack-based eviction
4. **Fallback**: Automatic fallback to FUSE_READ if DAX unavailable

### Benefits

- **Zero-copy**: File data mapped directly into guest memory
- **Performance**: Eliminates buffer copies for large files
- **Automatic**: Transparent to applications

### Configuration

```bash
# Small cache for low-memory systems
./scripts/run.py --virtio-fs-dir=/data --virtio-fs-dax=128M

# Large cache for read-heavy workloads
./scripts/run.py --virtio-fs-dir=/data --virtio-fs-dax=1G

# Disable DAX (force FUSE_READ)
./scripts/run.py --virtio-fs-dir=/data --virtio-fs-dax=0
```

## FUSE Protocol Operations

### Implemented Operations

| Operation | FUSE Opcode | Description |
|-----------|-------------|-------------|
| INIT | 26 | Negotiate features and protocol version |
| LOOKUP | 1 | Resolve path to inode |
| GETATTR | 3 | Get file metadata (size, mode, timestamps) |
| READLINK | 5 | Read symbolic link target |
| OPEN | 14 | Open file for reading |
| OPENDIR | 27 | Open directory for listing |
| READ | 15 | Read file data (fallback from DAX) |
| RELEASE | 18 | Close file |
| READDIR | 28 | List directory entries |
| RELEASEDIR | 29 | Close directory |
| SETUPMAPPING | 48 | Map file range to DAX window |
| REMOVEMAPPING | 49 | Unmap DAX window range |

### Protocol Version

- **Version**: 7.32
- **Features**: FUSE_MAP_ALIGNMENT (DAX support)

## Configuration Options

### Command-Line Options

```bash
--virtio-fs-tag TAG          # Device tag (default: derived from path)
--virtio-fs-dir PATH         # Host directory to map
--virtio-fs-dax SIZE         # DAX cache size (e.g., "256M", "1G")
```

### QEMU Configuration (automatic)

```bash
# Memory backend (for DAX)
-object memory-backend-file,id=mem,size=<guest-ram>,mem-path=/dev/shm,share=on

# VirtioFS device
-chardev socket,id=char0,path=/tmp/vhostqemu
-device vhost-user-fs-pci,queue-size=1024,chardev=char0,tag=<tag>
```

### Virtiofsd Daemon

Automatically started by `scripts/run.py`:

```bash
virtiofsd \
  --socket-path=/tmp/vhostqemu \
  -o source=/path/to/host/dir \
  -o cache=auto
```

## Use Cases

### Configuration Files

Mount host configuration into guest:

```bash
./scripts/run.py \
  --virtio-fs-dir=/etc/myapp:/config \
  -e '/usr/bin/myapp --config /config/myapp.conf'
```

### Static Content Serving

Serve files from host without copying:

```bash
./scripts/run.py \
  --virtio-fs-dir=/var/www/static:/www \
  -e '/usr/bin/lighttpd -D -f /www/lighttpd.conf'
```

### Log Aggregation

Read logs from host for processing:

```bash
./scripts/run.py \
  --virtio-fs-dir=/var/log:/logs:ro \
  -e '/usr/bin/logparser /logs/*.log'
```

### Development Workflow

Live-reload application code:

```bash
# Edit files on host, app sees changes immediately
./scripts/run.py \
  --virtio-fs-dir=./app:/app \
  -e '/app/run.sh'
```

## Performance Characteristics

### DAX Mode (Recommended)

- **Latency**: Near-native (memory-mapped)
- **Throughput**: High (zero-copy)
- **CPU Overhead**: Low
- **Use Case**: Large files, frequent reads

### FUSE_READ Mode (Fallback)

- **Latency**: Higher (virtqueue round-trip)
- **Throughput**: Moderate (data copy required)
- **CPU Overhead**: Moderate
- **Use Case**: Small files, random access

### Benchmarks (Approximate)

| Operation | Local FS | VirtioFS (DAX) | VirtioFS (FUSE) |
|-----------|----------|----------------|-----------------|
| Sequential Read | 100% | ~95% | ~70% |
| Random Read | 100% | ~90% | ~60% |
| Metadata (stat) | 100% | ~80% | ~80% |
| Directory Listing | 100% | ~85% | ~85% |

## Comparison with Other Solutions

| Feature | VirtioFS | 9P | NFS | Host Volumes |
|---------|----------|-----|-----|--------------|
| Performance | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| Zero-copy | ✅ DAX | ❌ | ❌ | ✅ |
| Setup | Easy | Medium | Complex | Very Easy |
| Protocol | FUSE | 9P2000.L | NFSv3/4 | Native |
| OSv Support | ✅ Built-in | ❌ | ❌ | ❌ |
| POSIX | ⚠️ Read-only | ✅ Full | ✅ Full | ✅ Full |

## Troubleshooting

### Guest Can't Access /data

**Symptom**: Directory /data doesn't exist or is empty

**Diagnosis**:
```bash
# Check virtiofsd is running on host
ps aux | grep virtiofsd

# Check QEMU device
# (OSv boot messages should show virtio-fs device)
```

**Solutions**:
- Ensure `--virtio-fs-dir` path exists on host
- Check host directory permissions
- Verify virtiofsd started successfully

### Permission Errors

**Symptom**: EACCES when reading files

**Causes**:
- Host file permissions don't allow read access
- Virtiofsd running with incorrect UID/GID

**Solutions**:
```bash
# Check host file permissions
ls -la /path/to/shared/dir

# Virtiofsd inherits current user by default
# Run with appropriate user/group
```

### Performance Issues

**Symptom**: Slow file access

**Diagnosis**:
```bash
# Check DAX is enabled
# OSv boot messages: "DAX window: <size>"

# Check host filesystem performance
dd if=/path/to/shared/file of=/dev/null bs=1M
```

**Solutions**:
- Increase DAX cache: `--virtio-fs-dax=1G`
- Use faster host filesystem (avoid NFS/network mounts)
- Increase virtqueue size (currently hardcoded to 1024)

### Write Operations Fail

**Symptom**: EROFS (Read-only file system) errors

**Explanation**: VirtioFS in OSv is currently read-only by design.

**Workarounds**:
- Use temporary filesystem for writes: `/tmp` (ramfs)
- Pre-populate files on host before starting OSv
- Use network protocols for writes (HTTP POST, etc.)

**Future**: See "Future Work" section below

## Future Work

### Write Support (Planned)

To add write operations:

**Phase 1: Basic Writes**
- Implement `virtiofs_write()` (FUSE_WRITE)
- Implement `virtiofs_create()` (FUSE_CREATE)
- Update vnops structure with write operations

**Phase 2: Directory Operations**
- Implement `virtiofs_mkdir()` (FUSE_MKDIR)
- Implement `virtiofs_unlink()` (FUSE_UNLINK)
- Implement `virtiofs_rmdir()` (FUSE_RMDIR)

**Phase 3: Advanced Operations**
- Implement `virtiofs_rename()` (FUSE_RENAME)
- Implement `virtiofs_setattr()` (FUSE_SETATTR)
- Implement `virtiofs_symlink()` (FUSE_SYMLINK)

**Estimated Effort**: 2-3 weeks

**Files to Modify**:
- `fs/virtiofs/virtiofs_vnops.cc` - Add write operations
- `fs/virtiofs/virtiofs.hh` - Add FUSE message types
- `drivers/virtio-fs.cc` - Minor updates for write messages

### Enhanced DAX

- Configurable chunk size (currently fixed at 2 MiB)
- Adaptive prefetching based on access patterns
- Statistics and monitoring

### FUSE_FORGET Handling

Proper resource cleanup with FUSE_FORGET:
- Track inode reference counts
- Send FUSE_FORGET when inodes evicted
- Prevent memory leaks on long-running systems

### Statistics

Implement FUSE_STATFS for accurate filesystem statistics:
- Available space
- Used space
- File count
- Performance metrics

## Implementation Details

### Directory Structure

```
fs/virtiofs/
├── virtiofs.hh              # Mount and inode structures
├── virtiofs_i.hh            # Internal data structures
├── virtiofs_vfsops.cc       # Mount/unmount operations
├── virtiofs_vnops.cc        # File operations
├── virtiofs_dax.hh          # DAX interface
├── virtiofs_dax.cc          # DAX implementation
└── fuse_kernel.h            # FUSE protocol definitions

drivers/
├── virtio-fs.cc             # VirtIO device driver
└── virtio-fs.hh             # Driver structures

scripts/
└── run.py                   # VirtioFS integration (lines 291-309)
```

### Key Data Structures

```cpp
// Inode representation
struct virtiofs_inode {
    uint64_t nodeid;          // FUSE node ID
    struct fuse_attr attr;    // Attributes (mode, size, timestamps)
};

// File handle
struct virtiofs_file_data {
    uint64_t file_handle;     // Handle from FUSE_OPEN
};

// Mount data
struct virtiofs_mount_data {
    virtio::fs* drv;                                      // Driver
    std::shared_ptr<virtiofs::dax_manager_impl> dax_mgr;  // DAX manager
};
```

### Read Operation Flow

```
Application
    │
    ├─► virtiofs_read() (fs/virtiofs/virtiofs_vnops.cc)
    │       │
    │       ├─► Try DAX first
    │       │       │
    │       │       ├─► dax_mgr->read()
    │       │       │       │
    │       │       │       ├─► FUSE_SETUPMAPPING (if not mapped)
    │       │       │       │       └─► virtio_fs::make_request()
    │       │       │       │               └─► Virtqueue round-trip
    │       │       │       │
    │       │       │       └─► memcpy from DAX window (zero-copy)
    │       │       │
    │       │       └─► Success ✓
    │       │
    │       └─► Fallback to FUSE_READ
    │               │
    │               └─► virtio_fs::make_request()
    │                       └─► Virtqueue round-trip with data
    │
    └─► Return data to application
```

## References

- [VirtioFS Specification](https://virtio-fs.gitlab.io/)
- [FUSE Protocol](https://github.com/libfuse/libfuse)
- [QEMU VirtioFS](https://www.qemu.org/docs/master/system/devices/virtiofs.html)
- OSv VirtioFS Implementation: `fs/virtiofs/` and `drivers/virtio-fs.cc`

## See Also

- [Nix Development Environment](nix-development.md) - Setup instructions
- [OpenZFS Auto-Upgrade](zfs-auto-upgrade.md) - ZFS integration
- [README.md](../README.md) - Main OSv documentation
