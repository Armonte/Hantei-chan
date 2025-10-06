# Automatic Version and Build Number System

## Overview

The project now has an automatic version tracking system that:
- **Increments build number** automatically based on git commit count
- **Shows git commit hash** in the version string
- **Detects uncommitted changes** (shows "-dirty" flag)
- **Updates on every build** without manual intervention
- **Displays in window titlebar** and About dialog

## What You'll See

### Window Title
```
gonptéchan v1.1.21 (build 247, a3f5b2c)
```
or if you have uncommitted changes:
```
gonptéchan v1.1.21 (build 247, a3f5b2c-dirty)
```

### About Dialog
Shows detailed build info:
- Version: 1.1.21
- Build: 247 (commit a3f5b2c)
- Built: 2025-10-06 14:32:18

## How It Works

1. **CMake generates version.h** - On every build, CMake runs a script that:
   - Queries git for commit count (used as build number)
   - Gets the short commit hash
   - Checks for uncommitted changes
   - Generates a header file with all this info

2. **Build number = git commit count** - This means:
   - Every new commit increments the build number
   - Build numbers are consistent across machines (same commit = same build number)
   - Easy to identify which commit a build came from

3. **Auto-updates** - No manual version bumping needed for builds
   - Just commit your changes and build
   - Build number automatically increments

## Files Created

- `cmake/GenerateVersionInfo.cmake` - Script that generates version info
- `cmake/version.h.in` - Template for the version header
- `build/generated/version.h` - Generated header (auto-created, git-ignored)

## Updating the Version Number

To change the major/minor/patch version (e.g., from 1.1.21 to 1.2.0):

Edit `CMakeLists.txt` line 2:
```cmake
project(hanteichan VERSION 1.2.0 LANGUAGES C CXX)
```

Build numbers continue to increment automatically based on commits.

## For Non-Git Builds

If building without git:
- Build number defaults to 0
- Commit hash shows as "unknown"
- System still works, just without git-specific info

## Technical Details

### Macros Available

From `version.h`:
```cpp
VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH  // Numeric version parts
BUILD_NUMBER                                  // Git commit count
GIT_COMMIT_HASH                              // Short git hash (string)
GIT_DIRTY_FLAG                               // "-dirty" or "" (string)
BUILD_TIMESTAMP                              // Build date/time (string)

VERSION_STRING                               // "1.1.21"
VERSION_STRING_FULL                          // "1.1.21.247"
VERSION_WITH_COMMIT                          // "1.1.21 (build 247, a3f5b2c)"
VERSION_WITH_COMMIT_W                        // Wide string version (for Windows)
```

### Why Use Git Commit Count?

- **Reproducible** - Same commit = same build number across all machines
- **Automatic** - No manual tracking needed
- **Meaningful** - Higher number = more recent
- **Simple** - No extra infrastructure needed

## Testing

After setting up, rebuild the project:
```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

Check the window title - you should see build number and commit hash!

## Troubleshooting

**Build number shows as 0:**
- Make sure you're in a git repository
- Check that git is in your PATH

**"dirty" flag always shows:**
- You have uncommitted changes
- Commit your changes to remove the flag

**Version not updating:**
- Try a clean rebuild: `cmake --build build --clean-first`
- Or delete build folder and reconfigure

