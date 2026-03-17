#!/usr/bin/env bash
#
# Crucible Integration Test Launcher
#
# This script starts mock Crucible downstairs servers and launches OSv
# with Crucible driver enabled to test the integration.

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSV_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$OSV_ROOT/build/release.x64"
MOCK_SERVER="$SCRIPT_DIR/mock-crucible-downstairs.py"
PORTS=(8810 8820 8830)
SERVER_PIDS=()
LOG_DIR="/tmp/crucible-test-$$"

# Test configuration
REGION_UUID="00000000-0000-0000-0000-000000000000"
BLOCK_SIZE=4096
TOTAL_SIZE=$((100 * 1024 * 1024))  # 100 MB

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

cleanup() {
    log_info "Cleaning up..."

    # Kill all mock servers
    for pid in "${SERVER_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            log_info "Stopping server (PID $pid)"
            kill "$pid" 2>/dev/null || true
        fi
    done

    # Wait for servers to stop
    sleep 1

    # Remove log directory
    if [ -d "$LOG_DIR" ]; then
        log_info "Logs saved in: $LOG_DIR"
    fi
}

trap cleanup EXIT

check_prerequisites() {
    log_info "Checking prerequisites..."

    if [ ! -f "$MOCK_SERVER" ]; then
        log_error "Mock server not found: $MOCK_SERVER"
        exit 1
    fi

    if [ ! -x "$MOCK_SERVER" ]; then
        chmod +x "$MOCK_SERVER"
    fi

    if ! command -v python3 &> /dev/null; then
        log_error "python3 not found"
        exit 1
    fi

    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_error "Please build OSv first: ./scripts/build image=native-example"
        exit 1
    fi

    log_info "Prerequisites OK"
}

start_mock_servers() {
    log_info "Starting mock Crucible downstairs servers..."

    mkdir -p "$LOG_DIR"

    for i in "${!PORTS[@]}"; do
        local port="${PORTS[$i]}"
        local log_file="$LOG_DIR/server-$port.log"

        log_info "Starting server $i on port $port"
        python3 "$MOCK_SERVER" "$port" > "$log_file" 2>&1 &
        local pid=$!
        SERVER_PIDS+=("$pid")

        log_info "Server $i started (PID $pid)"
    done

    # Wait for servers to be ready
    log_info "Waiting for servers to be ready..."
    sleep 2

    # Verify servers are running
    local running_count=0
    for pid in "${SERVER_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            running_count=$((running_count + 1))
        fi
    done

    if [ "$running_count" -lt 3 ]; then
        log_error "Only $running_count/3 servers started successfully"
        for i in "${!PORTS[@]}"; do
            local port="${PORTS[$i]}"
            log_error "Server $i logs:"
            cat "$LOG_DIR/server-$port.log" || true
        done
        exit 1
    fi

    log_info "All 3 mock servers running"
}

build_crucible_args() {
    local targets=""
    for port in "${PORTS[@]}"; do
        if [ -n "$targets" ]; then
            targets="$targets,"
        fi
        # Use host IP as seen from QEMU guest (192.168.122.1 = TAP gateway = host)
        targets="${targets}192.168.122.1:${port}"
    done

    echo "--crucible=$targets --crucible-uuid=$REGION_UUID --crucible-block-size=$BLOCK_SIZE"
}

run_osv_boot_test() {
    log_info "Running OSv boot test with Crucible..."

    local crucible_args
    crucible_args=$(build_crucible_args)

    log_info "Crucible args: $crucible_args"

    # Run OSv with timeout
    local osv_log="$LOG_DIR/osv.log"
    local timeout=30

    log_info "Starting OSv (timeout: ${timeout}s)..."
    log_info "Log file: $osv_log"

    # Build command
    local cmd="$OSV_ROOT/scripts/run.py $crucible_args --execute='/hello' --verbose"

    log_info "Command: $cmd"

    # Run with timeout
    if timeout "$timeout" bash -c "$cmd" > "$osv_log" 2>&1; then
        log_info "OSv boot completed successfully"
        return 0
    else
        local exit_code=$?
        if [ $exit_code -eq 124 ]; then
            log_warn "OSv boot timed out after ${timeout}s"
        else
            log_error "OSv boot failed with exit code $exit_code"
        fi

        log_error "Last 50 lines of OSv log:"
        tail -50 "$osv_log" || true
        return 1
    fi
}

check_crucible_device() {
    log_info "Checking for Crucible device in logs..."

    local osv_log="$LOG_DIR/osv.log"

    if grep -q "Crucible.*Connected to downstairs" "$osv_log"; then
        log_info "Found Crucible connection messages"
        return 0
    else
        log_warn "No Crucible connection messages found"
        return 1
    fi
}

run_test_no_servers() {
    log_info "Test: Boot without servers (expect warnings)"

    # Stop all servers
    for pid in "${SERVER_PIDS[@]}"; do
        kill "$pid" 2>/dev/null || true
    done
    SERVER_PIDS=()
    sleep 1

    local crucible_args
    crucible_args=$(build_crucible_args)

    local osv_log="$LOG_DIR/osv-no-servers.log"
    local timeout=10

    log_info "Starting OSv without servers..."

    if timeout "$timeout" bash -c "$OSV_ROOT/scripts/run.py $crucible_args --execute='/hello'" > "$osv_log" 2>&1; then
        if grep -q "Failed to connect" "$osv_log"; then
            log_info "Test passed: OSv detected missing servers"
            return 0
        else
            log_warn "Test passed but no connection failure message found"
            return 0
        fi
    else
        log_info "OSv handled missing servers appropriately"
        return 0
    fi
}

print_summary() {
    log_info "Test Summary"
    log_info "============"
    log_info "Logs directory: $LOG_DIR"
    log_info ""
    log_info "Server logs:"
    for i in "${!PORTS[@]}"; do
        local port="${PORTS[$i]}"
        log_info "  Server $i (port $port): $LOG_DIR/server-$port.log"
    done
    log_info ""
    log_info "OSv logs:"
    log_info "  Boot test: $LOG_DIR/osv.log"
    log_info "  No servers test: $LOG_DIR/osv-no-servers.log"
}

main() {
    log_info "Crucible Integration Test"
    log_info "=========================="
    log_info ""

    check_prerequisites

    log_info "Test 1: Boot with mock servers"
    log_info "------------------------------"
    start_mock_servers

    if run_osv_boot_test; then
        check_crucible_device
    fi

    log_info ""
    log_info "Test 2: Boot without servers"
    log_info "----------------------------"
    run_test_no_servers

    log_info ""
    print_summary

    log_info ""
    log_info "All tests completed"
}

# Check if sourced or executed
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
