// Stemmer.h — English word stemming via Snowball algorithm.
//
// The Snowball English stemmer (formerly Porter2) is the contemporary
// standard for English morphological analysis. It removes common suffixes
// to reduce words to their root form, enabling better matching during search.
//
// Example: "running" → "run", "flies" → "fli", "quickly" → "quickli"
//
// No external dependencies (pure C++ implementation). Self-contained,
// header + one .cp.
#pragma once

#include <string>
#include <string_view>

namespace Diskerror {

// Stem a single English word using the Snowball algorithm.
// Returns the stemmed form (lowercase). Non-alphabetic input returns
// an empty string.
std::string stem_en(std::string_view word);

} // namespace Diskerror
