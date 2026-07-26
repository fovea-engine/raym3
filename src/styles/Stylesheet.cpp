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

void Stylesheet::Clear() {
    rules_.clear();
    vars_.clear();
    varOverrides_.clear();
    stateMask_ = 0;
    varsVersion_++;
}

void Stylesheet::SetViewport(float widthDp, float heightDp) {
    if (widthDp == viewportW_ && heightDp == viewportH_) return;
    viewportW_ = widthDp;
    viewportH_ = heightDp;
    // Responsive rules/vars resolved against the old size are now stale.
    varsVersion_++;
}

bool Stylesheet::MediaMatches(const MediaQuery& media, bool effectiveDark) const {
    if (media.skip) return false;

    // Color scheme.
    if (media.scheme == MediaQuery::Scheme::Dark && !effectiveDark) return false;
    if (media.scheme == MediaQuery::Scheme::Light && effectiveDark) return false;

    // Dimensional features are only evaluated once the viewport is known
    // (viewportW_/H_ default to 0 before the first resize). A query that
    // constrains size but has no viewport yet is treated as non-matching so it
    // does not apply spuriously on the very first frame.
    const bool hasDimConstraint =
        media.minWidth >= 0.0f || media.maxWidth >= 0.0f ||
        media.minHeight >= 0.0f || media.maxHeight >= 0.0f ||
        media.orient != MediaQuery::Orient::Any;
    if (hasDimConstraint && (viewportW_ <= 0.0f || viewportH_ <= 0.0f)) return false;

    if (media.minWidth >= 0.0f && viewportW_ < media.minWidth) return false;
    if (media.maxWidth >= 0.0f && viewportW_ > media.maxWidth) return false;
    if (media.minHeight >= 0.0f && viewportH_ < media.minHeight) return false;
    if (media.maxHeight >= 0.0f && viewportH_ > media.maxHeight) return false;
    if (media.orient == MediaQuery::Orient::Portrait && viewportW_ > viewportH_)
        return false;
    if (media.orient == MediaQuery::Orient::Landscape && viewportW_ < viewportH_)
        return false;

    return true;
}

// `.btn:hover` -> selector becomes `.btn`, returns Hover. Unknown pseudo-classes
// leave the selector untouched so they simply never match anything.
StyleState Stylesheet::ExtractState(std::string& selector) {
    size_t colon = selector.rfind(':');
    if (colon == std::string::npos) return StyleState::Base;
    std::string pseudo = toLower(trimStr(selector.substr(colon + 1)));
    StyleState state = StyleState::Base;
    if (pseudo == "hover") state = StyleState::Hover;
    else if (pseudo == "active") state = StyleState::Active;
    else if (pseudo == "focus" || pseudo == "focus-visible" || pseudo == "focus-within")
        state = StyleState::Focus;
    else if (pseudo == "disabled") state = StyleState::Disabled;
    else return StyleState::Base;
    selector = trimStr(selector.substr(0, colon));
    return state;
}

bool Stylesheet::HasStateRules(StyleState state) const {
    return (stateMask_ & (1u << (int)state)) != 0;
}

std::string Stylesheet::NormalizeSelector(const std::string& selector) {
    std::string sel = trimStr(selector);
    if (sel.empty()) return sel;
    if (sel[0] != '.') sel = "." + sel;
    return sel;
}

// Pull the numeric part out of a `<feature>: <number>px` fragment. Returns -1
// when `feature` is absent or unparseable. Accepts px (the only length unit we
// model; 1px == 1dp in layout space) and bare numbers.
static float extractLengthFeature(const std::string& h, const std::string& feature) {
    size_t pos = h.find(feature);
    if (pos == std::string::npos) return -1.0f;
    pos = h.find(':', pos);
    if (pos == std::string::npos) return -1.0f;
    ++pos;
    while (pos < h.size() && (h[pos] == ' ' || h[pos] == '\t')) ++pos;
    size_t start = pos;
    while (pos < h.size() && (isdigit((unsigned char)h[pos]) || h[pos] == '.')) ++pos;
    if (pos == start) return -1.0f;
    try {
        return std::stof(h.substr(start, pos - start));
    } catch (...) {
        return -1.0f;
    }
}

MediaQuery Stylesheet::ParseMediaHeader(const std::string& atRuleHeader) {
    std::string h = toLower(trimStr(atRuleHeader));
    if (h.rfind("@media", 0) != 0) return MediaQuery::Skipped();

    MediaQuery q;

    // Media types we cannot target render to nothing, matching CSS semantics for
    // an unsupported target (a screen renderer never matches `print`/`speech`).
    if (h.find("print") != std::string::npos ||
        h.find("speech") != std::string::npos) {
        return MediaQuery::Skipped();
    }

    if (h.find("prefers-color-scheme") != std::string::npos) {
        if (h.find("dark") != std::string::npos) q.scheme = MediaQuery::Scheme::Dark;
        else if (h.find("light") != std::string::npos) q.scheme = MediaQuery::Scheme::Light;
    }

    q.minWidth = extractLengthFeature(h, "min-width");
    q.maxWidth = extractLengthFeature(h, "max-width");
    q.minHeight = extractLengthFeature(h, "min-height");
    q.maxHeight = extractLengthFeature(h, "max-height");

    if (h.find("orientation") != std::string::npos) {
        if (h.find("portrait") != std::string::npos) q.orient = MediaQuery::Orient::Portrait;
        else if (h.find("landscape") != std::string::npos) q.orient = MediaQuery::Orient::Landscape;
    }

    return q;
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

void Stylesheet::StoreRule(const std::string& selector, const CSSPropMap& props,
                           const MediaQuery& media, StyleState state) {
    if (props.empty()) return;
    std::string sel = unescapeCssSelector(NormalizeSelector(selector));
    if (sel.empty()) return;
    stateMask_ |= (1u << (int)state);
    rules_[sel].push_back({media, state, props});
    if (sel.size() > 1 && sel[0] == '.') {
        rules_[sel.substr(1)].push_back({media, state, props});
    }
}

void Stylesheet::AddRule(const std::string& selector, const CSSPropMap& props, const MediaQuery& media) {
    std::string sel = trimStr(selector);
    MediaQuery ruleMedia = media;

    if (sel.rfind(".dark ", 0) == 0 || sel.rfind(".dark.", 0) == 0) {
        ruleMedia = MediaQuery::DarkVariantQuery();
        if (sel.rfind(".dark ", 0) == 0) sel = trimStr(sel.substr(6));
        else if (sel.rfind(".dark.", 0) == 0) sel = sel.substr(5);
    }

    // A bare `.dark { --x: … }` block declares dark-mode variables. Only the
    // variables are reinterpreted that way: the rule's regular props keep their
    // existing behaviour of applying to elements that carry the `dark` class.
    MediaQuery varMedia = ruleMedia;
    if (varMedia == MediaQuery{}) {
        std::string bare = trimStr(sel);
        if (bare == ".dark" || bare == "dark") varMedia = MediaQuery::DarkVariantQuery();
    }

    // Custom properties are collected into the variable registry rather than
    // stored as rule props. Selector scoping is not modelled — a --name applies
    // globally, keyed only by media variant — which covers the usual
    // :root / .dark / @media (prefers-color-scheme) authoring pattern.
    CSSPropMap normal;
    for (const auto& [key, value] : props) {
        if (key.rfind("--", 0) == 0) SetVariable(key, value, varMedia);
        else normal[key] = value;
    }
    if (normal.empty()) return;

    // Each comma-separated selector carries its own pseudo-class, so the state
    // is extracted per part (`.a:hover, .b` is legal).
    for (auto& part : splitTrim(sel, ',')) {
        std::string selPart = part;
        StyleState state = ExtractState(selPart);
        StoreRule(selPart, normal, ruleMedia, state);
    }
}

void Stylesheet::SetVariable(const std::string& name, const std::string& value, const MediaQuery& media) {
    auto& entries = vars_[name];
    // A later declaration for the same media variant replaces the earlier one,
    // matching the cascade for equally-specific rules.
    for (auto& e : entries) {
        if (e.media == media) {
            e.value = value;
            varsVersion_++;
            return;
        }
    }
    entries.push_back({media, value});
    varsVersion_++;
}

void Stylesheet::SetVariableOverride(const std::string& name, const std::string& value) {
    varOverrides_[name] = value;
    varsVersion_++;
}

void Stylesheet::ClearVariableOverride(const std::string& name) {
    if (varOverrides_.erase(name)) varsVersion_++;
}

void Stylesheet::ClearVariableOverrides() {
    if (!varOverrides_.empty()) {
        varOverrides_.clear();
        varsVersion_++;
    }
}

std::string Stylesheet::GetVariable(const std::string& name, bool effectiveDark) const {
    auto ov = varOverrides_.find(name);
    if (ov != varOverrides_.end()) return ov->second;

    auto it = vars_.find(name);
    if (it == vars_.end()) return {};

    // Prefer a matching media-specific value (dark/light) over the unconditional
    // one, so `:root { --bg: white }` + a dark block resolve correctly.
    const std::string* generic = nullptr;
    const std::string* specific = nullptr;
    for (const VarEntry& e : it->second) {
        if (!MediaMatches(e.media, effectiveDark)) continue;
        if (e.media == MediaQuery{}) generic = &e.value;
        else specific = &e.value;
    }
    if (specific) return *specific;
    return generic ? *generic : std::string{};
}

std::string Stylesheet::SubstituteVars(const std::string& value, bool effectiveDark) const {
    if (value.find("var(") == std::string::npos) return value;

    std::string cur = value;
    // Each pass expands the innermost var() references; repeat so that values
    // which themselves contain var() resolve, with a cap against cycles.
    for (int pass = 0; pass < 8; pass++) {
        size_t start = cur.find("var(");
        if (start == std::string::npos) break;

        bool expandedAny = false;
        while (start != std::string::npos) {
            // Find the matching close paren for this var(.
            int depth = 0;
            size_t end = std::string::npos;
            for (size_t i = start + 3; i < cur.size(); i++) {
                if (cur[i] == '(') depth++;
                else if (cur[i] == ')') {
                    depth--;
                    if (depth == 0) { end = i; break; }
                }
            }
            if (end == std::string::npos) break;

            std::string inner = cur.substr(start + 4, end - start - 4);
            std::string name, fallback;
            // Split on the first top-level comma: var(--name, fallback)
            int d = 0;
            size_t comma = std::string::npos;
            for (size_t i = 0; i < inner.size(); i++) {
                if (inner[i] == '(') d++;
                else if (inner[i] == ')') d--;
                else if (inner[i] == ',' && d == 0) { comma = i; break; }
            }
            if (comma == std::string::npos) {
                name = trimStr(inner);
            } else {
                name = trimStr(inner.substr(0, comma));
                fallback = trimStr(inner.substr(comma + 1));
            }

            std::string resolved = GetVariable(name, effectiveDark);
            if (resolved.empty()) resolved = fallback;

            cur = cur.substr(0, start) + resolved + cur.substr(end + 1);
            expandedAny = true;
            start = cur.find("var(", start + resolved.size());
        }
        if (!expandedAny) break;
    }
    return cur;
}

CSSPropMap Stylesheet::ResolveClasses(const std::string& classNames, bool effectiveDark,
                                      StyleState state) const {
    CSSPropMap merged;
    for (auto& name : splitTrim(classNames, ' ')) {
        if (name.empty()) continue;
        for (const std::string& key : {"." + name, name}) {
            auto it = rules_.find(key);
            if (it == rules_.end()) continue;
            for (const RuleEntry& entry : it->second) {
                if (entry.state != state) continue;
                if (!MediaMatches(entry.media, effectiveDark)) continue;
                for (const auto& [k, v] : entry.props) merged[k] = v;
            }
            break;
        }
    }
    return merged;
}

} // namespace raym3
