# GitHub Actions Workflows

This directory contains automated build and release workflows for Hantei-chan.

## Quick Status

Add this badge to your README.md to show build status:
```markdown
![Build Status](https://github.com/YOUR-USERNAME/Hantei-chan/actions/workflows/build.yml/badge.svg)
```

## Workflows

### Build (`build.yml`)

**Triggers:**
- Push to **any branch** (including your feature branches!)
- Pull requests to `main`, `master`, or `develop`
- Manual workflow dispatch

**What it does:**
- Builds the project on both Windows (native) and Linux (MinGW cross-compile)
- Uploads build artifacts for 30 days
- Runs on every commit to verify builds don't break
- Linux build is optional (`continue-on-error: true`)

**Artifacts:**
- `hantei-chan-{branch-name}-windows-x64` - Built on Windows
- `hantei-chan-{branch-name}-linux-mingw-build` - Cross-compiled on Linux

**Example:** If you push to branch `fix-undo-bug`, you'll get:
- `hantei-chan-fix-undo-bug-windows-x64`
- `hantei-chan-fix-undo-bug-linux-mingw-build`

### Release (`release.yml`)

**Triggers:**
- Push of version tags (e.g., `v1.1.21`)
- Manual workflow dispatch with version input

**What it does:**
- Builds release version
- Packages executable with resources
- Creates a GitHub Release with downloadable ZIP
- Automatically generates release notes

**How to create a release:**

#### Option 1: Using Git Tags (Recommended)
```bash
git tag v1.1.22
git push origin v1.1.22
```

#### Option 2: Manual Trigger
1. Go to Actions → Release → Run workflow
2. Enter version (e.g., `v1.1.22`)
3. Click "Run workflow"

## Build Artifacts

Each successful build uploads artifacts containing:
- `gonptechan.exe` - Main executable
- `config/` - Configuration files (if present)
- `res/` - Resources (if present)

Download artifacts from:
- **Actions tab** → Select workflow run → Scroll to "Artifacts"

## Requirements

The workflows automatically install:
- CMake 3.27+
- MinGW (Windows native or cross-compile toolchain)
- All project dependencies are in the repository

## Troubleshooting

### Build fails on Windows
- Check that MinGW is properly configured
- Verify CMakeLists.txt is correct
- Check workflow logs in Actions tab

### Build fails on Linux cross-compile
- Ensure `mingw-toolchain.cmake` is present and correct
- Check MinGW package installation in workflow

### Release not created
- Verify you have push access to the repository
- Check that tag format matches `v*.*.*` pattern
- Review workflow logs for errors

## Local Testing

Test builds locally before pushing:

**Windows:**
```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

**Linux (cross-compile):**
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake
cmake --build build
```
