# OSv Crucible Driver - Testing Guide

## Build Status: ✅ COMPLETE

All components built successfully:
- **loader.img**: 4.0MB (bootable image)
- **usr.img**: 4.4MB (filesystem image)
- **lzloader.elf**: 4.0MB (compressed loader)
- **Crucible driver**: Integrated (1,327 lines C++)

## Prerequisites

### 1. Crucible Downstairs Servers

You need 3+ Crucible downstairs servers running. Example setup:

```bash
# Start 3 downstairs servers (in separate terminals or as services)
crucible-downstairs run -p 8810 -d /path/to/region1 ...
crucible-downstairs run -p 8820 -d /path/to/region2 ...
crucible-downstairs run -p 8830 -d /path/to/region3 ...
```

Verify they're running:
```bash
ss -tuln | grep -E ':(8810|8820|8830)'
```

### 2. KVM/QEMU Support

OSv requires KVM for optimal performance:
```bash
# Check KVM availability
ls -l /dev/kvm

# If missing, load KVM module
sudo modprobe kvm-intel  # or kvm-amd
```

## Test Scenarios

### Test 1: Basic Crucible Device Detection

**Goal**: Verify Crucible driver initializes and creates `/dev/crucible0`

```bash
./test-crucible-basic.sh
```

**Expected Output**:
```
OSv v0.XX-dev
Booting on processor #0...
[Crucible] Initializing upstairs client...
[Crucible] Connected to localhost:8810
[Crucible] Connected to localhost:8820
[Crucible] Connected to localhost:8830
[Crucible] Device initialized: /dev/crucible0 (10.00 GB)
```

**Test Steps**:
1. OSv boots with Crucible driver
2. Driver connects to 3 downstairs servers
3. Creates `/dev/crucible0` block device
4. Runs basic I/O tests
5. Logs results to `./tmp/crucible-test.log`

### Test 2: Block I/O Operations

The `crucible-basic-test` application performs:

1. **Device Detection**: Verifies `/dev/crucible0` exists
2. **Write Test**: Writes test pattern to blocks
3. **Read Verification**: Reads back and verifies data
4. **Integrity Test**: Random data write/read/compare
5. **Performance**: Basic throughput measurement

**Check Results**:
```bash
cat ./tmp/crucible-test.log
```

### Test 3: ZFS on Crucible (Future)

**Note**: Currently blocked by OpenZFS assembly issues. Use ramfs for now.

Once OpenZFS assembly is fixed:
```bash
./test-crucible.sh  # Full ZFS integration test
```

## Troubleshooting

### Issue: "Connection refused" to downstairs

**Cause**: Downstairs servers not running

**Fix**:
```bash
# Check if downstairs servers are running
ss -tuln | grep 8810

# Start downstairs servers before running test
```

### Issue: KVM not available

**Cause**: KVM module not loaded or no virtualization support

**Fix**:
```bash
# Check CPU virtualization support
grep -E 'vmx|svm' /proc/cpuinfo

# Load KVM module
sudo modprobe kvm-intel  # or kvm-amd

# Run without KVM (slower)
./scripts/run.py --no-kvm --execute='/crucible-test' ...
```

### Issue: OSv doesn't boot

**Cause**: QEMU/KVM configuration issue

**Fix**:
```bash
# Try with verbose output
./scripts/run.py --verbose --execute='/crucible-test' ...

# Check QEMU version
qemu-system-x86_64 --version
```

## Build Commands

### Rebuild OSv with Crucible

```bash
nix develop --command bash -c './scripts/build conf_drivers_crucible=1 fs=ramfs image=crucible-basic-test'
```

### Clean Build

```bash
make clean
nix develop --command bash -c './scripts/build conf_drivers_crucible=1 fs=ramfs image=crucible-basic-test'
```

## Configuration Options

Available kernel command-line options:

- `--crucible=host1:port1,host2:port2,host3:port3` - Downstairs server addresses
- `--crucible-uuid=UUID` - Region UUID
- `--crucible-block-size=N` - Block size (default: 4096)
- `--virtio-fs-dir=hostdir:guestdir` - Map host directory to guest
- `--memsize=SIZE` - Memory size (e.g., 2G)

## Test Matrix

| Test | Status | Description |
|------|--------|-------------|
| Build | ✅ PASS | All images build successfully |
| Driver Load | ⏳ PENDING | Awaiting downstairs servers |
| Device Creation | ⏳ PENDING | Awaiting downstairs servers |
| Block I/O | ⏳ PENDING | Awaiting downstairs servers |
| Quorum (2/3) | ⏳ PENDING | Awaiting downstairs servers |
| ZFS Integration | ❌ BLOCKED | OpenZFS assembly issues |

## Next Steps

1. **Start downstairs servers** on ports 8810, 8820, 8830
2. **Run basic test**: `./test-crucible-basic.sh`
3. **Check logs**: `cat ./tmp/crucible-test.log`
4. **Verify functionality**: Device should handle read/write operations
5. **Test quorum**: Stop one downstairs, verify 2/3 quorum works

## Known Limitations

- **ZFS Support**: Currently blocked by OpenZFS assembly preprocessing issues
- **Rust Component**: Disabled due to toolchain complexity (C++ driver is complete)
- **Testing**: Requires actual Crucible downstairs servers (no mock/simulator)

## Success Criteria

- ✅ OSv boots with Crucible driver loaded
- ✅ Driver connects to all 3 downstairs servers
- ✅ `/dev/crucible0` block device created
- ✅ Read/write operations succeed
- ✅ xxHash64 checksums verify data integrity
- ✅ 2/3 quorum works (degraded mode functional)

Once these pass, the Crucible driver is **production-ready** for OSv!
