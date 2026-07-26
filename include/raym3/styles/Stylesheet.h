#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace raym3 {

using CSSPropMap = std::map<std::string, std::string>;

// A parsed @media condition. Supersedes the old color-scheme-only enum: it now
// also models responsive width/height/orientation features so layouts can react
// to the viewport size, matching the CSS/RN authoring model.
struct MediaQuery {
    enum class Scheme { Any, Dark, Light };
    enum class Orient { Any, Portrait, Landscape };

    // A default-constructed query matches everything — that is the "no @media"
    // (top-level rule) case. `skip` marks an at-block whose contents must be
    // ignored entirely (font-face/keyframes/unknown at-rule, or an @media that
    // references a feature we do not understand, e.g. `print`).
    bool skip = false;
    Scheme scheme = Scheme::Any;
    bool darkVariant = false;      // `.dark` class-scoped variant (not a real @media)
    float minWidth = -1.0f;        // dp; -1 = unconstrained
    float maxWidth = -1.0f;
    float minHeight = -1.0f;
    float maxHeight = -1.0f;
    Orient orient = Orient::Any;

    bool operator==(const MediaQuery& o) const {
        return skip == o.skip && scheme == o.scheme &&
               darkVariant == o.darkVariant && minWidth == o.minWidth &&
               maxWidth == o.maxWidth && minHeight == o.minHeight &&
               maxHeight == o.maxHeight && orient == o.orient;
    }
    bool operator!=(const MediaQuery& o) const { return !(*this == o); }

    static MediaQuery DarkVariantQuery() {
        MediaQuery q; q.darkVariant = true; q.scheme = Scheme::Dark; return q;
    }
    static MediaQuery Skipped() { MediaQuery q; q.skip = true; return q; }
};

// Interaction state a rule applies to, parsed from a trailing pseudo-class on
// the selector (`.btn:hover`). Maps onto v2::StateStyles, which the renderer
// already resolves per frame — so pseudo-classes need no new runtime state.
enum class StyleState { Base, Hover, Active, Focus, Disabled };

class Stylesheet {
public:
    static Stylesheet& Global();

    void Clear();
    void AddRule(const std::string& selector, const CSSPropMap& props, const MediaQuery& media);
    // `state` selects which pseudo-class variant to resolve; Base returns the
    // plain rules. A state resolve returns ONLY that state's declarations —
    // callers layer them over the base themselves (that is what StateStyles
    // expects).
    CSSPropMap ResolveClasses(const std::string& classNames, bool effectiveDark,
                              StyleState state = StyleState::Base) const;
    // True if any rule anywhere declares this pseudo-class, so callers can skip
    // the per-state resolve work entirely for the common no-pseudo case.
    bool HasStateRules(StyleState state) const;
    size_t SelectorCount() const { return rules_.size(); }

    static MediaQuery ParseMediaHeader(const std::string& atRuleHeader);

    // Current viewport size in dp (CSS px). Set on window resize/rotation so
    // width/height/orientation media features evaluate against live dimensions.
    // Bumps VariablesVersion so cached resolutions invalidate.
    void SetViewport(float widthDp, float heightDp);
    float ViewportWidth() const { return viewportW_; }
    float ViewportHeight() const { return viewportH_; }

    // ─── CSS custom properties (--name) ──────────────────────────────────────
    // Declared values come from the stylesheet (per media variant, so :root and
    // its prefers-color-scheme/.dark counterparts can each define a value).
    // Overrides are set at runtime from JS and win over declared values.
    void SetVariable(const std::string& name, const std::string& value, const MediaQuery& media);
    void SetVariableOverride(const std::string& name, const std::string& value);
    void ClearVariableOverride(const std::string& name);
    void ClearVariableOverrides();
    // Returns the winning value for `name`, or empty if undeclared.
    std::string GetVariable(const std::string& name, bool effectiveDark) const;
    size_t VariableCount() const { return vars_.size(); }

    // Expand every var(--name[, fallback]) in `value`. Unresolvable references
    // fall back to their fallback argument, or to an empty string.
    std::string SubstituteVars(const std::string& value, bool effectiveDark) const;

    // Bumped whenever any variable changes, so callers can invalidate styles
    // they resolved from a previous state.
    uint64_t VariablesVersion() const { return varsVersion_; }

private:
    struct RuleEntry {
        MediaQuery media;
        StyleState state = StyleState::Base;
        CSSPropMap props;
    };

    struct VarEntry {
        MediaQuery media;
        std::string value;
    };

    std::map<std::string, std::vector<RuleEntry>> rules_;
    std::map<std::string, std::vector<VarEntry>> vars_;
    std::map<std::string, std::string> varOverrides_;
    uint64_t varsVersion_ = 0;
    float viewportW_ = 0.0f;
    float viewportH_ = 0.0f;

    bool MediaMatches(const MediaQuery& media, bool effectiveDark) const;
    static std::string NormalizeSelector(const std::string& selector);
    void StoreRule(const std::string& selector, const CSSPropMap& props,
                   const MediaQuery& media, StyleState state);
    // Splits a trailing pseudo-class off a selector: ".btn:hover" -> (".btn", Hover).
    static StyleState ExtractState(std::string& selector);
    uint32_t stateMask_ = 0;   // bit per StyleState seen while parsing
};

} // namespace raym3
