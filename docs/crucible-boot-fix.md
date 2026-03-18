# Crucible Driver Boot Fix

## Problem

The Crucible driver was causing a boot loop due to blocking initialization in an `__attribute__((constructor))` function. This auto-initialization code:

1. Ran unconditionally at boot time (even without `--crucible` parameter)
2. Attempted to connect to hardcoded localhost servers
3. Blocked boot if downstairs servers were unavailable

## Solution

### Changes Made

#### 1. Removed Auto-Initialization (`drivers/crucible-blk.cc`)

**Before:**
```cpp
__attribute__((constructor(101))) static void crucible_auto_init() {
    const std::string default_targets = "localhost:8810,localhost:8820,localhost:8830";
    const std::string default_uuid = "12345678-1234-1234-1234-123456789abc";

    int ret = crucible::crucible_init(default_targets, default_uuid, 4096, false);
    // ...
}
```

**After:**
- Removed entirely
- Driver only initializes when `--crucible` parameter is provided

#### 2. Improved Error Handling (`drivers/crucible-blk.cc`)

**Changes:**
- Added explicit try-catch around `client->connect()`
- Changed error messages from "failed" to "WARNING: ... boot will continue"
- Added "SUCCESS:" prefix to successful initialization
- Made connection failures non-fatal

**Key Messages:**
```
crucible_init: WARNING: connection failed: <error>
crucible_init: boot will continue, but /dev/crucible0 will not be available
```

#### 3. Updated Loader (`loader.cc`)

**Changes:**
- Capture return value from `crucible_init()`
- Log error if initialization fails
- Boot continues regardless of return code

```cpp
int ret = crucible::crucible_init(...);
if (ret != 0) {
    kprintf("loader: Crucible initialization returned error %d (boot continues)\n", ret);
}
```

#### 4. Updated Documentation (`drivers/crucible-blk.hh`)

**Changes:**
- Clarified non-blocking behavior in function comment
- Added note about boot continuation on failure

## Boot Behavior

### Without `--crucible` Parameter

```
OSv boots normally
No Crucible messages
/dev/crucible0 not created
```

### With `--crucible` but Servers Unavailable

```
crucible_init: Initializing Crucible block device
crucible_init: targets=..., uuid=...
crucible_init: attempting to connect to downstairs servers...
[Crucible] Failed to connect to downstairs 0 (...): <error>
[Crucible] Failed to connect to downstairs 1 (...): <error>
[Crucible] Failed to connect to downstairs 2 (...): <error>
crucible_init: WARNING: connection failed: Failed to connect to at least 2 downstairs servers
crucible_init: boot will continue, but /dev/crucible0 will not be available
loader: Crucible initialization returned error 107 (boot continues)

OSv continues booting...
/dev/crucible0 not created
```

### With `--crucible` and Servers Available

```
crucible_init: Initializing Crucible block device
crucible_init: targets=..., uuid=...
crucible_init: attempting to connect to downstairs servers...
[Crucible] Connected to downstairs 0: localhost:8810
[Crucible] Connected to downstairs 1: localhost:8820
[Crucible] Connected to downstairs 2: localhost:8830
crucible_init: SUCCESS: created device crucible0, size=... bytes, block_size=...

OSv continues booting...
/dev/crucible0 available for use
```

## Testing

### Prerequisites

1. Build OSv with Crucible driver enabled:
   ```bash
   ./scripts/build conf_drivers_crucible=1
   ```

### Test Cases

#### Test 1: Boot without Crucible
```bash
./scripts/run.py
# Expected: Normal boot, no Crucible messages
```

#### Test 2: Boot with Crucible but No Servers
```bash
./scripts/run.py \
  --crucible=localhost:8810,localhost:8820,localhost:8830 \
  --crucible-uuid=12345678-1234-1234-1234-123456789abc
# Expected: Boot succeeds with WARNING messages
# Verify: ls /dev/crucible* shows no device
```

#### Test 3: Boot with Crucible and Running Servers
```bash
# First, start downstairs servers (see crucible-basic-test.sh)
./scripts/crucible-basic-test.sh &

# Then boot OSv
./scripts/run.py \
  --crucible=localhost:8810,localhost:8820,localhost:8830 \
  --crucible-uuid=12345678-1234-1234-1234-123456789abc
# Expected: Boot succeeds with SUCCESS message
# Verify: ls /dev/crucible* shows device
```

## Implementation Notes

### Connection Timeout

The connection logic in `crucible-client.cc` already has proper error handling:
- Catches exceptions per downstairs connection
- Requires 2/3 quorum
- Returns errors instead of blocking indefinitely

### Error Codes

- `ENOTCONN` (107): Connection failed or insufficient quorum
- `EIO` (5): General I/O error during initialization
- `EINVAL` (22): Invalid parameters (wrong target count, bad UUID)

### Thread Safety

The `UpsairsClient::connect()` method is already thread-safe and non-blocking:
- Uses exceptions for error handling
- Returns immediately on connection failure
- No infinite retry loops

## Success Criteria

- [x] OSv boots normally when `--crucible` is not specified
- [x] When `--crucible` is specified, driver attempts initialization
- [x] Connection failures don't block boot, just log warnings
- [x] `/dev/crucible0` only appears when initialization succeeds
- [x] No boot loop or hang occurs when servers are unavailable
- [x] Error messages clearly indicate boot continues

## Related Files

- `/home/gburd/ws/osv/drivers/crucible-blk.cc` - Main implementation
- `/home/gburd/ws/osv/drivers/crucible-blk.hh` - Public interface
- `/home/gburd/ws/osv/loader.cc` - Boot integration
- `/home/gburd/ws/osv/tests/crucible-boot-test.sh` - Test script
