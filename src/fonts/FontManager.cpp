#include "raym3/fonts/FontManager.h"
#include "raym3/config.h"
#include "raym3/v2/Density.h"
#include "EmbeddedFonts.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace raym3 {

std::unordered_map<FontKey, Font, FontKeyHash> FontManager::fontCache_;
std::unordered_map<std::string, FontManager::FontSource> FontManager::fontRegistry_;
std::unordered_map<std::string, Font>           FontManager::customFontCache_;
Font FontManager::defaultFont_ = {0};
bool FontManager::initialized_ = false;
float FontManager::dpiScale_ = 1.0f;

void FontManager::SetDpiScale(float scale) {
  v2::Density::SetLayoutDensity(scale);
  dpiScale_ = v2::Density::GetLayoutDensity();
}
float FontManager::GetDpiScale() {
  dpiScale_ = v2::Density::GetLayoutDensity();
  return dpiScale_;
}

// Quantization buckets up to 72 dp — covers every tailwind font-size exactly.
// Sizes above 72 returned as-is (display/hero text; rarely repeated).
// The font *texture* is loaded at SnapSize(size) * GetDpiScale() so the bucket
// matches the layout's dp unit while the glyphs are at pixel resolution.
int FontManager::SnapSize(int size) {
  if (size > 72) return size;
  static const int kBuckets[] = {
      8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28,
      32, 36, 40, 48, 56, 64, 72
  };
  int best = kBuckets[0];
  int bestDist = std::abs(size - best);
  for (int b : kBuckets) {
    int d = std::abs(size - b);
    if (d < bestDist) { bestDist = d; best = b; }
  }
  return best;
}

void FontManager::Initialize() {
  if (initialized_) return;
  defaultFont_ = LoadFont(FontWeight::Regular, FontStyle::Normal, 16);
  initialized_ = true;
}

void FontManager::Shutdown() {
  for (auto &[key, font] : fontCache_)
    if (font.texture.id != 0) UnloadFont(font);
  fontCache_.clear();
  defaultFont_ = {0};

  for (auto &[key, font] : customFontCache_)
    if (font.texture.id != 0) UnloadFont(font);
  customFontCache_.clear();
  fontRegistry_.clear();

  initialized_ = false;
}

void FontManager::ResetDeviceCache() {
  fontCache_.clear();
  customFontCache_.clear();
  defaultFont_ = {0};
  initialized_ = false;
}

Font FontManager::LoadFont(FontWeight weight, FontStyle style, int size) {
  size = SnapSize(size);
  FontKey key{weight, style, size};
  auto it = fontCache_.find(key);
  if (it != fontCache_.end()) return it->second;
  Font font = LoadRobotoFont(weight, style, size);
  if (font.texture.id != 0) fontCache_[key] = font;
  return font;
}

Font FontManager::LoadCustomFont(const std::string &path, int size) {
  std::string resolvedPath = path;

  if (!std::filesystem::path(path).is_absolute()) {
    std::vector<std::string> searchPaths = {
      std::string(RAYM3_RESOURCE_DIR) + "/fonts/" + path,
      std::string(RAYM3_RESOURCE_DIR) + "/" + path,
      "./resources/fonts/" + path,
      "./raym3/resources/fonts/" + path,
      path
    };
    for (const auto& testPath : searchPaths) {
      if (std::filesystem::exists(testPath)) { resolvedPath = testPath; break; }
    }
  }

  if (!std::filesystem::exists(resolvedPath)) return {0};
  const int pxSize = v2::Density::RasterPixels((float)size);
  return LoadFontEx(resolvedPath.c_str(), pxSize, nullptr, 0);
}

void FontManager::InvalidateCustomFontCache(const std::string &name) {
  for (auto it = customFontCache_.begin(); it != customFontCache_.end(); ) {
    if (it->first.rfind(name + ":", 0) == 0) {
      if (it->second.texture.id != 0) UnloadFont(it->second);
      it = customFontCache_.erase(it);
    } else {
      ++it;
    }
  }
}

void FontManager::RegisterFont(const std::string &name, const std::string &path) {
  FontSource src;
  src.path = path;
  src.isMemory = false;
  fontRegistry_[name] = std::move(src);
  InvalidateCustomFontCache(name);
}

void FontManager::RegisterFontFromMemory(const std::string &name, std::vector<unsigned char> bytes) {
  FontSource src;
  src.bytes = std::move(bytes);
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

Font FontManager::LoadCustomFontFromMemory(const std::vector<unsigned char> &bytes, int size) {
  if (bytes.empty()) return {0};
  const int pxSize = v2::Density::RasterPixels((float)size);
  return LoadFontFromMemory(".ttf", bytes.data(), (int)bytes.size(), pxSize, nullptr, 0);
}

Font FontManager::LoadFontByFamily(const std::string &name, int size) {
  size = SnapSize(size);
  std::string cacheKey = name + ":" + std::to_string(size);
  auto it = customFontCache_.find(cacheKey);
  if (it != customFontCache_.end()) return it->second;

  auto reg = fontRegistry_.find(name);
  if (reg == fontRegistry_.end()) {
    // Unknown family — fall back to Roboto regular
    return LoadFont(FontWeight::Regular, FontStyle::Normal, size);
  }

  Font font = reg->second.isMemory ? LoadCustomFontFromMemory(reg->second.bytes, size)
                                   : LoadCustomFont(reg->second.path, size);
  if (font.texture.id == 0) {
    fprintf(stderr, "FontManager: failed to load font '%s' (%s)\n", name.c_str(),
            reg->second.isMemory ? "from memory" : reg->second.path.c_str());
    font = LoadFont(FontWeight::Regular, FontStyle::Normal, size);
  }
  customFontCache_[cacheKey] = font;
  return font;
}

void FontManager::UnloadFont(Font font) {
  if (font.texture.id != 0) ::UnloadFont(font);
}

// Roboto ships ASCII (32-126) + Latin Extended (160-591) only.
// Loading symbol ranges beyond that produces Raylib "glyph not found" warnings
// and wastes atlas space — skip them. Icons use a separate font (Material Icons).
static const std::vector<int>& GetCodepoints() {
  static std::vector<int> cps;
  if (!cps.empty()) return cps;
  auto addRange = [&](int from, int to) {
    for (int i = from; i <= to; i++) cps.push_back(i);
  };
  addRange(32,  126);   // Basic ASCII
#if !defined(__EMSCRIPTEN__)
  addRange(160, 591);   // Latin-1 Supplement + Latin Extended A/B
#endif
  return cps;
}

Font FontManager::LoadRobotoFont(FontWeight weight, FontStyle style, int size) {
  unsigned char *fontData = nullptr;
  unsigned int fontDataLen = 0;

  if (weight == FontWeight::Bold || weight == FontWeight::Black) {
    fontData = Roboto_v3_012_hinted_static_Roboto_Bold_ttf;
    fontDataLen = Roboto_v3_012_hinted_static_Roboto_Bold_ttf_len;
  } else {
    fontData = Roboto_v3_012_hinted_static_Roboto_Regular_ttf;
    fontDataLen = Roboto_v3_012_hinted_static_Roboto_Regular_ttf_len;
  }

  // `size` is the *device-independent* (dp) request. The font texture needs
  // to be at the actual pixel size, so we multiply by the DPI scale. This
  // way, when the host applies its dp-scaling matrix (rlScalef(dp, dp, 1)),
  // the font texture is sampled at 1:1 instead of being upscaled ~3.5x,
  // which is what makes Android text look blurry without this fix.
  const int pxSize = v2::Density::RasterPixels((float)size);

  const auto& cps = GetCodepoints();
  Font font = LoadFontFromMemory(".ttf", fontData, fontDataLen, pxSize,
                                 const_cast<int*>(cps.data()), (int)cps.size());
  return font;
}

} // namespace raym3
