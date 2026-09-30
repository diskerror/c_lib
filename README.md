# c_lib

## Project Overview

`c_lib` is Reid's collection of shared C++ utilities used by Ragger, SemanticSQLite and
audio-fir-filter: audio file I/O (WAVE/AIFF), string phonetics, stemming, embedding codec, structured
logging, CLI option parsing, and a vendored SQLite build. It builds static libraries into
`build/lib/` that consumers link directly — nothing is installed system-wide.

## Architecture & Technologies

- **Language:** C++23
- **Dependencies:** Boost (`program_options`, `endian`, `cstdfloat`) and the C++ standard library only
- **Build System:** CMake (`build_libs.sh` for the consumer libraries, `build_tests.sh` for tests)
- **Structure:** Headers (the feature list) are in the root; `.cp` sources in `src/`; tests in
  `tests/`; unmodified upstream code in `vendor/` (libstemmer_c, sqlite)

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
- **SQLite** (`diskerror_sqlite3`): pristine upstream amalgamation (`vendor/sqlite`, see
  `UPSTREAM_COMMIT.txt`) built with `SQLITE_DEFAULT_FOREIGN_KEYS=1` (FK rules on for every connection;
  `PRAGMA foreign_keys = OFF` still works), FTS5, R-Tree, DBSTAT, math functions.

## Building & Testing

**Libraries (what consumers link):** optimized `RelWithDebInfo` (`-O3 -g`), in `build/`:

```bash
./build_libs.sh             # incremental
./build_libs.sh --clean     # wipe build/ first, full rebuild
```

| Library | Contents |
|---|---|
| `diskerror_sqlite3` | SQLite 3.54.0, FKs default ON |
| `diskerror_audio` | AudioFile, AudioFormat, AudioSamples |
| `diskerror_double_metaphone` | DoubleMetaphone (+ C API) |
| `diskerror_stemmer_porter` / `_snowball` / `_capi` | stemmers (+ combined C API) |
| `diskerror_embedding_codec` | EmbeddingCodec (+ C API) |
| `diskerror_logger` | Logger |
| `diskerror_program_options` | ProgramOptions (Boost) |

Header-only features (VectorMath, WindowedSinc, BigFloat80, DiskerrorExceptions, WAVE.h/AIFF.h)
come with the root include directory.

Consumers: CMake projects `include(../c_lib/cmake/UseCLib.cmake)` + `use_c_lib(0.2)`, which runs
`build_libs.sh` before each build and imports the targets from `build/c_libConfig.cmake`. Make-based
projects link `build/lib/*.a` directly (see audio-fir-filter's Makefile). Consumer cleans never touch
`build/`; use `./build_libs.sh --clean` to rebuild from scratch.

**Tests (c_lib debugs itself):** Debug build in `build-debug/`:

```bash
./build_tests.sh            # configure, build, run all tests via ctest
./build_tests.sh --clean    # wipe build-debug/ first
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
