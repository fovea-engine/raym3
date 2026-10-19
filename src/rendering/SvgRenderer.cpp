#include "raym3/rendering/SvgRenderer.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/rendering/SvgModel.h"
#include "raym3/config.h"
#include "raym3/styles/Theme.h"
#include "rlgl.h"
#include "raym3/v2/EmojiFont.h"
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>
#include <cstring>

#if RAYM3_EMBED_RESOURCES
#include "EmbeddedResources.h"
#endif

#define NANOSVGRAST_IMPLEMENTATION
#include "external/nanosvgrast.h"

namespace raym3 {

std::string SvgRenderer::basePath = "";
std::unordered_map<std::string, Texture2D> SvgRenderer::textureCache;
std::unordered_map<std::string, SvgRenderer::DocumentTexture>
    SvgRenderer::documentCache;
std::uint64_t SvgRenderer::documentUseClock = 0;
bool SvgRenderer::autoDetected = false;

namespace {

std::string Attribute(std::string_view source, std::string_view name) {
  size_t pos = 0;
  while ((pos = source.find(name, pos)) != std::string_view::npos) {
    const bool leftBoundary = pos == 0 || std::isspace((unsigned char)source[pos - 1]);
    size_t endName = pos + name.size();
    while (endName < source.size() && std::isspace((unsigned char)source[endName]))
      ++endName;
    if (!leftBoundary || endName >= source.size() || source[endName] != '=') {
      pos += name.size();
      continue;
    }
    size_t value = endName + 1;
    while (value < source.size() && std::isspace((unsigned char)source[value]))
      ++value;
    if (value >= source.size()) return {};
    const char quote = source[value];
    if (quote == '\'' || quote == '"') {
      const size_t close = source.find(quote, value + 1);
      if (close == std::string_view::npos) return {};
      return std::string(source.substr(value + 1, close - value - 1));
    }
    const size_t close = source.find_first_of(" \t\r\n>", value);
    return std::string(source.substr(value, close == std::string_view::npos
                                                 ? source.size() - value
                                                 : close - value));
  }
  return {};
}

std::string SvgTextProperty(std::string_view tag, std::string_view name) {
  std::string value = Attribute(tag, name);
  if (!value.empty()) return value;
  const std::string style = Attribute(tag, "style");
  size_t pos = 0;
  while (pos < style.size()) {
    const size_t end = style.find(';', pos);
    const std::string_view declaration(style.data() + pos,
        (end == std::string::npos ? style.size() : end) - pos);
    const size_t colon = declaration.find(':');
    if (colon != std::string_view::npos) {
      auto key = declaration.substr(0, colon);
      auto val = declaration.substr(colon + 1);
      while (!key.empty() && std::isspace((unsigned char)key.front())) key.remove_prefix(1);
      while (!key.empty() && std::isspace((unsigned char)key.back())) key.remove_suffix(1);
      while (!val.empty() && std::isspace((unsigned char)val.front())) val.remove_prefix(1);
      while (!val.empty() && std::isspace((unsigned char)val.back())) val.remove_suffix(1);
      if (key == name) return std::string(val);
    }
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return {};
}

float SvgNumber(std::string_view text, float fallback) {
  if (text.empty()) return fallback;
  const std::string input(text);
  char *end = nullptr;
  const float value = std::strtof(input.c_str(), &end);
  return end == input.c_str() ? fallback : value;
}

std::string DecodeXmlText(std::string_view text) {
  std::string decoded(text);
  const std::pair<const char *, const char *> entities[] = {
      {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
      {"&quot;", "\""}, {"&apos;", "'"}};
  for (const auto &[encoded, plain] : entities) {
    size_t at = 0;
    while ((at = decoded.find(encoded, at)) != std::string::npos) {
      decoded.replace(at, std::strlen(encoded), plain);
      at += std::strlen(plain);
    }
  }
  return decoded;
}

Color SvgColor(std::string value) {
  if (!value.empty() && value[0] == '#') {
    value.erase(value.begin());
    if (value.size() == 3) {
      std::string expanded;
      for (char ch : value) { expanded += ch; expanded += ch; }
      value = expanded;
    }
    if (value.size() == 6 || value.size() == 8) {
      char *end = nullptr;
      const unsigned long rgba = std::strtoul(value.c_str(), &end, 16);
      if (end && *end == '\0') {
        if (value.size() == 6)
          return Color{(unsigned char)(rgba >> 16), (unsigned char)(rgba >> 8),
                       (unsigned char)rgba, 255};
        return Color{(unsigned char)(rgba >> 24), (unsigned char)(rgba >> 16),
                     (unsigned char)(rgba >> 8), (unsigned char)rgba};
      }
    }
  }
  if (value == "white") return WHITE;
  if (value == "red") return RED;
  if (value == "green") return GREEN;
  if (value == "blue") return BLUE;
  if (value == "transparent") return BLANK;
  return BLACK;
}

FontWeight SvgFontWeight(std::string value) {
  if (value == "bold" || value == "bolder") return FontWeight::Bold;
  const int weight = std::atoi(value.c_str());
  if (weight >= 700) return FontWeight::Bold;
  if (weight >= 500) return FontWeight::Medium;
  return FontWeight::Regular;
}

struct SvgTextRun {
  std::string text;
  float x = 0;
  float y = 0;
  float fontSize = 16;
  float letterSpacing = 0;
  float opacity = 1;
  Color fill = BLACK;
  FontWeight weight = FontWeight::Regular;
  std::string anchor;
  std::string baseline;
};

std::vector<SvgTextRun> SvgTextRuns(std::string_view source) {
  std::vector<SvgTextRun> runs;
  size_t pos = 0;
  while ((pos = source.find("<text", pos)) != std::string_view::npos) {
    const size_t nameEnd = pos + 5;
    if (nameEnd < source.size() && !std::isspace((unsigned char)source[nameEnd])
        && source[nameEnd] != '>') {
      pos = nameEnd;
      continue;
    }
    const size_t tagEnd = source.find('>', nameEnd);
    const size_t close = tagEnd == std::string_view::npos
        ? std::string_view::npos : source.find("</text>", tagEnd + 1);
    if (tagEnd == std::string_view::npos || close == std::string_view::npos) break;
    const std::string_view tag = source.substr(pos, tagEnd - pos + 1);
    std::string content(source.substr(tagEnd + 1, close - tagEnd - 1));
    // Inline <tspan> markup is recognized as text content. Position/style
    // overrides on individual spans remain outside this minimal text-run path.
    for (size_t at = 0; (at = content.find('<', at)) != std::string::npos;) {
      const size_t end = content.find('>', at);
      if (end == std::string::npos) break;
      content.erase(at, end - at + 1);
    }
    SvgTextRun run;
    run.text = DecodeXmlText(content);
    run.x = SvgNumber(SvgTextProperty(tag, "x"), 0);
    run.y = SvgNumber(SvgTextProperty(tag, "y"), 0);
    run.fontSize = SvgNumber(SvgTextProperty(tag, "font-size"), 16);
    run.letterSpacing = SvgNumber(SvgTextProperty(tag, "letter-spacing"), 0);
    run.opacity = std::clamp(SvgNumber(SvgTextProperty(tag, "opacity"), 1), 0.0f, 1.0f)
        * std::clamp(SvgNumber(SvgTextProperty(tag, "fill-opacity"), 1), 0.0f, 1.0f);
    const std::string fill = SvgTextProperty(tag, "fill");
    if (!fill.empty()) run.fill = SvgColor(fill);
    run.weight = SvgFontWeight(SvgTextProperty(tag, "font-weight"));
    run.anchor = SvgTextProperty(tag, "text-anchor");
    run.baseline = SvgTextProperty(tag, "dominant-baseline");
    if (!run.text.empty()) runs.push_back(std::move(run));
    pos = close + 7;
  }
  return runs;
}

void DrawSvgText(std::string_view source, Rectangle bounds, unsigned char opacity) {
  const auto textRuns = SvgTextRuns(source);
  const size_t root = source.find("<svg");
  const size_t rootEnd = root == std::string_view::npos
      ? std::string_view::npos : source.find('>', root);
  const std::string_view rootTag = rootEnd == std::string_view::npos
      ? std::string_view{} : source.substr(root, rootEnd - root + 1);
  float minX = 0, minY = 0, viewWidth = bounds.width, viewHeight = bounds.height;
  const std::string viewBox = Attribute(rootTag, "viewBox");
  if (!viewBox.empty()) {
    if (std::sscanf(viewBox.c_str(), "%f%*[, ]%f%*[, ]%f%*[, ]%f",
                    &minX, &minY, &viewWidth, &viewHeight) != 4)
      return;
  } else {
    viewWidth = SvgNumber(Attribute(rootTag, "width"), bounds.width);
    viewHeight = SvgNumber(Attribute(rootTag, "height"), bounds.height);
  }
  if (viewWidth <= 0 || viewHeight <= 0) return;
  const float scale = std::min(bounds.width / viewWidth, bounds.height / viewHeight);
  const float offsetX = bounds.x + (bounds.width - viewWidth * scale) * 0.5f;
  const float offsetY = bounds.y + (bounds.height - viewHeight * scale) * 0.5f;
  for (const SvgTextRun &run : textRuns) {
    const float fontSize = run.fontSize * scale;
    FontManager::EnsureGlyphsForText(run.weight, FontStyle::Normal,
        std::max(1, (int)std::ceil(fontSize)), run.text);
    Font font = Theme::GetFont(fontSize, run.weight);
    const float spacing = run.letterSpacing * scale;
    const Vector2 measured = v2::MeasureTextWithEmoji(font, run.text, fontSize, spacing);
    float x = offsetX + (run.x - minX) * scale;
    if (run.anchor == "middle") x -= measured.x * 0.5f;
    else if (run.anchor == "end") x -= measured.x;
    float y = offsetY + (run.y - minY) * scale;
    if (run.baseline == "middle" || run.baseline == "central") y -= measured.y * 0.5f;
    else if (run.baseline == "hanging") y += fontSize * 0.15f;
    else y -= fontSize * 0.8f; // SVG's default alphabetic baseline.
    Color color = run.fill;
    color.a = (unsigned char)std::round(color.a * run.opacity * opacity / 255.0f);
    v2::DrawTextWithEmoji(font, run.text, {x, y}, fontSize, spacing, color);
  }
}

} // namespace

static std::string DetectIconPath() {
#ifdef __EMSCRIPTEN__
  // On WASM with embedded resources, we don't need filesystem paths
  return "";
#else
  std::vector<std::string> searchPaths = {
    std::string(RAYM3_RESOURCE_DIR) + "/icons",
    std::string(RAYM3_RESOURCE_DIR),
    "./resources/icons",
    "./raym3/resources/icons",
    "../raym3/resources/icons",
    "../../raym3/resources/icons"
  };

  for (const auto& path : searchPaths) {
    std::filesystem::path testPath(path);
    std::filesystem::path filledPath = testPath / "filled";
    if (std::filesystem::exists(filledPath) && std::filesystem::is_directory(filledPath)) {
      std::string result = path;
      if (!result.empty() && result.back() != '/') {
        result += '/';
      }
      return result;
    }
  }

  return "";
#endif
}

void SvgRenderer::Initialize(const char *path) {
  if (path && strlen(path) > 0) {
    basePath = path;
    if (!basePath.empty() && basePath.back() != '/') {
      basePath += '/';
    }
    autoDetected = false;
  } else if (!autoDetected) {
    basePath = DetectIconPath();
    autoDetected = true;
  }
}

void SvgRenderer::Shutdown() {
  for (auto &pair : textureCache) {
    UnloadTexture(pair.second);
  }
  textureCache.clear();
  for (auto &pair : documentCache) {
    if (pair.second.texture.id != 0)
      UnloadTexture(pair.second.texture);
  }
  documentCache.clear();
  documentUseClock = 0;
}

const char *SvgRenderer::GetVariationFolder(IconVariation variation) {
  switch (variation) {
  case IconVariation::Filled:
    return "filled";
  case IconVariation::Outlined:
    return "outlined";
  case IconVariation::Round:
    return "round";
  case IconVariation::Sharp:
    return "sharp";
  case IconVariation::TwoTone:
    return "two-tone";
  default:
    return "filled";
  }
}

std::string SvgRenderer::GetCacheKey(const char *name, IconVariation variation,
                                     int width, int height) {
  return std::string(name) + "_" + GetVariationFolder(variation) + "_" +
         std::to_string(width) + "x" + std::to_string(height);
}

Texture2D SvgRenderer::LoadSvgTexture(const char *name, IconVariation variation,
                                      int width, int height) {
  SvgModel model;
  bool loaded = false;

#if RAYM3_EMBED_RESOURCES
  std::string folder = GetVariationFolder(variation);
  std::string iconKey = folder + "/" + name;
  
  // Linear search in static array (simpler/safer than static map init).
  // Optimization: Could use binary search if sorted, or build map once lazily.
  for (const auto* asset = embedded_assets; asset->path != nullptr; ++asset) {
      if (iconKey == asset->path) {
          loaded = model.LoadFromString(asset->content);
          break;
      }
  }
#endif

  if (!loaded) {
#ifdef __EMSCRIPTEN__
    // On WASM, icons should be embedded. If not found, log and return empty.
    std::string folder = GetVariationFolder(variation);
    std::cerr << "Icon not found in embedded resources: " << folder << "/" << name << std::endl;
    return {0};
#else
    if (basePath.empty() && !autoDetected) {
      basePath = DetectIconPath();
      autoDetected = true;
    }

    std::string folder = GetVariationFolder(variation);
    std::string fullPath = basePath + folder + "/" + name + ".svg";

    if (!std::filesystem::exists(fullPath)) {
      std::cerr << "Icon not found: " << fullPath << std::endl;
      return {0};
    }

    loaded = model.LoadFromFile(fullPath.c_str());
    if (!loaded) {
      std::cerr << "Failed to load SVG: " << fullPath << std::endl;
      return {0};
    }
#endif
  }

  // Rasterize using nanosvgrast
  NSVGrasterizer *rast = nsvgCreateRasterizer();
  if (rast == nullptr) {
    return {0};
  }

  // Allocate pixel buffer (RGBA)
  int stride = width * 4;
  unsigned char *data = (unsigned char *)MemAlloc(width * height * 4);
  if (data == nullptr) {
    nsvgDeleteRasterizer(rast);
    return {0};
  }

  // Scale SVG to fit width/height
  // NSVGimage has width/height.
  NSVGimage *image = model.GetImage();
  float scaleX = (float)width / image->width;
  float scaleY = (float)height / image->height;
  float scale =
      (scaleX < scaleY) ? scaleX : scaleY; // Maintain aspect ratio? Or fill?
  // Icons are usually square.

  nsvgRasterize(rast, image, 0, 0, scale, data, width, height, stride);

  // Post-process: Convert all pixels to White (preserving Alpha) so they can be
  // tinted by Raylib
  int pixelCount = width * height;
  for (int i = 0; i < pixelCount; ++i) {
    // data is RGBA
    // We only care if there is some opacity.
    // Actually, we can just set RGB to 255 regardless, relying on Alpha.
    // But let's be safe and only touch non-transparent ones if needed.
    // Raylib tinting multiplies: Final = Tex * Tint.
    // If Tex is (0,0,0,A), Final is (0,0,0,A*TintA). Color is lost.
    // If Tex is (255,255,255,A), Final is (TintR, TintG, TintB, A*TintA).
    // Correct.

    data[i * 4 + 0] = 255; // R
    data[i * 4 + 1] = 255; // G
    data[i * 4 + 2] = 255; // B
                           // Alpha (data[i*4+3]) remains unchanged
  }

  // Create Raylib Image
  Image rayImage = {.data = data,
                    .width = width,
                    .height = height,
                    .mipmaps = 1,
                    .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

  Texture2D texture = LoadTextureFromImage(rayImage);

  // Cleanup
  UnloadImage(rayImage); // Frees data
  nsvgDeleteRasterizer(rast);

  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  return texture;
}

Texture2D SvgRenderer::LoadDocumentTexture(const char *source, int width,
                                          int height) {
  if (!source || !*source || width <= 0 || height <= 0)
    return {0};

  SvgModel model;
  if (!model.LoadFromString(source))
    return {0};
  NSVGimage *image = model.GetImage();
  if (!image || image->width <= 0.0f || image->height <= 0.0f)
    return {0};

  NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
  if (!rasterizer)
    return {0};
  const size_t byteCount = (size_t)width * (size_t)height * 4;
  unsigned char *pixels = (unsigned char *)MemAlloc((unsigned int)byteCount);
  if (!pixels) {
    nsvgDeleteRasterizer(rasterizer);
    return {0};
  }
  std::memset(pixels, 0, byteCount);
  const float scale = std::min((float)width / image->width,
                               (float)height / image->height);
  const float offsetX = ((float)width - image->width * scale) * 0.5f;
  const float offsetY = ((float)height - image->height * scale) * 0.5f;
  nsvgRasterize(rasterizer, image, offsetX, offsetY, scale, pixels, width,
                height, width * 4);

  Image raster = {.data = pixels,
                  .width = width,
                  .height = height,
                  .mipmaps = 1,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(raster);
  UnloadImage(raster);
  nsvgDeleteRasterizer(rasterizer);
  if (texture.id != 0)
    SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  return texture;
}

void SvgRenderer::DrawDocument(const char *source, Rectangle bounds,
                               unsigned char opacity) {
  if (!source || bounds.width <= 0.0f || bounds.height <= 0.0f)
    return;
  const float dpiScale = FontManager::GetDpiScale();
  const int width = std::max(1, (int)std::ceil(bounds.width * dpiScale));
  const int height = std::max(1, (int)std::ceil(bounds.height * dpiScale));
  // Raster dimensions affect curve flattening and sampling. The same SVG can
  // appear in multiple CSS boxes (or at multiple DPI scales), so include the
  // requested resolution in the source snapshot key.
  const std::string key = std::string(source) + "\n#raster=" +
                          std::to_string(width) + "x" +
                          std::to_string(height);

  auto it = documentCache.find(key);
  if (it == documentCache.end()) {
    // Inline SVGs can be stateful (for example, a game board regenerated each
    // tick). Keep an LRU sized above the ~150 unique tile faces a game board
    // renders per frame; thrashing the LRU re-rasterizes every SVG each frame.
    constexpr size_t kDocumentCacheLimit = 512;
    if (documentCache.size() >= kDocumentCacheLimit) {
      auto oldest = documentCache.begin();
      for (auto candidate = documentCache.begin();
           candidate != documentCache.end(); ++candidate) {
        if (candidate->second.lastUse < oldest->second.lastUse)
          oldest = candidate;
      }
      if (oldest->second.texture.id != 0) {
        // Flush pending batches so no recorded batch still references the
        // texture id we are about to unload (ids get recycled).
        rlDrawRenderBatchActive();
        UnloadTexture(oldest->second.texture);
      }
      documentCache.erase(oldest);
    }
    Texture2D texture = LoadDocumentTexture(source, width, height);
    if (texture.id == 0)
      return;
    it = documentCache.emplace(key, DocumentTexture{texture, 0}).first;
  }
  it->second.lastUse = ++documentUseClock;
  Texture2D texture = it->second.texture;
  Rectangle sourceRect = {0.0f, 0.0f, (float)texture.width,
                          (float)texture.height};
  DrawTexturePro(texture, sourceRect, bounds, {0.0f, 0.0f}, 0.0f,
                 Color{255, 255, 255, opacity});
  DrawSvgText(source, bounds, opacity);
}

void SvgRenderer::DrawIcon(const char *name, Rectangle bounds,
                           IconVariation variation, Color color) {
  float dpiScale = FontManager::GetDpiScale();
  int width = std::max(1, (int)std::round(bounds.width * dpiScale));
  int height = std::max(1, (int)std::round(bounds.height * dpiScale));

  if (width <= 0 || height <= 0)
    return;

  std::string key = GetCacheKey(name, variation, width, height);

  Texture2D texture;
  auto it = textureCache.find(key);
  if (it != textureCache.end()) {
    texture = it->second;
  } else {
    texture = LoadSvgTexture(name, variation, width, height);
    if (texture.id != 0) {
      textureCache[key] = texture;
    }
  }

  if (texture.id != 0) {
    Rectangle source = {0.0f, 0.0f, (float)texture.width,
                        (float)texture.height};
    DrawTexturePro(texture, source, bounds, {0.0f, 0.0f}, 0.0f, color);
  }
}

} // namespace raym3
