#!/bin/bash

# Simple test for 500 concurrent connections to SOCKS5 proxy
PROXY_HOST="127.0.0.1"
PROXY_PORT="1080"
MAX_CONNECTIONS=500

echo "Testing $MAX_CONNECTIONS concurrent connections to $PROXY_HOST:$PROXY_PORT"

# Function to test one connection
test_connection() {
    local id=$1
    timeout 5 nc $PROXY_HOST $PROXY_PORT < /dev/null >/dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "Connection $id: OK"
    else
        echo "Connection $id: FAILED"
    fi
}

# Start all connections in background
echo "Starting connections..."
for i in $(seq 1 $MAX_CONNECTIONS); do
    test_connection $i &
done

# Wait for all to finish
wait

# Count results
echo "Test completed. Checking results..."
successful=$(jobs -p | wc -l)
echo "Connections that completed: Check output above"
echo "If most connections show 'OK', your server handles concurrent connections well"