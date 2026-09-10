// StemmerCapi.cp — C API bridge to the C++ stemmer

#include "StemmerCapi.h"
#include "Stemmer.h"
#include <cstring>
#include <cstdlib>

extern "C" {

char *diskerror_stem_en(const char *word) {
    if (!word || *word == '\0') {
        return nullptr;
    }
    
    std::string result = Diskerror::stem_en(word);
    
    if (result.empty()) {
        return nullptr;
    }
    
    char *out = static_cast<char*>(std::malloc(result.length() + 1));
    if (out) {
        std::strcpy(out, result.c_str());
    }
    return out;
}

void diskerror_free(char *p) {
    std::free(p);
}

} // extern "C"
