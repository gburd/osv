/*
 * Copyright (C) 2024 Waldemar Kozaczuk
 * Copyright (C) 2024 OSv Contributors
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#include "crucible-client.hh"
#include "crucible-messages.hh"
#include <osv/debug.h>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace crucible;

namespace crucible {

// Helper: Generate random UUID
Uuid generate_uuid()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint8_t> dis(0, 255);

    Uuid uuid;
    for (int i = 0; i < 16; i++) {
        uuid.bytes[i] = dis(gen);
    }

    // Set version (4) and variant bits
    uuid.bytes[6] = (uuid.bytes[6] & 0x0F) | 0x40;  // Version 4
    uuid.bytes[8] = (uuid.bytes[8] & 0x3F) | 0x80;  // Variant 10

    return uuid;
}

// Helper: Parse "host:port" string
std::pair<std::string, uint16_t> parse_target_string(const std::string& target)
{
    auto colon = target.rfind(':');
    if (colon == std::string::npos) {
        throw std::runtime_error("Invalid target format (expected host:port): " + target);
    }

    std::string host = target.substr(0, colon);
    std::string port_str = target.substr(colon + 1);

    try {
        uint16_t port = static_cast<uint16_t>(std::stoul(port_str));
        return {host, port};
    } catch (...) {
        throw std::runtime_error("Invalid port number: " + port_str);
    }
}

// UpsairsClient implementation

UpsairsClient::UpsairsClient(const std::vector<std::string>& targets,
                             const Uuid& region_uuid,
                             uint32_t block_size,
                             uint64_t total_blocks,
                             bool read_only,
                             bool encrypted)
    : targets_(targets)
    , region_uuid_(region_uuid)
    , upstairs_id_(generate_uuid())
    , session_id_(generate_uuid())
    , block_size_(block_size)
    , total_blocks_(total_blocks)
    , read_only_(read_only)
    , encrypted_(encrypted)
{
    if (targets.size() != 3) {
        throw std::runtime_error("Crucible requires exactly 3 downstairs targets");
    }

    if (block_size == 0 || (block_size & (block_size - 1)) != 0) {
        throw std::runtime_error("Block size must be power of 2");
    }
}

UpsairsClient::~UpsairsClient()
{
    disconnect();
}

void UpsairsClient::connect()
{
    if (running_) {
        return;  // Already connected
    }

    // Parse targets and establish connections
    for (size_t i = 0; i < 3; i++) {
        try {
            auto [host, port] = parse_target_string(targets_[i]);
            connections_[i] = std::make_unique<Connection>(host, port);
            connected_count_++;

            debug("[Crucible] Connected to downstairs %zu: %s:%u\n",
                  i, host.c_str(), port);

        } catch (const std::exception& e) {
            debug("[Crucible] Failed to connect to downstairs %zu (%s): %s\n",
                  i, targets_[i].c_str(), e.what());
        }
    }

    if (connected_count_ < 2) {
        disconnect();
        throw std::runtime_error("Failed to connect to at least 2 downstairs servers");
    }

    // Perform handshake with all connected downstairs
    for (size_t i = 0; i < 3; i++) {
        if (connections_[i] && connections_[i]->is_connected()) {
            try {
                handshake(i);
                query_region_info(i);
            } catch (const std::exception& e) {
                debug("[Crucible] Handshake/query failed for downstairs %zu: %s\n",
                      i, e.what());
                connections_[i]->close();
                connections_[i].reset();
                connected_count_--;
            }
        }
    }

    if (connected_count_ < 2) {
        disconnect();
        throw std::runtime_error("Failed to complete handshake with at least 2 downstairs");
    }

    // Start I/O thread
    running_ = true;
    io_thread_ = sched::thread::make([this] { this->io_loop(); });
    io_thread_->start();

    debug("[Crucible] Upstairs client connected (%d/3 downstairs)\n",
          connected_count_.load());
}

void UpsairsClient::disconnect()
{
    if (running_) {
        running_ = false;

        if (io_thread_) {
            io_thread_->join();
            delete io_thread_;
            io_thread_ = nullptr;
        }
    }

    // Close connections
    for (auto& conn : connections_) {
        if (conn) {
            conn->close();
            conn.reset();
        }
    }

    connected_count_ = 0;

    // Cancel pending requests
    request_mgr_.cancel_all();
}

bool UpsairsClient::is_connected() const
{
    return connected_count_ >= 2;
}

int UpsairsClient::read_sync(uint64_t offset, uint32_t length, void* buffer)
{
    if (!is_connected()) {
        return EIO;
    }

    // Validate parameters
    if (offset + length > total_size()) {
        return EINVAL;
    }

    if (length % block_size_ != 0 || offset % block_size_ != 0) {
        return EINVAL;
    }

    // TODO: Implement read protocol (Phase 3)
    // For now, return error
    debug("[Crucible] read_sync not yet implemented (Phase 3)\n");
    return ENOSYS;
}

int UpsairsClient::write_sync(uint64_t offset, uint32_t length, const void* buffer)
{
    if (!is_connected()) {
        return EIO;
    }

    if (read_only_) {
        return EROFS;
    }

    // Validate parameters
    if (offset + length > total_size()) {
        return EINVAL;
    }

    if (length % block_size_ != 0 || offset % block_size_ != 0) {
        return EINVAL;
    }

    // TODO: Implement write protocol (Phase 3)
    // For now, return error
    debug("[Crucible] write_sync not yet implemented (Phase 3)\n");
    return ENOSYS;
}

int UpsairsClient::flush_sync()
{
    if (!is_connected()) {
        return EIO;
    }

    if (read_only_) {
        return 0;  // No-op for read-only
    }

    // TODO: Implement flush protocol (Phase 3)
    // For now, return success
    debug("[Crucible] flush_sync not yet implemented (Phase 3)\n");
    return 0;
}

std::pair<std::string, uint16_t> UpsairsClient::parse_target(const std::string& target)
{
    return parse_target_string(target);
}

void UpsairsClient::io_loop()
{
    debug("[Crucible] I/O thread started\n");

    while (running_) {
        // TODO: Implement response processing (Phase 3)
        // For now, just sleep
        sched::thread::sleep(std::chrono::milliseconds(100));
    }

    debug("[Crucible] I/O thread stopped\n");
}

void UpsairsClient::process_responses(int downstairs_idx)
{
    // TODO: Implement in Phase 3
}

void UpsairsClient::handshake(int downstairs_idx)
{
    auto& conn = connections_[downstairs_idx];
    if (!conn || !conn->is_connected()) {
        throw ConnectionError("Downstairs not connected");
    }

    // Send HereIAm
    HereIAm here_msg;
    here_msg.version = static_cast<uint32_t>(ProtocolVersion::V13);
    here_msg.upstairs_id = upstairs_id_;
    here_msg.session_id = session_id_;
    here_msg.gen = generation_;
    here_msg.read_only = read_only_;
    here_msg.encrypted = encrypted_;
    here_msg.alternate_region = false;

    auto frame = encode_message(here_msg);
    conn->send(frame.data(), frame.size());

    debug("[Crucible] Sent HereIAm to downstairs %d\n", downstairs_idx);

    // Receive response
    auto response_frame = receive_frame(downstairs_idx);
    bincode::Decoder dec(response_frame);

    MessageType type = decode_message_type(dec);

    if (type == MessageType::YesItsMe) {
        auto yes_msg = YesItsMe::decode(dec);

        if (yes_msg.upstairs_id != upstairs_id_) {
            throw std::runtime_error("UUID mismatch in YesItsMe");
        }

        debug("[Crucible] Handshake successful with downstairs %d\n", downstairs_idx);

    } else if (type == MessageType::VersionMismatch) {
        auto mismatch = VersionMismatch::decode(dec);
        throw std::runtime_error("Version mismatch: offered " +
                                std::to_string(mismatch.offered));

    } else if (type == MessageType::ReadOnlyMismatch) {
        throw std::runtime_error("Read-only mode mismatch");

    } else if (type == MessageType::EncryptedMismatch) {
        throw std::runtime_error("Encryption mode mismatch");

    } else if (type == MessageType::UuidMismatch) {
        throw std::runtime_error("Region UUID mismatch");

    } else {
        throw std::runtime_error("Unexpected handshake response: " +
                                std::to_string(static_cast<uint32_t>(type)));
    }
}

void UpsairsClient::query_region_info(int downstairs_idx)
{
    auto& conn = connections_[downstairs_idx];
    if (!conn || !conn->is_connected()) {
        throw ConnectionError("Downstairs not connected");
    }

    // Send RegionInfoPlease
    RegionInfoPlease req_msg;
    auto frame = encode_message(req_msg);
    conn->send(frame.data(), frame.size());

    debug("[Crucible] Sent RegionInfoPlease to downstairs %d\n", downstairs_idx);

    // Receive RegionInfo response
    auto response_frame = receive_frame(downstairs_idx);
    bincode::Decoder dec(response_frame);

    MessageType type = decode_message_type(dec);
    if (type != MessageType::RegionInfo) {
        throw std::runtime_error("Expected RegionInfo, got " +
                                std::to_string(static_cast<uint32_t>(type)));
    }

    auto info_msg = RegionInfo::decode(dec);
    region_def_ = info_msg.region_def;

    // Validate region definition
    if (region_def_.block_size != block_size_) {
        throw std::runtime_error("Block size mismatch: expected " +
                                std::to_string(block_size_) + ", got " +
                                std::to_string(region_def_.block_size));
    }

    debug("[Crucible] Region info: block_size=%u, extent_size=%lu, extents=%lu\n",
          region_def_.block_size, region_def_.extent_size, region_def_.extent_count);
}

void UpsairsClient::send_frame(int downstairs_idx, const std::vector<uint8_t>& data)
{
    auto& conn = connections_[downstairs_idx];
    if (!conn || !conn->is_connected()) {
        throw ConnectionError("Downstairs not connected");
    }

    // Send length prefix (4 bytes, little-endian)
    uint32_t length = data.size();
    conn->send(&length, 4);

    // Send data
    conn->send(data.data(), data.size());
}

std::vector<uint8_t> UpsairsClient::receive_frame(int downstairs_idx)
{
    auto& conn = connections_[downstairs_idx];
    if (!conn || !conn->is_connected()) {
        throw ConnectionError("Downstairs not connected");
    }

    // Receive length prefix
    uint32_t length;
    conn->recv_exact(&length, 4);

    // Validate length
    if (length > 100 * 1024 * 1024) {  // 100 MB max
        throw std::runtime_error("Frame too large");
    }

    // Receive data
    std::vector<uint8_t> data(length);
    conn->recv_exact(data.data(), length);

    return data;
}

} // namespace crucible
