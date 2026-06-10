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

struct MaterialIconKey {
  int cp;
  int sizeDp;
  bool filled;
  bool operator<(const MaterialIconKey &o) const {
    if (cp != o.cp)
      return cp < o.cp;
    if (sizeDp != o.sizeDp)
      return sizeDp < o.sizeDp;
    return filled < o.filled;
  }
};

struct MaterialFontKey {
  int sizeDp;
  bool filled;
  bool operator<(const MaterialFontKey &o) const {
    return sizeDp != o.sizeDp ? sizeDp < o.sizeDp : filled < o.filled;
  }
};

static std::set<MaterialIconKey> requests_;
static std::set<int> codepoints_;
static std::map<MaterialFontKey, Font> fonts_;
static Texture2D atlas_ = {0};
static std::map<MaterialIconKey, Rectangle> rects_;
static bool atlasDirty_ = false;

static int PixelSize(int sizeDp) {
  return Density::RasterPixels((float)sizeDp);
}

static const char *FindIconFontPath(bool filled) {
  static std::vector<std::string> filledCandidates = {
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
      std::string(RAYM3_RESOURCE_DIR) + "/fonts/MaterialSymbolsRounded.ttf",
      std::string(RAYM3_RESOURCE_DIR) + "/MaterialSymbolsRounded.ttf",
      "./resources/fonts/MaterialSymbolsRounded.ttf",
      "./resources/fonts/MaterialSymbolsRounded-Filled.ttf",
      "./resources/fonts/MaterialIcons-Regular.ttf",
  };
  auto &candidates = filled ? filledCandidates : outlinedCandidates;
  for (const auto &path : candidates) {
    if (std::filesystem::exists(path))
      return path.c_str();
  }
  return nullptr;
}

static Font GetMaterialFont(int sizeDp, bool filled) {
  MaterialFontKey key{sizeDp, filled};
  auto it = fonts_.find(key);
  if (it != fonts_.end())
    return it->second;

  Font font = {0};
  const char *path = FindIconFontPath(filled);
  if (path && !codepoints_.empty()) {
    std::vector<int> cps(codepoints_.begin(), codepoints_.end());
    font = LoadFontEx(path, PixelSize(sizeDp), cps.data(), (int)cps.size());
  }
  if (font.texture.id == 0)
    font = GetFontDefault();
  fonts_[key] = font;
  return font;
}

void RegisterMaterialIcon(int codepoint, int sizeDp, bool filled) {
  if (codepoint <= 0 || sizeDp <= 0)
    return;
  MaterialIconKey key{codepoint, FontManager::SnapSize(sizeDp), filled};
  bool inserted = requests_.insert(key).second;
  bool cpInserted = codepoints_.insert(codepoint).second;
  if (cpInserted) {
    for (auto &[fontKey, font] : fonts_) {
      if (font.texture.id != 0)
        UnloadFont(font);
    }
    fonts_.clear();
  }
  atlasDirty_ = atlasDirty_ || inserted || cpInserted;
}

static void EnsureAtlas() {
  if (!atlasDirty_ && atlas_.id != 0)
    return;
  if (requests_.empty())
    return;

  rects_.clear();
  constexpr int kPad = 2;

  struct Entry {
    MaterialIconKey key;
    int x;
    int cellPx;
  };

  std::vector<Entry> entries;
  int curX = kPad;
  int maxCellPx = 0;
  for (const auto &key : requests_) {
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

  Image atlas = GenImageColor(texW, texH, Color{0, 0, 0, 0});
  ImageFormat(&atlas, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

  for (const auto &entry : entries) {
    Font font = GetMaterialFont(entry.key.sizeDp, entry.key.filled);
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
      ImageDraw(&atlas, glyph, src, dst, WHITE);
    }
    rects_[entry.key] = {(float)entry.x, (float)kPad, (float)entry.cellPx,
                         (float)entry.cellPx};
  }

  if (atlas_.id != 0)
    UnloadTexture(atlas_);
  atlas_ = LoadTextureFromImage(atlas);
  SetTextureFilter(atlas_, TEXTURE_FILTER_BILINEAR);
  if (std::getenv("RAYM3_ICON_ATLAS_DBG"))
    ExportImage(atlas, "raym3-icon-atlas.png");
  UnloadImage(atlas);
  atlasDirty_ = false;
}

void DrawMaterialIcon(int codepoint, Rectangle bounds, Color color, int sizeDp,
                      bool filled) {
  if (codepoint <= 0 || bounds.width <= 0.0f || bounds.height <= 0.0f)
    return;
  if (sizeDp <= 0)
    sizeDp = (int)std::round(std::min(bounds.width, bounds.height));
  sizeDp = FontManager::SnapSize(sizeDp);

  RegisterMaterialIcon(codepoint, sizeDp, filled);
  EnsureAtlas();

  MaterialIconKey key{codepoint, sizeDp, filled};
  auto it = rects_.find(key);
  if (atlas_.id != 0 && it != rects_.end()) {
    Rectangle src = it->second;
    float drawSize = (float)sizeDp;
    Rectangle dst = {bounds.x + (bounds.width - drawSize) * 0.5f,
                     bounds.y + (bounds.height - drawSize) * 0.5f, drawSize,
                     drawSize};
    DrawTexturePro(atlas_, src, dst, {0.0f, 0.0f}, 0.0f, color);
    return;
  }

  char utf8[5] = {0};
  if (codepoint < 0x800) {
    utf8[0] = (char)(0xC0 | (codepoint >> 6));
    utf8[1] = (char)(0x80 | (codepoint & 0x3F));
  } else {
    utf8[0] = (char)(0xE0 | (codepoint >> 12));
    utf8[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    utf8[2] = (char)(0x80 | (codepoint & 0x3F));
  }
  Font font = GetMaterialFont(sizeDp, filled);
  Vector2 measured = MeasureTextEx(font, utf8, (float)sizeDp, 0.0f);
  Vector2 pos = {bounds.x + (bounds.width - measured.x) * 0.5f,
                 bounds.y + (bounds.height - measured.y) * 0.5f};
  DrawTextEx(font, utf8, pos, (float)sizeDp, 0.0f, color);
}

void ResetMaterialIconAtlas() {
  if (atlas_.id != 0)
    UnloadTexture(atlas_);
  atlas_ = {0};
  rects_.clear();
  for (auto &[key, font] : fonts_) {
    if (font.texture.id != 0)
      UnloadFont(font);
  }
  fonts_.clear();
  requests_.clear();
  codepoints_.clear();
  atlasDirty_ = false;
}

} // namespace raym3::v2
