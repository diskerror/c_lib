# c_lib

## Project Overview

`c_lib` is a C++ library (specifically `libdiskerror_audio`) focused on audio file processing (WAVE, AIFF).

## Architecture & Technologies

- **Language:** C++23
- **Dependencies:** Boost (specifically `boost/cstdfloat.hpp`, `boost/endian/arithmetic.hpp`).
- **Build System:** GNU Make
- **Structure:** Source files are in the root. Object files go to `build/`. Compiled libraries go to `lib/`.

## Key Components

- **AudioFile:** Chunk-based I/O for WAVE and AIFF audio files. Stores metadata chunks as opaque byte blobs; audio data
  stays on disk. Enforces singleton chunk rules and writes chunks in spec-required order.
- **AudioFormat:** Format-agnostic access to sample rate, channels, bit depth, and encoding. Produces all required
  format chunks (`fmt`/`fact` for WAVE, `FVER`/`COMM` for AIFC) via `toChunk()`.
- **AudioSamples:** Reads/writes audio data as normalized float32, with automatic deinterleaving.
- **VectorMath:** SIMD-friendly numeric vector with reduction operations for DSP.
- **WindowedSinc:** Windowed sinc filter kernel generator for resampling and filtering.
- **BigFloat80:** Software conversion of big-endian 80-bit float (AIFF sample rate).

## Building

The project uses a `Makefile` for compilation.

```bash
make
```

**Note:** The Makefile is currently configured with specific include paths for macOS (MacPorts). You may need to adjust
`CXXFLAGS` in the `Makefile` if your environment differs.

## Development Conventions

- **Source Extension:** `.cp` is used for C++ source files (instead of `.cpp`).
- **Header Extension:** `.h`
- **Namespace:** Code is contained within the `Diskerror` namespace.
