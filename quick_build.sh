#!/bin/bash

# One-stop macOS 15 arm64 build for BambuStudio.
# Uses CVM (CMake Version Manager) so CMake 3.31.x coexists with other versions.
# Produces an app bundle at build/arm64/BambuStudio/BambuStudio.app.

set -euo pipefail

ARCH="${ARCH:-arm64}"
OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET:-10.15}"
BUILD_CONFIG="${BUILD_CONFIG:-Release}"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
PARALLEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(sysctl -n hw.logicalcpu)}"
SDKROOT_PATH="${SDKROOT:-$(xcrun --sdk macosx --show-sdk-path)}"
MAC_WARN_SUPPRESS="-Wno-error=unguarded-availability -Wno-error=unguarded-availability-new -Wno-error=partial-availability"
OPENVDB_USE_CCACHE="${OPENVDB_USE_CCACHE:-OFF}"
OPENCV_ENABLE_CCACHE="${OPENCV_ENABLE_CCACHE:-OFF}"

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="${PROJECT_DIR}/deps"
DEPS_BUILD_DIR="${DEPS_DIR}/build/${ARCH}"
DEPS_DEST="${DEPS_BUILD_DIR}/BambuStudio_deps"
BUILD_DIR="${PROJECT_DIR}/build/${ARCH}"
APP_STAGING="${BUILD_DIR}/BambuStudio"

# Xcode builds place the bundle under /<config>, Ninja does not.
BUILD_DIR_CONFIG_SUBDIR=""
if [ "${CMAKE_GENERATOR}" = "Xcode" ]; then
  BUILD_DIR_CONFIG_SUBDIR="/${BUILD_CONFIG}"
fi
SRC_APP="${BUILD_DIR}/src${BUILD_DIR_CONFIG_SUBDIR}/BambuStudio.app"

log() { printf '\n[%s] %s\n' "$(date '+%H:%M:%S')" "$*"; }
require_cmd() { command -v "$1" >/dev/null 2>&1 || { echo "Missing command: $1"; exit 1; }; }

if [ ! -f "${PROJECT_DIR}/CMakeLists.txt" ] || [ ! -d "${DEPS_DIR}" ]; then
  echo "Run from repo root (CMakeLists.txt + deps/ required)."
  exit 1
fi

if [ "$(uname -s)" != "Darwin" ]; then
  echo "This script targets macOS 15 arm64."
  exit 1
fi

if [ "${ARCH}" != "arm64" ]; then
  echo "ARCH is fixed to arm64 for this helper (got ${ARCH})."
  exit 1
fi

# Prefer CVM-managed CMake (non-destructive to Homebrew installs).
CVM_BIN_CANDIDATES=(
  "$HOME/.cvm/bins/3.31.0/CMake.app/Contents/bin"
  "$HOME/.cvm/bins/current/CMake.app/Contents/bin"
)
CMAKE_BIN=""
for c in "${CVM_BIN_CANDIDATES[@]}"; do
  if [ -x "${c}/cmake" ]; then
    CMAKE_BIN="${c}"
    break
  fi
done

if [ -z "${CMAKE_BIN}" ]; then
  if command -v cmake >/dev/null 2>&1 && cmake --version 2>/dev/null | head -1 | grep -q " 3\.31\."; then
    CMAKE_BIN="$(dirname "$(command -v cmake)")"
  else
    cat <<'EOF'
Please install/pin CMake 3.31.x with CVM (non-destructive):
  curl -s https://raw.githubusercontent.com/paragonpawns/cmake-version-manager/main/install.sh | bash
  cvm install 3.31.0 && cvm switch 3.31.0
Then re-run this script.
EOF
    exit 1
  fi
fi

export PATH="${CMAKE_BIN}:${PATH}"
CMAKE_VERSION=$(cmake --version | head -1 | awk '{print $3}')
if [[ ! "${CMAKE_VERSION}" =~ ^3\.31\. ]]; then
  echo "CMake ${CMAKE_VERSION} detected; need 3.31.x. Please switch via CVM."
  exit 1
fi

if [ "${CMAKE_GENERATOR}" = "Ninja" ]; then
  require_cmd ninja
fi
require_cmd brew

log "Using CMake ${CMAKE_VERSION} from ${CMAKE_BIN}"
log "Generator: ${CMAKE_GENERATOR}"
log "Parallel: ${PARALLEL}"
log "OpenVDB ccache: ${OPENVDB_USE_CCACHE}"
log "OpenCV ccache: ${OPENCV_ENABLE_CCACHE}"

install_build_prereqs() {
  log "Ensuring build prerequisites (non-destructive)..."
  for pkg in automake texinfo nasm yasm x264 ninja; do
    if ! brew list "$pkg" >/dev/null 2>&1; then
      brew install "$pkg"
    fi
  done
}

configure_deps() {
  log "Configuring deps (${DEPS_BUILD_DIR})..."
  mkdir -p "${DEPS_BUILD_DIR}"
  if [ -f "${DEPS_BUILD_DIR}/CMakeCache.txt" ]; then
    if ! grep -q "CMAKE_GENERATOR:INTERNAL=${CMAKE_GENERATOR}" "${DEPS_BUILD_DIR}/CMakeCache.txt"; then
      log "Deps cache uses different generator; clearing ${DEPS_BUILD_DIR}"
      rm -rf "${DEPS_BUILD_DIR}/CMakeFiles" "${DEPS_BUILD_DIR}/CMakeCache.txt"
    fi
  fi
  pushd "${DEPS_BUILD_DIR}" >/dev/null
  cmake "${DEPS_DIR}" \
    -G "${CMAKE_GENERATOR}" \
    -DDESTDIR="${DEPS_DEST}" \
    -DOPENVDB_USE_CCACHE="${OPENVDB_USE_CCACHE}" \
    -DOPENCV_ENABLE_CCACHE="${OPENCV_ENABLE_CCACHE}" \
    -DOPENSSL_ARCH="darwin64-${ARCH}-cc" \
    -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
    -DCMAKE_OSX_SYSROOT="${SDKROOT_PATH}" \
    -DCMAKE_C_FLAGS="${MAC_WARN_SUPPRESS}" \
    -DCMAKE_CXX_FLAGS="${MAC_WARN_SUPPRESS}" \
    -DDEP_WERRORS_SDK=""
  popd >/dev/null
}

build_deps() {
  log "Building deps..."
  pushd "${DEPS_BUILD_DIR}" >/dev/null
  cmake --build . --parallel "${PARALLEL}" --config "${BUILD_CONFIG}" --target deps
  popd >/dev/null
}

configure_app() {
  log "Configuring app (${BUILD_DIR})..."
  mkdir -p "${BUILD_DIR}"
  if [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    if ! grep -q "CMAKE_GENERATOR:INTERNAL=${CMAKE_GENERATOR}" "${BUILD_DIR}/CMakeCache.txt"; then
      log "App cache uses different generator; clearing ${BUILD_DIR}"
      rm -rf "${BUILD_DIR}/CMakeFiles" "${BUILD_DIR}/CMakeCache.txt"
    fi
  fi
  pushd "${BUILD_DIR}" >/dev/null
  cmake "${PROJECT_DIR}" \
    -G "${CMAKE_GENERATOR}" \
    -DBBL_RELEASE_TO_PUBLIC=1 \
    -DBBL_INTERNAL_TESTING=0 \
    -DCMAKE_PREFIX_PATH="${DEPS_DEST}/usr/local" \
    -DCMAKE_INSTALL_PREFIX="${APP_STAGING}" \
    -DCMAKE_BUILD_TYPE="${BUILD_CONFIG}" \
    -DCMAKE_MACOSX_RPATH=ON \
    -DCMAKE_INSTALL_RPATH="${DEPS_DEST}/usr/local" \
    -DCMAKE_MACOSX_BUNDLE=ON \
    -DCMAKE_OSX_ARCHITECTURES="${ARCH}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${OSX_DEPLOYMENT_TARGET}" \
    -DCMAKE_OSX_SYSROOT="${SDKROOT_PATH}" \
    -DCMAKE_C_FLAGS="${MAC_WARN_SUPPRESS}" \
    -DCMAKE_CXX_FLAGS="${MAC_WARN_SUPPRESS}" \
    -DAPPLE_AVAILABILITY_WERROR=OFF
  popd >/dev/null
}

build_app() {
  log "Building app..."
  pushd "${BUILD_DIR}" >/dev/null
  if [ "${CMAKE_GENERATOR}" = "Xcode" ]; then
    cmake --build . --parallel "${PARALLEL}" --config "${BUILD_CONFIG}" --target ALL_BUILD
  else
    cmake --build . --parallel "${PARALLEL}" --config "${BUILD_CONFIG}" --target all
  fi
  popd >/dev/null
}

stage_app() {
  log "Staging app bundle..."
  mkdir -p "${APP_STAGING}"
  if [ ! -d "${SRC_APP}" ]; then
    echo "App bundle not found at ${SRC_APP}"
    exit 1
  fi
  rm -rf "${APP_STAGING}/BambuStudio.app"
  cp -R "${SRC_APP}" "${APP_STAGING}/BambuStudio.app"

  if [ -L "${APP_STAGING}/BambuStudio.app/Contents/Resources" ]; then
    resources_path="$(readlink "${APP_STAGING}/BambuStudio.app/Contents/Resources")"
    rm "${APP_STAGING}/BambuStudio.app/Contents/Resources"
    cp -R "${resources_path}" "${APP_STAGING}/BambuStudio.app/Contents/Resources"
  fi

  find "${APP_STAGING}/BambuStudio.app" -name '.DS_Store' -delete
  log "App ready at ${APP_STAGING}/BambuStudio.app"
}

main() {
  export CMAKE_BUILD_PARALLEL_LEVEL="${PARALLEL}"

  install_build_prereqs
  configure_deps
  build_deps
  configure_app
  build_app
  stage_app

  echo
  echo "Build complete."
  echo "App location: ${APP_STAGING}/BambuStudio.app"
  echo "Run with: open \"${APP_STAGING}/BambuStudio.app\""
}

main "$@"
