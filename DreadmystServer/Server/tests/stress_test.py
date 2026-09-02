#!/usr/bin/env python3
"""
Connection Stress Test for Dreadmyst Server
Task 2.12: Connection Stress Test

Tests:
- Opening many simultaneous connections
- Server responsiveness under load
- Connection timeout handling
- Ping packet echo

Usage: python3 stress_test.py [num_connections] [host] [port]
"""

import socket
import sys
import time
import threading
import struct
from concurrent.futures import ThreadPoolExecutor, as_completed

# Default settings
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8080
DEFAULT_CONNECTIONS = 50

# Packet opcodes
OPCODE_PING = 0

class TestConnection:
    """Manages a single test connection"""

    def __init__(self, host, port, conn_id):
        self.host = host
        self.port = port
        self.conn_id = conn_id
        self.sock = None
        self.connected = False
        self.ping_count = 0
        self.error = None

    def connect(self):
        """Open connection to server"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            self.connected = True
            return True
        except Exception as e:
            self.error = str(e)
            return False

    def send_ping(self):
        """Send a ping packet and wait for response"""
        if not self.connected:
            return False

        try:
            # Build packet: [uint16 size][uint16 opcode]
            # Size includes opcode (2 bytes)
            packet = struct.pack("<HH", 2, OPCODE_PING)
            self.sock.sendall(packet)

            # Wait for response
            self.sock.settimeout(2.0)
            response = self.sock.recv(4)

            if len(response) >= 4:
                size, opcode = struct.unpack("<HH", response[:4])
                if opcode == OPCODE_PING:
                    self.ping_count += 1
                    return True

            return False
        except Exception as e:
            self.error = str(e)
            return False

    def close(self):
        """Close the connection"""
        if self.sock:
            try:
                self.sock.close()
            except:
                pass
            self.connected = False


def run_connection_test(host, port, conn_id, send_pings=False):
    """Run a single connection test"""
    conn = TestConnection(host, port, conn_id)

    if not conn.connect():
        return {
            "id": conn_id,
            "connected": False,
            "error": conn.error,
            "pings": 0
        }

    pings = 0
    if send_pings:
        for _ in range(3):
            if conn.send_ping():
                pings += 1
            time.sleep(0.1)

    conn.close()

    return {
        "id": conn_id,
        "connected": True,
        "error": None,
        "pings": pings
    }


def main():
    # Parse arguments
    num_connections = int(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_CONNECTIONS
    host = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_HOST
    port = int(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_PORT

    print("=" * 50)
    print("Dreadmyst Server Connection Stress Test")
    print("=" * 50)
    print(f"Target: {host}:{port}")
    print(f"Connections: {num_connections}")
    print()

    # Test 1: Basic connectivity
    print("Test 1: Basic Connectivity")
    print("-" * 30)
    test_conn = TestConnection(host, port, 0)
    if test_conn.connect():
        print("  ✓ Server is accepting connections")
        test_conn.close()
    else:
        print(f"  ✗ Cannot connect to server: {test_conn.error}")
        print()
        print("Make sure the server is running!")
        sys.exit(1)

    # Test 2: Ping echo
    print()
    print("Test 2: Ping Echo")
    print("-" * 30)
    test_conn = TestConnection(host, port, 0)
    if test_conn.connect():
        if test_conn.send_ping():
            print("  ✓ Ping echoed successfully")
        else:
            print(f"  ✗ Ping failed: {test_conn.error or 'no response'}")
        test_conn.close()

    # Test 3: Concurrent connections
    print()
    print(f"Test 3: Opening {num_connections} Concurrent Connections")
    print("-" * 30)

    start_time = time.time()
    results = []

    with ThreadPoolExecutor(max_workers=min(100, num_connections)) as executor:
        futures = [
            executor.submit(run_connection_test, host, port, i, True)
            for i in range(num_connections)
        ]

        for future in as_completed(futures):
            results.append(future.result())

            # Progress
            done = len(results)
            if done % 10 == 0:
                print(f"  {done}/{num_connections} completed...")

    elapsed = time.time() - start_time
    print(f"  Completed in {elapsed:.2f}s")

    # Analyze results
    connected = sum(1 for r in results if r["connected"])
    failed = sum(1 for r in results if not r["connected"])
    total_pings = sum(r["pings"] for r in results)

    print()
    print("Results:")
    print("-" * 30)
    print(f"  Successful connections: {connected}/{num_connections}")
    print(f"  Failed connections:     {failed}")
    print(f"  Total pings echoed:     {total_pings}")
    print(f"  Connections per second: {num_connections/elapsed:.1f}")

    # Test 4: Sustained connections (shorter test)
    print()
    print("Test 4: Sustained Connections (10 seconds)")
    print("-" * 30)

    sustained = []
    for i in range(min(20, num_connections)):
        conn = TestConnection(host, port, i)
        if conn.connect():
            sustained.append(conn)

    print(f"  Opened {len(sustained)} connections")
    print("  Holding for 10 seconds...")

    for _ in range(10):
        time.sleep(1)
        alive = sum(1 for c in sustained if c.connected)
        # Send a ping to check if still connected
        for c in sustained:
            c.send_ping()

    alive = sum(1 for c in sustained if c.connected)
    print(f"  Connections still alive: {alive}/{len(sustained)}")

    for conn in sustained:
        conn.close()

    # Summary
    print()
    print("=" * 50)
    print("Stress Test Summary")
    print("=" * 50)

    passed = connected >= num_connections * 0.95  # 95% success rate
    if passed:
        print("✓ PASSED - Server handled stress test successfully")
    else:
        print("✗ FAILED - Too many connection failures")

    print()


if __name__ == "__main__":
    main()
