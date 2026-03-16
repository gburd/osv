# Crucible Protocol Specification (C++ Implementation Guide)

## Overview

Crucible uses a binary protocol over TLS-encrypted TCP connections. Messages are serialized using Rust's `bincode` format with a 4-byte length prefix.

**Protocol Version**: V13 (current)

## Wire Format

```
┌─────────────────┬────────────────────────────┐
│ Length (4 bytes)│ Bincode Message (N bytes)  │
│    (LE u32)     │                            │
└─────────────────┴────────────────────────────┘
```

- **Length field**: Little-endian 32-bit unsigned integer
- **Maximum frame size**: 100 MB
- **Length includes**: All data after the 4-byte length field
- **Serialization**: Bincode format with explicit discriminants

## Network Layer

### Connection Requirements

- **Transport**: TCP with TLS encryption (mTLS)
- **Encryption**: TLS for metadata, AES-GCM-SIV-256 for data
- **Certificate verification**: Required (mtLS)
- **Clock synchronization**: Required for TLS certificate validation

### TLS Notes for C++ Implementation

```cpp
// TLS connection example (using OpenSSL or similar)
SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

// Load certificates
SSL_CTX_use_certificate_file(ctx, cert_path, SSL_FILETYPE_PEM);
SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM);

// Connect
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, ...);

SSL* ssl = SSL_new(ctx);
SSL_set_fd(ssl, sock);
SSL_connect(ssl);
```

## Bincode Serialization Format

### Primitive Types

| Rust Type | Size | C++ Equivalent | Encoding |
|-----------|------|----------------|----------|
| u8 | 1 byte | uint8_t | Little-endian |
| u16 | 2 bytes | uint16_t | Little-endian |
| u32 | 4 bytes | uint32_t | Little-endian |
| u64 | 8 bytes | uint64_t | Little-endian |
| bool | 1 byte | bool | 0=false, 1=true |

### Complex Types

**UUID (16 bytes)**:
```
┌────────────────────────────┐
│ 16 bytes (raw UUID bytes)  │
└────────────────────────────┘
```

```cpp
struct Uuid {
    uint8_t bytes[16];
};
```

**String**:
```
┌─────────────┬──────────────┐
│ Length (u64)│ UTF-8 bytes  │
└─────────────┴──────────────┘
```

```cpp
struct BincodeString {
    uint64_t length;
    char* data;
};
```

**Vec<T>**:
```
┌─────────────┬────────────────────┐
│ Length (u64)│ Elements (N × T)   │
└─────────────┴────────────────────┘
```

```cpp
template<typename T>
struct BincodeVec {
    uint64_t length;
    T* elements;
};
```

**Option<T>**:
```
┌───────────────┬──────────────────┐
│ Tag (1 byte)  │ Value (if Some)  │
│ 0=None, 1=Some│                  │
└───────────────┴──────────────────┘
```

```cpp
template<typename T>
struct BincodeOption {
    uint8_t tag;  // 0=None, 1=Some
    T value;      // Only valid if tag==1
};
```

**Result<T, E>**:
```
┌───────────────┬──────────────────┐
│ Tag (1 byte)  │ Value (T or E)   │
│ 0=Ok, 1=Err   │                  │
└───────────────┴──────────────────┘
```

**Enum**:
```
┌──────────────────┬────────────────┐
│ Discriminant(u32)│ Variant Data   │
└──────────────────┴────────────────┘
```

## Message Protocol Version

```cpp
enum class ProtocolVersion : uint32_t {
    V1 = 1,
    V2 = 2,
    // ... (version history omitted)
    V13 = 13,  // Current version
};
```

**Version Negotiation**:
1. Upstairs sends `HereIAm` with its protocol version
2. Downstairs responds with `YesItsMe` if compatible
3. Downstairs responds with `VersionMismatch` if incompatible

## Message Types

### Message Enum (discriminants)

```cpp
enum class MessageType : uint32_t {
    // Negotiation
    HereIAm = 0,
    YesItsMe = 1,
    VersionMismatch = 2,
    ReadOnlyMismatch = 3,
    EncryptedMismatch = 4,
    UuidMismatch = 5,

    // Metadata
    RegionInfoPlease = 6,
    RegionInfo = 7,
    ExtentVersionsPlease = 8,
    ExtentVersions = 9,
    LastFlush = 10,

    // IO Operations
    Write = 11,
    WriteAck = 12,
    WriteUnwritten = 13,
    WriteUnwrittenAck = 14,
    ReadRequest = 15,
    ReadResponse = 16,
    Flush = 17,
    FlushAck = 18,
    Barrier = 19,
    BarrierAck = 20,

    // Reconciliation (startup)
    ExtentClose = 21,
    ExtentReopen = 22,
    ExtentFlush = 23,
    ExtentRepair = 24,
    RepairAckId = 25,
    ExtentError = 26,

    // Live Repair (active)
    ExtentLiveClose = 27,
    ExtentLiveFlushClose = 28,
    ExtentLiveRepair = 29,
    ExtentLiveReopen = 30,
    ExtentLiveNoOp = 31,
    ExtentLiveCloseAck = 32,
    ExtentLiveRepairAckId = 33,
    ExtentLiveAckId = 34,

    // Control
    Ruok = 35,
    Imok = 36,
    PromoteToActive = 37,
    YouAreNowActive = 38,
    YouAreNoLongerActive = 39,
    ErrorReport = 40,
};
```

## Connection Handshake

### Phase 1: Initial Negotiation

```
Upstairs                                Downstairs
   │                                          │
   │──────── HereIAm ────────────────────────>│
   │                                          │
   │<─────── YesItsMe ───────────────────────│
   │   or                                     │
   │<─────── VersionMismatch ────────────────│
   │<─────── ReadOnlyMismatch ───────────────│
   │<─────── EncryptedMismatch ──────────────│
   │<─────── UuidMismatch ────────────────────│
```

**HereIAm Message**:
```cpp
struct HereIAm {
    uint32_t discriminant = 0;  // MessageType::HereIAm
    uint32_t version;           // Protocol version (13)
    Uuid upstairs_id;           // Persistent upstairs UUID
    Uuid session_id;            // Session UUID (changes per connection)
    uint64_t gen;               // Generation number
    bool read_only;             // Read-only mode flag
    bool encrypted;             // Encryption expectation flag
    bool alternate_region;      // Alternate region support
};
```

**YesItsMe Response**:
```cpp
struct YesItsMe {
    uint32_t discriminant = 1;  // MessageType::YesItsMe
    uint32_t version;           // Protocol version
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t gen;
    bool repair_addr_set;       // Repair address available
    SocketAddr repair_addr;     // Optional repair endpoint
};
```

### Phase 2: Region Information

```
Upstairs                                Downstairs
   │                                          │
   │────── RegionInfoPlease ──────────────────>│
   │                                          │
   │<────── RegionInfo ───────────────────────│
```

**RegionInfo Response**:
```cpp
struct RegionInfo {
    uint32_t discriminant = 7;
    RegionDefinition region_def;
};

struct RegionDefinition {
    uint32_t block_size;        // Bytes per block (e.g., 512, 4096)
    uint64_t extent_size;       // Blocks per extent
    Uuid uuid;                  // Region UUID
    bool encrypted;             // Encryption enabled
    uint64_t extent_count;      // Number of extents
    // ... additional fields
};
```

## IO Operations

### Write Operation

```
Upstairs                                Downstairs
   │                                          │
   │────── Write(header + data) ──────────────>│
   │                                          │
   │<────── WriteAck ─────────────────────────│
```

**Write Message**:
```cpp
struct WriteHeader {
    uint32_t discriminant = 11;  // MessageType::Write
    Uuid upstairs_id;            // Persistent upstairs UUID
    Uuid session_id;             // Current session UUID
    uint64_t job_id;             // Unique job identifier
    Vec<uint64_t> dependencies;  // Jobs that must complete first
    uint64_t start_block;        // Starting block index
    Vec<BlockContext> contexts;  // Per-block metadata
};

struct BlockContext {
    uint64_t hash;               // xxHash u64
    Option<EncryptionContext> encryption_context;
};

struct EncryptionContext {
    uint8_t nonce[12];           // AES-GCM-SIV nonce
    uint8_t tag[16];             // Authentication tag
};
```

**C++ Implementation**:
```cpp
// Write operation
void send_write(uint64_t job_id, uint64_t start_block,
                const std::vector<uint8_t>& data,
                const std::vector<uint64_t>& dependencies) {
    // Calculate hashes for each block
    std::vector<BlockContext> contexts;
    for (size_t i = 0; i < data.size() / block_size; i++) {
        const uint8_t* block = &data[i * block_size];
        uint64_t hash = xxhash64(block, block_size);
        contexts.push_back({hash, std::nullopt});
    }

    // Serialize WriteHeader
    WriteHeader header{
        .discriminant = 11,
        .upstairs_id = upstairs_id_,
        .session_id = session_id_,
        .job_id = job_id,
        .dependencies = dependencies,
        .start_block = start_block,
        .contexts = contexts
    };

    // Encode with bincode
    std::vector<uint8_t> encoded = bincode_encode(header);

    // Add data payload
    encoded.insert(encoded.end(), data.begin(), data.end());

    // Send with length prefix
    uint32_t length = encoded.size();
    ssl_write(&length, 4);
    ssl_write(encoded.data(), encoded.size());
}
```

**WriteAck Response**:
```cpp
struct WriteAck {
    uint32_t discriminant = 12;
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t job_id;
    Result<(), CrucibleError> result;
};
```

### Read Operation

```
Upstairs                                Downstairs
   │                                          │
   │────── ReadRequest ───────────────────────>│
   │                                          │
   │<────── ReadResponse(header + data) ──────│
```

**ReadRequest Message**:
```cpp
struct ReadRequest {
    uint32_t discriminant = 15;
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t job_id;
    Vec<uint64_t> dependencies;
    uint64_t start_block;
    uint64_t count;              // Number of blocks to read
};
```

**ReadResponse Message**:
```cpp
struct ReadResponseHeader {
    uint32_t discriminant = 16;
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t job_id;
    Result<Vec<ReadBlockContext>, CrucibleError> blocks;
};

enum class ReadBlockContext : uint32_t {
    Empty = 0,
    Encrypted = 1,
    Unencrypted = 2,
};

// If Encrypted (discriminant 1):
struct ReadBlockContextEncrypted {
    EncryptionContext ctx;
};

// If Unencrypted (discriminant 2):
struct ReadBlockContextUnencrypted {
    uint64_t hash;               // xxHash for verification
};
```

**C++ Implementation**:
```cpp
// Read operation
std::vector<uint8_t> send_read(uint64_t job_id, uint64_t start_block, uint64_t count) {
    // Send request
    ReadRequest req{
        .discriminant = 15,
        .upstairs_id = upstairs_id_,
        .session_id = session_id_,
        .job_id = job_id,
        .dependencies = {},
        .start_block = start_block,
        .count = count
    };

    std::vector<uint8_t> encoded = bincode_encode(req);
    send_frame(encoded);

    // Receive response
    auto frame = receive_frame();
    ReadResponseHeader header = bincode_decode<ReadResponseHeader>(frame);

    if (!header.blocks.is_ok()) {
        throw std::runtime_error("Read failed");
    }

    // Read data payload after header
    std::vector<uint8_t> data(count * block_size);
    ssl_read(data.data(), data.size());

    // Verify hashes
    for (size_t i = 0; i < header.blocks.value().size(); i++) {
        auto& ctx = header.blocks.value()[i];
        if (ctx.discriminant == 2) {  // Unencrypted
            const uint8_t* block = &data[i * block_size];
            uint64_t computed_hash = xxhash64(block, block_size);
            if (computed_hash != ctx.hash) {
                throw std::runtime_error("Hash mismatch");
            }
        }
    }

    return data;
}
```

### Flush Operation

```
Upstairs                                Downstairs
   │                                          │
   │────── Flush ──────────────────────────────>│
   │                                          │
   │<────── FlushAck ─────────────────────────│
```

**Flush Message**:
```cpp
struct Flush {
    uint32_t discriminant = 17;
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t job_id;
    Vec<uint64_t> dependencies;
    uint64_t flush_number;       // Monotonically increasing
    uint64_t gen_number;         // Generation number
    Option<uint64_t> snapshot_details;  // Optional snapshot info
    uint64_t extent_limit;       // Extent boundary
};
```

**FlushAck Response**:
```cpp
struct FlushAck {
    uint32_t discriminant = 18;
    Uuid upstairs_id;
    Uuid session_id;
    uint64_t job_id;
    Result<(), CrucibleError> result;
};
```

## Data Integrity

### xxHash Algorithm

Crucible uses xxHash (64-bit variant) for block integrity:

```cpp
#include <xxhash.h>

uint64_t compute_block_hash(const uint8_t* block, size_t block_size) {
    return XXH64(block, block_size, 0);  // seed=0
}
```

**Hash Properties**:
- **Algorithm**: xxHash64
- **Output**: 64-bit unsigned integer
- **Speed**: RAM speed limits (~GB/s)
- **Collision resistance**: Good for error detection

### Encryption

**Algorithm**: AES-256-GCM-SIV

```cpp
struct EncryptionContext {
    uint8_t nonce[12];           // 96-bit nonce
    uint8_t tag[16];             // 128-bit authentication tag
};
```

**Encryption Flow** (for reference, OSv implementation may not need this):
1. Generate random nonce
2. Encrypt block with AES-GCM-SIV
3. Compute authentication tag
4. Include nonce + tag in BlockContext
5. Downstairs validates tag on write

## Error Handling

### CrucibleError Type

```cpp
enum class CrucibleError : uint32_t {
    GenNumberMismatch = 0,
    IoError = 1,
    DecryptionError = 2,
    HashMismatch = 3,
    // ... more error types
};
```

**Error in Result<T, E>**:
```
┌───────────────┬──────────────────────┐
│ Tag = 1 (Err) │ CrucibleError (u32)  │
└───────────────┴──────────────────────┘
```

## Control Messages

### Health Check

```
Upstairs                                Downstairs
   │                                          │
   │────── Ruok ──────────────────────────────>│
   │                                          │
   │<────── Imok ─────────────────────────────│
```

```cpp
struct Ruok {
    uint32_t discriminant = 35;
};

struct Imok {
    uint32_t discriminant = 36;
};
```

## Job ID Management

- **Job ID**: Unique 64-bit identifier per operation
- **Monotonically increasing**: Each new operation gets next ID
- **Dependencies**: Vec<u64> lists jobs that must complete first

```cpp
class JobIdManager {
    std::atomic<uint64_t> next_id_{1};
public:
    uint64_t allocate() {
        return next_id_.fetch_add(1, std::memory_order_relaxed);
    }
};
```

## Example Message Flows

### Successful Write

```
1. Upstairs → Write(job_id=1, start=0, count=10, data=[...])
2. Downstairs processes write
3. Downstairs → WriteAck(job_id=1, result=Ok)
```

### Read with Verification

```
1. Upstairs → ReadRequest(job_id=2, start=0, count=10)
2. Downstairs reads blocks
3. Downstairs → ReadResponse(job_id=2, blocks=[ctx0..ctx9]) + data
4. Upstairs verifies hashes
```

### Write with Dependencies

```
1. Upstairs → Write(job_id=3, dependencies=[1, 2], ...)
2. Downstairs waits for jobs 1 and 2
3. Downstairs → WriteAck(job_id=3, result=Ok)
```

## C++ Implementation Skeleton

```cpp
namespace crucible {

class Downstairs Connection {
    SSL* ssl_;
    Uuid upstairs_id_;
    Uuid session_id_;
    uint64_t gen_;

public:
    // Send message with length prefix
    void send_frame(const std::vector<uint8_t>& data) {
        uint32_t length = data.size();
        SSL_write(ssl_, &length, 4);
        SSL_write(ssl_, data.data(), data.size());
    }

    // Receive message with length prefix
    std::vector<uint8_t> receive_frame() {
        uint32_t length;
        SSL_read(ssl_, &length, 4);

        if (length > 100 * 1024 * 1024) {
            throw std::runtime_error("Frame too large");
        }

        std::vector<uint8_t> data(length);
        SSL_read(ssl_, data.data(), length);
        return data;
    }

    // Handshake
    void handshake(ProtocolVersion version, bool read_only, bool encrypted) {
        // Send HereIAm
        HereIAm msg{
            .discriminant = 0,
            .version = static_cast<uint32_t>(version),
            .upstairs_id = upstairs_id_,
            .session_id = session_id_,
            .gen = gen_,
            .read_only = read_only,
            .encrypted = encrypted,
            .alternate_region = false
        };

        auto encoded = bincode_encode(msg);
        send_frame(encoded);

        // Receive YesItsMe
        auto response = receive_frame();
        auto yes = bincode_decode<YesItsMe>(response);

        // Validate response
        if (yes.upstairs_id != upstairs_id_) {
            throw std::runtime_error("UUID mismatch");
        }
    }
};

} // namespace crucible
```

## Implementation Checklist

- [ ] TLS connection wrapper
- [ ] Bincode serialization library (or manual implementation)
- [ ] xxHash integration
- [ ] Message type definitions (all enums/structs)
- [ ] Send/receive with length framing
- [ ] Handshake protocol (HereIAm/YesItsMe)
- [ ] Region info query
- [ ] Write operation with dependencies
- [ ] Read operation with hash verification
- [ ] Flush operation
- [ ] Error handling (Result type)
- [ ] Job ID management
- [ ] Quorum logic (2/3 responses)
- [ ] Connection health checks (Ruok/Imok)

## References

- [Crucible GitHub](https://github.com/oxidecomputer/crucible)
- [RFD 177: Crucible Design](https://rfd.shared.oxide.computer/rfd/0177)
- [Bincode Format](https://github.com/bincode-org/bincode)
- [xxHash](https://github.com/Cyan4973/xxHash)
