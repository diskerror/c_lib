// StemmerPorter.cp — English word stemming using the classic Porter
// algorithm (1980).
//
// Based on Martin Porter's original stemming algorithm (1980).
// A simpler, proven approach to English stemming that covers common suffixes.

#include "StemmerPorter.h"
#include <algorithm>
#include <cctype>

namespace Diskerror {

class PorterStemmer {
private:
    std::string b;  // buffer holding the word to be stemmed
    int k, k0;
    int j;   // offset within the buffer
    
    bool is_vowel(int i) const {
        char c = b[i];
        return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
    }
    
    bool contains_vowel() const {
        for (int i = 0; i <= j; ++i) {
            if (is_vowel(i)) return true;
        }
        return false;
    }
    
    bool ends_double_consonant() const {
        if (j < 1) return false;
        return b[j] == b[j-1] && !is_vowel(j) && !is_vowel(j-1);
    }
    
    bool cvc(int i) const {
        // Returns true if i-2,i-1,i form a consonant-vowel-consonant sequence
        // with the consonant at i not being w, x or y
        if (i < 2 || is_vowel(i-2) || !is_vowel(i-1) || is_vowel(i))
            return false;
        char ch = b[i];
        return ch != 'w' && ch != 'x' && ch != 'y';
    }
    
    bool ends(const std::string& s) const {
        int l = s.length();
        if (j < k0 + l - 1) return false;
        for (int i = 0; i < l; ++i) {
            if (b[k0 + j - l + 1 + i] != s[i]) return false;
        }
        return true;
    }
    
    void setto(const std::string& s) {
        int l = s.length();
        for (int i = 0; i < l; ++i)
            b[k0 + j - ends_len() + 1 + i] = s[i];
        j = k0 + j - ends_len() + l;
    }
    
    int ends_len() const {
        // Returns the length of the ending
        if (ends("sses")) return 4;
        if (ends("ies")) return 3;
        if (ends("ss")) return 2;
        if (ends("s")) return 1;
        return 0;
    }
    
    void r(const std::string& s) {
        if (m() > 0) setto(s);
    }
    
    int m() const {
        // Measure function: counts the VC (vowel-consonant) sequence length
        // Returns VC, where V = one or more vowels, C = one or more consonants
        int n = 0;
        bool vowel = false;
        for (int i = k0; i <= j; ++i) {
            bool v = is_vowel(i);
            if (v != vowel) {
                if (!v) n++;
                vowel = v;
            }
        }
        return n;
    }
    
public:
    std::string stem_word(const std::string& word) {
        b = word;
        k = word.length() - 1;
        k0 = 0;
        j = k;
        
        if (k <= 1) return b;  // Words of 1 or 2 chars are not stemmed
        
        step1ab();
        step1c();
        step2();
        step3();
        step4();
        step5();
        
        return b.substr(k0, j + 1 - k0);
    }
    
private:
    void step1ab() {
        if (b[j] == 's') {
            if (ends("sses")) {
                j -= 2;
            } else if (ends("ies")) {
                setto("i");
            } else if (b[j-1] != 's') {
                j--;
            }
        }
        if (ends("eed")) {
            if (m() > 0) j--;
        } else if ((ends("ed") || ends("ing")) && contains_vowel()) {
            j -= (b[j] == 'e' ? 2 : 3);
            step1ab_post();
        }
    }
    
    void step1ab_post() {
        if (ends("at") || ends("bl") || ends("iz")) {
            b += 'e';
            ++j;
        } else if (b[j] == b[j-1] && b[j] != 'l' && b[j] != 's' && b[j] != 'z') {
            j--;
        } else if (m() == 1 && cvc(j)) {
            b += 'e';
            ++j;
        }
    }
    
    void step1c() {
        if (ends("y") && contains_vowel())
            b[j] = 'i';
    }
    
    void step2() {
        if (b[j-1] != 'o' || b[j] != 'y') {  // optimization to avoid looking beyond j
            switch (b[j-1]) {
            case 'a': if (ends("ational")) { r("ate"); return; }
                      if (ends("tional")) { r("tion"); return; }
                      break;
            case 'c': if (ends("enci")) { r("ence"); return; }
                      if (ends("anci")) { r("ance"); return; }
                      break;
            case 'e': if (ends("izer")) { r("ize"); return; }
                      break;
            case 'l': if (ends("bli")) { r("ble"); return; }
                      if (ends("alli")) { r("al"); return; }
                      if (ends("entli")) { r("ent"); return; }
                      if (ends("eli")) { r("e"); return; }
                      if (ends("ousli")) { r("ous"); return; }
                      break;
            case 'o': if (ends("ization")) { r("ize"); return; }
                      if (ends("ation")) { r("ate"); return; }
                      if (ends("ator")) { r("ate"); return; }
                      break;
            case 's': if (ends("alism")) { r("al"); return; }
                      if (ends("iveness")) { r("ive"); return; }
                      if (ends("fulness")) { r("ful"); return; }
                      if (ends("ousness")) { r("ous"); return; }
                      break;
            case 't': if (ends("aliti")) { r("al"); return; }
                      if (ends("iviti")) { r("ive"); return; }
                      if (ends("biliti")) { r("ble"); return; }
                      break;
            case 'g': if (ends("logi")) { r("log"); return; }
                      break;
            }
        }
    }
    
    void step3() {
        switch (b[j]) {
        case 'e': if (ends("icate")) { r("ic"); return; }
                  if (ends("ative")) { if (m() > 0) j -= 5; return; }
                  if (ends("alize")) { r("al"); return; }
                  return;
        case 'i': if (ends("iciti")) { r("ic"); return; }
                  return;
        case 'l': if (ends("ical")) { r("ic"); return; }
                  if (ends("ful")) { j -= 3; return; }
                  return;
        case 's': if (ends("ness")) { j -= 4; return; }
                  return;
        }
    }
    
    void step4() {
        switch (b[j-1]) {
        case 'a': if (ends("al")) break; else return;
        case 'c': if (ends("ance")) break;
                  if (ends("ence")) break; else return;
        case 'e': if (ends("er")) break; else return;
        case 'i': if (ends("ic")) break; else return;
        case 'l': if (ends("able")) break;
                  if (ends("ible")) break; else return;
        case 'n': if (ends("ant")) break;
                  if (ends("ement")) break;
                  if (ends("ment")) break;
                  if (ends("ent")) break; else return;
        case 'o': if (ends("ion")) break;
                  if (ends("ou")) break; else return;
        case 's': if (ends("ism")) break; else return;
        case 't': if (ends("ate")) break;
                  if (ends("iti")) break; else return;
        case 'u': if (ends("ous")) break; else return;
        case 'v': if (ends("ive")) break; else return;
        case 'z': if (ends("ize")) break; else return;
        default: return;
        }
        if (m() > 1) j -= 1;
    }
    
    void step5() {
        if (b[j] == 'e') {
            int a = m();
            if (a > 1 || (a == 1 && !cvc(j - 1)))
                j--;
        }
        if (b[j] == 'l' && ends_double_consonant() && m() > 1)
            j--;
    }
};

std::string stem_en(std::string_view word) {
    if (word.empty()) return "";
    
    // Convert to lowercase and check if alphabetic
    std::string s(word);
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    
    if (!std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isalpha(c);
    })) {
        return "";
    }
    
    PorterStemmer stemmer;
    return stemmer.stem_word(s);
}

} // namespace Diskerror
