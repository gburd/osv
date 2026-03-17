# OSv Storage Integration Project - State Restoration Guide

This directory contains the complete team and task state for the OSv storage integration project, which can be restored on any system running Claude Code.

## Quick Start

**Save state (run periodically to backup progress):**
```bash
./scripts/save-claude-state.sh
```

**Restore state on another system:**
```bash
./scripts/restore-claude-state.sh
```

Then resume work in Claude Code by saying: "Resume the OSv storage integration project"

## Project Overview

**Team:** osv-storage-integration
**Objective:** Modernize OSv storage with two major components:
1. **ZFS Update:** Migrate from FreeBSD 9.1 ZFS to OpenZFS 2.3.6
2. **Crucible Integration:** Add Rust-based distributed block storage

**Status:** In progress with 2 specialists working autonomously
- **ZFS Track:** ~70% complete (tasks 1-4 done, task 5 in progress)
- **Crucible Track:** ~15% complete (task 7 done, task 8 in progress)

## Team Members

1. **team-lead** (you)
   - Model: Claude Sonnet 4.5
   - Role: Coordinate specialists, monitor progress

2. **zfs-specialist**
   - Model: Claude Opus 4.6
   - Role: Update ZFS to OpenZFS 2.3.6
   - Status: Working on task #5 (update automation script)
   - Color: Blue

3. **crucible-specialist**
   - Model: Claude Opus 4.6
   - Role: Integrate Crucible distributed storage
   - Status: Working on task #8 (dependency management)
   - Color: Green

## Task Status Summary

### ZFS Track (Tasks 1-6, 17)
- ✅ Task #1: Extract ZFS OSv modifications (COMPLETE)
- ✅ Task #2: Add OpenZFS 2.3.6 submodule (COMPLETE)
- ✅ Task #3: Port OSv modifications to OpenZFS (COMPLETE)
  - Created module/os/osv/ platform layer (14 files + 3 headers)
- ✅ Task #4: Build system integration (COMPLETE)
  - Created bsd/sys/cddl/openzfs_sources.mk
- 🔄 Task #5: Create update automation script (IN PROGRESS)
- ⏳ Task #6: Test and validate (BLOCKED by #5)
- ⏳ Task #17: Generate git format-patch series (BLOCKED by #6)

### Crucible Track (Tasks 7-13, 18)
- ✅ Task #7: Rust build infrastructure (COMPLETE)
  - Created 9 files: Rust workspace, FFI bindings, Makefile integration
- 🔄 Task #8: Manage Crucible dependencies (IN PROGRESS)
- ⏳ Task #9: Implement block device driver (BLOCKED by #8)
- ⏳ Task #10: Create CLI tools (BLOCKED by #9)
- ⏳ Task #11: Implement snapshot features (BLOCKED by #10)
- ⏳ Task #12: Build system integration (BLOCKED by #9, #10, #11)
- ⏳ Task #13: Test and validate (BLOCKED by #8-#12)
- ⏳ Task #18: Generate git format-patch series (BLOCKED by #13)

### Documentation (Task 14)
- ⏳ Task #14: Create comprehensive documentation (BLOCKED by #6, #13, #17, #18)

## How to Restore on Another System

### Prerequisites
1. Claude Code CLI installed and authenticated
2. Git repository cloned: `git clone <osv-repo-url>`
3. Working directory: `cd osv/`

### Restoration Steps

#### Option 1: Automatic Restore (Recommended)
```bash
# From the osv/ project directory with .claude/ present:
# The team and task state should automatically be recognized by Claude Code

# Verify team exists
claude task list --team osv-storage-integration

# Resume conversation with context
claude chat
# Then say: "Resume the OSv storage integration project"
```

#### Option 2: Manual Restore
```bash
# Copy state from project to user directory
cp -r .claude/teams/osv-storage-integration ~/.claude/teams/
cp -r .claude/tasks/osv-storage-integration ~/.claude/tasks/

# Verify restoration
ls -la ~/.claude/teams/osv-storage-integration/
ls -la ~/.claude/tasks/osv-storage-integration/

# Start Claude Code in the project directory
cd /path/to/osv
claude chat
```

### Resuming Work

Once restored, tell Claude:
```
Resume the OSV storage integration project. Check the status of both specialists
(zfs-specialist and crucible-specialist) and continue coordinating their work
toward completion. The final deliverable is git format-patch series for both
ZFS and Crucible components.
```

Claude will:
1. Read the team config from `~/.claude/teams/osv-storage-integration/config.json`
2. Load all 18 tasks from `~/.claude/tasks/osv-storage-integration/*.json`
3. Check specialist status via inboxes
4. Continue coordination through completion

## Key Files Created So Far

### ZFS Update
```
bsd/sys/cddl/osv-patches/              # OSv modification documentation
  ├── manifest.json                     # 22 modified files inventory
  ├── INTEGRATION.md                    # Architecture documentation
  ├── README.md                         # Patch system overview
  └── patches/001-021                   # Individual patch docs

bsd/sys/cddl/OPENZFS_MAPPING.md        # Path mapping old→new OpenZFS

external/openzfs/                      # Git submodule (zfs-2.3.6)
  ├── module/os/osv/zfs/               # OSv platform layer (14 files)
  │   ├── vdev_disk.c                  # Bio layer + ABD integration
  │   ├── arc_os.c                     # Memory management
  │   ├── spa_os.c                     # Root pool discovery
  │   └── ...                          # 11 more platform files
  └── include/os/osv/zfs/sys/          # OSv headers (3 files)

bsd/sys/cddl/openzfs_sources.mk        # Build system integration
bsd/sys/cddl/compat/opensolaris/openzfs_osv_compat.c  # Compatibility shim
```

### Crucible Integration
```
rust/                                  # Rust workspace
  ├── Cargo.toml                       # Workspace root
  ├── .cargo/config.toml               # Default target x86_64-unknown-none
  ├── osv-sys/                         # FFI bindings
  │   ├── Cargo.toml
  │   ├── build.rs                     # Bindgen configuration
  │   ├── wrapper.h                    # C header declarations
  │   ├── osv_bio_accessors.cc         # C++ accessors with extern "C"
  │   └── src/lib.rs                   # no_std FFI bindings
  └── crucible-osv/                    # Crucible integration
      ├── Cargo.toml                   # staticlib crate
      └── src/lib.rs                   # Stub FFI entry points

conf/profiles/x64/base.mk              # Added conf_drivers_crucible?=0
Makefile                               # Rust build rules (lines 988-990, 2579-2616)
```

## Important Notes

### Specialist Autonomy
Both specialists have been given full autonomy to:
- Make implementation decisions without approval
- Continue through their entire task chains
- Report only blockers or major milestones

**Do not micromanage** - they will work efficiently without constant check-ins.

### Communication Pattern
- Specialists send messages when tasks complete or blockers arise
- Idle notifications are normal between work sessions
- No response needed unless they explicitly request help

### Timeline Estimates
- **ZFS completion:** ~2-3 weeks (testing + patch generation)
- **Crucible completion:** ~8-10 weeks (complex async runtime replacement)
- **Total project:** ~10-12 weeks with parallel work

### Final Deliverables

1. **patches/zfs-update/**
   - Commit series for OpenZFS 2.3.6 integration
   - Cover letter with testing results and migration notes

2. **patches/crucible/**
   - Commit series for Crucible block device driver
   - Cover letter with architecture and usage examples

Both ready for submission to OSv mailing list via `git format-patch`.

## Troubleshooting

### Team not found
```bash
# Verify files exist
ls ~/.claude/teams/osv-storage-integration/
ls ~/.claude/tasks/osv-storage-integration/

# Re-copy from project
cp -r .claude/teams/osv-storage-integration ~/.claude/teams/
cp -r .claude/tasks/osv-storage-integration ~/.claude/tasks/
```

### Specialists not responding
```bash
# Check specialist status
cat ~/.claude/teams/osv-storage-integration/config.json | grep -A5 "members"

# Verify tasks are assigned
ls ~/.claude/tasks/osv-storage-integration/*.json
```

### Build failures
The specialists are creating new files in `external/openzfs/` and `rust/`.
Build failures are expected during development and will be resolved as
build integration progresses.

## Contact

This is an autonomous implementation project. The specialists will work
through completion without requiring constant supervision. Check back for
milestone updates or respond to blocker notifications.

Project started: 2026-03-05
Last state snapshot: 2026-03-05
