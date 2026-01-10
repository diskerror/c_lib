# c_lib

## Project Overview
`c_lib` is a C++ library (specifically `libdiskerror_audio`) focused on audio file processing (WAVE, AIFF) and high-precision mathematics (`BigFloat80`). It also includes a command-line application helper (`clapp`).

## Architecture & Technologies
-   **Language:** C++23
-   **Dependencies:** Boost (specifically `boost/cstdfloat.hpp`, `boost/endian/arithmetic.hpp`).
-   **Build System:** GNU Make
-   **Structure:** Source files are in the root. Object files go to `build/`. Compiled libraries go to `lib/`.

## Key Components
-   **AudioFile:** Handling of WAVE and AIFF audio file formats.
-   **BigFloat80:** High-precision floating-point arithmetic.
-   **clapp:** "Command Line APPlication" - A base class for building CLI tools.

## Building
The project uses a `Makefile` for compilation.

```bash
make
```

**Note:** The Makefile is currently configured with specific include paths for macOS (MacPorts). You may need to adjust `CXXFLAGS` in the `Makefile` if your environment differs.

## Development Conventions
-   **Source Extension:** `.cp` is used for C++ source files (instead of `.cpp`).
-   **Header Extension:** `.h`
-   **Namespace:** Code is contained within the `Diskerror` namespace.
``