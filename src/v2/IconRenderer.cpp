#include "raym3/v2/IconRenderer.h"

#include "raym3/config.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/v2/Density.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace raym3::v2 {

struct IconKey {
  int cp;
  int sizeDp;
  bool filled;
  bool operator<(const IconKey &o) const {
    if (cp != o.cp)
      return cp < o.cp;
    if (sizeDp != o.sizeDp)
      return sizeDp < o.sizeDp;
    return filled < o.filled;
  }
};

struct IconFontKey {
  int sizeDp;
  bool filled;
  bool operator<(const IconFontKey &o) const {
    return sizeDp != o.sizeDp ? sizeDp < o.sizeDp : filled < o.filled;
  }
};

// A loaded font variant for one style within a set ("outlined"/"filled"/
// "regular"/anything else), sourced from either raw bytes or a filesystem
// path. Bytes take priority when both are present.
struct VariantSource {
  std::vector<unsigned char> bytes;
  std::string path;
  bool HasBytes() const { return !bytes.empty(); }
};

// All state for one named icon set. Kept independent per set so loading a
// custom set never invalidates another set's (e.g. "material"'s) atlas.
struct IconSetState {
  std::unordered_map<std::string, VariantSource> variants; // style -> source
  std::unordered_map<std::string, int> names;              // icon name -> codepoint
  std::set<IconKey> requests;
  std::set<int> codepoints;
  std::map<IconFontKey, Font> fonts;
  Texture2D atlas = {0};
  std::map<IconKey, Rectangle> rects;
  bool atlasDirty = false;
};

static std::unordered_map<std::string, IconSetState> g_sets;
static std::string g_fontSearchPrefix;

static IconSetState &GetOrCreateSet(const std::string &setName) {
  return g_sets[setName];
}

void SetIconFontSearchPrefix(const char *prefix) {
  g_fontSearchPrefix = (prefix && *prefix) ? prefix : "";
}

static int PixelSize(int sizeDp) { return Density::RasterPixels((float)sizeDp); }

// Fallback candidate search used ONLY for the built-in "material" set when no
// explicit variant has been loaded for it via LoadIconSet/LoadIconSetFromPaths.
// The Material Symbols variant matching `filled` must be tried FIRST — it's
// the only font family that can actually distinguish fill state (classic
// MaterialIcons-Regular.ttf is single-style, so putting it first made
// `filled` a no-op wherever it shipped). Verified material_icons.js's full
// codepoint map resolves 4253/4253 in both MaterialSymbolsRounded.ttf and
// -Filled.ttf, vs 2053/4253 in classic, so this ordering doesn't regress
// coverage; classic stays as the final fallback.
static const char *FindMaterialFontPathFallback(bool filled) {
  static std::string s_found;
  static std::vector<std::string> filledCandidates = {
      "./rayact/resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./rayact/resources/fonts/MaterialSymbolsRounded.ttf",
      "./rayact/resources/fonts/MaterialIcons-Regular.ttf",
      std::string(RAYM3_RESOURCE_DIR) +
          "/fonts/MaterialSymbolsRounded-Filled.ttf",
      std::string(RAYM3_RESOURCE_DIR) + "/MaterialSymbolsRounded-Filled.ttf",
      "./resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./raym3/resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "../raym3/resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./resources/fonts/MaterialSymbolsRounded.ttf",
      "./resources/fonts/MaterialIcons-Regular.ttf",
  };
  static std::vector<std::string> outlinedCandidates = {
      "./rayact/resources/fonts/MaterialSymbolsRounded.ttf",
      "./rayact/resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./rayact/resources/fonts/MaterialIcons-Regular.ttf",
      std::string(RAYM3_RESOURCE_DIR) + "/fonts/MaterialSymbolsRounded.ttf",
      std::string(RAYM3_RESOURCE_DIR) + "/MaterialSymbolsRounded.ttf",
      "./resources/fonts/MaterialSymbolsRounded.ttf",
      "./resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./resources/fonts/MaterialIcons-Regular.ttf",
  };
  auto tryPath = [&](const std::string &path) -> const char * {
    if (std::filesystem::exists(path)) {
      s_found = path;
      return s_found.c_str();
    }
    return nullptr;
  };
  if (!g_fontSearchPrefix.empty()) {
    // Must vary by `filled` — this used to always try the Filled font first
    // regardless of the request, making every icon here render filled.
    static const char *kFilledRelPaths[] = {
        "resources/fonts/MaterialSymbolsRounded-Filled.ttf",
        "resources/fonts/MaterialSymbolsRounded.ttf",
        "resources/fonts/MaterialIcons-Regular.ttf",
        nullptr};
    static const char *kOutlinedRelPaths[] = {
        "resources/fonts/MaterialSymbolsRounded.ttf",
        "resources/fonts/MaterialSymbolsRounded-Filled.ttf",
        "resources/fonts/MaterialIcons-Regular.ttf",
        nullptr};
    const char **relPaths = filled ? kFilledRelPaths : kOutlinedRelPaths;
    for (int i = 0; relPaths[i]; ++i) {
      if (auto hit = tryPath(g_fontSearchPrefix + "/" + relPaths[i]))
        return hit;
    }
  }
  auto &candidates = filled ? filledCandidates : outlinedCandidates;
  for (const auto &path : candidates) {
    if (auto hit = tryPath(path))
      return hit;
  }
  return nullptr;
}

// Resolve which loaded variant to use for a request: exact style match first
// ("filled"/"outlined"), then "regular" (single-style custom icon fonts),
// then whatever is registered (last resort, for sets with only one
// unconventionally-named variant).
static const VariantSource *FindVariant(const IconSetState &set, bool filled) {
  const char *style = filled ? "filled" : "outlined";
  auto it = set.variants.find(style);
  if (it != set.variants.end())
    return &it->second;
  it = set.variants.find("regular");
  if (it != set.variants.end())
    return &it->second;
  if (!set.variants.empty())
    return &set.variants.begin()->second;
  return nullptr;
}

static Font GetSetFont(const std::string &setName, IconSetState &set, int sizeDp, bool filled) {
  IconFontKey key{sizeDp, filled};
  auto it = set.fonts.find(key);
  if (it != set.fonts.end())
    return it->second;

  Font font = {0};
  if (!set.codepoints.empty()) {
    std::vector<int> cps(set.codepoints.begin(), set.codepoints.end());
    const VariantSource *variant = FindVariant(set, filled);
    if (variant && variant->HasBytes()) {
      font = LoadFontFromMemory(".ttf", variant->bytes.data(), (int)variant->bytes.size(),
                                PixelSize(sizeDp), cps.data(), (int)cps.size());
    } else if (variant && !variant->path.empty()) {
      font = LoadFontEx(variant->path.c_str(), PixelSize(sizeDp), cps.data(), (int)cps.size());
    } else if (setName == "material") {
      const char *path = FindMaterialFontPathFallback(filled);
      if (path)
        font = LoadFontEx(path, PixelSize(sizeDp), cps.data(), (int)cps.size());
    }
  }
  if (font.texture.id == 0)
    font = GetFontDefault();
  set.fonts[key] = font;
  return font;
}

void LoadIconSet(const std::string &setName,
                 const std::unordered_map<std::string, std::vector<unsigned char>> &variantBytes) {
  IconSetState &set = GetOrCreateSet(setName);
  for (const auto &[style, bytes] : variantBytes) {
    VariantSource src;
    src.bytes = bytes;
    set.variants[style] = std::move(src);
  }
  // Loaded fonts for this set are now stale (variant sources changed).
  for (auto &[key, font] : set.fonts) {
    if (font.texture.id != 0)
      UnloadFont(font);
  }
  set.fonts.clear();
  set.atlasDirty = true;
}

void LoadIconSetFromPaths(const std::string &setName,
                          const std::unordered_map<std::string, std::string> &variantPaths) {
  IconSetState &set = GetOrCreateSet(setName);
  for (const auto &[style, path] : variantPaths) {
    VariantSource src;
    src.path = path;
    set.variants[style] = std::move(src);
  }
  for (auto &[key, font] : set.fonts) {
    if (font.texture.id != 0)
      UnloadFont(font);
  }
  set.fonts.clear();
  set.atlasDirty = true;
}

void RegisterIconNames(const std::string &setName,
                       const std::unordered_map<std::string, int> &nameToCodepoint) {
  IconSetState &set = GetOrCreateSet(setName);
  for (const auto &[name, cp] : nameToCodepoint) {
    set.names[name] = cp;
  }
}

int ResolveIconCodepoint(const std::string &setName, const std::string &name) {
  auto setIt = g_sets.find(setName);
  if (setIt == g_sets.end())
    return 0;
  auto it = setIt->second.names.find(name);
  return it != setIt->second.names.end() ? it->second : 0;
}

void RegisterIcon(int codepoint, int sizeDp, bool filled, const std::string &setName) {
  if (codepoint <= 0 || sizeDp <= 0)
    return;
  IconSetState &set = GetOrCreateSet(setName);
  IconKey key{codepoint, FontManager::SnapSize(sizeDp), filled};
  bool inserted = set.requests.insert(key).second;
  bool cpInserted = set.codepoints.insert(codepoint).second;
  if (cpInserted) {
    for (auto &[fontKey, font] : set.fonts) {
      if (font.texture.id != 0)
        UnloadFont(font);
    }
    set.fonts.clear();
  }
  set.atlasDirty = set.atlasDirty || inserted || cpInserted;
}

static void EnsureAtlas(const std::string &setName, IconSetState &set) {
  if (!set.atlasDirty && set.atlas.id != 0)
    return;
  if (set.requests.empty())
    return;

  set.rects.clear();
  constexpr int kPad = 2;

  struct Entry {
    IconKey key;
    int x;
    int cellPx;
  };

  std::vector<Entry> entries;
  int curX = kPad;
  int maxCellPx = 0;
  for (const auto &key : set.requests) {
    int cellPx = PixelSize(key.sizeDp);
    entries.push_back({key, curX, cellPx});
    curX += cellPx + kPad * 2;
    maxCellPx = std::max(maxCellPx, cellPx);
  }

  int texW = 1;
  int texH = 1;
  int rawW = curX + kPad;
  int rawH = maxCellPx + kPad * 2;
  while (texW < rawW)
    texW <<= 1;
  while (texH < rawH)
    texH <<= 1;

  Image atlasImg = GenImageColor(texW, texH, Color{0, 0, 0, 0});
  ImageFormat(&atlasImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

  for (const auto &entry : entries) {
    Font font = GetSetFont(setName, set, entry.key.sizeDp, entry.key.filled);
    int gidx = GetGlyphIndex(font, entry.key.cp);
    Image glyph = font.glyphs[gidx].image;

    int top = glyph.height;
    int bottom = -1;
    int left = glyph.width;
    int right = -1;
    for (int y = 0; y < glyph.height; ++y) {
      for (int x = 0; x < glyph.width; ++x) {
        Color p = GetImageColor(glyph, x, y);
        if (p.a > 16 || p.r > 16) {
          top = std::min(top, y);
          bottom = std::max(bottom, y);
          left = std::min(left, x);
          right = std::max(right, x);
        }
      }
    }

    if (bottom >= top && right >= left) {
      float inkW = (float)(right - left + 1);
      float inkH = (float)(bottom - top + 1);
      float scale =
          font.baseSize > 0 ? (float)entry.cellPx / (float)font.baseSize : 1.0f;
      float dw = inkW * scale;
      float dh = inkH * scale;
      float fit = std::min(1.0f, std::min((float)entry.cellPx / dw,
                                          (float)entry.cellPx / dh));
      dw *= fit;
      dh *= fit;
      Rectangle src = {(float)left, (float)top, inkW, inkH};
      Rectangle dst = {(float)entry.x + ((float)entry.cellPx - dw) * 0.5f,
                       (float)kPad + ((float)entry.cellPx - dh) * 0.5f, dw,
                       dh};
#if RAYLIB_VERSION_MAJOR >= 6 && RAYLIB_VERSION_MINOR == 0
      ImageDraw(&atlasImg, glyph, src, dst, WHITE);
#else
      ImageDrawImagePro(&atlasImg, glyph, src, dst, (Vector2){0, 0}, 0.0f,
                        WHITE);
#endif
    }
    set.rects[entry.key] = {(float)entry.x, (float)kPad, (float)entry.cellPx,
                            (float)entry.cellPx};
  }

  if (set.atlas.id != 0)
    UnloadTexture(set.atlas);
  set.atlas = LoadTextureFromImage(atlasImg);
  SetTextureFilter(set.atlas, TEXTURE_FILTER_BILINEAR);
  if (std::getenv("RAYM3_ICON_ATLAS_DBG"))
    ExportImage(atlasImg, ("raym3-icon-atlas-" + setName + ".png").c_str());
  UnloadImage(atlasImg);
  set.atlasDirty = false;
}

void DrawIcon(int codepoint, Rectangle bounds, Color color, int sizeDp, bool filled,
             const std::string &setName) {
  if (codepoint <= 0 || bounds.width <= 0.0f || bounds.height <= 0.0f)
    return;
  if (sizeDp <= 0)
    sizeDp = (int)std::round(std::min(bounds.width, bounds.height));
  sizeDp = FontManager::SnapSize(sizeDp);

  RegisterIcon(codepoint, sizeDp, filled, setName);
  IconSetState &set = GetOrCreateSet(setName);
#if defined(__EMSCRIPTEN__)
  Font font = GetSetFont(setName, set, sizeDp, filled);
  float drawSize = (float)sizeDp;
  Vector2 pos = {bounds.x + (bounds.width - drawSize) * 0.5f,
                 bounds.y + (bounds.height - drawSize) * 0.5f};
  DrawTextCodepoint(font, codepoint, pos, drawSize, color);
  return;
#endif
  EnsureAtlas(setName, set);

  IconKey key{codepoint, sizeDp, filled};
  auto it = set.rects.find(key);
  if (set.atlas.id != 0 && it != set.rects.end()) {
    Rectangle src = it->second;
    float drawSize = (float)sizeDp;
    Rectangle dst = {bounds.x + (bounds.width - drawSize) * 0.5f,
                     bounds.y + (bounds.height - drawSize) * 0.5f, drawSize,
                     drawSize};
    DrawTexturePro(set.atlas, src, dst, {0.0f, 0.0f}, 0.0f, color);
  }
}

void RegisterMaterialIcon(int codepoint, int sizeDp, bool filled) {
  RegisterIcon(codepoint, sizeDp, filled, "material");
}

void DrawMaterialIcon(int codepoint, Rectangle bounds, Color color, int sizeDp, bool filled) {
  DrawIcon(codepoint, bounds, color, sizeDp, filled, "material");
}

static void DropIconGpuState(IconSetState &set, bool unloadLiveResources) {
  if (unloadLiveResources && set.atlas.id != 0)
    UnloadTexture(set.atlas);
  set.atlas = {0};
  set.rects.clear();
  for (auto &[key, font] : set.fonts) {
    if (unloadLiveResources && font.texture.id != 0 &&
        font.texture.id != GetFontDefault().texture.id)
      UnloadFont(font);
  }
  set.fonts.clear();
  set.atlasDirty = !set.requests.empty();
}

void ResetIconSetAtlas(const std::string &setName) {
  auto it = g_sets.find(setName);
  if (it == g_sets.end())
    return;
  DropIconGpuState(it->second, true);
}

void ResetAllIconAtlases() {
  for (auto &[name, set] : g_sets) {
    DropIconGpuState(set, true);
  }
}

void IconRendererResetDeviceCache() {
  // The graphics device was re-initialized: every texture id these caches
  // hold belongs to the dead device. Do NOT UnloadTexture — the ids may
  // already be reused by the new device, and freeing them would destroy live
  // textures. Keep registrations/requests so the first frame on the rebuilt
  // device can rebuild the exact icon atlases it had before teardown.
  for (auto &[name, set] : g_sets) {
    DropIconGpuState(set, false);
  }
}

void IconRendererInvalidateLiveDeviceCache() {
  // Fast Android resume keeps the Vulkan device but can still leave font/icon
  // atlas textures in an invalid presentation state. The device is live here,
  // so unload old GPU resources and force deterministic atlas rebuild.
  for (auto &[name, set] : g_sets) {
    DropIconGpuState(set, true);
  }
}

} // namespace raym3::v2
