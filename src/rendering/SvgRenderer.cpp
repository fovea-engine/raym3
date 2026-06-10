#include "raym3/rendering/SvgRenderer.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/rendering/SvgModel.h"
#include "raym3/config.h"
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif
#include <algorithm>
#include <cmath>
#include <iostream>
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
bool SvgRenderer::autoDetected = false;

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
