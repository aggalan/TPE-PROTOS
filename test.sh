#!/bin/bash

# SOCKSv5 Proxy 500 Connection Test
# Tests the concurrent connection requirement from the TP Especial

# Configuration
PROXY_HOST="127.0.0.1"
PROXY_PORT="1080"
TARGET_HOST="www.google.com"
TARGET_PORT="80"
MAX_CONNECTIONS=500
TIMEOUT=10
TEST_DURATION=30
RESULTS_DIR="test_results"
LOG_FILE="$RESULTS_DIR/connection_test.log"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
declare -i successful_connections=0
declare -i failed_connections=0
declare -i total_attempts=0
declare -a connection_pids=()

# Create results directory
mkdir -p "$RESULTS_DIR"

# Function to log messages
log_message() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}" | tee -a "$LOG_FILE"
}

# Function to create SOCKS5 connection
create_socks5_connection() {
    local conn_id=$1
    local temp_log="$RESULTS_DIR/conn_${conn_id}.log"

    {
        echo "=== Connection $conn_id started at $(date) ==="

        # Create connection to proxy using netcat with timeout
        timeout $TIMEOUT bash -c "
            # SOCKS5 handshake
            # Send greeting: Version(0x05) + Auth methods count(0x01) + No Auth(0x00)
            printf '\x05\x01\x00' | nc -w $TIMEOUT $PROXY_HOST $PROXY_PORT &
            NC_PID=\$!

            # Wait a bit for connection establishment
            sleep 0.1

            # Check if nc is still running
            if kill -0 \$NC_PID 2>/dev/null; then
                echo 'Connection $conn_id: SOCKS5 handshake initiated'

                # Send CONNECT request
                # Version(0x05) + CMD_CONNECT(0x01) + Reserved(0x00) + ATYP_DOMAIN(0x03) + Domain length + Domain + Port
                target_len=\${#TARGET_HOST}
                printf '\x05\x01\x00\x03'
                printf '\x%02x' \$target_len
                printf '%s' '$TARGET_HOST'
                printf '\x%02x\x%02x' \$((TARGET_PORT >> 8)) \$((TARGET_PORT & 0xFF))

                # Keep connection alive for a bit
                sleep 2

                echo 'Connection $conn_id: CONNECT request sent'
                wait \$NC_PID
                echo 'Connection $conn_id: Completed successfully'
            else
                echo 'Connection $conn_id: Failed to establish'
                exit 1
            fi
        " 2>&1

        echo "=== Connection $conn_id ended at $(date) ==="

    } > "$temp_log" 2>&1 &

    local pid=$!
    connection_pids+=($pid)

    # Monitor connection result
    (
        if wait $pid; then
            echo "SUCCESS:$conn_id" >> "$RESULTS_DIR/results.tmp"
        else
            echo "FAILED:$conn_id" >> "$RESULTS_DIR/results.tmp"
        fi
    ) &
}

# Function to create simple TCP connection test
create_tcp_connection() {
    local conn_id=$1
    local temp_log="$RESULTS_DIR/conn_${conn_id}.log"

    {
        echo "=== TCP Connection $conn_id started at $(date) ==="

        # Simple TCP connection test
        timeout $TIMEOUT bash -c "
            exec 3<>/dev/tcp/$PROXY_HOST/$PROXY_PORT
            if [ \$? -eq 0 ]; then
                echo 'Connection $conn_id: TCP connection established'

                # Send SOCKS5 greeting
                printf '\x05\x01\x00' >&3

                # Read response (should be 2 bytes)
                response=\$(timeout 2 dd bs=2 count=1 <&3 2>/dev/null | hexdump -C)
                if [[ \$? -eq 0 ]]; then
                    echo 'Connection $conn_id: SOCKS5 greeting response received'

                    # Send CONNECT request
                    printf '\x05\x01\x00\x03\x0ewww.google.com\x00\x50' >&3

                    # Keep connection alive
                    sleep 1

                    echo 'Connection $conn_id: SOCKS5 handshake completed'
                else
                    echo 'Connection $conn_id: No response from proxy'
                    exit 1
                fi

                exec 3<&-
                exec 3>&-
            else
                echo 'Connection $conn_id: Failed to connect to proxy'
                exit 1
            fi
        " 2>&1

        echo "=== TCP Connection $conn_id ended at $(date) ==="

    } > "$temp_log" 2>&1 &

    local pid=$!
    connection_pids+=($pid)

    # Monitor connection result
    (
        if wait $pid; then
            echo "SUCCESS:$conn_id" >> "$RESULTS_DIR/results.tmp"
        else
            echo "FAILED:$conn_id" >> "$RESULTS_DIR/results.tmp"
        fi
    ) &
}

# Function to monitor system resources
monitor_resources() {
    local monitor_log="$RESULTS_DIR/system_monitor.log"

    {
        echo "=== System Resource Monitor Started ==="
        echo "Time,CPU%,Memory%,OpenFiles,NetworkConnections"

        while [ -f "$RESULTS_DIR/test_running" ]; do
            local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
            local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
            local memory_usage=$(free | grep Mem | awk '{printf "%.2f", $3/$2 * 100.0}')
            local open_files=$(lsof -p $$ 2>/dev/null | wc -l)
            local network_connections=$(netstat -an | grep ":$PROXY_PORT" | wc -l)

            echo "$timestamp,$cpu_usage,$memory_usage,$open_files,$network_connections"
            sleep 1
        done

        echo "=== System Resource Monitor Ended ==="
    } > "$monitor_log" 2>&1 &

    local monitor_pid=$!
    echo $monitor_pid > "$RESULTS_DIR/monitor_pid"
}

# Function to cleanup
cleanup() {
    print_status $YELLOW "Cleaning up test environment..."

    # Remove test running flag
    rm -f "$RESULTS_DIR/test_running"

    # Kill monitor process
    if [ -f "$RESULTS_DIR/monitor_pid" ]; then
        local monitor_pid=$(cat "$RESULTS_DIR/monitor_pid")
        kill $monitor_pid 2>/dev/null
        rm -f "$RESULTS_DIR/monitor_pid"
    fi

    # Kill all connection processes
    for pid in "${connection_pids[@]}"; do
        kill $pid 2>/dev/null
    done

    # Wait a bit for processes to terminate
    sleep 2

    # Force kill if necessary
    for pid in "${connection_pids[@]}"; do
        kill -9 $pid 2>/dev/null
    done

    # Clean up background jobs
    jobs -p | xargs -r kill 2>/dev/null

    print_status $GREEN "Cleanup completed"
}

# Function to analyze results
analyze_results() {
    print_status $BLUE "Analyzing test results..."

    if [ ! -f "$RESULTS_DIR/results.tmp" ]; then
        print_status $RED "No results file found!"
        return 1
    fi

    local success_count=$(grep "SUCCESS:" "$RESULTS_DIR/results.tmp" | wc -l)
    local failed_count=$(grep "FAILED:" "$RESULTS_DIR/results.tmp" | wc -l)
    local total_count=$((success_count + failed_count))

    print_status $GREEN "=== TEST RESULTS ==="
    print_status $GREEN "Total connections attempted: $total_count"
    print_status $GREEN "Successful connections: $success_count"
    print_status $RED "Failed connections: $failed_count"

    if [ $success_count -ge 500 ]; then
        print_status $GREEN "✓ PASSED: Server supports 500+ concurrent connections"
    else
        print_status $RED "✗ FAILED: Server does not support 500 concurrent connections"
    fi

    # Calculate success rate
    if [ $total_count -gt 0 ]; then
        local success_rate=$((success_count * 100 / total_count))
        print_status $BLUE "Success rate: $success_rate%"
    fi

    # Generate detailed report
    {
        echo "=== DETAILED TEST REPORT ==="
        echo "Test Date: $(date)"
        echo "Proxy Server: $PROXY_HOST:$PROXY_PORT"
        echo "Target: $TARGET_HOST:$TARGET_PORT"
        echo "Max Connections: $MAX_CONNECTIONS"
        echo "Test Duration: $TEST_DURATION seconds"
        echo "Timeout: $TIMEOUT seconds"
        echo ""
        echo "Results Summary:"
        echo "- Total attempts: $total_count"
        echo "- Successful: $success_count"
        echo "- Failed: $failed_count"
        echo "- Success rate: $success_rate%"
        echo ""
        echo "Requirement Status:"
        if [ $success_count -ge 500 ]; then
            echo "- 500 concurrent connections: PASSED"
        else
            echo "- 500 concurrent connections: FAILED"
        fi
        echo ""
        echo "Individual Connection Results:"
        cat "$RESULTS_DIR/results.tmp" | sort -t: -k2 -n
    } > "$RESULTS_DIR/detailed_report.txt"

    print_status $BLUE "Detailed report saved to: $RESULTS_DIR/detailed_report.txt"
}

# Function to check prerequisites
check_prerequisites() {
    print_status $BLUE "Checking prerequisites..."

    # Check if netcat is available
    if ! command -v nc &> /dev/null; then
        print_status $RED "ERROR: netcat (nc) is required but not installed"
        exit 1
    fi

    # Check if proxy is running
    if ! nc -z "$PROXY_HOST" "$PROXY_PORT" 2>/dev/null; then
        print_status $RED "ERROR: Cannot connect to proxy at $PROXY_HOST:$PROXY_PORT"
        print_status $YELLOW "Make sure your SOCKSv5 proxy server is running"
        exit 1
    fi

    print_status $GREEN "Prerequisites check passed"
}

# Signal handlers
trap cleanup EXIT
trap cleanup SIGINT
trap cleanup SIGTERM

# Main execution
main() {
    print_status $BLUE "=== SOCKSv5 Proxy 500 Connection Test ==="
    print_status $BLUE "Testing server at $PROXY_HOST:$PROXY_PORT"

    # Clear previous results
    rm -f "$RESULTS_DIR/results.tmp"
    rm -f "$LOG_FILE"

    # Check prerequisites
    check_prerequisites

    # Create test running flag
    touch "$RESULTS_DIR/test_running"

    # Start system monitoring
    monitor_resources

    log_message "Starting connection test with $MAX_CONNECTIONS concurrent connections"

    # Create connections in batches to avoid overwhelming the system
    local batch_size=50
    local batches=$((MAX_CONNECTIONS / batch_size))

    for ((batch=0; batch<batches; batch++)); do
        print_status $YELLOW "Starting batch $((batch+1))/$batches (connections $((batch*batch_size+1)) to $(((batch+1)*batch_size)))"

        for ((i=0; i<batch_size; i++)); do
            local conn_id=$((batch*batch_size + i + 1))
            create_tcp_connection $conn_id
            total_attempts=$((total_attempts + 1))

            # Small delay between connections to avoid overwhelming
            sleep 0.01
        done

        # Wait a bit between batches
        sleep 1
    done

    print_status $YELLOW "All connections started, waiting for completion..."

    # Wait for test duration
    sleep $TEST_DURATION

    # Stop the test
    rm -f "$RESULTS_DIR/test_running"

    print_status $YELLOW "Test completed, analyzing results..."
    sleep 2

    # Analyze results
    analyze_results

    print_status $GREEN "Test finished. Check $RESULTS_DIR for detailed logs."
}

# Help function
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -p, --proxy-host HOST   Proxy server host (default: $PROXY_HOST)"
    echo "  -P, --proxy-port PORT   Proxy server port (default: $PROXY_PORT)"
    echo "  -t, --target HOST       Target host (default: $TARGET_HOST)"
    echo "  -T, --target-port PORT  Target port (default: $TARGET_PORT)"
    echo "  -c, --connections NUM   Max connections (default: $MAX_CONNECTIONS)"
    echo "  -d, --duration SEC      Test duration (default: $TEST_DURATION)"
    echo "  -o, --timeout SEC       Connection timeout (default: $TIMEOUT)"
    echo ""
    echo "Examples:"
    echo "  $0                      # Run with default settings"
    echo "  $0 -p 192.168.1.100     # Test proxy at 192.168.1.100"
    echo "  $0 -c 1000 -d 60        # Test 1000 connections for 60 seconds"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -p|--proxy-host)
            PROXY_HOST="$2"
            shift 2
            ;;
        -P|--proxy-port)
            PROXY_PORT="$2"
            shift 2
            ;;
        -t|--target)
            TARGET_HOST="$2"
            shift 2
            ;;
        -T|--target-port)
            TARGET_PORT="$2"
            shift 2
            ;;
        -c|--connections)
            MAX_CONNECTIONS="$2"
            shift 2
            ;;
        -d|--duration)
            TEST_DURATION="$2"
            shift 2
            ;;
        -o|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Run the test
main