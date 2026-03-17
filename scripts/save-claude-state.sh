#!/bin/bash
set -euo pipefail

# Save Claude Code team and task state for the osv-storage-integration project
# This allows resuming work on another system

echo "Saving Claude Code state for osv-storage-integration..."

# Create destination directories
mkdir -p .claude/teams .claude/tasks

# Copy team configuration
if [ -d ~/.claude/teams/osv-storage-integration ]; then
    echo "Copying team configuration..."
    cp -r ~/.claude/teams/osv-storage-integration .claude/teams/
    # Remove inboxes (temporary session data)
    rm -rf .claude/teams/osv-storage-integration/inboxes
else
    echo "Warning: Team not found at ~/.claude/teams/osv-storage-integration"
    exit 1
fi

# Copy task state
if [ -d ~/.claude/tasks/osv-storage-integration ]; then
    echo "Copying task state..."
    cp -r ~/.claude/tasks/osv-storage-integration .claude/tasks/
    # Remove lock files
    rm -f .claude/tasks/osv-storage-integration/.lock
else
    echo "Warning: Tasks not found at ~/.claude/tasks/osv-storage-integration"
    exit 1
fi

echo "State saved to .claude/"
echo ""
echo "Files saved:"
echo "  - .claude/teams/osv-storage-integration/config.json"
echo "  - .claude/tasks/osv-storage-integration/*.json ($(ls .claude/tasks/osv-storage-integration/*.json 2>/dev/null | wc -l | tr -d ' ') tasks)"
echo ""
echo "To transfer to another system:"
echo "  1. Copy the entire OSv repository"
echo "  2. On the new system, run: scripts/restore-claude-state.sh"
echo ""
echo "Note: .claude/ is in .gitignore and won't be committed."
echo "Use 'tar czf osv-claude-state.tar.gz .claude/' to create a portable backup."
