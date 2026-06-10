#include "raym3/styles/Stylesheet.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace raym3 {

static std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::vector<std::string> splitTrim(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::istringstream ss(s);
    std::string part;
    while (std::getline(ss, part, delim)) {
        std::string t = trimStr(part);
        if (!t.empty()) parts.push_back(t);
    }
    return parts;
}

Stylesheet& Stylesheet::Global() {
    static Stylesheet instance;
    return instance;
}

void Stylesheet::Clear() { rules_.clear(); }

bool Stylesheet::MediaMatches(MediaMatch media, bool effectiveDark) {
    switch (media) {
        case MediaMatch::None: return true;
        case MediaMatch::PrefersDark: return effectiveDark;
        case MediaMatch::PrefersLight: return !effectiveDark;
        case MediaMatch::DarkVariant: return effectiveDark;
    }
    return true;
}

std::string Stylesheet::NormalizeSelector(const std::string& selector) {
    std::string sel = trimStr(selector);
    if (sel.empty()) return sel;
    if (sel[0] != '.') sel = "." + sel;
    return sel;
}

MediaMatch Stylesheet::ParseMediaHeader(const std::string& atRuleHeader) {
    std::string h = toLower(trimStr(atRuleHeader));
    if (h.rfind("@media", 0) != 0) return MediaMatch::None;
    if (h.find("prefers-color-scheme") == std::string::npos) return MediaMatch::None;
    if (h.find("dark") != std::string::npos) return MediaMatch::PrefersDark;
    if (h.find("light") != std::string::npos) return MediaMatch::PrefersLight;
    return MediaMatch::None;
}

static std::string unescapeCssSelector(const std::string& selector) {
    std::string out;
    out.reserve(selector.size());
    for (size_t i = 0; i < selector.size(); ++i) {
        if (selector[i] == '\\' && i + 1 < selector.size()) {
            out += selector[i + 1];
            ++i;
        } else {
            out += selector[i];
        }
    }
    return out;
}

void Stylesheet::StoreRule(const std::string& selector, const CSSPropMap& props, MediaMatch media) {
    if (props.empty()) return;
    std::string sel = unescapeCssSelector(NormalizeSelector(selector));
    if (sel.empty()) return;
    rules_[sel].push_back({media, props});
    if (sel.size() > 1 && sel[0] == '.') {
        rules_[sel.substr(1)].push_back({media, props});
    }
}

void Stylesheet::AddRule(const std::string& selector, const CSSPropMap& props, MediaMatch media) {
    std::string sel = trimStr(selector);
    MediaMatch ruleMedia = media;

    if (sel.rfind(".dark ", 0) == 0 || sel.rfind(".dark.", 0) == 0) {
        ruleMedia = MediaMatch::DarkVariant;
        if (sel.rfind(".dark ", 0) == 0) sel = trimStr(sel.substr(6));
        else if (sel.rfind(".dark.", 0) == 0) sel = sel.substr(5);
    }

    for (auto& part : splitTrim(sel, ',')) {
        StoreRule(part, props, ruleMedia);
    }
}

CSSPropMap Stylesheet::ResolveClasses(const std::string& classNames, bool effectiveDark) const {
    CSSPropMap merged;
    for (auto& name : splitTrim(classNames, ' ')) {
        if (name.empty()) continue;
        for (const std::string& key : {"." + name, name}) {
            auto it = rules_.find(key);
            if (it == rules_.end()) continue;
            for (const RuleEntry& entry : it->second) {
                if (!MediaMatches(entry.media, effectiveDark)) continue;
                for (const auto& [k, v] : entry.props) merged[k] = v;
            }
            break;
        }
    }
    return merged;
}

} // namespace raym3
