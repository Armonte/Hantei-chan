#pragma once
#include "cmd_document.h"
#include <algorithm>
namespace cmdfile::grab {
inline constexpr const char* Routes = "GameplayExtension.ThrowTechRoutes.v1";
inline constexpr const char* Vectors = "GameplayExtension.ThrowTechVectors.v1";
inline std::string Key(int profile, const char* role, int domain, int context) {
    std::string key = "Profile." + std::to_string(profile) + "." + role;
    if (domain) { key += domain == 1 ? ".Ground" : ".Air";
        if (context) key += context == 1 ? ".Raw" : context == 2 ? ".Combo" : ".Counter";
    }
    return key;
}
inline std::optional<std::string> Exact(const Document& doc, const std::string& section, const std::string& key) {
    for (const auto& a : doc.assignments) if (a.section == section && a.key == key) return a.value;
    return std::nullopt;
}
inline std::optional<std::string> Effective(const Document& doc, const char* section, int profile, const char* role, int domain, int context) {
    if (auto v = Exact(doc, section, Key(profile, role, domain, context))) return v;
    if (domain && context) if (auto v = Exact(doc, section, Key(profile, role, domain, 0))) return v;
    if (domain) return Exact(doc, section, Key(profile, role, 0, 0));
    return std::nullopt;
}
inline std::vector<int> Ids(const Document& doc) {
    std::vector<int> ids;
    for (const auto& a : doc.assignments) if ((a.section == Routes || a.section == Vectors) && a.key.rfind("Profile.", 0) == 0) {
        const auto end = a.key.find('.', 8);
        if (end == std::string::npos) continue;
        const auto id = ParseInteger(a.key.substr(8, end - 8));
        if (id && *id >= 1 && *id <= 255) ids.push_back((int)*id);
    }
    std::sort(ids.begin(), ids.end()); ids.erase(std::unique(ids.begin(), ids.end()), ids.end()); return ids;
}
inline bool Pattern(const std::optional<std::string>& v, int& pattern) {
    if (!v || v->rfind("Pattern:", 0) != 0) return false;
    const auto n = ParseInteger(v->substr(8));
    if (!n || *n < 0 || *n > 65534) return false;
    pattern = (int)*n; return true;
}
inline bool Vector(const std::optional<std::string>& v, int& id, int& step, int& direction) {
    if (!v || v->rfind("HitVector:", 0) != 0) return false;
    const auto s = v->find(",Step:"), d = v->find(",Direction:");
    if (s == std::string::npos || d == std::string::npos || d <= s + 6) return false;
    auto i = ParseInteger(v->substr(10, s - 10)), t = ParseInteger(v->substr(s + 6, d - s - 6));
    if (!i || !t || *i < 0 || *i > 254 || *t < 0 || *t > 4) return false;
    const char* names[] = { "Away", "Toward", "Left", "Right" };
    for (int n = 0; n < 4; ++n) if (v->substr(d + 11) == names[n]) { id = (int)*i; step = (int)*t; direction = n; return true; }
    return false;
}
inline std::string VectorText(int id, int step, int direction) {
    const char* names[] = { "Away", "Toward", "Left", "Right" };
    return "HitVector:" + std::to_string(id) + ",Step:" + std::to_string(step) + ",Direction:" + names[direction];
}
}
