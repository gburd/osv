/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef CRUCIBLE_TYPES_HH
#define CRUCIBLE_TYPES_HH

#include <cstdint>
#include <array>
#include <vector>
#include <optional>

namespace crucible {

/**
 * Crucible protocol version.
 */
enum class ProtocolVersion : uint32_t {
    V13 = 13,  // Current version
};

/**
 * 128-bit UUID.
 */
struct Uuid {
    uint8_t bytes[16];

    bool operator==(const Uuid& other) const {
        return std::memcmp(bytes, other.bytes, 16) == 0;
    }

    bool operator!=(const Uuid& other) const {
        return !(*this == other);
    }
};

/**
 * Encryption context for AES-GCM-SIV.
 */
struct EncryptionContext {
    uint8_t nonce[12];  // 96-bit nonce
    uint8_t tag[16];    // 128-bit authentication tag
};

/**
 * Block context containing hash and optional encryption.
 */
struct BlockContext {
    uint64_t hash;                                    // xxHash64
    std::optional<EncryptionContext> encryption_ctx;  // Optional encryption
};

/**
 * Read block context returned in responses.
 */
enum class ReadBlockType : uint32_t {
    Empty = 0,
    Encrypted = 1,
    Unencrypted = 2,
};

struct ReadBlockContext {
    ReadBlockType type;
    uint64_t hash;                                    // For Unencrypted
    std::optional<EncryptionContext> encryption_ctx;  // For Encrypted
};

/**
 * Crucible error codes.
 */
enum class CrucibleError : uint32_t {
    GenNumberMismatch = 0,
    IoError = 1,
    DecryptionError = 2,
    HashMismatch = 3,
    InvalidBlockSize = 4,
    InvalidOffset = 5,
    ConnectionError = 6,
    ProtocolError = 7,
    QuorumFailed = 8,
    Timeout = 9,
};

/**
 * Result type (like Rust Result<T, E>).
 */
template<typename T>
struct Result {
    bool is_ok;
    union {
        T value;
        CrucibleError error;
    };

    Result() : is_ok(false), error(CrucibleError::IoError) {}

    static Result ok(const T& val) {
        Result r;
        r.is_ok = true;
        new (&r.value) T(val);
        return r;
    }

    static Result ok(T&& val) {
        Result r;
        r.is_ok = true;
        new (&r.value) T(std::move(val));
        return r;
    }

    static Result err(CrucibleError e) {
        Result r;
        r.is_ok = false;
        r.error = e;
        return r;
    }

    ~Result() {
        if (is_ok) {
            value.~T();
        }
    }

    // Move constructor
    Result(Result&& other) noexcept : is_ok(other.is_ok) {
        if (is_ok) {
            new (&value) T(std::move(other.value));
        } else {
            error = other.error;
        }
    }

    // Move assignment
    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            if (is_ok) {
                value.~T();
            }
            is_ok = other.is_ok;
            if (is_ok) {
                new (&value) T(std::move(other.value));
            } else {
                error = other.error;
            }
        }
        return *this;
    }

    // Delete copy
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;
};

/**
 * Specialization for void (Result<()> in Rust).
 */
template<>
struct Result<void> {
    bool is_ok;
    CrucibleError error;

    Result() : is_ok(false), error(CrucibleError::IoError) {}

    static Result ok() {
        Result r;
        r.is_ok = true;
        return r;
    }

    static Result err(CrucibleError e) {
        Result r;
        r.is_ok = false;
        r.error = e;
        return r;
    }
};

/**
 * Region definition information.
 */
struct RegionDefinition {
    uint32_t block_size;        // Bytes per block (e.g., 512, 4096)
    uint64_t extent_size;       // Blocks per extent
    Uuid uuid;                  // Region UUID
    bool encrypted;             // Encryption enabled
    uint64_t extent_count;      // Number of extents
};

} // namespace crucible

#endif // CRUCIBLE_TYPES_HH
