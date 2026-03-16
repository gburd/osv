# Crucible Block Device Implementation Plan

## Overview

Implement a Crucible upstairs client in C/C++ as a native OSv block device driver, providing distributed block storage with triple replication and 2/3 quorum logic.

**Key Decision**: Implement in C/C++ (not Rust) to fit OSv's codebase and avoid Rust toolchain complexity.

**Architecture**: Network block device following virtio-blk.cc pattern.

## Crucible Background

[Crucible](https://github.com/oxidecomputer/crucible) is a distributed block storage system developed by Oxide Computer Company for their cloud infrastructure.

### Architecture

```
Application
    │
    ├─> crucible-blk.cc (OSv block device driver)
    │       │
    │       └─> crucible-client.cc (Upstairs client)
    │               │
    │               ├─> Downstairs #1 (TCP)
    │               ├─> Downstairs #2 (TCP)
    │               └─> Downstairs #3 (TCP)
    │                       │
    │                       └─> Persistent storage
```

### Key Concepts

- **Upstairs**: Client that distributes I/O across downstairs servers
- **Downstairs**: Storage server that persists blocks to disk
- **Quorum**: 2/3 responses required for success
- **Replication**: All writes go to 3 downstairs servers

## Implementation Phases

### Phase 1: Protocol Analysis (1-2 weeks)

#### Objectives
- Understand Crucible wire protocol
- Document message formats and state machine
- Extract protocol specification from Rust code

#### Tasks

1. **Clone and Study Crucible Repository**
   ```bash
   git clone https://github.com/oxidecomputer/crucible external/crucible-reference
   ```

2. **Analyze Rust Protocol Implementation**
   - Key files:
     * `protocol/src/lib.rs` - Message definitions
     * `upstairs/src/lib.rs` - Client implementation
     * `upstairs/src/client.rs` - Network communication
     * `downstairs/src/lib.rs` - Server implementation

3. **Document Message Format**
   - Message types (Read, Write, Flush, WriteUnwritten, etc.)
   - Request/response structures
   - Serialization format (likely bincode or custom binary)
   - Byte order (endianness)

4. **Document State Machine**
   - Connection lifecycle
   - Request tracking
   - Error handling and retries
   - Quorum logic

5. **Create Protocol Specification**
   - Output: `docs/crucible-protocol.md`
   - Include example message flows
   - Document error codes

#### Deliverables
- docs/crucible-protocol.md (protocol specification)
- Message format diagrams
- State machine documentation

#### Success Criteria
- Can manually construct valid Crucible messages
- Understand complete request/response lifecycle
- Clear understanding of quorum logic

### Phase 2: Network Infrastructure (1 week)

#### Objectives
- Implement TCP connection management
- Create async I/O infrastructure
- Test basic connectivity to downstairs

#### Tasks

1. **TCP Connection Wrapper**
   ```cpp
   class CrucibleConnection {
   public:
       CrucibleConnection(const std::string& host, uint16_t port);
       ssize_t send(const void* buf, size_t len);
       ssize_t recv(void* buf, size_t len);
       bool is_connected() const;
       void close();
   };
   ```

2. **Async I/O Thread**
   ```cpp
   class CrucibleClient {
       sched::thread* io_thread_;
       void io_loop();  // Main I/O processing loop
   };
   ```

3. **Request Queue**
   ```cpp
   struct PendingRequest {
       uint64_t request_id;
       MessageType type;
       std::condition_variable cv;
       int result;
       void* response_data;
   };

   std::map<uint64_t, PendingRequest*> pending_requests_;
   std::mutex pending_mutex_;
   ```

4. **Connection Test**
   - Simple connectivity test to downstairs
   - Send/receive test messages
   - Verify TCP works correctly

#### Deliverables
- drivers/crucible-connection.cc
- drivers/crucible-connection.hh
- Basic connectivity test program

#### Success Criteria
- Can establish TCP connections to downstairs servers
- Can send/receive raw data
- I/O thread processes requests correctly

### Phase 3: Protocol Implementation (3-4 weeks)

#### Objectives
- Implement Crucible wire protocol in C++
- Create upstairs client logic
- Implement quorum handling

#### Week 1: Message Structures

1. **Protocol Header Definitions**
   ```cpp
   namespace crucible {

   enum class MessageType : uint8_t {
       Read = 1,
       Write = 2,
       Flush = 3,
       WriteUnwritten = 4,
       ReadResponse = 5,
       WriteResponse = 6,
       FlushResponse = 7,
   };

   struct MessageHeader {
       uint32_t length;          // Total message length
       MessageType type;
       uint8_t padding[3];
   } __attribute__((packed));

   struct ReadRequest {
       MessageHeader header;
       uint64_t request_id;
       uint64_t offset;          // Block offset
       uint32_t length;          // Bytes to read
       uint32_t padding;
   } __attribute__((packed));

   struct WriteRequest {
       MessageHeader header;
       uint64_t request_id;
       uint64_t offset;
       uint32_t length;
       uint32_t padding;
       // Followed by data bytes
   } __attribute__((packed));

   struct ReadResponse {
       MessageHeader header;
       uint64_t request_id;
       uint32_t status;          // 0 = success
       uint32_t data_length;
       // Followed by data bytes
   } __attribute__((packed));

   } // namespace crucible
   ```

2. **Serialization/Deserialization**
   - Host-to-network byte order conversion
   - Message packing/unpacking
   - Checksum validation (if protocol includes it)

#### Week 2: Basic Operations

1. **Synchronous Read**
   ```cpp
   int Upstairs::read_sync(uint64_t offset, uint32_t length, void* buffer) {
       auto req_id = next_request_id_++;

       // Send to all downstairs
       ReadRequest req{};
       req.header.type = MessageType::Read;
       req.request_id = req_id;
       req.offset = offset;
       req.length = length;

       for (auto& conn : connections_) {
           conn->send(&req, sizeof(req));
       }

       // Wait for quorum (2/3)
       return wait_for_read_quorum(req_id, buffer, length);
   }
   ```

2. **Synchronous Write**
   ```cpp
   int Upstairs::write_sync(uint64_t offset, uint32_t length, const void* buffer) {
       auto req_id = next_request_id_++;

       WriteRequest req{};
       req.header.type = MessageType::Write;
       req.request_id = req_id;
       req.offset = offset;
       req.length = length;

       for (auto& conn : connections_) {
           conn->send(&req, sizeof(req));
           conn->send(buffer, length);
       }

       return wait_for_write_quorum(req_id);
   }
   ```

3. **Flush Operation**
   ```cpp
   int Upstairs::flush_sync() {
       auto req_id = next_request_id_++;

       FlushRequest req{};
       req.header.type = MessageType::Flush;
       req.request_id = req_id;

       for (auto& conn : connections_) {
           conn->send(&req, sizeof(req));
       }

       return wait_for_quorum(req_id);
   }
   ```

#### Week 3: Quorum Logic

1. **Response Tracking**
   ```cpp
   struct ResponseTracker {
       std::atomic<int> responses_received{0};
       std::atomic<int> success_count{0};
       std::atomic<int> error_count{0};
       std::mutex mutex;
       std::condition_variable cv;

       std::array<bool, 3> downstairs_responded{false, false, false};
       std::array<int, 3> downstairs_status{-1, -1, -1};
       void* data[3] = {nullptr, nullptr, nullptr};
   };
   ```

2. **Quorum Wait Logic**
   ```cpp
   int Upstairs::wait_for_read_quorum(uint64_t req_id, void* buffer, size_t length) {
       auto tracker = create_tracker(req_id);

       std::unique_lock<std::mutex> lock(tracker->mutex);

       // Wait until quorum (2/3) responds
       tracker->cv.wait(lock, [tracker] {
           return tracker->success_count >= 2 ||
                  tracker->error_count >= 2;
       });

       if (tracker->success_count >= 2) {
           // Copy data from first successful response
           for (int i = 0; i < 3; i++) {
               if (tracker->downstairs_status[i] == 0) {
                   memcpy(buffer, tracker->data[i], length);
                   break;
               }
           }
           return 0;
       }

       return EIO;  // Quorum failed
   }
   ```

3. **Response Processing**
   ```cpp
   void Upstairs::process_response(int downstairs_idx, ReadResponse* resp) {
       auto tracker = find_tracker(resp->request_id);
       if (!tracker) return;

       std::unique_lock<std::mutex> lock(tracker->mutex);

       tracker->downstairs_responded[downstairs_idx] = true;
       tracker->downstairs_status[downstairs_idx] = resp->status;
       tracker->responses_received++;

       if (resp->status == 0) {
           tracker->success_count++;
           // Allocate and copy response data
           tracker->data[downstairs_idx] = malloc(resp->data_length);
           memcpy(tracker->data[downstairs_idx], resp + 1, resp->data_length);
       } else {
           tracker->error_count++;
       }

       // Notify waiting thread
       tracker->cv.notify_one();
   }
   ```

#### Week 4: Error Handling and Retries

1. **Connection Failure Handling**
   - Detect disconnections
   - Mark downstairs as degraded
   - Continue with 2/3 quorum

2. **Timeout Handling**
   - Per-request timeouts
   - Retry logic for failed requests
   - Exponential backoff

3. **Degraded Mode**
   - Track downstairs health
   - Operate with 2/3 downstairs
   - Alert on single-downstairs mode (dangerous)

#### Deliverables
- drivers/crucible-protocol.hh (300+ lines)
- drivers/crucible-client.cc (800+ lines)
- drivers/crucible-client.hh (150+ lines)
- Unit tests for protocol encoding/decoding

#### Success Criteria
- Can send/receive all Crucible message types
- Quorum logic works correctly (2/3 responses)
- Handles connection failures gracefully
- No data corruption in stress testing

### Phase 4: Block Device Driver (2 weeks)

#### Objectives
- Integrate Crucible client with OSv block device layer
- Implement bio strategy function
- Add device initialization

#### Tasks

1. **Block Device Structure**
   ```cpp
   struct crucible_priv {
       std::unique_ptr<crucible::Upstairs> client;
       uint64_t disk_size;
       uint64_t block_size;
       sched::thread* strategy_thread;
   };
   ```

2. **Strategy Function**
   ```cpp
   static void crucible_strategy(struct bio* bio) {
       auto priv = static_cast<crucible_priv*>(bio->bio_dev->private_data);
       int error = 0;

       try {
           switch (bio->bio_cmd) {
           case BIO_READ:
               error = priv->client->read_sync(
                   bio->bio_offset,
                   bio->bio_bcount,
                   bio->bio_data
               );
               break;

           case BIO_WRITE:
               error = priv->client->write_sync(
                   bio->bio_offset,
                   bio->bio_bcount,
                   bio->bio_data
               );
               break;

           case BIO_FLUSH:
               error = priv->client->flush_sync();
               break;

           default:
               error = ENOTSUP;
           }
       } catch (const std::exception& e) {
           kprintf("[Crucible] I/O error: %s\n", e.what());
           error = EIO;
       }

       biodone(bio, error == 0);
   }
   ```

3. **Device Initialization**
   ```cpp
   void crucible_init() {
       // Parse boot options
       auto targets_str = osv::options::option_value("crucible");
       auto uuid_str = osv::options::option_value("crucible-uuid");

       if (!targets_str || !uuid_str) {
           return;  // Not configured
       }

       // Parse targets (host1:port1,host2:port2,host3:port3)
       std::vector<std::string> targets = parse_targets(*targets_str);

       // Create device
       struct device* dev = device_create(&crucible_driver, "crucible0", D_BLK);
       auto priv = new (dev->private_data) crucible_priv;

       // Initialize client
       priv->client = std::make_unique<crucible::Upstairs>(
           targets, *uuid_str, 512, 20971520  // 512-byte blocks, 10GB
       );

       priv->disk_size = 512 * 20971520;
       dev->size = priv->disk_size;
       dev->max_io_size = 1024 * 1024;  // 1MB

       kprintf("[Crucible] Device initialized: /dev/crucible0 (%.2f GB)\n",
               priv->disk_size / (1024.0 * 1024.0 * 1024.0));

       read_partition_table(dev);
   }
   ```

4. **IOctl Operations**
   ```cpp
   static int crucible_ioctl(struct device* dev, u_long cmd, void* arg) {
       auto priv = static_cast<crucible_priv*>(dev->private_data);

       switch (cmd) {
       case BLKGETSIZE64:
           *(uint64_t*)arg = priv->disk_size;
           return 0;
       case BLKSSZGET:
           *(int*)arg = priv->block_size;
           return 0;
       default:
           return ENOTTY;
       }
   }
   ```

#### Deliverables
- drivers/crucible-blk.cc (400+ lines)
- drivers/crucible-blk.hh (50+ lines)

#### Success Criteria
- Device appears as /dev/crucible0
- Can read/write via dd
- ZFS can format and use device
- No kernel panics under load

### Phase 5: Build Integration (3-4 days)

#### Tasks

1. **Makefile Integration**
   ```makefile
   ifeq ($(CONF_drivers_crucible),1)
   drivers += drivers/crucible-blk.o \
              drivers/crucible-client.o \
              drivers/crucible-connection.o \
              drivers/crucible-protocol.o

   $(out)/drivers/crucible-%.o: drivers/crucible-%.cc
       $(CXX) $(CXXFLAGS) -c -o $@ $<
   endif
   ```

2. **Driver Profile**
   ```makefile
   # conf/profiles/x64/crucible.mk
   conf_drivers_crucible=1
   ```

3. **Boot Options**
   - Add --crucible option parser
   - Add --crucible-uuid option
   - Add --crucible-block-size option (optional)

4. **Documentation**
   - docs/crucible-usage.md
   - Update README.md
   - Add examples to docs/

#### Deliverables
- Makefile changes
- conf/profiles/x64/crucible.mk
- loader.cc changes for boot options
- docs/crucible-usage.md

#### Success Criteria
- Can build with `./scripts/build conf_drivers_profile=crucible`
- Boot options parsed correctly
- Documentation complete and accurate

### Phase 6: Testing (2-3 weeks)

#### Unit Tests
1. Protocol encoding/decoding
2. Quorum logic (2/3, 1/3 failures)
3. Connection handling
4. Request/response matching

#### Integration Tests
1. **Basic I/O**
   ```bash
   # Write test
   dd if=/dev/zero of=/dev/crucible0 bs=1M count=100

   # Read test
   dd if=/dev/crucible0 of=/dev/null bs=1M count=100

   # Integrity test
   dd if=/dev/urandom of=/tmp/test bs=1M count=10
   dd if=/tmp/test of=/dev/crucible0 bs=1M
   dd if=/dev/crucible0 of=/tmp/verify bs=1M count=10
   cmp /tmp/test /tmp/verify
   ```

2. **Filesystem Tests**
   ```bash
   # ZFS
   zpool create testpool /dev/crucible0
   zfs create testpool/data
   echo "test" > /testpool/data/file.txt
   cat /testpool/data/file.txt

   # EXT4
   mkfs.ext4 /dev/crucible0
   mount -t ext /dev/crucible0 /data
   echo "test" > /data/file.txt
   cat /data/file.txt
   ```

3. **Resilience Tests**
   ```bash
   # Stop one downstairs during I/O
   # Verify degraded mode operation
   # Restart downstairs
   # Verify recovery
   ```

4. **Performance Tests**
   ```bash
   # Sequential read
   dd if=/dev/crucible0 of=/dev/null bs=1M count=1000

   # Sequential write
   dd if=/dev/zero of=/dev/crucible0 bs=1M count=1000

   # Random I/O with fio
   fio --filename=/dev/crucible0 --rw=randrw --bs=4k --numjobs=4
   ```

#### Deliverables
- Test suite in tests/
- Performance benchmark results
- Resilience test scenarios
- Bug fixes from testing

#### Success Criteria
- All unit tests pass
- Filesystem integrity maintained
- Degraded mode works (1 downstairs offline)
- Performance within 30% of local virtio-blk
- No data corruption under stress

## Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| 1. Protocol Analysis | 1-2 weeks | None |
| 2. Network Infrastructure | 1 week | Phase 1 |
| 3. Protocol Implementation | 3-4 weeks | Phase 2 |
| 4. Block Device Driver | 2 weeks | Phase 3 |
| 5. Build Integration | 3-4 days | Phase 4 |
| 6. Testing | 2-3 weeks | Phase 5 |
| **Total** | **8-10 weeks** | |

## Risk Mitigation

### Protocol Complexity
- **Risk**: Crucible protocol may be more complex than anticipated
- **Mitigation**: Thorough analysis in Phase 1, reference Rust implementation
- **Fallback**: Simpler NBD (Network Block Device) protocol

### Performance Issues
- **Risk**: C++ implementation may have higher overhead
- **Mitigation**: Profile early and often, optimize critical paths
- **Fallback**: Accept higher latency for distributed benefits

### Network Stack Limitations
- **Risk**: OSv networking may not handle Crucible well
- **Mitigation**: Test early with simple connectivity tests
- **Fallback**: Enhanced network stack (separate project)

### Quorum Logic Bugs
- **Risk**: Subtle bugs in 2/3 quorum implementation
- **Mitigation**: Extensive unit tests, state machine testing
- **Fallback**: Single-replica mode for testing

## Success Criteria

- ✅ /dev/crucible0 appears and is usable
- ✅ Read/write operations work correctly
- ✅ Data integrity verified (checksum tests)
- ✅ ZFS/EXT4 filesystems work on Crucible
- ✅ Degraded mode works (1 downstairs offline)
- ✅ Performance within 30% of local virtio-blk
- ✅ Quorum logic functions correctly

## Future Enhancements

- Multiple Crucible devices (/dev/crucible0, /dev/crucible1, ...)
- Snapshots and clones
- Encryption support
- Compression
- Monitoring and statistics (phi-accrued failure detector)
- Async I/O for better performance
- Connection pooling

## References

- [Crucible GitHub](https://github.com/oxidecomputer/crucible)
- [Oxide Computer Blog](https://oxide.computer/)
- OSv Block Device: drivers/virtio-blk.cc (reference implementation)
- OSv Networking: bsd/sys/sys/socket.h
