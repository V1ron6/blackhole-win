#!/usr/bin/env bash
# Blackhole for Windows - build wrapper.
#
# Automates the commands in docs/BUILD_WINDOWS.md. Run this from Git Bash
# (comes with Git for Windows) or any other bash on Windows that has
# `cmake` on PATH. You do NOT need to run this from a "Developer Command
# Prompt" - passing a Visual Studio generator (the default below) makes
# CMake invoke MSBuild with the right MSVC toolchain itself.
#
# Usage (pick ONE of --webview2-dir / --vcpkg - see docs/BUILD_WINDOWS.md
# Option A vs Option B for how to obtain either path):
#   ./builder.sh --webview2-dir "C:/path/to/packages/Microsoft.Web.WebView2.1.0.2792.45"
#   ./builder.sh --vcpkg "C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
#   ./builder.sh --webview2-dir "C:/...WebView2.1.0.2792.45" --config Debug
#   ./builder.sh --webview2-dir "C:/...WebView2.1.0.2792.45" --clean   # wipe build/ first

set -euo pipefail

CONFIG="Release"
GENERATOR="Visual Studio 17 2022"
ARCH="x64"
BUILD_DIR="build"
WEBVIEW2_DIR=""
VCPKG_TOOLCHAIN=""
CLEAN=0

usage() {
    sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --webview2-dir) WEBVIEW2_DIR="$2"; shift 2 ;;
        --vcpkg)        VCPKG_TOOLCHAIN="$2"; shift 2 ;;
        --config)       CONFIG="$2"; shift 2 ;;
        --generator)    GENERATOR="$2"; shift 2 ;;
        --arch)         ARCH="$2"; shift 2 ;;
        --build-dir)    BUILD_DIR="$2"; shift 2 ;;
        --clean)        CLEAN=1; shift ;;
        -h|--help)      usage ;;
        *) echo "Unknown argument: $1"; usage ;;
    esac
done

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found on PATH."
    echo "Install it via the 'Desktop development with C++' Visual Studio"
    echo "workload (includes CMake tools), or from https://cmake.org/download/,"
    echo "then re-open your shell so PATH picks it up."
    exit 1
fi

if [[ -z "$WEBVIEW2_DIR" && -z "$VCPKG_TOOLCHAIN" ]]; then
    echo "ERROR: pass either --webview2-dir (NuGet layout) or --vcpkg (toolchain file)."
    echo "See docs/BUILD_WINDOWS.md Option A / Option B for how to get each one."
    exit 1
fi

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
    echo "Removing existing $BUILD_DIR/ ..."
    rm -rf "$BUILD_DIR"
fi

CONFIGURE_ARGS=(-S . -B "$BUILD_DIR" -G "$GENERATOR" -A "$ARCH")
if [[ -n "$VCPKG_TOOLCHAIN" ]]; then
    CONFIGURE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$VCPKG_TOOLCHAIN")
    # NOTE: as called out in docs/BUILD_WINDOWS.md Option B, vcpkg's
    # webview2 port exposes a different CMake target name than the raw
    # NuGet layout CMakeLists.txt currently assumes. If configure fails
    # looking for WEBVIEW2_DIR's include path, tell me and I'll switch
    # CMakeLists.txt to `find_package(unofficial-webview2 CONFIG REQUIRED)`.
else
    CONFIGURE_ARGS+=(-DWEBVIEW2_DIR="$WEBVIEW2_DIR")
fi

echo "== Configuring =========================================="
printf 'cmake'; printf ' %q' "${CONFIGURE_ARGS[@]}"; echo
cmake "${CONFIGURE_ARGS[@]}"

echo "== Building ($CONFIG) ===================================="
cmake --build "$BUILD_DIR" --config "$CONFIG"

OUT_DIR="$BUILD_DIR/$CONFIG"
echo "== Done =================================================="
if [[ -f "$OUT_DIR/Blackhole.exe" ]]; then
    echo "Built: $OUT_DIR/Blackhole.exe"
    echo "(WebView2Loader.dll and assets/ were copied alongside it automatically.)"
else
    echo "Build finished but $OUT_DIR/Blackhole.exe wasn't found - check the"
    echo "output above for errors, or look under $BUILD_DIR/ for your"
    echo "generator's actual output layout if you passed --generator/--build-dir."
fi
