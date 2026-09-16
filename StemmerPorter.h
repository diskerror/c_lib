// StemmerPorter.h — English word stemming via the classic Porter (1980)
// algorithm.
//
// Superseded for new code by StemmerSnowball.h, which wraps the vendored
// Snowball libstemmer_c and supports English plus 28 other languages
// through one common API. This file is kept for existing callers/tests
// during the migration; it implements ONLY the original 1980 Porter
// algorithm (not Snowball/Porter2, despite this file's previous header
// comment claiming otherwise).
//
// Example: "running" → "run", "flies" → "fli", "quickly" → "quickli"
//
// No external dependencies (pure C++ implementation). Self-contained,
// header + one .cp.
#pragma once

#include <string>
#include <string_view>

namespace Diskerror {

// Stem a single English word using the classic Porter algorithm.
// Returns the stemmed form (lowercase). Non-alphabetic input returns
// an empty string.
std::string stem_en(std::string_view word);

} // namespace Diskerror
