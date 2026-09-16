// StemmerCapi.h — plain-C bridge to the C++ stemmers (StemmerPorter.h and
// StemmerSnowball.h), so C translation units can call them without pulling
// in C++ headers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Stem an English word via the classic 1980 Porter algorithm. Returns a
// malloc'd string (caller must diskerror_free() it), or NULL if input is
// non-alphabetic or empty.
char *diskerror_stem_en(const char *word);

// Stem a UTF-8 word via the named Snowball algorithm (accepts either a
// canonical long name, e.g. "english", or a short code, e.g. "en"; pass
// NULL or "" for the default, "english"). Returns a malloc'd string
// (caller must diskerror_free() it). Never returns NULL for non-empty
// input (unrecognised languages degrade to a lowercased passthrough of
// the input); returns NULL only for empty input.
char *diskerror_stem_snowball(const char *word, const char *language);

// True if `language` is a recognised Snowball algorithm name (long or
// short form).
int diskerror_snowball_language_supported(const char *language);

// Free a string returned by diskerror_stem_en() or diskerror_stem_snowball().
void diskerror_free(char *p);

#ifdef __cplusplus
}
#endif
