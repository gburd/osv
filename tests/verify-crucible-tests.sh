#!/usr/bin/env bash
#
# Verify Crucible test infrastructure is properly set up

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSV_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
}

fail() {
    echo -e "${RED}[FAIL]${NC} $*"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

info() {
    echo "[INFO] $*"
}

ERRORS=0

echo "Crucible Test Infrastructure Verification"
echo "=========================================="
echo ""

# Check test files exist
info "Checking test files..."

FILES=(
    "tests/mock-crucible-downstairs.py"
    "tests/test-crucible-integration.sh"
    "tests/crucible-scenarios.sh"
    "tests/crucible-io-test.cc"
    "tests/README-crucible.md"
    "tests/QUICKSTART-crucible.md"
    "docs/crucible-testing.md"
)

for file in "${FILES[@]}"; do
    if [ -f "$OSV_ROOT/$file" ]; then
        pass "$file exists"
    else
        fail "$file missing"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# Check file permissions
info "Checking file permissions..."

EXECUTABLE_FILES=(
    "tests/mock-crucible-downstairs.py"
    "tests/test-crucible-integration.sh"
    "tests/crucible-scenarios.sh"
)

for file in "${EXECUTABLE_FILES[@]}"; do
    if [ -x "$OSV_ROOT/$file" ]; then
        pass "$file is executable"
    else
        fail "$file is not executable (run: chmod +x $file)"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# Check Python
info "Checking Python..."

if command -v python3 &> /dev/null; then
    PYTHON_VERSION=$(python3 --version)
    pass "Python found: $PYTHON_VERSION"

    # Check Python version
    PYTHON_MAJOR=$(python3 -c 'import sys; print(sys.version_info.major)')
    PYTHON_MINOR=$(python3 -c 'import sys; print(sys.version_info.minor)')

    if [ "$PYTHON_MAJOR" -ge 3 ] && [ "$PYTHON_MINOR" -ge 7 ]; then
        pass "Python version OK (>= 3.7)"
    else
        warn "Python version may be too old (need >= 3.7)"
    fi
else
    fail "Python3 not found"
    ERRORS=$((ERRORS + 1))
fi

echo ""

# Check Python script syntax
info "Checking Python script syntax..."

if python3 -m py_compile "$OSV_ROOT/tests/mock-crucible-downstairs.py" 2>/dev/null; then
    pass "mock-crucible-downstairs.py syntax OK"
else
    fail "mock-crucible-downstairs.py has syntax errors"
    ERRORS=$((ERRORS + 1))
fi

echo ""

# Check Bash script syntax
info "Checking Bash script syntax..."

for script in "test-crucible-integration.sh" "crucible-scenarios.sh"; do
    if bash -n "$OSV_ROOT/tests/$script" 2>/dev/null; then
        pass "$script syntax OK"
    else
        fail "$script has syntax errors"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# Check ports availability
info "Checking test ports..."

PORTS=(8810 8820 8830)
for port in "${PORTS[@]}"; do
    if nc -z 127.0.0.1 "$port" 2>/dev/null; then
        warn "Port $port is already in use"
    else
        pass "Port $port is available"
    fi
done

echo ""

# Check OSv build directory
info "Checking OSv build..."

if [ -d "$OSV_ROOT/build/release.x64" ]; then
    pass "Build directory exists: build/release.x64"
else
    warn "Build directory not found (run: make)"
    info "Tests will fail without a built OSv image"
fi

echo ""

# Check Crucible driver files
info "Checking Crucible driver files..."

DRIVER_FILES=(
    "drivers/crucible-client.cc"
    "drivers/crucible-client.hh"
    "drivers/crucible-types.hh"
    "drivers/crucible-messages.hh"
    "drivers/crucible-bincode.hh"
    "drivers/crucible-connection.hh"
    "drivers/crucible-hash.hh"
)

for file in "${DRIVER_FILES[@]}"; do
    if [ -f "$OSV_ROOT/$file" ]; then
        pass "$file exists"
    else
        fail "$file missing"
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""

# Summary
echo "Summary"
echo "======="

if [ $ERRORS -eq 0 ]; then
    pass "All checks passed!"
    echo ""
    echo "You can now run tests:"
    echo "  ./tests/test-crucible-integration.sh"
    exit 0
else
    fail "$ERRORS check(s) failed"
    echo ""
    echo "Please fix the issues above before running tests."
    exit 1
fi
