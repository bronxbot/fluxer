#pragma once
// emojis.h — Configurable bot emoji strings for Fluxer.
// Falls back to unicode emojis if env vars are not set.

#include <cstdlib>
#include <string>

namespace bronx {

inline std::string emj_yes;      // ✅ check / approved / affirmative
inline std::string emj_no;       // ❌ denied / cancelled / no
inline std::string emj_warning;  // ⚠️ warning / caution
inline std::string emj_error;    // 🛑 error / permission failure

inline void load_emojis() {
    auto from_env = [](const char* key, const char* name, const std::string& unicode_fallback) -> std::string {
        const char* val = std::getenv(key);
        if (val && val[0] != '\0') {
            std::string s(val);
            if (s.front() == '<' || s.front() == ':') return s;
            return std::string("<:") + name + ":" + s + ">";
        }
        return unicode_fallback;
    };

    emj_yes     = from_env("YES_EMJ",     "yes",     "✅");
    emj_no      = from_env("NO_EMJ",      "no",      "❌");
    emj_warning = from_env("WARNING_EMJ", "warning", "⚠️");
    emj_error   = from_env("ERROR_EMJ",   "error",   "🛑");
}

}  // namespace bronx
