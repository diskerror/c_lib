// test_stemmer_capi.cp — Unit tests for the combined plain-C stemmer
// bridge (StemmerCapi.h): Porter + Snowball surfaced as C functions.

#include "StemmerCapi.h"
#include "DoubleMetaphoneCapi.h"  // for diskerror_free's single definition
#include <cassert>
#include <cstring>
#include <iostream>

int main() {
    std::cout << "Running Stemmer C API tests...\n";

    // Porter
    char *p = diskerror_stem_en("running");
    assert(p && std::strcmp(p, "run") == 0);
    diskerror_free(p);

    assert(diskerror_stem_en("") == nullptr);
    assert(diskerror_stem_en(nullptr) == nullptr);
    std::cout << "  \xE2\x9C\x93 diskerror_stem_en()\n";

    // Snowball — default language (English)
    char *s = diskerror_stem_snowball("running", nullptr);
    assert(s && std::strcmp(s, "run") == 0);
    diskerror_free(s);

    // Snowball — canonical long name
    s = diskerror_stem_snowball("chevaux", "french");
    assert(s && std::strcmp(s, "cheval") == 0);
    diskerror_free(s);

    // Snowball — short name equivalent to long name
    s = diskerror_stem_snowball("chevaux", "fr");
    assert(s && std::strcmp(s, "cheval") == 0);
    diskerror_free(s);

    s = diskerror_stem_snowball("laufen", "de");
    assert(s && std::strcmp(s, "lauf") == 0);
    diskerror_free(s);

    assert(diskerror_stem_snowball("", "english") == nullptr);
    std::cout << "  \xE2\x9C\x93 diskerror_stem_snowball() (long + short names)\n";

    // Language support introspection, long and short forms
    assert(diskerror_snowball_language_supported("english"));
    assert(diskerror_snowball_language_supported("en"));
    assert(diskerror_snowball_language_supported("french"));
    assert(diskerror_snowball_language_supported("fr"));
    assert(diskerror_snowball_language_supported("porter"));  // long-only
    assert(!diskerror_snowball_language_supported("klingon"));
    std::cout << "  \xE2\x9C\x93 diskerror_snowball_language_supported()\n";

    std::cout << "\n\xE2\x9C\x93 All Stemmer C API tests passed\n";
    return 0;
}
