#!/bin/bash
set -euo pipefail

# Restore Claude Code team and task state for the osv-storage-integration project

echo "Restoring Claude Code state for osv-storage-integration..."

# Check if state exists in project
if [ ! -d .claude/teams/osv-storage-integration ]; then
    echo "Error: No saved state found in .claude/"
    echo ""
    echo "Expected to find:"
    echo "  .claude/teams/osv-storage-integration/"
    echo "  .claude/tasks/osv-storage-integration/"
    echo ""
    echo "If you have a backup tarball, extract it first:"
    echo "  tar xzf osv-claude-state.tar.gz"
    exit 1
fi

# Create Claude directories if they don't exist
mkdir -p ~/.claude/teams ~/.claude/tasks

# Copy team configuration
echo "Restoring team configuration..."
cp -r .claude/teams/osv-storage-integration ~/.claude/teams/

# Copy task state
echo "Restoring task state..."
cp -r .claude/tasks/osv-storage-integration ~/.claude/tasks/

# Verify restoration
if [ -f ~/.claude/teams/osv-storage-integration/config.json ]; then
    echo "✓ Team config restored"
else
    echo "✗ Team config restoration failed"
    exit 1
fi

TASK_COUNT=$(ls ~/.claude/tasks/osv-storage-integration/*.json 2>/dev/null | wc -l | tr -d ' ')
if [ "$TASK_COUNT" -gt 0 ]; then
    echo "✓ $TASK_COUNT tasks restored"
else
    echo "✗ Task restoration failed"
    exit 1
fi

echo ""
echo "State successfully restored!"
echo ""
echo "To resume work:"
echo "  cd $(pwd)"
echo "  claude chat"
echo ""
echo "Then say:"
echo "  Resume the OSv storage integration project with the"
echo "  osv-storage-integration team. Check status of both"
echo "  specialists and continue coordinating their work."
