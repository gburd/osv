#!/usr/bin/env python3
"""
Mock Crucible Downstairs Server for Testing

This is a minimal implementation of a Crucible downstairs server that can
respond to the Crucible protocol handshake and basic I/O operations. It's
designed for testing the OSv Crucible driver without needing real Crucible
infrastructure.

Protocol Overview:
- All messages are prefixed with 4-byte little-endian length
- Messages use bincode serialization (Rust format)
- Handshake: HereIAm -> YesItsMe
- Region info: RegionInfoPlease -> RegionInfo
- I/O operations: ReadRequest/Write/Flush -> responses
"""

import socket
import struct
import sys
import threading
import time
from dataclasses import dataclass
from typing import Optional

# Protocol version
PROTOCOL_VERSION = 13

# Message type discriminants
class MessageType:
    HERE_I_AM = 0
    YES_ITS_ME = 1
    VERSION_MISMATCH = 2
    READ_ONLY_MISMATCH = 3
    ENCRYPTED_MISMATCH = 4
    UUID_MISMATCH = 5
    REGION_INFO_PLEASE = 6
    REGION_INFO = 7
    WRITE = 11
    WRITE_ACK = 12
    READ_REQUEST = 15
    READ_RESPONSE = 16
    FLUSH = 17
    FLUSH_ACK = 18
    RUOK = 35
    IMOK = 36

@dataclass
class RegionConfig:
    """Configuration for the mock region"""
    block_size: int = 4096
    extent_size: int = 256  # blocks per extent
    extent_count: int = 100
    uuid: bytes = b'\x00' * 16
    encrypted: bool = False

def encode_u8(value: int) -> bytes:
    return struct.pack('<B', value)

def encode_u16(value: int) -> bytes:
    return struct.pack('<H', value)

def encode_u32(value: int) -> bytes:
    return struct.pack('<I', value)

def encode_u64(value: int) -> bytes:
    return struct.pack('<Q', value)

def encode_bool(value: bool) -> bytes:
    return encode_u8(1 if value else 0)

def encode_uuid(uuid: bytes) -> bytes:
    assert len(uuid) == 16
    return uuid

def decode_u8(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from('<B', data, offset)[0], offset + 1

def decode_u32(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from('<I', data, offset)[0], offset + 4

def decode_u64(data: bytes, offset: int) -> tuple[int, int]:
    return struct.unpack_from('<Q', data, offset)[0], offset + 8

def decode_bool(data: bytes, offset: int) -> tuple[bool, int]:
    val, offset = decode_u8(data, offset)
    return bool(val), offset

def decode_uuid(data: bytes, offset: int) -> tuple[bytes, int]:
    return data[offset:offset+16], offset + 16

def decode_vec_u64(data: bytes, offset: int) -> tuple[list, int]:
    """Decode a vector of u64 values"""
    length, offset = decode_u64(data, offset)
    values = []
    for _ in range(length):
        val, offset = decode_u64(data, offset)
        values.append(val)
    return values, offset

def recv_frame(conn: socket.socket) -> Optional[bytes]:
    """Receive a length-prefixed frame"""
    length_bytes = conn.recv(4)
    if len(length_bytes) != 4:
        return None

    length = struct.unpack('<I', length_bytes)[0]
    if length > 100 * 1024 * 1024:  # 100 MB max
        print(f"Frame too large: {length} bytes", file=sys.stderr)
        return None

    data = b''
    while len(data) < length:
        chunk = conn.recv(min(length - len(data), 8192))
        if not chunk:
            return None
        data += chunk

    return data

def send_frame(conn: socket.socket, data: bytes):
    """Send a length-prefixed frame"""
    length = struct.pack('<I', len(data))
    conn.sendall(length + data)

class MockDownstairsServer:
    def __init__(self, port: int, region: RegionConfig, server_id: int):
        self.port = port
        self.region = region
        self.server_id = server_id
        self.running = False
        self.sock = None
        self.thread = None

        # Store session state
        self.upstairs_id = None
        self.session_id = None
        self.generation = 0
        self.read_only = False

        # Simple in-memory storage
        total_blocks = region.extent_size * region.extent_count
        self.storage = bytearray(total_blocks * region.block_size)

    def start(self):
        """Start the server in a background thread"""
        self.running = True
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
        time.sleep(0.1)  # Give server time to start

    def stop(self):
        """Stop the server"""
        self.running = False
        if self.sock:
            self.sock.close()
        if self.thread:
            self.thread.join(timeout=2)

    def _run(self):
        """Main server loop"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        try:
            self.sock.bind(('0.0.0.0', self.port))
            self.sock.listen(1)
            self.sock.settimeout(1.0)
            print(f"[Server {self.server_id}] Listening on port {self.port}")

            while self.running:
                try:
                    conn, addr = self.sock.accept()
                    print(f"[Server {self.server_id}] Connection from {addr}")
                    self._handle_client(conn)
                except socket.timeout:
                    continue
                except Exception as e:
                    if self.running:
                        print(f"[Server {self.server_id}] Accept error: {e}")
        except Exception as e:
            print(f"[Server {self.server_id}] Server error: {e}")
        finally:
            if self.sock:
                self.sock.close()

    def _handle_client(self, conn: socket.socket):
        """Handle a client connection"""
        conn.settimeout(10.0)

        try:
            while self.running:
                frame = recv_frame(conn)
                if frame is None:
                    break

                self._process_message(conn, frame)
        except Exception as e:
            print(f"[Server {self.server_id}] Client handler error: {e}")
        finally:
            conn.close()
            print(f"[Server {self.server_id}] Connection closed")

    def _process_message(self, conn: socket.socket, data: bytes):
        """Process a received message"""
        if len(data) < 4:
            return

        msg_type, offset = decode_u32(data, 0)

        if msg_type == MessageType.HERE_I_AM:
            self._handle_here_i_am(conn, data, offset)
        elif msg_type == MessageType.REGION_INFO_PLEASE:
            self._handle_region_info_please(conn)
        elif msg_type == MessageType.READ_REQUEST:
            self._handle_read_request(conn, data, offset)
        elif msg_type == MessageType.WRITE:
            self._handle_write(conn, data, offset)
        elif msg_type == MessageType.FLUSH:
            self._handle_flush(conn, data, offset)
        elif msg_type == MessageType.RUOK:
            self._handle_ruok(conn)
        else:
            print(f"[Server {self.server_id}] Unknown message type: {msg_type}")

    def _handle_here_i_am(self, conn: socket.socket, data: bytes, offset: int):
        """Handle HereIAm handshake"""
        version, offset = decode_u32(data, offset)
        upstairs_id, offset = decode_uuid(data, offset)
        session_id, offset = decode_uuid(data, offset)
        gen, offset = decode_u64(data, offset)
        read_only, offset = decode_bool(data, offset)
        encrypted, offset = decode_bool(data, offset)

        print(f"[Server {self.server_id}] HereIAm: version={version}, gen={gen}, "
              f"read_only={read_only}, encrypted={encrypted}")

        # Store session info
        self.upstairs_id = upstairs_id
        self.session_id = session_id
        self.generation = gen
        self.read_only = read_only

        # Build YesItsMe response
        response = encode_u32(MessageType.YES_ITS_ME)
        response += encode_u32(PROTOCOL_VERSION)
        response += encode_uuid(upstairs_id)
        response += encode_uuid(session_id)
        response += encode_u64(gen)
        response += encode_bool(False)  # No repair address

        send_frame(conn, response)
        print(f"[Server {self.server_id}] Sent YesItsMe")

    def _handle_region_info_please(self, conn: socket.socket):
        """Handle RegionInfoPlease request"""
        print(f"[Server {self.server_id}] RegionInfoPlease")

        # Build RegionInfo response
        response = encode_u32(MessageType.REGION_INFO)
        response += encode_u32(self.region.block_size)
        response += encode_u64(self.region.extent_size)
        response += encode_uuid(self.region.uuid)
        response += encode_bool(self.region.encrypted)
        response += encode_u64(self.region.extent_count)

        send_frame(conn, response)
        print(f"[Server {self.server_id}] Sent RegionInfo: "
              f"block_size={self.region.block_size}, "
              f"extents={self.region.extent_count}")

    def _handle_read_request(self, conn: socket.socket, data: bytes, offset: int):
        """Handle ReadRequest"""
        upstairs_id, offset = decode_uuid(data, offset)
        session_id, offset = decode_uuid(data, offset)
        job_id, offset = decode_u64(data, offset)
        dependencies, offset = decode_vec_u64(data, offset)
        start_block, offset = decode_u64(data, offset)
        count, offset = decode_u64(data, offset)

        print(f"[Server {self.server_id}] ReadRequest: job_id={job_id}, "
              f"start_block={start_block}, count={count}")

        # Build ReadResponse with dummy data
        response = encode_u32(MessageType.READ_RESPONSE)
        response += encode_uuid(upstairs_id)
        response += encode_uuid(session_id)
        response += encode_u64(job_id)

        # Result::Ok tag
        response += encode_u8(0)

        # Vector of ReadBlockContext (Unencrypted type)
        response += encode_u64(count)  # Vector length
        for i in range(count):
            response += encode_u32(2)  # ReadBlockType::Unencrypted
            response += encode_u64(0x1234567890abcdef)  # Dummy hash

        send_frame(conn, response)

        # Send dummy data blocks
        block_data = bytes(count * self.region.block_size)
        conn.sendall(block_data)

        print(f"[Server {self.server_id}] Sent ReadResponse for job {job_id}")

    def _handle_write(self, conn: socket.socket, data: bytes, offset: int):
        """Handle Write operation"""
        upstairs_id, offset = decode_uuid(data, offset)
        session_id, offset = decode_uuid(data, offset)
        job_id, offset = decode_u64(data, offset)
        dependencies, offset = decode_vec_u64(data, offset)
        start_block, offset = decode_u64(data, offset)

        # Decode block contexts
        context_count, offset = decode_u64(data, offset)

        print(f"[Server {self.server_id}] Write: job_id={job_id}, "
              f"start_block={start_block}, count={context_count}")

        # Skip context data and actual write data
        # (In real implementation, would store the data)

        # Build WriteAck response
        response = encode_u32(MessageType.WRITE_ACK)
        response += encode_uuid(upstairs_id)
        response += encode_uuid(session_id)
        response += encode_u64(job_id)
        response += encode_u8(0)  # Result::Ok

        send_frame(conn, response)
        print(f"[Server {self.server_id}] Sent WriteAck for job {job_id}")

    def _handle_flush(self, conn: socket.socket, data: bytes, offset: int):
        """Handle Flush operation"""
        upstairs_id, offset = decode_uuid(data, offset)
        session_id, offset = decode_uuid(data, offset)
        job_id, offset = decode_u64(data, offset)

        print(f"[Server {self.server_id}] Flush: job_id={job_id}")

        # Build FlushAck response
        response = encode_u32(MessageType.FLUSH_ACK)
        response += encode_uuid(upstairs_id)
        response += encode_uuid(session_id)
        response += encode_u64(job_id)
        response += encode_u8(0)  # Result::Ok

        send_frame(conn, response)
        print(f"[Server {self.server_id}] Sent FlushAck for job {job_id}")

    def _handle_ruok(self, conn: socket.socket):
        """Handle health check"""
        response = encode_u32(MessageType.IMOK)
        send_frame(conn, response)
        print(f"[Server {self.server_id}] Sent Imok")

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <port>")
        sys.exit(1)

    port = int(sys.argv[1])
    server_id = (port - 8800) // 10  # Derive ID from port

    region = RegionConfig(
        block_size=4096,
        extent_size=256,
        extent_count=100,
        uuid=b'\x00' * 16,
        encrypted=False
    )

    server = MockDownstairsServer(port, region, server_id)

    print(f"Starting mock Crucible downstairs server on port {port}")
    server.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print(f"\nShutting down server on port {port}")
        server.stop()

if __name__ == '__main__':
    main()
