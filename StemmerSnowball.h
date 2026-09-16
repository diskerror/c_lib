// StemmerSnowball.h — multi-language word stemming via the Snowball
// stemming algorithm family (vendored libstemmer_c, UTF-8 build).
//
// Unlike the single-language Porter stemmer (StemmerPorter.h, English
// only), Snowball ships one algorithm per language and a common runtime,
// so the same API stems English, French, Russian, Arabic, etc. — pick the
// algorithm by name at call time. See ragger::lang::SNOWBALL_LANGUAGES (in
// the Ragger project, once wired up) for the list this project intends to
// expose; libstemmer itself supports the following canonical names:
// arabic, armenian, basque, catalan, danish, dutch, english, finnish,
// french, german, greek, hindi, hungarian, indonesian, irish, italian,
// lithuanian, nepali, norwegian, porter, portuguese, romanian, russian,
// serbian, spanish, swedish, tamil, turkish, yiddish.
//
// Backed by vendor/libstemmer_c (BSD-3-Clause, snowballstem.org). See
// vendor/libstemmer_c/UPSTREAM_COMMIT.txt for provenance/refresh steps.
#pragma once

#include <string>
#include <string_view>

namespace Diskerror {

// Stem a single UTF-8 word using the named Snowball algorithm (default:
// "english"). Returns the stemmed form (lowercase for scripts with case).
// Returns the input unchanged (lowercased where applicable) if `language`
// is not a recognised algorithm name, and an empty string for empty input.
//
// Each distinct `language` value lazily creates and caches one stemmer
// instance for the lifetime of the process (thread-safe: one cache per
// calling thread — see .cp for details). This mirrors stem_en()'s
// signature shape from StemmerPorter.h so callers can switch between the
// two with a minimal diff, but takes an explicit language rather than
// assuming English.
std::string stem_snowball(std::string_view word, std::string_view language = "english");

// True if `language` is a recognised Snowball algorithm name (accepts
// either a canonical long name, e.g. "english", or a short ISO-639-1-ish
// code, e.g. "en" — see snowball_canonical_name()).
bool snowball_language_supported(std::string_view language);

// Maps a short language code (e.g. "en", "fr", "de") to its canonical
// Snowball algorithm name (e.g. "english", "french", "german"),
// case-insensitively. If `language` is already a canonical long name, or
// isn't a recognised short code, it is returned unchanged (lowercased) —
// callers should still check snowball_language_supported() on the result.
// "porter" has no short code (it names the original-Porter variant bundled
// with Snowball, not a natural language) and is only reachable by its long
// name.
std::string snowball_canonical_name(std::string_view language);

} // namespace Diskerror
