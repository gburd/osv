# Quick Start: ZFS RAID-Z on Crucible

## TL;DR

Run the automated test:
```bash
cd /path/to/osv
./tests/zfs-crucible-raidz-l2arc.sh
```

## Prerequisites

### Required Infrastructure
- 9 Crucible downstairs servers (3 per volume, 3 volumes)
- Network connectivity to servers
- OSv source code

### Start Downstairs Servers

```bash
# Volume 0 - ports 3000, 3001, 3002
crucible-downstairs --port 3000 --data /data/vol0-r0 &
crucible-downstairs --port 3001 --data /data/vol0-r1 &
crucible-downstairs --port 3002 --data /data/vol0-r2 &

# Volume 1 - ports 3010, 3011, 3012
crucible-downstairs --port 3010 --data /data/vol1-r0 &
crucible-downstairs --port 3011 --data /data/vol1-r1 &
crucible-downstairs --port 3012 --data /data/vol1-r2 &

# Volume 2 - ports 3020, 3021, 3022
crucible-downstairs --port 3020 --data /data/vol2-r0 &
crucible-downstairs --port 3021 --data /data/vol2-r1 &
crucible-downstairs --port 3022 --data /data/vol2-r2 &
```

## Option 1: Automated Script

```bash
# Default configuration
./tests/zfs-crucible-raidz-l2arc.sh

# Custom configuration
CRUCIBLE_BASE_IP=192.168.1.10 \
UUID_BASE=my-pool \
./tests/zfs-crucible-raidz-l2arc.sh
```

## Option 2: Manual Setup

### Step 1: Build OSv

```bash
./scripts/build conf_drivers_profile=crucible image=native-example
```

### Step 2: Boot with 3 Volumes

```bash
./scripts/run.py \
  --crucible0=10.0.0.10:3000,10.0.0.10:3001,10.0.0.10:3002 \
  --crucible0-uuid=vol0-uuid \
  --crucible1=10.0.0.10:3010,10.0.0.10:3011,10.0.0.10:3012 \
  --crucible1-uuid=vol1-uuid \
  --crucible2=10.0.0.10:3020,10.0.0.10:3021,10.0.0.10:3022 \
  --crucible2-uuid=vol2-uuid \
  -e '/tests/native-example.so'
```

### Step 3: Create RAID-Z Pool (Inside OSv)

```bash
# Verify devices
ls -l /dev/crucible*

# Create pool
zpool create -f \
  -o ashift=12 \
  -O compression=lz4 \
  -O atime=off \
  datapool raidz /dev/crucible0 /dev/crucible1 /dev/crucible2

# Check status
zpool status datapool
```

### Step 4: Use the Pool

```bash
# Create dataset
zfs create datapool/mydata

# Write data
dd if=/dev/urandom of=/datapool/mydata/test.dat bs=1M count=100

# Create snapshot
zfs snapshot datapool/mydata@snap1

# Check compression ratio
zfs get compressratio datapool
```

## Troubleshooting

### Devices Not Appearing

```bash
# Check downstairs servers
netstat -ln | grep -E '3000|3010|3020'

# Test connectivity
telnet 10.0.0.10 3000

# Check OSv logs
dmesg | grep crucible
```

### Pool Creation Fails

```bash
# Force creation (destroys existing data)
zpool create -f datapool raidz /dev/crucible{0,1,2}

# Check device availability
ls -l /dev/crucible*
```

### Poor Performance

```bash
# Check I/O stats
zpool iostat -v datapool 5

# Disable compression if CPU-bound
zfs set compression=off datapool

# Check network latency
ping -c 10 10.0.0.10
```

## Architecture Overview

```
┌─────────────────────────────┐
│   ZFS RAID-Z Pool           │
│                             │
│  ┌────────┐ ┌────────┐ ┌─┐ │
│  │cruc0   │ │cruc1   │ │2││ │
│  └───┬────┘ └───┬────┘ └┬┘│ │
└──────┼──────────┼────────┼──┘
       │          │        │
       ▼          ▼        ▼
   3 replicas  3 replicas  3
   (quorum)    (quorum)  (...)
```

## Key Features

- **Triple Replication**: Each volume replicated 3x via Crucible
- **Parity Protection**: RAID-Z protects against 1 volume failure
- **Compression**: LZ4 provides ~1.5-3× space savings
- **Snapshots**: Instant, space-efficient snapshots
- **Fault Tolerance**: Survives multiple failures

## Performance

| Operation | Latency | Notes |
|-----------|---------|-------|
| Write | 5-15ms | Network + quorum |
| Read (cached) | 1-10ms | ZFS ARC |
| Read (uncached) | 5-20ms | Network + storage |

## Capacity

For 3 × 10GB volumes:
- Raw: 30GB
- Usable: ~20GB (RAID-Z parity)
- Effective: ~30-60GB (with compression)

## Common Commands

```bash
# Pool status
zpool status datapool

# Pool capacity
zpool list datapool

# I/O statistics
zpool iostat -v datapool

# Dataset list
zfs list

# Create snapshot
zfs snapshot datapool/mydata@name

# List snapshots
zfs list -t snapshot

# Compression ratio
zfs get compressratio datapool

# ARC statistics
cat /proc/spl/kstat/zfs/arcstats
```

## Documentation

- **Complete Guide**: [docs/zfs-raidz-crucible-example.md](docs/zfs-raidz-crucible-example.md)
- **Driver Usage**: [docs/crucible-usage.md](docs/crucible-usage.md)
- **Testing**: [docs/crucible-testing.md](docs/crucible-testing.md)

## Help

For issues or questions, see:
1. [Troubleshooting Guide](docs/zfs-raidz-crucible-example.md#troubleshooting)
2. [Crucible Documentation](docs/crucible-usage.md)
3. Check `dmesg | grep crucible` for driver errors
4. Verify downstairs servers are running and reachable
