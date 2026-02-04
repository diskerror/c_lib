# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
make          # Builds lib/libdiskerror_audio.a (static library)
```

The Makefile uses `g++` with `-std=c++23 -O3`. Include paths are configured for macOS MacPorts (`/opt/local/libexec/gcc15/libc++/include`, `/opt/local/libexec/boost/1.87/include`). Adjust `CXXFLAGS` in the Makefile for other environments.

Currently only `AudioFile.cp` and `AudioSamples.cp` are compiled. The `clapp` components are commented out in the Makefile.

There is no test framework or linter configured.

## Conventions

- **Source extension**: `.cp` (not `.cpp`)
- **Namespace**: All code lives in `Diskerror`
- **Dependencies**: Boost (`endian/arithmetic.hpp`, `endian/conversion.hpp`, `cstdfloat.hpp`) and C++ standard library only
- Object files go to `build/`, library output to `lib/`
- Files ending with `-orig` are the original fully working versions. They are functional but unfocused and lack the ability to create new audio files. Refactored versions drop the suffix.

## Architecture

### Core class hierarchy

`AudioFile` is the central class. It manages chunk-based audio containers (WAVE, AIFF, RF64) using a direct I/O model: all metadata chunks are loaded into RAM as a `vector<shared_ptr<Chunk>>`, while audio data stays on disk and is accessed via stream-like `read`/`write`/`seekg`/`seekp` methods constrained to the audio payload region.

`AudioSamples` inherits from both `VectorMath<float32_t>` and `AudioFile`. It adds sample-level operations: reading raw PCM/float data into a float32 vector with endianness conversion, writing back with optional dithering, and normalization.

### Chunk system (AudioChunks.h)

All chunks inherit from `Chunk` (virtual `getSize()` and `serialize()`). Key subtypes:
- `UnknownChunk` / `GenericChunk`: Preserves unrecognized metadata as opaque bytes
- `AlignmentChunk`: JUNK/PAD padding for disk alignment
- `WaveFmtChunk` / `AiffCommChunk`: Format-specific header chunks
- `DataChunk`: Audio payload with file offset tracking (not loaded into RAM)
- `BextChunk`, `Ds64Chunk`, `FactChunk`: Specialized metadata

Chunks are stored as `shared_ptr<Chunk>` and polymorphically serialized during `flush()`.

### Format handling

Endianness is format-dependent: WAVE is little-endian, AIFF is big-endian. The code uses `boost::endian` types throughout for type-safe conversions. FourCC codes are always `big_uint32_t`. Binary structures for each format are defined in `WAVE.h` and `AIFF.h`.

`ChunkHead` (in `AudioTypes.h`) is a generic 8-byte chunk header with a union of `little_uint32_t lSize` and `big_uint32_t bSize`, mirroring the container header pattern in `AudioFile::m_header`. It allows users to construct chunks that work with either format. `addChunk()` and `replaceChunk()` derive the total chunk size from this header automatically — no explicit size parameter is needed.

`BigFloat80.h` implements software conversion between 80-bit IEEE 754 extended float (used in AIFF COMM chunks for sample rate) and `double`.

### Flush model

`AudioFile::flush()` rebuilds the entire file atomically: writes to a temp file, then renames. It inserts alignment padding (4KB for WAVE, 512B for AIFF) between metadata and audio data. Metadata chunks are written first, then a padding/alignment chunk, then the audio payload.

### Math utilities

- `VectorMath<T>`: Template extending `std::vector<T>` with SIMD-friendly operations (`sum`, `max_mag`, `normalize_mag`, operator overloads using `std::reduce` / `std::transform_reduce`)
- `WindowedSinc<T>`: Windowed sinc filter kernel generator with Blackman/Hamming windows and fused multiply-sum convolution

### Exception hierarchy (DiskerrorExceptions.h)

```
Exception (std::runtime_error)
  FileError -> FileNotFound, FileExists, NotARegularFile, FileOpenError
  FormatError -> InvalidHeader, UnsupportedFormat, BitDepthError
  UsageError -> ReservedChunkError
```

## Reference documentation

`AUDIO_SPECS.md` is the primary specification reference for WAVE/AIFF/RF64 format details, chunk mappings, alignment rules, and encoding. The `docs/` directory contains raw format specifications.
