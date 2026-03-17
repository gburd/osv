#!/bin/bash
#
# Setup OSv build environment for macOS with ZFS
# This script fixes permissions and sets up podman with Lima
#
# Run with: bash scripts/setup-build-environment.sh
#

set -e

echo "================================================"
echo "OSv ZFS Build Environment Setup"
echo "================================================"
echo ""

# Check if running on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "Error: This script is for macOS only"
    exit 1
fi

# Step 1: Fix Homebrew permissions
echo "Step 1: Fixing Homebrew permissions..."
echo "This will require your admin password."
echo ""

sudo chown -R $(whoami) \
    /Users/$(whoami)/Library/Caches/Homebrew \
    /Users/$(whoami)/Library/Logs/Homebrew \
    /opt/homebrew \
    /opt/homebrew/Cellar \
    /opt/homebrew/Frameworks \
    /opt/homebrew/bin \
    /opt/homebrew/etc \
    /opt/homebrew/include \
    /opt/homebrew/lib \
    /opt/homebrew/opt \
    /opt/homebrew/sbin \
    /opt/homebrew/share \
    /opt/homebrew/var 2>/dev/null || true

sudo chmod u+w \
    /Users/$(whoami)/Library/Caches/Homebrew \
    /Users/$(whoami)/Library/Logs/Homebrew \
    /opt/homebrew \
    /opt/homebrew/Cellar \
    /opt/homebrew/Frameworks \
    /opt/homebrew/bin \
    /opt/homebrew/etc \
    /opt/homebrew/include \
    /opt/homebrew/lib \
    /opt/homebrew/opt \
    /opt/homebrew/sbin \
    /opt/homebrew/share \
    /opt/homebrew/var 2>/dev/null || true

echo "✓ Homebrew permissions fixed"
echo ""

# Step 2: Clear podman lock files
echo "Step 2: Clearing podman lock files..."
sudo rm -f ~/.config/containers/podman/machine/applehv/*.lock 2>/dev/null || true
sudo rm -f ~/.config/containers/podman-connections.json.lock 2>/dev/null || true
echo "✓ Lock files cleared"
echo ""

# Step 3: Install Lima if not present
echo "Step 3: Installing Lima (VM backend for podman)..."
if ! command -v limactl &> /dev/null; then
    brew install lima
    echo "✓ Lima installed"
else
    echo "✓ Lima already installed"
fi
echo ""

# Step 4: Remove old podman connections
echo "Step 4: Cleaning up old podman connections..."
podman system connection list | grep -v "^Name" | awk '{print $1}' | while read conn; do
    podman system connection remove "$conn" 2>/dev/null || true
done
echo "✓ Old connections removed"
echo ""

# Step 5: Create Lima VM for OSv building
echo "Step 5: Creating Lima VM for OSv building..."
if limactl list | grep -q osv-builder; then
    echo "Lima VM 'osv-builder' already exists, stopping it..."
    limactl stop osv-builder 2>/dev/null || true
    limactl delete osv-builder 2>/dev/null || true
fi

cat > /tmp/osv-builder.yaml <<'EOF'
# Lima configuration for OSv building
cpus: 4
memory: "8GiB"
disk: "50GiB"

images:
  - location: "https://cloud-images.ubuntu.com/releases/22.04/release/ubuntu-22.04-server-cloudimg-amd64.img"
    arch: "x86_64"
  - location: "https://cloud-images.ubuntu.com/releases/22.04/release/ubuntu-22.04-server-cloudimg-arm64.img"
    arch: "aarch64"

mounts:
  - location: "~"
    writable: true
  - location: "/tmp/lima"
    writable: true

containerd:
  system: false
  user: false

provision:
  - mode: system
    script: |
      #!/bin/bash
      apt-get update
      apt-get install -y podman buildah
EOF

limactl start --name=osv-builder /tmp/osv-builder.yaml
rm /tmp/osv-builder.yaml
echo "✓ Lima VM created and started"
echo ""

# Step 6: Configure podman to use Lima
echo "Step 6: Configuring podman to use Lima VM..."
podman system connection add osv-builder \
    "unix:///Users/$(whoami)/.lima/osv-builder/sock/podman.sock" \
    --identity "/Users/$(whoami)/.lima/_config/user"

podman system connection default osv-builder
echo "✓ Podman configured"
echo ""

# Step 7: Verify setup
echo "Step 7: Verifying setup..."
if podman version &>/dev/null; then
    echo "✓ Podman is working"
    podman version | head -5
else
    echo "✗ Podman verification failed"
    exit 1
fi
echo ""

# Step 8: Pull OSv builder image
echo "Step 8: Pulling OSv builder container image..."
podman pull osvunikernel/osv-builder:latest
echo "✓ OSv builder image ready"
echo ""

echo "================================================"
echo "Setup Complete!"
echo "================================================"
echo ""
echo "You can now build OSv with ZFS using:"
echo ""
echo "  cd /Users/$(whoami)/src/osv"
echo "  podman run --rm -v \$(pwd):/osv -w /osv osvunikernel/osv-builder \\"
echo "    bash -c './scripts/build arch=aarch64 fs=zfs image=native-example'"
echo ""
echo "Or run the build script directly:"
echo "  bash scripts/build-osv-zfs.sh"
echo ""
