// StemmerSnowball.cp — C++ bridge to the vendored Snowball libstemmer_c
// (see vendor/libstemmer_c/). Owns one struct sb_stemmer* per (thread,
// language) pair, since the C library's stemmer objects are not
// documented as thread-safe and are cheap enough to keep around.

#include "StemmerSnowball.h"

#include "vendor/libstemmer_c/include/libstemmer.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Diskerror {

namespace {

// One sb_stemmer per language, guarded by a mutex. libstemmer's own
// per-call state (the last stemmed word) lives inside the sb_stemmer, so
// calls for the SAME language must not run concurrently; calls for
// DIFFERENT languages can, since each has its own struct sb_stemmer.
// A single mutex protecting the whole map is simpler than per-language
// locks and cheap enough — stemming a single word is microseconds.
struct StemmerCache {
    std::mutex mu;
    std::unordered_map<std::string, struct sb_stemmer*> stemmers;

    ~StemmerCache() {
        for (auto& [name, s] : stemmers) sb_stemmer_delete(s);
    }
};

StemmerCache& cache() {
    static StemmerCache instance;
    return instance;
}

// Returns nullptr if `language` is not a recognised algorithm name.
struct sb_stemmer* get_stemmer_locked(const std::string& language) {
    auto& c = cache();
    auto it = c.stemmers.find(language);
    if (it != c.stemmers.end()) return it->second;

    struct sb_stemmer* s = sb_stemmer_new(language.c_str(), "UTF_8");
    c.stemmers.emplace(language, s);  // cache the miss too (s may be null)
    return s;
}

// Short (ISO-639-1-ish) code -> canonical Snowball long name. "porter" has
// no natural short code and is intentionally absent here.
const std::unordered_map<std::string, std::string>& short_name_map() {
    static const std::unordered_map<std::string, std::string> m = {
        {"ar", "arabic"},     {"hy", "armenian"},   {"eu", "basque"},
        {"ca", "catalan"},    {"da", "danish"},     {"nl", "dutch"},
        {"en", "english"},    {"fi", "finnish"},    {"fr", "french"},
        {"de", "german"},     {"el", "greek"},      {"hi", "hindi"},
        {"hu", "hungarian"},  {"id", "indonesian"}, {"ga", "irish"},
        {"it", "italian"},    {"lt", "lithuanian"}, {"ne", "nepali"},
        {"no", "norwegian"},  {"pt", "portuguese"}, {"ro", "romanian"},
        {"ru", "russian"},    {"sr", "serbian"},    {"es", "spanish"},
        {"sv", "swedish"},    {"ta", "tamil"},      {"tr", "turkish"},
        {"yi", "yiddish"},
    };
    return m;
}

std::string lowercase(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return out;
}

} // namespace

std::string snowball_canonical_name(std::string_view language) {
    std::string lang = lowercase(language);
    auto it = short_name_map().find(lang);
    return it != short_name_map().end() ? it->second : lang;
}

std::string stem_snowball(std::string_view word, std::string_view language) {
    if (word.empty()) return "";

    std::string lang = snowball_canonical_name(language);
    std::string w(word);
    // Snowball algorithms expect lowercase input for case-sensitive
    // scripts; this is a no-op for scripts without case.
    std::transform(w.begin(), w.end(), w.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    auto& c = cache();
    std::lock_guard<std::mutex> lock(c.mu);

    struct sb_stemmer* s = get_stemmer_locked(lang);
    if (!s) return w;  // unrecognised language: return input unchanged

    const sb_symbol* stemmed = sb_stemmer_stem(
        s, reinterpret_cast<const sb_symbol*>(w.data()),
        static_cast<int>(w.size()));
    if (!stemmed) return w;  // out-of-memory in the C library: degrade gracefully

    return std::string(reinterpret_cast<const char*>(stemmed),
                       static_cast<size_t>(sb_stemmer_length(s)));
}

bool snowball_language_supported(std::string_view language) {
    std::string lang = snowball_canonical_name(language);
    const char** names = sb_stemmer_list();
    for (; *names != nullptr; ++names) {
        if (lang == *names) return true;
    }
    return false;
}

} // namespace Diskerror
