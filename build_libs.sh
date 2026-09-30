#!/usr/bin/env bash
# Build c_lib's optimized libraries (RelWithDebInfo: -O3 -g) into build/.
#
# This is the tree consumers (Ragger, SemanticSQLite, audio-fir-filter) link
# against: build/c_libConfig.cmake + build/lib/*.a. Incremental — only
# changed sources recompile. Consumers call this before their own build;
# it never cleans unless asked.
#
#   ./build_libs.sh            # incremental build
#   ./build_libs.sh --clean    # wipe build/ first, then full rebuild
#                              # (also accepts "clean", like Ragger's build.sh)
#
# Consumers always call it with no arguments, so they can never clean it.
#
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="build"

for arg in "$@"; do
    case "$arg" in
        --clean|clean)
            echo "[+] c_lib: clean build (removing $BUILD_DIR/)"
            rm -rf "$BUILD_DIR"
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [--clean]" >&2
            exit 1
            ;;
    esac
done
CMAKE_ARGS=(-B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo)
if [[ "$(uname)" == "Darwin" ]]; then
    CMAKE_ARGS+=(-DBOOST_ROOT="/opt/local/libexec/boost/1.88")
fi

# Configure only when needed (first run, or CMakeLists changed — CMake
# re-checks that itself during --build).
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    cmake "${CMAKE_ARGS[@]}"
fi
# Detach from a calling consumer's make jobserver (its MAKEFLAGS would
# otherwise force -j1 with a warning) and use all cores.
unset MAKEFLAGS MFLAGS MAKELEVEL CARGO_MAKEFLAGS
cmake --build "$BUILD_DIR" --target c_lib_libs -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
