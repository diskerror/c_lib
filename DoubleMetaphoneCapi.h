// DoubleMetaphoneCapi.h — plain-C bridge to the C++ Double Metaphone
// implementation (DoubleMetaphone.h/.cp), so C translation units can call
// it without pulling in C++ headers.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Phonize `text` per `mode`:
//   0 = all words, both codes (primary + alternate when they differ)
//   1 = first code per word (the primary — always present)
//   2 = last code per word (the alternate when one exists, else the
//       primary — always present, never NULL for alphabetic input)
// Returns a malloc'd, space-joined string (caller must diskerror_free() it),
// or NULL only for non-alphabetic/empty input.
char *diskerror_phonize_mode(const char *text, int mode);

// Free a string returned by diskerror_phonize_mode().
void diskerror_free(char *p);

#ifdef __cplusplus
}
#endif
