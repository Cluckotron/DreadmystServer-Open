#!/usr/bin/env python3
"""
Dreadmyst Server - Auth Flow Test Script
Tests Task 3.8: End-to-End Authentication Flow

This script simulates a game client to verify the server's authentication
and character management handlers work correctly.

Packet format: [uint16_t size][uint16_t opcode][payload...]
All integers are little-endian.
Strings are: [uint16_t length][bytes...]
"""

import socket
import struct
import time
import sys

# =============================================================================
# Opcodes (from GamePacketBase.h)
# =============================================================================
class Opcode:
    Mutual_Ping = 0
    Server_Validate = 100
    Server_CharacterList = 102
    Server_CharaCreateResult = 103
    Client_Authenticate = 300
    Client_CharacterList = 301
    Client_CharCreate = 302
    Client_DeleteCharacter = 303

# =============================================================================
# Auth Result Codes (from AccountDefines.h)
# =============================================================================
class AuthResult:
    Validated = 0
    WrongVersion = 1
    BadPassword = 2
    ServerFull = 3
    Banned = 4

    @staticmethod
    def to_string(code):
        names = {0: "Validated", 1: "WrongVersion", 2: "BadPassword",
                 3: "ServerFull", 4: "Banned"}
        return names.get(code, f"Unknown({code})")

# =============================================================================
# Packet Builder Helpers
# =============================================================================
def pack_string(s: str) -> bytes:
    """Pack a string as [uint16_t length][bytes]"""
    encoded = s.encode('utf-8')
    return struct.pack('<H', len(encoded)) + encoded

def unpack_string(data: bytes, offset: int) -> tuple:
    """Unpack a string, returns (string, new_offset)"""
    length = struct.unpack_from('<H', data, offset)[0]
    offset += 2
    string = data[offset:offset + length].decode('utf-8')
    return string, offset + length

def build_packet(opcode: int, payload: bytes = b'') -> bytes:
    """Build a full packet with size header"""
    # Size includes the 2-byte size header itself
    size = 2 + 2 + len(payload)  # size(2) + opcode(2) + payload
    return struct.pack('<HH', size, opcode) + payload

# =============================================================================
# Packet Builders
# =============================================================================
def build_authenticate(token: str, build_version: int = 0, fingerprint: str = "") -> bytes:
    """Build GP_Client_Authenticate packet"""
    payload = pack_string(token)
    payload += struct.pack('<i', build_version)
    payload += pack_string(fingerprint)
    return build_packet(Opcode.Client_Authenticate, payload)

def build_character_list() -> bytes:
    """Build GP_Client_CharacterList packet (empty payload)"""
    return build_packet(Opcode.Client_CharacterList)

def build_char_create(name: str, class_id: int, gender: int, portrait_id: int) -> bytes:
    """Build GP_Client_CharCreate packet in released-client field order."""
    payload = struct.pack('<Bi', gender, portrait_id)
    payload += pack_string(name)
    payload += struct.pack('<B', class_id)
    return build_packet(Opcode.Client_CharCreate, payload)

def build_delete_character(guid: int) -> bytes:
    """Build GP_Client_DeleteCharacter packet"""
    payload = struct.pack('<I', guid)
    return build_packet(Opcode.Client_DeleteCharacter, payload)

def build_ping() -> bytes:
    """Build GP_Mutual_Ping packet"""
    return build_packet(Opcode.Mutual_Ping)

# =============================================================================
# Packet Parsers
# =============================================================================
def parse_validate(data: bytes) -> dict:
    """Parse GP_Server_Validate response"""
    result = struct.unpack_from('<B', data, 0)[0]
    server_time = struct.unpack_from('<q', data, 1)[0]
    return {'result': result, 'server_time': server_time}

def parse_character_list(data: bytes) -> list:
    """Parse GP_Server_CharacterList response"""
    characters = []
    count = struct.unpack_from('<H', data, 0)[0]
    offset = 2

    for _ in range(count):
        guid = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        name, offset = unpack_string(data, offset)
        class_id = struct.unpack_from('<B', data, offset)[0]
        offset += 1
        gender = struct.unpack_from('<B', data, offset)[0]
        offset += 1
        level = struct.unpack_from('<i', data, offset)[0]
        offset += 4
        portrait = struct.unpack_from('<i', data, offset)[0]
        offset += 4

        characters.append({
            'guid': guid,
            'name': name,
            'class_id': class_id,
            'gender': gender,
            'level': level,
            'portrait': portrait
        })

    return characters

def parse_char_create_result(data: bytes) -> int:
    """Parse GP_Server_CharaCreateResult response"""
    return struct.unpack_from('<B', data, 0)[0]

# =============================================================================
# Network Client
# =============================================================================
class DreadmystTestClient:
    def __init__(self, host: str = '127.0.0.1', port: int = 16383):
        self.host = host
        self.port = port
        self.sock = None
        self.recv_buffer = b''

    def connect(self) -> bool:
        """Connect to the server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            print(f"  ✓ Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"  ✗ Failed to connect: {e}")
            return False

    def disconnect(self):
        """Disconnect from the server"""
        if self.sock:
            self.sock.close()
            self.sock = None

    def send_packet(self, packet: bytes):
        """Send a packet to the server"""
        self.sock.sendall(packet)

    def recv_packet(self, timeout: float = 5.0) -> tuple:
        """
        Receive a packet from the server.
        Returns (opcode, payload) or (None, None) on timeout/error.
        """
        self.sock.settimeout(timeout)

        try:
            # Read until we have enough for the header
            while len(self.recv_buffer) < 4:
                chunk = self.sock.recv(1024)
                if not chunk:
                    return None, None
                self.recv_buffer += chunk

            # Parse size and opcode
            size, opcode = struct.unpack_from('<HH', self.recv_buffer, 0)

            # Read until we have the full packet
            while len(self.recv_buffer) < size:
                chunk = self.sock.recv(1024)
                if not chunk:
                    return None, None
                self.recv_buffer += chunk

            # Extract payload (skip size and opcode)
            payload = self.recv_buffer[4:size]
            self.recv_buffer = self.recv_buffer[size:]

            return opcode, payload

        except socket.timeout:
            return None, None
        except Exception as e:
            print(f"  ✗ Receive error: {e}")
            return None, None

    def wait_for_opcode(self, expected_opcode: int, timeout: float = 5.0) -> bytes:
        """Wait for a specific opcode, handling pings"""
        start = time.time()
        while time.time() - start < timeout:
            opcode, payload = self.recv_packet(timeout=1.0)

            if opcode is None:
                continue

            # Handle ping automatically
            if opcode == Opcode.Mutual_Ping:
                self.send_packet(build_ping())
                continue

            if opcode == expected_opcode:
                return payload
            else:
                print(f"  ? Unexpected opcode {opcode}, expected {expected_opcode}")

        return None

# =============================================================================
# Test Functions
# =============================================================================
def test_authentication(client: DreadmystTestClient, username: str, password: str) -> bool:
    """Test authentication flow"""
    print("\n[1] Testing Authentication...")

    # Send auth packet with token as "username:password"
    token = f"{username}:{password}"
    client.send_packet(build_authenticate(token))
    print(f"    Sent: Client_Authenticate (token='{token}')")

    # Wait for Server_Validate
    payload = client.wait_for_opcode(Opcode.Server_Validate)
    if payload is None:
        print("  ✗ No Server_Validate response received")
        return False

    result = parse_validate(payload)
    result_str = AuthResult.to_string(result['result'])
    print(f"    Recv: Server_Validate (result={result_str})")

    if result['result'] == AuthResult.Validated:
        print("  ✓ Authentication successful!")
        return True
    else:
        print(f"  ✗ Authentication failed: {result_str}")
        return False

def test_character_list(client: DreadmystTestClient) -> list:
    """Test character list retrieval"""
    print("\n[2] Testing Character List...")

    client.send_packet(build_character_list())
    print("    Sent: Client_CharacterList")

    payload = client.wait_for_opcode(Opcode.Server_CharacterList)
    if payload is None:
        print("  ✗ No Server_CharacterList response received")
        return None

    characters = parse_character_list(payload)
    print(f"    Recv: Server_CharacterList ({len(characters)} characters)")

    for char in characters:
        print(f"         - {char['name']} (GUID {char['guid']}, Level {char['level']}, Class {char['class_id']})")

    print(f"  ✓ Character list received ({len(characters)} characters)")
    return characters

def test_character_creation(client: DreadmystTestClient, name: str) -> int:
    """Test character creation, returns character GUID or 0"""
    print("\n[3] Testing Character Creation...")

    client.send_packet(build_char_create(name, class_id=1, gender=0, portrait_id=0))
    print(f"    Sent: Client_CharCreate (name='{name}', class=1)")

    # First we get the create result
    payload = client.wait_for_opcode(Opcode.Server_CharaCreateResult)
    if payload is None:
        print("  ✗ No Server_CharaCreateResult response received")
        return 0

    result = parse_char_create_result(payload)
    print(f"    Recv: Server_CharaCreateResult (result={result})")

    if result != 0:
        print(f"  ✗ Character creation failed with error code {result}")
        return 0

    # Then we should get an updated character list
    payload = client.wait_for_opcode(Opcode.Server_CharacterList)
    if payload is None:
        print("  ? No character list update received (may be OK)")
        return 0

    characters = parse_character_list(payload)
    print(f"    Recv: Server_CharacterList ({len(characters)} characters)")

    # Find our character
    for char in characters:
        if char['name'] == name:
            print(f"  ✓ Character '{name}' created with GUID {char['guid']}")
            return char['guid']

    print(f"  ✗ Character '{name}' not found in list")
    return 0

def test_character_deletion(client: DreadmystTestClient, guid: int, name: str) -> bool:
    """Test character deletion"""
    print("\n[4] Testing Character Deletion...")

    client.send_packet(build_delete_character(guid))
    print(f"    Sent: Client_DeleteCharacter (GUID {guid})")

    # Should get an updated character list
    payload = client.wait_for_opcode(Opcode.Server_CharacterList)
    if payload is None:
        print("  ✗ No Server_CharacterList response received")
        return False

    characters = parse_character_list(payload)
    print(f"    Recv: Server_CharacterList ({len(characters)} characters)")

    # Verify character is gone
    for char in characters:
        if char['guid'] == guid:
            print(f"  ✗ Character still in list!")
            return False

    print(f"  ✓ Character '{name}' (GUID {guid}) deleted successfully")
    return True

# =============================================================================
# Main Test Runner
# =============================================================================
def run_tests(host: str = '127.0.0.1', port: int = 16383):
    """Run all authentication flow tests"""
    print("=" * 60)
    print("  Dreadmyst Server - Auth Flow Test (Task 3.8)")
    print("=" * 60)

    client = DreadmystTestClient(host, port)

    # Test variables
    # Username can have any format for local auth (parsed as username:password)
    test_username = f"testuser{int(time.time()) % 10000}"
    test_password = "testpass123"
    # Character names must be letters only (no numbers, underscores, etc.)
    test_character = "Testchar"

    results = {
        'connect': False,
        'authenticate': False,
        'character_list': False,
        'character_create': False,
        'character_delete': False
    }

    try:
        # Step 0: Connect
        print("\n[0] Connecting to server...")
        if not client.connect():
            return results
        results['connect'] = True

        # Step 1: Authenticate
        if not test_authentication(client, test_username, test_password):
            return results
        results['authenticate'] = True

        # Step 2: Get character list (should be empty for new account)
        characters = test_character_list(client)
        if characters is None:
            return results
        results['character_list'] = True

        # Step 3: Create a character
        char_guid = test_character_creation(client, test_character)
        if char_guid == 0:
            return results
        results['character_create'] = True

        # Step 4: Delete the character
        if not test_character_deletion(client, char_guid, test_character):
            return results
        results['character_delete'] = True

    except Exception as e:
        print(f"\n  ✗ Test error: {e}")
        import traceback
        traceback.print_exc()
    finally:
        client.disconnect()

    return results

def print_summary(results: dict):
    """Print test summary"""
    print("\n" + "=" * 60)
    print("  TEST SUMMARY")
    print("=" * 60)

    all_passed = all(results.values())

    for test, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {status}  {test}")

    print("=" * 60)
    if all_passed:
        print("  ✓ ALL TESTS PASSED - Phase 3 Auth Flow Complete!")
    else:
        print("  ✗ SOME TESTS FAILED - Review server logs")
    print("=" * 60)

    return all_passed

if __name__ == '__main__':
    host = sys.argv[1] if len(sys.argv) > 1 else '127.0.0.1'
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 16383

    results = run_tests(host, port)
    success = print_summary(results)

    sys.exit(0 if success else 1)
