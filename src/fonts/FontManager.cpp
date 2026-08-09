#include "raym3/fonts/FontManager.h"
#include "raym3/fonts/SystemUiFont.h"
#include "raym3/config.h"
#include "raym3/v2/Density.h"

#include <rlgl.h>

#if defined(__EMSCRIPTEN__)
#include "EmbeddedFonts.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <set>

namespace raym3 {

std::unordered_map<FontKey, FontManager::CachedFont, FontKeyHash>
    FontManager::fontCache_;
std::unordered_map<std::string, FontManager::FontSource>
    FontManager::fontRegistry_;
std::unordered_map<std::string, FontManager::CachedFont>
    FontManager::customFontCache_;
Font FontManager::defaultFont_ = {0};
bool FontManager::initialized_ = false;
float FontManager::dpiScale_ = 1.0f;
std::uint64_t FontManager::fontGeneration_ = 1;

std::vector<int> FontManager::AsciiSeed() {
  std::vector<int> cps;
  cps.reserve(95);
  for (int i = 32; i <= 126; ++i) cps.push_back(i);
  return cps;
}

std::vector<int>
FontManager::SortedCodepoints(const std::unordered_set<int> &set) {
  std::vector<int> cps(set.begin(), set.end());
  std::sort(cps.begin(), cps.end());
  return cps;
}

bool FontManager::UnionCodepointsFromUtf8(std::unordered_set<int> &set,
                                          std::string_view utf8) {
  bool grew = false;
  for (std::size_t i = 0; i < utf8.size();) {
    const unsigned char c = static_cast<unsigned char>(utf8[i]);
    std::uint32_t cp = 0;
    std::size_t extra = 0;
    if (c < 0x80) {
      cp = c;
      extra = 0;
    } else if (c < 0xC0) {
      ++i;
      continue;
    } else if (c < 0xE0) {
      cp = c & 0x1F;
      extra = 1;
    } else if (c < 0xF0) {
      cp = c & 0x0F;
      extra = 2;
    } else {
      cp = c & 0x07;
      extra = 3;
    }
    ++i;
    for (std::size_t k = 0; k < extra && i < utf8.size(); ++k, ++i)
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i]) & 0x3F);
    if (cp == 0 || cp > 0x10FFFF) continue;
    if (set.insert(static_cast<int>(cp)).second) grew = true;
  }
  return grew;
}

void FontManager::SetDpiScale(float scale) {
  v2::Density::SetLayoutDensity(scale);
  const float newScale = v2::Density::GetLayoutDensity();
  if (std::fabs(newScale - dpiScale_) > 1e-3f) {
    InvalidateLiveDeviceCache();
  }
  dpiScale_ = newScale;
}

float FontManager::GetDpiScale() {
  dpiScale_ = v2::Density::GetLayoutDensity();
  return dpiScale_;
}

std::uint64_t FontManager::FontGeneration() { return fontGeneration_; }

int FontManager::SnapSize(int size) { return std::max(1, size); }

void FontManager::Initialize() {
  if (initialized_) return;
  defaultFont_ = LoadFont(FontWeight::Regular, FontStyle::Normal, 16);
  initialized_ = true;
}

void FontManager::Shutdown() {
  for (auto &[key, entry] : fontCache_)
    if (entry.font.texture.id != 0) UnloadFont(entry.font);
  fontCache_.clear();
  defaultFont_ = {0};

  for (auto &[key, entry] : customFontCache_)
    if (entry.font.texture.id != 0) UnloadFont(entry.font);
  customFontCache_.clear();
  fontRegistry_.clear();

  initialized_ = false;
}

void FontManager::ResetDeviceCache() {
  fontCache_.clear();
  customFontCache_.clear();
  defaultFont_ = {0};
  initialized_ = false;
  ++fontGeneration_;
}

void FontManager::InvalidateLiveDeviceCache() {
  std::set<unsigned int> unloaded;
  auto unloadOnce = [&](Font font) {
    if (font.texture.id != 0 && unloaded.insert(font.texture.id).second)
      UnloadFont(font);
  };
  for (auto &[key, entry] : fontCache_) unloadOnce(entry.font);
  for (auto &[key, entry] : customFontCache_) unloadOnce(entry.font);
  fontCache_.clear();
  customFontCache_.clear();
  defaultFont_ = {0};
  initialized_ = false;
  ++fontGeneration_;
}

namespace {

// raylib bakes with stbtt_ScaleForPixelHeight(), i.e. it fits the font's
// ascent−descent band into the pixel size it is given. CSS, React Native and
// Lynx all define font-size as the **em size**, and that band is ~1.17–1.20 em
// on the faces we ship — so asking raylib for 14 produced an em of ~11.9 and
// every default label rendered ~15% smaller than the same number does on the
// web or in React Native.
//
// Fix at the only choke point that covers every caller: bake the atlas at
// `size * emRatio` so the em lands exactly on `size`, then report a baseSize of
// `size` px. Draw and measure calls keep passing CSS sizes and raylib's
// `fontSize / baseSize` scale factor resolves to 1:1 texels, so nothing
// downstream (components, TextEngine, the pixel-space text path in Renderer)
// has to know. Icon faces are loaded by IconRenderer, not here, and keep their
// own dp sizing.
int EmBakePixels(int cssSize, const FontVMetrics &metrics) {
  const float ratio = metrics.emRatio > 0.0f ? metrics.emRatio : 1.0f;
  return v2::Density::RasterPixels(static_cast<float>(cssSize) * ratio);
}

void RebaseToEm(Font &font, int cssSize) {
  if (font.texture.id == 0) return;
  font.baseSize = std::max(1, v2::Density::RasterPixels(static_cast<float>(cssSize)));
}

// Free a font atlas that is being replaced mid-frame.
//
// rlgl batches glyph quads and only flushes when the bound texture changes, so
// two Text nodes sharing a face accumulate into ONE batch with no flush between
// them. Re-baking that face (a later node used a codepoint the atlas lacked —
// "·" is the usual culprit, since the seed is ASCII) would otherwise free the
// texture the already-batched quads still point at: the next draw binds the new
// atlas, that bind flushes the pending batch, and the earlier node's glyphs
// sample freed memory. Vulkan renders whatever is there (random glyph soup),
// Metal reads opaque white so each glyph becomes a solid block in the text
// colour. Because rendering is on demand, that single bad frame then stays on
// screen until something else forces a repaint, which is why it reads as
// permanent corruption rather than a flicker.
//
// Submitting the queued geometry first means those quads are drawn while the
// old atlas is still alive; only then is it safe to unload.
void RetireFont(const Font &font) {
  rlDrawRenderBatchActive();
  UnloadFont(font);
}

} // namespace

Font FontManager::LoadDefaultUiFont(FontWeight weight, FontStyle style,
                                    int size,
                                    const std::vector<int> &codepoints) {
  const FontVMetrics metrics = MetricsFor(weight, style);
  const int pxSize = EmBakePixels(size, metrics);

#if defined(__EMSCRIPTEN__)
  // Web has no system UI font file API — ship embedded Roboto only here.
  (void)style;
  unsigned char *fontData = nullptr;
  unsigned int fontDataLen = 0;
  if (weight == FontWeight::Bold || weight == FontWeight::Black) {
    fontData = Roboto_v3_012_hinted_static_Roboto_Bold_ttf;
    fontDataLen = Roboto_v3_012_hinted_static_Roboto_Bold_ttf_len;
  } else {
    fontData = Roboto_v3_012_hinted_static_Roboto_Regular_ttf;
    fontDataLen = Roboto_v3_012_hinted_static_Roboto_Regular_ttf_len;
  }
  Font font =
      LoadFontFromMemory(".ttf", fontData, static_cast<int>(fontDataLen),
                         pxSize, const_cast<int *>(codepoints.data()),
                         static_cast<int>(codepoints.size()));
  RebaseToEm(font, size);
  return font;
#else
  // Native hosts use the platform UI face only — no embedded Roboto fallback.
  std::string path;
  if (!ResolveSystemUiFontPath(weight, style, path)) {
    TraceLog(LOG_WARNING,
             "FontManager: no system UI font for weight=%d style=%d",
             static_cast<int>(weight), static_cast<int>(style));
    return {0};
  }
  Font font =
      LoadFontEx(path.c_str(), pxSize, const_cast<int *>(codepoints.data()),
                 static_cast<int>(codepoints.size()));
  if (font.texture.id == 0) {
    TraceLog(LOG_WARNING, "FontManager: failed to load system UI font '%s'",
             path.c_str());
  }
  RebaseToEm(font, size);
  return font;
#endif
}

FontVMetrics FontManager::MetricsFor(FontWeight weight, FontStyle style) {
#if defined(__EMSCRIPTEN__)
  (void)style;
  const bool bold = weight == FontWeight::Bold || weight == FontWeight::Black;
  const unsigned char *data =
      bold ? Roboto_v3_012_hinted_static_Roboto_Bold_ttf
           : Roboto_v3_012_hinted_static_Roboto_Regular_ttf;
  const unsigned int len = bold ? Roboto_v3_012_hinted_static_Roboto_Bold_ttf_len
                                : Roboto_v3_012_hinted_static_Roboto_Regular_ttf_len;
  return ReadFontVMetrics(data, len);
#else
  std::string path;
  if (!ResolveSystemUiFontPath(weight, style, path)) return {};
  return ReadFontVMetricsFromFile(path);
#endif
}

FontVMetrics FontManager::MetricsForFamily(const std::string &name) {
  auto reg = fontRegistry_.find(name);
  if (reg == fontRegistry_.end())
    return MetricsFor(FontWeight::Regular, FontStyle::Normal);
  if (reg->second.isMemory)
    return ReadFontVMetrics(reg->second.bytes.data(), reg->second.bytes.size());
  return ReadFontVMetricsFromFile(ResolveCustomFontPath(reg->second.path));
}

Font FontManager::BakeFont(FontWeight weight, FontStyle style, int size,
                           const std::vector<int> &codepoints) {
  return LoadDefaultUiFont(weight, style, size, codepoints);
}

Font FontManager::LoadFont(FontWeight weight, FontStyle style, int size) {
  size = SnapSize(size);
  FontKey key{weight, style, size};
  auto it = fontCache_.find(key);
  if (it != fontCache_.end()) return it->second.font;

  CachedFont entry;
  const auto seed = AsciiSeed();
  entry.codepoints.insert(seed.begin(), seed.end());
  entry.font = BakeFont(weight, style, size, seed);
  if (entry.font.texture.id != 0) {
    fontCache_[key] = std::move(entry);
    return fontCache_[key].font;
  }
  return {0};
}

void FontManager::EnsureGlyphsForText(FontWeight weight, FontStyle style,
                                      int size, std::string_view utf8) {
  size = SnapSize(size);
  FontKey key{weight, style, size};
  auto it = fontCache_.find(key);
  if (it == fontCache_.end()) {
    LoadFont(weight, style, size);
    it = fontCache_.find(key);
    if (it == fontCache_.end()) return;
  }
  if (!UnionCodepointsFromUtf8(it->second.codepoints, utf8)) return;

  const auto cps = SortedCodepoints(it->second.codepoints);
  Font rebuilt = BakeFont(weight, style, size, cps);
  if (rebuilt.texture.id == 0) return;
  if (it->second.font.texture.id != 0) RetireFont(it->second.font);
  it->second.font = rebuilt;
  ++fontGeneration_;
  if (size == 16 && weight == FontWeight::Regular &&
      style == FontStyle::Normal)
    defaultFont_ = rebuilt;
}

std::string FontManager::ResolveCustomFontPath(const std::string &path) {
  if (path.empty() || std::filesystem::path(path).is_absolute()) return path;
  const std::vector<std::string> searchPaths = {
      std::string(RAYM3_RESOURCE_DIR) + "/fonts/" + path,
      std::string(RAYM3_RESOURCE_DIR) + "/" + path,
      "./resources/fonts/" + path,
      "./raym3/resources/fonts/" + path,
      path};
  for (const auto &testPath : searchPaths)
    if (std::filesystem::exists(testPath)) return testPath;
  return path;
}

Font FontManager::LoadCustomFont(const std::string &path, int size,
                                 const std::vector<int> &codepoints) {
  const std::string resolvedPath = ResolveCustomFontPath(path);

  if (!std::filesystem::exists(resolvedPath)) return {0};
  const int pxSize = EmBakePixels(size, ReadFontVMetricsFromFile(resolvedPath));
  const std::vector<int> &cps =
      codepoints.empty() ? AsciiSeed() : codepoints;
  Font font = LoadFontEx(resolvedPath.c_str(), pxSize,
                         const_cast<int *>(cps.data()),
                         static_cast<int>(cps.size()));
  RebaseToEm(font, size);
  return font;
}

void FontManager::InvalidateCustomFontCache(const std::string &name) {
  for (auto it = customFontCache_.begin(); it != customFontCache_.end();) {
    if (it->first.rfind(name + ":", 0) == 0) {
      if (it->second.font.texture.id != 0) UnloadFont(it->second.font);
      it = customFontCache_.erase(it);
    } else {
      ++it;
    }
  }
  ++fontGeneration_;
}

void FontManager::RegisterFont(const std::string &name, const std::string &path,
                               std::vector<int> codepoints) {
  FontSource src;
  src.path = path;
  src.codepoints = std::move(codepoints);
  src.isMemory = false;
  fontRegistry_[name] = std::move(src);
  InvalidateCustomFontCache(name);
}

void FontManager::RegisterFontFromMemory(const std::string &name,
                                         std::vector<unsigned char> bytes,
                                         std::vector<int> codepoints) {
  FontSource src;
  src.bytes = std::move(bytes);
  src.codepoints = std::move(codepoints);
  src.isMemory = true;
  fontRegistry_[name] = std::move(src);
  InvalidateCustomFontCache(name);
}

bool FontManager::HasFont(const std::string &name) {
  return fontRegistry_.find(name) != fontRegistry_.end();
}

std::vector<std::string> FontManager::ListRegisteredFonts() {
  std::vector<std::string> names;
  names.reserve(fontRegistry_.size());
  for (const auto &[name, src] : fontRegistry_) names.push_back(name);
  return names;
}

Font FontManager::LoadCustomFontFromMemory(
    const std::vector<unsigned char> &bytes, int size,
    const std::vector<int> &codepoints) {
  if (bytes.empty()) return {0};
  const int pxSize =
      EmBakePixels(size, ReadFontVMetrics(bytes.data(), bytes.size()));
  const std::vector<int> &cps =
      codepoints.empty() ? AsciiSeed() : codepoints;
  Font font =
      LoadFontFromMemory(".ttf", bytes.data(), static_cast<int>(bytes.size()),
                         pxSize, const_cast<int *>(cps.data()),
                         static_cast<int>(cps.size()));
  RebaseToEm(font, size);
  return font;
}

Font FontManager::LoadFontByFamily(const std::string &name, int size) {
  size = SnapSize(size);
  std::string cacheKey = name + ":" + std::to_string(size);
  auto it = customFontCache_.find(cacheKey);
  if (it != customFontCache_.end()) return it->second.font;

  auto reg = fontRegistry_.find(name);
  if (reg == fontRegistry_.end()) {
    return LoadFont(FontWeight::Regular, FontStyle::Normal, size);
  }

  CachedFont entry;
  std::vector<int> seed = reg->second.codepoints.empty()
                              ? AsciiSeed()
                              : reg->second.codepoints;
  entry.codepoints.insert(seed.begin(), seed.end());
  entry.font = reg->second.isMemory
                   ? LoadCustomFontFromMemory(reg->second.bytes, size, seed)
                   : LoadCustomFont(reg->second.path, size, seed);
  if (entry.font.texture.id == 0) {
    fprintf(stderr, "FontManager: failed to load font '%s' (%s)\n", name.c_str(),
            reg->second.isMemory ? "from memory" : reg->second.path.c_str());
    return LoadFont(FontWeight::Regular, FontStyle::Normal, size);
  }
  customFontCache_[cacheKey] = std::move(entry);
  return customFontCache_[cacheKey].font;
}

void FontManager::EnsureGlyphsForFamily(std::string_view name, int size,
                                        std::string_view utf8) {
  size = SnapSize(size);
  std::string cacheKey = std::string(name) + ":" + std::to_string(size);
  auto it = customFontCache_.find(cacheKey);
  if (it == customFontCache_.end()) {
    LoadFontByFamily(std::string(name), size);
    it = customFontCache_.find(cacheKey);
    if (it == customFontCache_.end()) return;
  }
  if (!UnionCodepointsFromUtf8(it->second.codepoints, utf8)) return;

  auto reg = fontRegistry_.find(std::string(name));
  if (reg == fontRegistry_.end()) return;

  const auto cps = SortedCodepoints(it->second.codepoints);
  Font rebuilt =
      reg->second.isMemory
          ? LoadCustomFontFromMemory(reg->second.bytes, size, cps)
          : LoadCustomFont(reg->second.path, size, cps);
  if (rebuilt.texture.id == 0) return;
  // Same mid-frame hazard as EnsureGlyphsForText; see RetireFont.
  if (it->second.font.texture.id != 0) RetireFont(it->second.font);
  it->second.font = rebuilt;
  ++fontGeneration_;
}

void FontManager::UnloadFont(Font font) {
  if (font.texture.id != 0) ::UnloadFont(font);
}

} // namespace raym3
