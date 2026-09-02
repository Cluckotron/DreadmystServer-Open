#!/bin/bash
# Connection Stress Test for Dreadmyst Server
# Task 2.12: Connection Stress Test
#
# Usage: ./stress_test.sh [num_connections] [host] [port]
#
# This script opens multiple TCP connections to the server and verifies:
# - Server can handle simultaneous connections
# - Connections timeout properly (30 sec for unauthenticated)
# - Server remains responsive under load

NUM_CONNECTIONS=${1:-50}
HOST=${2:-127.0.0.1}
PORT=${3:-8080}

echo "========================================"
echo "Dreadmyst Server Connection Stress Test"
echo "========================================"
echo "Target: $HOST:$PORT"
echo "Connections: $NUM_CONNECTIONS"
echo ""

# Check if nc (netcat) is available
if ! command -v nc &> /dev/null; then
    echo "Error: netcat (nc) is required but not installed."
    echo "Install with: apt install netcat-openbsd"
    exit 1
fi

# Array to store background PIDs
declare -a PIDS

# Function to create a connection and keep it open
open_connection() {
    local id=$1
    # Open connection, send nothing (should timeout in 30 sec)
    (sleep 35; echo "Connection $id done") | nc -q 35 $HOST $PORT &>/dev/null &
    echo $!
}

# Function to cleanup
cleanup() {
    echo ""
    echo "Cleaning up connections..."
    for pid in "${PIDS[@]}"; do
        kill $pid 2>/dev/null
    done
    echo "Done."
}

trap cleanup EXIT

# Open connections
echo "Opening $NUM_CONNECTIONS connections..."
start_time=$(date +%s.%N)

for i in $(seq 1 $NUM_CONNECTIONS); do
    pid=$(open_connection $i)
    PIDS+=($pid)

    # Show progress every 10 connections
    if [ $((i % 10)) -eq 0 ]; then
        echo "  Opened $i connections..."
    fi
done

end_time=$(date +%s.%N)
elapsed=$(echo "$end_time - $start_time" | bc)
echo "All connections opened in ${elapsed}s"
echo ""

# Wait a moment for connections to establish
sleep 2

# Count active connections
active=$(netstat -ant 2>/dev/null | grep ":$PORT" | grep ESTABLISHED | wc -l)
echo "Active connections on port $PORT: $active"
echo ""

# Test server responsiveness with a quick ping
echo "Testing server responsiveness..."
ping_start=$(date +%s.%N)

# Open a new connection and immediately close (tests accept path)
for i in {1..5}; do
    nc -z $HOST $PORT 2>/dev/null
done

ping_end=$(date +%s.%N)
ping_time=$(echo "($ping_end - $ping_start) / 5" | bc -l)
printf "Average connection time: %.3fs\n" $ping_time
echo ""

# Wait for timeout to test that connections are properly closed
echo "Waiting for connection timeout test (35 seconds)..."
echo "(Unauthenticated connections should timeout in ~30 seconds)"

countdown=35
while [ $countdown -gt 0 ]; do
    printf "\r  Countdown: %2d seconds remaining..." $countdown
    sleep 5
    countdown=$((countdown - 5))

    # Check remaining connections
    remaining=$(netstat -ant 2>/dev/null | grep ":$PORT" | grep ESTABLISHED | wc -l)
    echo " (active: $remaining)"
done

echo ""
echo ""

# Final check
final_count=$(netstat -ant 2>/dev/null | grep ":$PORT" | grep ESTABLISHED | wc -l)
echo "Final active connections: $final_count"

if [ "$final_count" -lt "$((NUM_CONNECTIONS / 2))" ]; then
    echo "SUCCESS: Most connections timed out as expected"
else
    echo "WARNING: Many connections still active (timeout may not be working)"
fi

echo ""
echo "Stress test complete!"
