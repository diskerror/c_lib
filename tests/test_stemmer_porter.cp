// test_stemmer_porter.cp — Unit tests for the classic Porter English stemmer

#include "StemmerPorter.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace Diskerror;

int main() {
    std::cout << "Running stemmer tests...\n";
    
    // Test basic stemming
    auto test_stem = [](const std::string& word, const std::string& expected) {
        std::string result = stem_en(word);
        if (result != expected) {
            std::cerr << "FAIL: stem_en(\"" << word << "\") = \"" << result 
                      << "\", expected \"" << expected << "\"\n";
            return false;
        }
        std::cout << "  ✓ \"" << word << "\" → \"" << result << "\"\n";
        return true;
    };
    
    // Common test cases for Porter stemmer
    assert(test_stem("running", "run"));
    assert(test_stem("runs", "run"));
    assert(test_stem("flies", "fli"));
    assert(test_stem("hopping", "hop"));
    assert(test_stem("hoped", "ho"));
    assert(test_stem("agreed", "agre"));
    assert(test_stem("disabled", "disab"));
    
    // Edge cases
    assert(test_stem("a", "a"));     // Too short
    assert(test_stem("ab", "ab"));   // Too short
    assert(test_stem("abc", "abc")); // Minimal length
    
    // Stemming should always return lowercase
    std::string upper_result = stem_en("RUNNING");
    assert(upper_result == "run" && upper_result[0] != 'R');
    std::cout << "  ✓ \"RUNNING\" → \"" << upper_result << "\" (lowercase)\n";
    
    // Non-alphabetic input returns empty string
    assert(stem_en("123") == "");
    assert(stem_en("run123") == "");
    assert(stem_en("") == "");
    std::cout << "  ✓ Non-alphabetic input handled correctly\n";
    
    std::cout << "\n✓ All stemmer tests passed\n";
    return 0;
}
