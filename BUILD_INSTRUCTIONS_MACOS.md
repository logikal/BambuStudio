# BambuStudio Build Instructions for macOS 15

This document provides step-by-step instructions for building BambuStudio on macOS 15 (tested on ARM64).

## Prerequisites

### System Requirements
- macOS 15+ (tested on 15.6.1)
- Xcode command line tools
- Homebrew package manager

### Install Required Tools

```bash
# Install Xcode command line tools
xcode-select --install

# Install Homebrew if not already installed
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build dependencies (NOTE: Do NOT install cmake via brew)
brew install git gettext pkg-config autoconf automake libtool nasm yasm expat

# Install CMake Version Manager (CVM) for proper CMake version
curl -s https://raw.githubusercontent.com/paragonpawns/cmake-version-manager/main/install.sh | bash

# Install and use CMake 3.31.6 (matches GitHub Actions)
cvm install 3.31.6
cvm switch 3.31.6

# Verify CMake version (should show 3.31.6, NOT 4.x)
cmake --version
```

## Build Process

### Step 1: Set Up Environment

```bash
# Set environment variables for EXPAT and CMake
export PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L/opt/homebrew/opt/expat/lib $LDFLAGS"
export CPPFLAGS="-I/opt/homebrew/opt/expat/include $CPPFLAGS"

# Verify CMake version (critical - must be 3.31.6)
cmake --version
```

### Step 2: Build Dependencies

```bash
# Navigate to BambuStudio directory
cd /path/to/BambuStudio

# Create dependencies build directory
cd deps
rm -rf build  # Clean any existing build
mkdir build
cd build

# Configure dependencies build with proper CMake and environment
PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH" \
PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH" \
cmake ../ \
  -DDESTDIR="/Users/$(whoami)/BambuStudio_dep" \
  -DOPENSSL_ARCH="darwin64-arm64-cc"

# Build dependencies (this takes ~30-60 minutes)
PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH" \
PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH" \
make -j$(sysctl -n hw.ncpu)
```

### Step 3: Build Main Application

```bash
# Navigate back to main project directory
cd ../../

# Create build and install directories
mkdir -p build install_dir
cd build

# Configure main application build
cmake .. \
  -DBBL_RELEASE_TO_PUBLIC=1 \
  -DCMAKE_PREFIX_PATH="/Users/$(whoami)/BambuStudio_dep/usr/local" \
  -DCMAKE_INSTALL_PREFIX="../install_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_MACOSX_RPATH=ON \
  -DCMAKE_INSTALL_RPATH="/Users/$(whoami)/BambuStudio_dep/usr/local" \
  -DCMAKE_MACOSX_BUNDLE=ON

# Build and install application (this takes ~15-30 minutes)
cmake --build . --target install --config Release -j$(sysctl -n hw.ncpu)
```

### Step 4: Run Application

```bash
# Navigate to install directory
cd ../install_dir

# Run BambuStudio
open BambuStudio.app
```

## Alternative: Using Build Script

You can also use the provided build script:

```bash
# For ARM64 (Apple Silicon)
./BuildMac.sh -a arm64

# For x86_64 (Intel)
./BuildMac.sh -a x86_64

# Build dependencies only
./BuildMac.sh -d

# Build application only (after dependencies are built)
./BuildMac.sh -s
```

## Troubleshooting

### Common Issues

1. **CMake Version Error**: If you see "Compatibility with CMake < 3.5" errors, you're using the wrong CMake version. Use CVM to switch:
   ```bash
   # Remove Homebrew cmake if installed
   brew uninstall cmake
   # Install and use correct version
   cvm install 3.31.6 && cvm switch 3.31.6
   # Verify version
   cmake --version  # Should show 3.31.6
   ```

2. **EXPAT Not Found**: Ensure EXPAT is installed and PKG_CONFIG_PATH is set:
   ```bash
   brew install expat
   export PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH"
   ```

3. **CURL Errors**: The original "curl error" was actually a CMake configuration issue. Following these instructions should resolve it.

4. **Build Failures**: Clean build directories and retry:
   ```bash
   cd deps && rm -rf build && mkdir build
   cd ../build && rm -rf * && cd ..
   # Then repeat build steps
   ```

### Environment Variables for Persistent Setup

Add to your `~/.zshrc` or `~/.bash_profile`:

```bash
# BambuStudio build environment
export PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L/opt/homebrew/opt/expat/lib $LDFLAGS"
export CPPFLAGS="-I/opt/homebrew/opt/expat/include $CPPFLAGS"
```

## Build Time Estimates

- **Dependencies**: 30-60 minutes (first time only)
- **Main Application**: 15-30 minutes
- **Total First Build**: ~45-90 minutes
- **Subsequent Builds**: ~15-30 minutes (dependencies cached)

## Directory Structure After Build

```
BambuStudio/
├── deps/
│   └── build/           # Dependencies build artifacts
├── build/               # Main application build artifacts
├── install_dir/         # Final application bundle
│   └── BambuStudio.app  # Runnable application
└── BambuStudio_dep/     # Dependencies installation
    └── usr/local/       # Dependency libraries and headers
```

## Notes

- This process builds a static binary with all dependencies included
- The first build takes longer due to dependency compilation
- Subsequent builds are much faster as dependencies are cached
- The resulting app bundle is self-contained and doesn't require additional libraries
- Build tested on macOS 15.6.1 with Apple Silicon (ARM64)
- For Intel Macs, change `-DOPENSSL_ARCH="darwin64-arm64-cc"` to `-DOPENSSL_ARCH="darwin64-x86_64-cc"`