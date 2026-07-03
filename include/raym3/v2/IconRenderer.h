#pragma once

#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace raym3::v2 {

// Icon rendering is organized into named "icon sets". The built-in "material"
// set is pre-registered at startup and resolves against the bundled/system
// Material Symbols search paths when no explicit variant has been loaded for
// it (so `<Icon name="settings"/>` keeps working with zero setup). Custom
// sets are added via LoadIconSet/LoadIconSetFromPaths and are independently
// atlased/cached — loading a custom set never invalidates another set's atlas.

void SetIconFontSearchPrefix(const char* prefix);

// Register/replace a named icon set's font variants. `variantBytes`/
// `variantPaths` map a style name ("outlined", "filled", or any custom style
// string such as "regular" for a single-style icon font) to either raw font
// bytes (TTF/OTF) or a filesystem path. Call from the JS bridge thread only
// (same threading assumption FontManager::RegisterFont already has).
void LoadIconSet(const std::string& setName,
                 const std::unordered_map<std::string, std::vector<unsigned char>>& variantBytes);
void LoadIconSetFromPaths(const std::string& setName,
                          const std::unordered_map<std::string, std::string>& variantPaths);

// name -> codepoint table for a set, so name resolution doesn't need a
// separate lookup table per caller. Additive: repeated calls merge into the
// existing table for that set rather than replacing it.
void RegisterIconNames(const std::string& setName,
                       const std::unordered_map<std::string, int>& nameToCodepoint);
int ResolveIconCodepoint(const std::string& setName, const std::string& name); // 0 if not found

// setName defaults to "material" for source compat with existing call sites.
void RegisterIcon(int codepoint, int sizeDp, bool filled,
                  const std::string& setName = "material");
void DrawIcon(int codepoint, Rectangle bounds, Color color, int sizeDp = 0,
             bool filled = false, const std::string& setName = "material");

// Back-compat wrappers for existing call sites (Checkbox/RadioButton/Switch/
// Components.cpp chevron all call these directly with hardcoded codepoints).
// Default outlined (false), matching Material Symbols' own FILL=0 default.
void RegisterMaterialIcon(int codepoint, int sizeDp, bool filled = false);
void DrawMaterialIcon(int codepoint, Rectangle bounds, Color color,
                      int sizeDp = 0, bool filled = false);

void ResetIconSetAtlas(const std::string& setName = "material");
void ResetAllIconAtlases();
// Drop all GPU-side state WITHOUT unloading (the device was re-initialized,
// so held texture ids are stale and may already be reused). Atlases rebuild
// lazily on the next DrawIcon per set.
void IconRendererResetDeviceCache();

} // namespace raym3::v2
