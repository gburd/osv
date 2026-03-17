# Crucible Driver Testing - Quick Start

## 5-Minute Test

Run the full automated test suite:

```bash
cd /home/gburd/ws/osv/tests
./test-crucible-integration.sh
```

This will:
1. Start 3 mock downstairs servers
2. Boot OSv with Crucible driver
3. Verify connections and device creation
4. Test with/without servers
5. Save logs to `/tmp/crucible-test-*/`

## Manual Testing

### Start Mock Servers

```bash
cd /home/gburd/ws/osv/tests

# Start all 3 servers
for port in 8810 8820 8830; do
    python3 mock-crucible-downstairs.py $port &
done
```

### Boot OSv with Crucible

```bash
cd /home/gburd/ws/osv

./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=00000000-0000-0000-0000-000000000000 \
    --crucible-bs=4096 \
    --crucible-size=104857600 \
    --execute='/hello'
```

### Check Device

Inside OSv:
```bash
ls -l /dev/crucible0
```

### Stop Servers

```bash
killall -9 python3  # or: pkill -f mock-crucible
```

## Test Scenarios

### All Servers (Normal)

```bash
./crucible-scenarios.sh all-servers
```

### Quorum (2/3 Servers)

```bash
./crucible-scenarios.sh two-servers
```

### No Servers (Failure Handling)

```bash
./crucible-scenarios.sh no-servers
```

### Interactive Mode

```bash
./crucible-scenarios.sh interactive
```

## Expected Results

### Success (3/3 or 2/3 servers)

OSv logs should show:
```
[Crucible] Connected to downstairs 0: 127.0.0.1:8810
[Crucible] Connected to downstairs 1: 127.0.0.1:8820
[Crucible] Connected to downstairs 2: 127.0.0.1:8830
[Crucible] Handshake successful with downstairs 0
[Crucible] Handshake successful with downstairs 1
[Crucible] Handshake successful with downstairs 2
[Crucible] Upstairs client connected (3/3 downstairs)
```

Device `/dev/crucible0` appears.

### Degraded (2/3 servers)

OSv logs should show:
```
[Crucible] Connected to downstairs 0: 127.0.0.1:8810
[Crucible] Connected to downstairs 1: 127.0.0.1:8820
[Crucible] Failed to connect to downstairs 2 (127.0.0.1:8830): Connection refused
[Crucible] Upstairs client connected (2/3 downstairs)
```

Device `/dev/crucible0` appears (quorum maintained).

### Failure (0/3 or 1/3 servers)

OSv logs should show:
```
[Crucible] Failed to connect to downstairs 0 (127.0.0.1:8810): Connection refused
[Crucible] Failed to connect to downstairs 1 (127.0.0.1:8820): Connection refused
[Crucible] Failed to connect to downstairs 2 (127.0.0.1:8830): Connection refused
Failed to connect to at least 2 downstairs servers
```

No device created. System continues booting.

## Troubleshooting

### Ports in use

```bash
# Kill processes on test ports
for port in 8810 8820 8830; do
    lsof -ti:$port | xargs kill -9 2>/dev/null || true
done
```

### Mock server won't start

```bash
# Check Python version (need 3.7+)
python3 --version

# Check if script is executable
chmod +x tests/mock-crucible-downstairs.py
```

### OSv build not found

```bash
# Build OSv first
cd /home/gburd/ws/osv
make
```

### Can't see device in OSv

Check logs for:
- "Failed to connect to at least 2 downstairs servers"
- "Failed to complete handshake with at least 2 downstairs"

Ensure at least 2 mock servers are running before booting OSv.

## Files

| File | Purpose |
|------|---------|
| `mock-crucible-downstairs.py` | Mock Crucible server (Python) |
| `test-crucible-integration.sh` | Automated test launcher (Bash) |
| `crucible-scenarios.sh` | Individual test scenarios (Bash) |
| `crucible-io-test.cc` | I/O test program (C++) |
| `README-crucible.md` | Detailed test documentation |
| `QUICKSTART-crucible.md` | This file |

## Documentation

- Full testing guide: `/home/gburd/ws/osv/docs/crucible-testing.md`
- Test suite README: `/home/gburd/ws/osv/tests/README-crucible.md`
- Driver code: `/home/gburd/ws/osv/drivers/crucible-*.{cc,hh}`

## Common Commands

```bash
# Run all tests
./tests/test-crucible-integration.sh

# Run specific scenario
./tests/crucible-scenarios.sh all-servers

# Start servers manually
python3 tests/mock-crucible-downstairs.py 8810 &
python3 tests/mock-crucible-downstairs.py 8820 &
python3 tests/mock-crucible-downstairs.py 8830 &

# Boot OSv with Crucible
./scripts/run.py \
    --crucible=127.0.0.1:8810,127.0.0.1:8820,127.0.0.1:8830 \
    --crucible-uuid=00000000-0000-0000-0000-000000000000 \
    --crucible-bs=4096 \
    --crucible-size=104857600

# Stop all servers
pkill -f mock-crucible

# View logs
cat /tmp/crucible-test-*/osv.log
cat /tmp/crucible-test-*/server-*.log

# Check device exists
# (inside OSv)
ls -l /dev/crucible0
```

## Performance Testing

```bash
# Inside OSv with Crucible device
dd if=/dev/zero of=/dev/crucible0 bs=4k count=1000
dd if=/dev/crucible0 of=/dev/null bs=4k count=1000
```

Note: Mock servers have minimal overhead. Real performance testing requires real Crucible infrastructure.

## Next Steps

1. Run automated tests: `./test-crucible-integration.sh`
2. Try manual scenarios with `crucible-scenarios.sh`
3. Read full documentation in `/home/gburd/ws/osv/docs/crucible-testing.md`
4. Test with real Crucible infrastructure for production validation
