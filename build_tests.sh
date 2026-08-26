#!/usr/bin/env bash
# Configure, build, and run c_lib's test suite via CMake/CTest.
#
# CMake needs a couple of extra params beyond plain `make` (out-of-source
# build dir, Boost root on macOS), so this wraps them in one command:
#
#   ./build_tests.sh            # configure (if needed), build, run tests
#   ./build_tests.sh --clean    # wipe build/ first, then configure+build+run
#   ./build_tests.sh --verbose  # pass -V to ctest for full test output
#
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

BUILD_DIR="build"
CTEST_ARGS=()

for arg in "$@"; do
    case "$arg" in
        --clean)
            rm -rf "$BUILD_DIR"
            ;;
        --verbose|-V)
            CTEST_ARGS+=("-V")
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

CMAKE_ARGS=(-B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug)

if [[ "$(uname)" == "Darwin" ]]; then
    CMAKE_ARGS+=(-DBOOST_ROOT="/opt/local/libexec/boost/1.88")
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

ctest --test-dir "$BUILD_DIR" --output-on-failure "${CTEST_ARGS[@]}"
