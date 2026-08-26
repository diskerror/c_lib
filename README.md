# c_lib

## Project Overview

`c_lib` is Reid's collection of shared C++ utilities used by Ragger and SemanticSQLite: audio file
I/O (WAVE/AIFF), string phonetics, embedding codec, structured logging, and CLI option parsing.
It is consumed via CMake `FetchContent` — there is no standalone library artifact to install.

## Architecture & Technologies

- **Language:** C++23
- **Dependencies:** Boost (`program_options`, `endian`, `cstdfloat`) and the C++ standard library only
- **Build System:** CMake (via `build_tests.sh` for local test runs, or `FetchContent` for consumers)
- **Structure:** Source files are in the root; tests are in `tests/`

## Key Components

- **AudioFile:** Chunk-based I/O for WAVE and AIFF audio files. Stores metadata chunks as opaque byte blobs; audio data
  stays on disk. Enforces singleton chunk rules and writes chunks in spec-required order.
- **AudioFormat:** Format-agnostic access to sample rate, channels, bit depth, and encoding. Produces all required
  format chunks (`fmt`/`fact` for WAVE, `FVER`/`COMM` for AIFC) via `toChunk()`.
- **AudioSamples:** Reads/writes audio data as normalized float32, with automatic deinterleaving.
- **VectorMath:** SIMD-friendly numeric vector with reduction operations for DSP.
- **WindowedSinc:** Windowed sinc filter kernel generator for resampling and filtering.
- **BigFloat80:** Software conversion of big-endian 80-bit float (AIFF sample rate).
- **DoubleMetaphone / DoubleMetaphoneCapi:** Phonetic string matching (+ C API wrapper). Consumed by Ragger/SemanticSQLite.
- **EmbeddingCodec:** Round-trip codec for storing embedding vectors across dtypes. Consumed by Ragger/SemanticSQLite.
- **Logger:** File-based logger with size/age-based rotation, no external dependency.
- **ProgramOptions:** Thin wrapper over `boost::program_options` for CLI parsing.

## Building & Testing

This project is test-only — there is no standalone library build/install target. Downstream consumers (Ragger,
SemanticSQLite) pull the CMake `add_library` targets they need directly via `FetchContent`.

To build and run the full test suite locally:

```bash
./build_tests.sh            # configure, build, run all tests via ctest
./build_tests.sh --clean    # wipe build/ first
./build_tests.sh --verbose  # full ctest output (-V)
```

CMake requires a few extra parameters beyond a plain `cmake`/`make` invocation (Boost root on macOS, out-of-source
build dir) — `build_tests.sh` wraps them so you don't have to remember the flags.

**Requirements:** CMake >= 3.24, a C++23 compiler, Boost >= 1.74 (>= 1.88 on macOS via MacPorts at
`/opt/local/libexec/boost/1.88`).

## Development Conventions

- **Source Extension:** `.cp` is used for C++ source files (instead of `.cpp`)
- **Header Extension:** `.h`
- **Namespace:** Code is contained within the `Diskerror` namespace
