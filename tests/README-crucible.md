# Crucible Driver Test Suite

This directory contains test infrastructure for the OSv Crucible driver.

## Files

- `mock-crucible-downstairs.py` - Mock Crucible downstairs server (Python)
- `test-crucible-integration.sh` - Automated integration test launcher (Bash)
- `crucible-io-test.cc` - Basic I/O test program (C++)
- `crucible-boot-test.sh` - Simple boot test with Crucible

## Quick Start

Run all automated tests:
```bash
./test-crucible-integration.sh
```

## Components

### Mock Downstairs Server

Minimal Python implementation of Crucible downstairs protocol.

**Features:**
- TCP server listening on configurable port
- Handles protocol handshake (HereIAm/YesItsMe)
- Responds to region info queries
- Accepts read/write/flush operations
- Returns dummy data (no actual storage)
- Logs all protocol interactions

**Usage:**
```bash
# Start single server
python3 mock-crucible-downstairs.py 8810

# Start all 3 servers for quorum
for port in 8810 8820 8830; do
    python3 mock-crucible-downstairs.py $port > /tmp/server-$port.log 2>&1 &
done
```

**Limitations:**
- No encryption support
- No data persistence
- Dummy hash values
- Single client at a time
- No repair operations

### Integration Test Launcher

Automated test script that manages the full test lifecycle.

**What it does:**
1. Validates prerequisites (Python, build directory, etc.)
2. Starts 3 mock downstairs servers
3. Launches OSv with Crucible driver
4. Captures logs from all components
5. Verifies expected behaviors
6. Cleans up on exit

**Output:**
- Test results printed to console
- Logs saved in `/tmp/crucible-test-<pid>/`
- Exit code 0 on success, non-zero on failure

**Environment variables:**
- `CRUCIBLE_TEST_TIMEOUT` - Override default timeout (30s)
- `CRUCIBLE_TEST_PORTS` - Custom port list (default: 8810,8820,8830)

### I/O Test Program

C++ program that tests basic block device operations.

**Tests:**
- Device existence check
- Open/close operations
- Write operations
- Read operations
- Write-read-verify cycle
- Block-aligned I/O

**Build:**
```bash
cd /home/gburd/ws/osv
make -C build/release.x64 tests/crucible-io-test
```

**Run:**
```bash
# Inside OSv
/tests/crucible-io-test
```

## Test Scenarios

### Scenario 1: Normal Operation (3/3 servers)

Start all 3 mock servers and boot OSv.

**Expected:** Device created, all I/O operations succeed.

### Scenario 2: Degraded Mode (2/3 servers)

Start only 2 mock servers and boot OSv.

**Expected:** Device created with warning, I/O still works (quorum).

### Scenario 3: No Servers (0/3)

Boot OSv without starting any mock servers.

**Expected:** Connection failures logged, no device created, system continues booting.

### Scenario 4: Server Failure During Operation

Start all 3 servers, boot OSv, then kill one server during I/O.

**Expected:** I/O continues using remaining 2 servers.

## Directory Structure

```
tests/
├── README-crucible.md                  # This file
├── mock-crucible-downstairs.py         # Mock server
├── test-crucible-integration.sh        # Integration test launcher
├── crucible-io-test.cc                 # I/O test program
├── crucible-boot-test.sh               # Simple boot test
└── ... (other OSv tests)
```

## Integration with CI/CD

Add to your CI pipeline:

```yaml
- name: Test Crucible Driver
  run: |
    cd tests
    ./test-crucible-integration.sh

- name: Upload Test Logs
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: crucible-test-logs
    path: /tmp/crucible-test-*/
```

## Troubleshooting

### Ports in use

```bash
# Kill any processes using test ports
for port in 8810 8820 8830; do
    lsof -ti:$port | xargs kill -9 2>/dev/null || true
done
```

### Server won't start

Check Python version:
```bash
python3 --version  # Should be 3.7+
```

Check socket permissions:
```bash
# May need to allow binding to privileged ports
sudo setcap 'cap_net_bind_service=+ep' /usr/bin/python3.x
```

### OSv won't connect

1. Verify servers are running: `ps aux | grep mock-crucible`
2. Check network: `netstat -tlnp | grep 881`
3. Test connectivity: `telnet 127.0.0.1 8810`

### Device not created

Check OSv logs for:
- "Failed to connect to at least 2 downstairs servers"
- "Handshake/query failed for downstairs"
- Protocol version mismatches

Enable verbose logging:
```bash
./scripts/run.py --crucible=... --verbose
```

## Testing with Real Crucible

For production validation, test with real Crucible infrastructure:

1. Build Crucible from https://github.com/oxidecomputer/crucible
2. Create region with `crucible-downstairs` tool
3. Start 3 real downstairs instances
4. Boot OSv with real downstairs targets

See `/home/gburd/ws/osv/docs/crucible-testing.md` for detailed instructions.

## Further Documentation

- Complete testing guide: `../docs/crucible-testing.md`
- Crucible driver code: `../drivers/crucible-*.{cc,hh}`
- Protocol reference: Crucible GitHub repository

## Contributing

When adding new tests:

1. Follow existing test naming conventions
2. Add test case to `test-crucible-integration.sh`
3. Document expected behavior
4. Update this README
5. Ensure tests clean up resources

## License

Tests are part of OSv and licensed under the BSD license.
