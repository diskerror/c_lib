// test_double_metaphone.cp — basic tests for Diskerror::double_metaphone/phonize.

#include "double_metaphone.h"

#include <cstdio>
#include <string>

using namespace Diskerror;

static int failures = 0;

static void check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL %s\n", what);
        ++failures;
    }
}

int main() {
    // Single-code word
    auto codes = double_metaphone("Smith");
    check(codes.size() == 2, "Smith has two codes");
    check(codes[0] == "SM0", "Smith primary = SM0");  // 'th' → '0'
    check(codes[1] == "XMT", "Smith alternate = XMT");

    // Phonize multi-word
    std::string p = phonize("Don't panic");
    check(p == "TNT PNK", "phonize Don't panic");

    // Empty input
    check(double_metaphone("").empty(), "empty input -> empty");
    check(phonize("123").empty(), "digits -> empty");

    // Schmidt — Germanic SCH
    auto sc = double_metaphone("Schmidt");
    check(!sc.empty() && sc[0] == "XMT", "Schmidt primary = XMT");

    if (failures == 0) std::printf("all double_metaphone tests passed\n");
    else               std::printf("%d double_metaphone test(s) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
