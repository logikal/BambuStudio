# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BambuStudio is a cutting-edge, feature-rich slicing software for 3D printing based on PrusaSlicer. It's a C++ application with a wxWidgets GUI that provides project-based workflows, optimized slicing algorithms, and an intuitive interface.

Key features:
- Multi-plate management and remote printer control
- Advanced support types and multi-material printing
- Cross-platform support (Windows/Mac/Linux)
- Real-time monitoring and HMS (Health Management System)
- Networking capabilities for Bambu printers

## Build System

The project uses **CMake** as its primary build system. Key files:
- `CMakeLists.txt` - Main build configuration
- `version.inc` - Version definitions and build flags
- Platform-specific build scripts: `BuildMac.sh`, `BuildLinux.sh`, `build_win.bat`

### Core Build Commands

**CRITICAL: CMake Version Requirement**
BambuStudio requires CMake 3.31.x (NOT 4.x) to match GitHub Actions. Use CVM:
```bash
# Install CMake Version Manager if not installed
curl -s https://raw.githubusercontent.com/paragonpawns/cmake-version-manager/main/install.sh | bash

# Remove Homebrew cmake if installed
brew uninstall cmake

# Install and use correct CMake version
cvm install 3.31.6
cvm switch 3.31.6
cmake --version  # Should show 3.31.6
```

**Mac (recommended approach):**
```bash
# Set up environment (required for every build session)
export PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH"

# Use the quick build script
./quick_build.sh        # Build everything
./quick_build.sh deps   # Build dependencies only  
./quick_build.sh app    # Build application only
./quick_build.sh clean  # Clean build directories

# Or use traditional BuildMac.sh
./BuildMac.sh -d  # Dependencies
./BuildMac.sh -s  # Application
./BuildMac.sh     # Everything
```

**Manual CMake approach (all platforms):**
```bash
# IMPORTANT: Set PATH for correct CMake version
export PATH="$HOME/.cvm/bins/current/CMake.app/Contents/bin:$PATH"

# Configure dependencies
cd deps && mkdir build && cd build
PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH" \
cmake .. -DDESTDIR="/path/to/deps/install"
make -j

# Configure main project  
cd ../../ && mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/deps/install/usr/local"
make -j
```

**Windows:**
```cmd
build_win.bat -d "C:\path\to\deps"
```

### Build Options
- `BBL_RELEASE_TO_PUBLIC` - Public release build (set to 1)
- `BBL_INTERNAL_TESTING` - Internal testing features (set to 0 for release)
- `SLIC3R_STATIC` - Static linking (auto-detected per platform)
- `SLIC3R_GUI` - Build with GUI components (default: ON)

## Testing

The project uses **Catch2** testing framework located in `tests/` directory:

### Test Structure
- `tests/libslic3r/` - Core library tests 
- `tests/fff_print/` - FFF printing logic tests
- `tests/sla_print/` - SLA printing tests
- `tests/libnest2d/` - Nesting algorithm tests

### Running Tests
```bash
# Build and run all tests
cd build
make test

# Run specific test suite
./tests/libslic3r/libslic3r_tests

# Run with Catch2 filters
./tests/libslic3r/libslic3r_tests --test-case-suffix="*geometry*"
```

## Architecture

### Core Components

**libslic3r** (`src/libslic3r/`):
- Core slicing engine and algorithms
- Geometry processing (`Polygon.cpp`, `ExPolygon.cpp`, `TriangleMesh.cpp`)
- Print logic (`Print.cpp`, `PrintObject.cpp`, `Layer.cpp`)
- G-code generation (`GCode.cpp`, `GCodeWriter.cpp`)
- Fill patterns (`Fill/` directory)
- Configuration system (`Config.cpp`, `Preset.cpp`)

**GUI Layer** (`src/slic3r/GUI/`):
- Main application (`GUI_App.cpp`, `MainFrame.cpp`)
- 3D viewer (`GLCanvas3D.cpp`, `GCodeViewer.cpp`)
- Object manipulation (`Plater.cpp`, `GUI_ObjectList.cpp`)
- Device management (`Monitor.cpp`, `DeviceManager.cpp`)
- Gizmos for 3D editing (`Gizmos/` directory)
- Print job management (`Jobs/` directory)

**Device Integration**:
- Network communication (`src/slic3r/Utils/Http.cpp`, `NetworkAgent.cpp`)
- Printer monitoring and control (`src/slic3r/GUI/DeviceManager.cpp`)
- AMS (Automatic Material System) integration (`AMSControl.cpp`)

### Key Design Patterns

**Model-View Architecture**:
- `Model.cpp` - 3D model representation
- GUI classes handle display and interaction
- Background processing via `BackgroundSlicingProcess.cpp`

**Configuration System**:
- Hierarchical presets (Print/Filament/Printer settings)
- Dynamic option resolution with `PlaceholderParser.cpp`
- Validation and dependency management

**Multi-threading**:
- Slicing occurs in background threads
- UI updates via event system
- Job queue management in `Jobs/` directory

## Development Guidelines

### Key Dependencies
- **wxWidgets** - GUI framework (version 3.1+)
- **OpenGL** - 3D rendering 
- **Boost** - Various utilities (nowide for Unicode, etc.)
- **Eigen** - Linear algebra
- **libnest2d** - 2D packing algorithms
- **CGAL/Clipper** - 2D geometry operations

### Common Development Tasks

**Adding New Fill Patterns:**
1. Create new class in `src/libslic3r/Fill/`
2. Register in `Fill.cpp`
3. Add GUI controls in relevant tab

**Printer Integration:**
1. Extend `PrinterTechnology` enum if needed
2. Add configuration options to `PrintConfig.cpp`
3. Implement communication in device management layer

**Adding Calibration Features:**
1. Implement logic in `src/libslic3r/Calib.cpp`
2. Add GUI in `src/slic3r/GUI/Calibration.cpp`
3. Create wizard pages if complex workflow

### Important Notes
- The codebase has evolved from Slic3r/PrusaSlicer - look for "SLIC3R" prefixes in build system
- Cross-platform considerations are critical - test on multiple OS
- Network features may require special build flags or optional dependencies
- Version information is managed centrally in `version.inc`

## File Formats
- **3MF** - Primary project format (`src/libslic3r/Format/3mf.cpp`)
- **STL/OBJ** - Model import (`src/libslic3r/Format/`)
- **G-code** - Output format with custom extensions
- **Config** - INI-style configuration files

## Troubleshooting Common Issues

### CMake Version Problems
**Symptoms:** "Compatibility with CMake < 3.5 has been removed" errors
**Solution:** Wrong CMake version - you need 3.31.x, not 4.x
```bash
# Remove Homebrew cmake
brew uninstall cmake

# Install correct version via CVM
cvm install 3.31.6 && cvm switch 3.31.6

# Verify version (critical!)
cmake --version  # Must show 3.31.6, not 4.x
```

### EXPAT Not Found (macOS)
**Symptoms:** "Could not find EXPAT" during dependency build
**Solution:** Install EXPAT and set PKG_CONFIG_PATH
```bash
brew install expat
export PKG_CONFIG_PATH="/opt/homebrew/opt/expat/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### "CURL Error" (Actually CMake Config Issue)
**Symptoms:** Confusing "curl error" messages
**Solution:** This is usually a CMake configuration problem, not actual CURL issues:
1. Ensure you're running cmake from `deps/build` directory
2. Clean build directories: `rm -rf deps/build && mkdir deps/build`  
3. Use correct CMake version (see above)

### GitHub Actions vs Local Build Differences
**Root Cause:** Local environment has different tool versions than CI
**Solution:** Match GitHub Actions environment:
- CMake 3.31.6 (via CVM)
- Use same build flags as CI
- Check `.github/workflows/` for exact CI commands

## Debugging Tips
- Use `CMAKE_BUILD_TYPE=Debug` for debug builds
- GUI debugging: Set `SLIC3R_LOGLEVEL=debug` environment variable  
- Logging available via `BOOST_LOG_TRIVIAL` macros
- Memory debugging: Enable ASAN with `SLIC3R_ASAN=1`