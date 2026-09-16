// test_stemmer_snowball.cp — Unit tests for the Snowball multi-language stemmer

#include "StemmerSnowball.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace Diskerror;

int main() {
    std::cout << "Running Snowball stemmer tests...\n";

    auto test_stem = [](const std::string& word, const std::string& language,
                        const std::string& expected) {
        std::string result = stem_snowball(word, language);
        if (result != expected) {
            std::cerr << "FAIL: stem_snowball(\"" << word << "\", \"" << language
                      << "\") = \"" << result << "\", expected \"" << expected << "\"\n";
            return false;
        }
        std::cout << "  \xE2\x9C\x93 [" << language << "] \"" << word << "\" -> \""
                  << result << "\"\n";
        return true;
    };

    // English (default language)
    assert(test_stem("running", "english", "run"));
    assert(test_stem("flies", "english", "fli"));
    assert(test_stem("nationalization", "english", "nation"));

    // Default language argument omitted -> english
    assert(stem_snowball("running") == "run");

    // A second language, to prove multi-language support actually works
    // (not just a single hardcoded English table).
    assert(test_stem("chevaux", "french", "cheval"));
    assert(test_stem("manger", "french", "mang"));

    // A third, unrelated language family.
    assert(test_stem("laufen", "german", "lauf"));

    // Case-insensitivity
    std::string upper_result = stem_snowball("RUNNING", "english");
    assert(upper_result == "run");
    std::cout << "  \xE2\x9C\x93 \"RUNNING\" -> \"" << upper_result << "\" (lowercase)\n";

    // Empty input
    assert(stem_snowball("", "english") == "");

    // Unrecognised language: returns lowercased input unchanged, not a crash.
    assert(stem_snowball("Running", "klingon") == "running");
    std::cout << "  \xE2\x9C\x93 Unknown language degrades to lowercased passthrough\n";

    // Algorithm-name introspection.
    assert(snowball_language_supported("english"));
    assert(snowball_language_supported("russian"));
    assert(!snowball_language_supported("klingon"));
    std::cout << "  \xE2\x9C\x93 snowball_language_supported() correct\n";

    std::cout << "\n\xE2\x9C\x93 All Snowball stemmer tests passed\n";
    return 0;
}
