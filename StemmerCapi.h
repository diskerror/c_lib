// StemmerCapi.h — plain-C bridge to the C++ Porter stemmer
// (StemmerPorter.h/.cp), so C translation units can call it without
// pulling in C++ headers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Stem an English word. Returns a malloc'd string (caller must
// diskerror_free() it), or NULL if input is non-alphabetic or empty.
char *diskerror_stem_en(const char *word);

// Free a string returned by diskerror_stem_en().
void diskerror_free(char *p);

#ifdef __cplusplus
}
#endif
