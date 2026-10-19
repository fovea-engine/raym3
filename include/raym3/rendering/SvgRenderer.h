#pragma once

#include "raym3/types.h"
#include <raylib.h>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace raym3 {

class SvgRenderer {
public:
  static void Initialize(const char *basePath);
  static void Shutdown();

  static void DrawIcon(const char *name, Rectangle bounds,
                       IconVariation variation = IconVariation::Filled,
                       Color color = BLACK);

  // Rasterize an inline SVG document once per distinct source and draw it at
  // the requested bounds. The bounded cache is shared by native Miso SVG
  // nodes and ordinary RayM3 icon rendering.
  static void DrawDocument(const char *source, Rectangle bounds,
                           unsigned char opacity = 255);

  // Helper to get the variation folder name
  static const char *GetVariationFolder(IconVariation variation);

private:
  static std::string basePath;
  static std::unordered_map<std::string, Texture2D> textureCache;
  struct DocumentTexture {
    Texture2D texture;
    std::uint64_t lastUse;
  };
  static std::unordered_map<std::string, DocumentTexture> documentCache;
  static std::uint64_t documentUseClock;
  static bool autoDetected;

  static std::string GetCacheKey(const char *name, IconVariation variation,
                                 int width, int height);
  static Texture2D LoadSvgTexture(const char *name, IconVariation variation,
                                  int width, int height);
  static Texture2D LoadDocumentTexture(const char *source, int width,
                                       int height);
};

} // namespace raym3
