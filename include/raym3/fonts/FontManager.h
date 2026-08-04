#pragma once

#include "raym3/types.h"
#include <cstdint>
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace raym3 {

struct FontKey {
  FontWeight weight;
  FontStyle style;
  int size;

  bool operator==(const FontKey &other) const {
    return weight == other.weight && style == other.style && size == other.size;
  }
};

struct FontKeyHash {
  std::size_t operator()(const FontKey &key) const {
    return std::hash<int>()(static_cast<int>(key.weight)) ^
           (std::hash<int>()(static_cast<int>(key.style)) << 1) ^
           (std::hash<int>()(key.size) << 2);
  }
};

class FontManager {
public:
  static void Initialize();
  static void Shutdown();
  // Clear cached GPU font handles after graphics-device loss/recreation.
  // This intentionally does not call UnloadFont() because the old handles may
  // belong to a destroyed device/swapchain.
  static void ResetDeviceCache();
  // Clear cached GPU font handles while the graphics device is still live.
  // Unlike ResetDeviceCache(), this unloads the old font textures first.
  static void InvalidateLiveDeviceCache();

  // Set the device DPI scale (e.g. 3.5 on a 560dpi Android phone, 2.0 on
  // a Retina display, 1.0 on a standard-density desktop). The font system
  // multiplies requested sizes by this so font textures are generated at
  // the actual pixel size — otherwise the host's dp-scaling matrix would
  // upscale the font ~3.5x and text would render blurry.
  // Default 1.0 (no scaling). Call once at startup.
  static void SetDpiScale(float scale);
  static float GetDpiScale();

  // Bumped whenever an atlas is rebuilt (lazy glyph growth or DPI reload).
  // Include in PreparedText cache keys so pretext widths cannot go stale.
  static std::uint64_t FontGeneration();

  static Font LoadFont(FontWeight weight = FontWeight::Regular,
                       FontStyle style = FontStyle::Normal, int size = 16);
  static Font LoadCustomFont(const std::string &path, int size,
                             const std::vector<int> &codepoints = {});

  // Ensure every codepoint in `utf8` is present in the default-family atlas
  // for (weight, style, size). Must run before MeasureTextEx / prepare.
  static void EnsureGlyphsForText(FontWeight weight, FontStyle style, int size,
                                  std::string_view utf8);
  // Same for a registered custom family (size only; no style axis).
  static void EnsureGlyphsForFamily(std::string_view name, int size,
                                    std::string_view utf8);

  // Named font registry — call RegisterFont once at startup, then reference
  // the name in CSS font-family or style.text.fontFamily.
  static void RegisterFont(const std::string &name, const std::string &path,
                           std::vector<int> codepoints = {});
  // Register a font family from raw bytes (TTF/OTF) instead of a path —
  // needed for assets delivered as bytes (web MEMFS-less runtime load,
  // Android APK asset bytes, or any RayactAsset.bytes() payload). FontManager
  // copies the bytes into its own storage.
  static void RegisterFontFromMemory(const std::string &name,
                                     std::vector<unsigned char> bytes,
                                     std::vector<int> codepoints = {});
  static Font LoadFontByFamily(const std::string &name, int size);

  static bool HasFont(const std::string &name);
  static std::vector<std::string> ListRegisteredFonts();

  static void UnloadFont(Font font);

  static Font GetDefaultFont() { return defaultFont_; }

  // Exact integer dp size (min 1). Lazy ASCII atlases keep memory bounded, so
  // coarse bucket snapping is no longer needed.
  static int SnapSize(int size);

private:
  struct CachedFont {
    Font font = {0};
    std::unordered_set<int> codepoints;
  };

  struct FontSource {
    std::string path;
    std::vector<unsigned char> bytes;
    std::vector<int> codepoints;
    bool isMemory = false;
  };

  static Font LoadDefaultUiFont(FontWeight weight, FontStyle style, int size,
                                const std::vector<int> &codepoints);
  static Font BakeFont(FontWeight weight, FontStyle style, int size,
                       const std::vector<int> &codepoints);
  static Font LoadCustomFontFromMemory(const std::vector<unsigned char> &bytes,
                                       int size,
                                       const std::vector<int> &codepoints);
  static void InvalidateCustomFontCache(const std::string &name);
  static std::vector<int> AsciiSeed();
  static bool UnionCodepointsFromUtf8(std::unordered_set<int> &set,
                                      std::string_view utf8);
  static std::vector<int> SortedCodepoints(const std::unordered_set<int> &set);

  static std::unordered_map<FontKey, CachedFont, FontKeyHash> fontCache_;
  static Font defaultFont_;
  static bool initialized_;
  static float dpiScale_;
  static std::uint64_t fontGeneration_;

  static std::unordered_map<std::string, FontSource> fontRegistry_;
  // Custom font cache: "name:size" → CachedFont
  static std::unordered_map<std::string, CachedFont> customFontCache_;
};

} // namespace raym3
