#pragma once

#include <map>
#include <string>
#include <vector>

namespace raym3 {

using CSSPropMap = std::map<std::string, std::string>;

enum class MediaMatch {
    None,
    PrefersDark,
    PrefersLight,
    DarkVariant,
};

class Stylesheet {
public:
    static Stylesheet& Global();

    void Clear();
    void AddRule(const std::string& selector, const CSSPropMap& props, MediaMatch media);
    CSSPropMap ResolveClasses(const std::string& classNames, bool effectiveDark) const;
    size_t SelectorCount() const { return rules_.size(); }

    static MediaMatch ParseMediaHeader(const std::string& atRuleHeader);

private:
    struct RuleEntry {
        MediaMatch media;
        CSSPropMap props;
    };

    std::map<std::string, std::vector<RuleEntry>> rules_;

    static bool MediaMatches(MediaMatch media, bool effectiveDark);
    static std::string NormalizeSelector(const std::string& selector);
    void StoreRule(const std::string& selector, const CSSPropMap& props, MediaMatch media);
};

} // namespace raym3
