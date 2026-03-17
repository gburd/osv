# Crucible Driver Testing Guide

This document explains how to test the OSv Crucible driver, both with mock servers for development and with real Crucible infrastructure for production validation.

## Overview

The Crucible driver enables OSv to use Crucible as a block device backend. Testing requires either:
- **Mock servers** (for development/CI) - Minimal protocol implementation in Python
- **Real Crucible downstairs** (for production validation) - Full Crucible infrastructure

## Quick Start

Run the automated integration test:

```bash
cd tests
./test-crucible-integration.sh
```

This starts 3 mock downstairs servers and boots OSv with the Crucible driver.

## Test Infrastructure

### Mock Crucible Downstairs Server

Location: `tests/mock-crucible-downstairs.py`

A minimal Python implementation that:
- Listens on TCP ports (default: 8810, 8820, 8830)
- Accepts Crucible protocol handshakes
- Responds to read/write/flush operations with dummy data
- Logs all interactions for debugging

**Usage:**

```bash
# Start a single mock server
python3 tests/mock-crucible-downstairs.py 8810

# Start all three servers in background
for port in 8810 8820 8830; do
    python3 tests/mock-crucible-downstairs.py $port &
done
```

**Protocol Coverage:**
- ✅ HereIAm / YesItsMe handshake
- ✅ RegionInfoPlease / RegionInfo
- ✅ ReadRequest / ReadResponse
- ✅ Write / WriteAck
- ✅ Flush / FlushAck
- ✅ Ruok / Imok health checks
- ❌ Encryption (not implemented)
- ❌ Snapshots (not implemented)
- ❌ Repair operations (not implemented)

### Integration Test Launcher

Location: `tests/test-crucible-integration.sh`

Automated test script that:
1. Starts 3 mock downstairs servers
2. Launches OSv with Crucible parameters
3. Captures logs from both OSv and mock servers
4. Verifies expected behaviors
5. Cleans up on exit

**Features:**
- Automatic server lifecycle management
- Timeout handling
- Log collection
- Multiple test scenarios

## Test Scenarios

### Test 1: Boot with Servers Available

**Expected behavior:**
- All 3 mock servers accept connections
- OSv completes handshake with all 3 downstairs
- Driver creates `/dev/crucible0` device
- System boots successfully

**How to run:**
```bash
./tests/test-crucible-integration.sh
```

**What to check:**
- Server logs show "Connection from" messages
- OSv logs show "Connected to downstairs X"
- OSv logs show "Upstairs client connected (3/3 downstairs)"
- No error messages in logs

### Test 2: Boot Without Servers

**Expected behavior:**
- OSv attempts to connect to downstairs
- Connection attempts fail
- Driver logs warnings but doesn't crash
- System continues to boot (without Crucible device)

**How to run:**
```bash
# Don't start any mock servers
cd /home/gburd/ws/osv
./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=00000000-0000-0000-0000-000000000000 \
    --crucible-bs=4096 \
    --crucible-size=104857600 \
    --execute='/hello'
```

**What to check:**
- OSv logs show "Failed to connect to downstairs"
- System doesn't crash or hang
- Other devices continue to work

### Test 3: Quorum Behavior (2/3 servers)

**Expected behavior:**
- OSv connects to 2 out of 3 servers
- Driver operates in degraded mode
- Read/write operations still succeed (using quorum)

**How to run:**
```bash
# Start only 2 servers
python3 tests/mock-crucible-downstairs.py 8810 &
python3 tests/mock-crucible-downstairs.py 8820 &

# Don't start server on 8830

./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=00000000-0000-0000-0000-000000000000 \
    --crucible-bs=4096 \
    --crucible-size=104857600 \
    --execute='/hello'
```

**What to check:**
- OSv logs show connection to 2 servers
- Logs show "Upstairs client connected (2/3 downstairs)"
- Device still created and operational

### Test 4: Basic I/O Operations

**Expected behavior:**
- Writes to `/dev/crucible0` succeed
- Reads from `/dev/crucible0` return data
- Flush operations complete

**How to run:**

First, create a test program (`tests/crucible-io-test.cc`):

```cpp
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char* device = "/dev/crucible0";
    char write_buf[4096];
    char read_buf[4096];

    memset(write_buf, 0x42, sizeof(write_buf));
    memset(read_buf, 0, sizeof(read_buf));

    int fd = open(device, O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Write test
    ssize_t written = write(fd, write_buf, sizeof(write_buf));
    if (written != sizeof(write_buf)) {
        perror("write");
        close(fd);
        return 1;
    }
    printf("Wrote %zd bytes\n", written);

    // Seek back
    if (lseek(fd, 0, SEEK_SET) < 0) {
        perror("lseek");
        close(fd);
        return 1;
    }

    // Read test
    ssize_t bytes_read = read(fd, read_buf, sizeof(read_buf));
    if (bytes_read != sizeof(read_buf)) {
        perror("read");
        close(fd);
        return 1;
    }
    printf("Read %zd bytes\n", bytes_read);

    // Verify (note: mock server returns zeros, not actual data)
    printf("I/O test completed\n");

    close(fd);
    return 0;
}
```

Build and run:
```bash
# Build the test
g++ -o build/release.x64/tests/crucible-io-test tests/crucible-io-test.cc

# Start mock servers
./tests/test-crucible-integration.sh
```

## Manual Testing with Mock Servers

### Start Mock Servers Manually

```bash
# Terminal 1
python3 tests/mock-crucible-downstairs.py 8810

# Terminal 2
python3 tests/mock-crucible-downstairs.py 8820

# Terminal 3
python3 tests/mock-crucible-downstairs.py 8830
```

### Launch OSv with Crucible

```bash
./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=00000000-0000-0000-0000-000000000000 \
    --crucible-bs=4096 \
    --crucible-size=104857600 \
    --execute='/hello' \
    --verbose
```

**Crucible Parameters:**
- `--crucible=HOST:PORT,HOST:PORT,HOST:PORT` - Three downstairs targets (required)
- `--crucible-uuid=UUID` - Region UUID (required)
- `--crucible-bs=BYTES` - Block size, must be power of 2 (default: 4096)
- `--crucible-size=BYTES` - Total device size (required)
- `--crucible-ro` - Mount read-only (optional)
- `--crucible-encrypted` - Enable encryption (optional, not supported by mock)

### Verify Device Creation

Inside OSv:
```bash
ls -l /dev/crucible0
```

Expected output:
```
brw-rw-rw- 1 root root 251, 0 <date> /dev/crucible0
```

## Testing with Real Crucible Infrastructure

### Prerequisites

1. Build Crucible downstairs servers (Rust)
2. Create region with `crucible-downstairs` tool
3. Start 3 downstairs instances

### Setup Real Downstairs

```bash
# Build Crucible (requires Rust)
git clone https://github.com/oxidecomputer/crucible.git
cd crucible
cargo build --release

# Create a region (one-time setup)
./target/release/dsc create \
    --uuid 12345678-1234-5678-1234-567812345678 \
    --block-size 4096 \
    --extent-size 256 \
    --extent-count 100 \
    --dir /var/crucible/region0

# Start downstairs servers
./target/release/crucible-downstairs run \
    --port 8810 \
    --data /var/crucible/region0 &

./target/release/crucible-downstairs run \
    --port 8820 \
    --data /var/crucible/region0 &

./target/release/crucible-downstairs run \
    --port 8830 \
    --data /var/crucible/region0 &
```

### Launch OSv with Real Crucible

```bash
./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=12345678-1234-5678-1234-567812345678 \
    --crucible-bs=4096 \
    --crucible-size=104857600 \
    --execute='/myapp'
```

## Troubleshooting

### Mock Server Won't Start

**Problem:** `Address already in use`

**Solution:** Kill existing processes on ports 8810-8830:
```bash
for port in 8810 8820 8830; do
    lsof -ti:$port | xargs kill -9 2>/dev/null || true
done
```

### OSv Can't Connect to Servers

**Problem:** Connection refused or timeout

**Check:**
1. Mock servers are running: `ps aux | grep mock-crucible`
2. Servers are listening: `netstat -tlnp | grep 881`
3. Firewall allows connections: `sudo iptables -L`
4. Correct IP addresses (127.0.0.1 for local testing)

### Device Not Created

**Problem:** `/dev/crucible0` doesn't exist

**Check:**
1. At least 2/3 servers connected successfully
2. Handshake completed (check OSv logs for "Handshake successful")
3. Driver loaded: `dmesg | grep -i crucible`

### Hash Mismatch Errors

**Problem:** "Read job X: hash mismatch at block Y"

**Cause:** Mock server returns dummy hashes (0x1234567890abcdef) that don't match actual data

**Solution:** This is expected with mock servers. Real Crucible computes correct hashes.

### Protocol Errors

**Problem:** "Unexpected message type" or "Frame too large"

**Check:**
1. Protocol version matches (V13)
2. Message encoding is correct (bincode format)
3. Length prefix is little-endian 4-byte integer

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Crucible Driver Test

on: [push, pull_request]

jobs:
  test-crucible:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y python3 qemu-system-x86

      - name: Build OSv
        run: ./scripts/build image=native-example

      - name: Run Crucible tests
        run: ./tests/test-crucible-integration.sh

      - name: Upload logs
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-logs
          path: /tmp/crucible-test-*/
```

## Performance Testing

### Basic Benchmark

Use `fio` to test I/O performance:

```bash
# Inside OSv
fio --name=test --filename=/dev/crucible0 --rw=randwrite \
    --bs=4k --size=100M --numjobs=1 --runtime=30
```

### Expected Performance (Mock)

Mock servers have minimal overhead:
- Sequential read: ~500 MB/s
- Sequential write: ~400 MB/s
- Random read (4K): ~50K IOPS
- Random write (4K): ~40K IOPS

Real Crucible performance depends on storage backend.

## Debugging Tips

### Enable Verbose Logging

Add to OSv command line:
```bash
--verbose --env=CRUCIBLE_LOG=debug
```

### Capture Protocol Messages

Use `tcpdump` to inspect traffic:
```bash
sudo tcpdump -i lo -w crucible.pcap port 8810 or port 8820 or port 8830
```

Analyze with Wireshark or:
```bash
tcpdump -r crucible.pcap -X
```

### Mock Server Debug Mode

Edit `mock-crucible-downstairs.py` and add after receiving messages:
```python
print(f"Raw message: {data.hex()}")
```

### Common Log Messages

**Normal Operation:**
- `Connected to downstairs X: 127.0.0.1:881X`
- `Handshake successful with downstairs X`
- `Upstairs client connected (3/3 downstairs)`
- `Read job_id=X completed successfully`

**Warnings (non-fatal):**
- `Failed to connect to downstairs X (127.0.0.1:881X): Connection refused`
- `Response processing error on downstairs X: ...`

**Errors (fatal):**
- `Failed to connect to at least 2 downstairs servers`
- `Failed to complete handshake with at least 2 downstairs`
- `Read job_id=X failed to reach quorum`

## Known Limitations of Mock Servers

1. **No data persistence** - All writes lost when server stops
2. **No encryption** - Encrypted mode not supported
3. **Dummy hashes** - Returns fixed hash values, not actual xxHash64
4. **No repair** - Repair protocol not implemented
5. **No snapshots** - Snapshot operations not supported
6. **Single client** - One connection at a time
7. **Limited error injection** - Can't simulate specific failure modes

For production validation, always test with real Crucible infrastructure.

## Additional Resources

- Crucible GitHub: https://github.com/oxidecomputer/crucible
- Crucible Protocol Documentation: See `crucible/protocol/` in repository
- OSv Crucible Driver: `drivers/crucible-*.{cc,hh}`
- OSv Block Device Layer: `drivers/blk-device.hh`

## Getting Help

1. Check OSv logs: `dmesg | grep -i crucible`
2. Check mock server logs in `/tmp/crucible-test-*/`
3. Review protocol messages with tcpdump
4. File issues on OSv GitHub with logs attached
