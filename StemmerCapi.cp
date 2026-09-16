// StemmerCapi.cp — C API bridge to the C++ Porter and Snowball stemmers

#include "StemmerCapi.h"
#include "StemmerPorter.h"
#include "StemmerSnowball.h"
#include <cstring>
#include <cstdlib>

namespace {

char *dup_result(const std::string &result) {
    char *out = static_cast<char *>(std::malloc(result.length() + 1));
    if (out) {
        std::strcpy(out, result.c_str());
    }
    return out;
}

} // namespace

extern "C" {

char *diskerror_stem_en(const char *word) {
    if (!word || *word == '\0') {
        return nullptr;
    }

    std::string result = Diskerror::stem_en(word);

    if (result.empty()) {
        return nullptr;
    }

    return dup_result(result);
}

char *diskerror_stem_snowball(const char *word, const char *language) {
    if (!word || *word == '\0') {
        return nullptr;
    }
    std::string_view lang = (language && *language) ? std::string_view(language)
                                                      : std::string_view("english");
    std::string result = Diskerror::stem_snowball(word, lang);
    return dup_result(result);
}

int diskerror_snowball_language_supported(const char *language) {
    if (!language) return 0;
    return Diskerror::snowball_language_supported(language) ? 1 : 0;
}

// diskerror_free() is defined once, in DoubleMetaphoneCapi.cp — every
// consumer of this bridge already links diskerror_double_metaphone too
// (both are always linked together into semqlite_ext), and a second
// identical definition here would collide at link time.

} // extern "C"
