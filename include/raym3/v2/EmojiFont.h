#pragma once

#include <raylib.h>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace raym3::v2 {

// Full-color emoji support with a hybrid backend:
//
//   • OS rasterization (preferred, zero bundled storage): each emoji cluster is
//     drawn by the platform's own emoji renderer into an RGBA bitmap and cached
//     as a GPU texture. Used on Android (Paint/Canvas via JNI), Apple
//     (CoreText), and Windows (DirectWrite/D2D Segoe UI Emoji). This uses the
//     device's real emoji set, including newer glyphs and whatever color format
//     the OS ships (e.g. COLRv1). When a rasterizer is installed it is the only
//     automatic backend — there is no silent fall-through to a full bundled TTF.
//
//   • Windows country-flag exception: Segoe UI Emoji ships no ISO flag glyphs
//     (regional indicators render as letter pairs like "US"). Flag clusters fall
//     through to a small bundled `NotoColorEmoji-Flags.ttf` CBDT subset (~0.85 MB).
//
//   • Bundled/system CBDT: parses NotoColorEmoji.ttf (CBDT/CBLC + GSUB) when no
//     OS rasterizer is available (Linux / web), or when an app explicitly calls
//     loadEmoji()/RegisterCustomEmojiFont. ~10MB; ships only on Linux desktop
//     and in the web embed.
//
// Cluster *segmentation* (which spans are emoji, incl. ZWJ sequences, flags,
// keycaps and skin-tone modifiers) is Unicode-based and font-independent, so it
// works for every backend and is also the grapheme fundamental shared with the
// pretext-derived TextEngine.
//
// If no backend is available the drawing helpers fall back to plain
// DrawTextEx/MeasureTextEx — no behavior change.
class EmojiFont {
public:
  static EmojiFont &Instance();

  // Inject a platform emoji rasterizer. Renders a UTF-8 emoji cluster at `px`
  // pixels into a freshly-allocated RGBA8 (straight alpha) buffer of *outW x
  // *outH; the EmojiFont takes ownership and frees it with std::free. Return
  // nullptr on failure. Called on the render thread.
  using Rasterizer =
      std::function<unsigned char *(const char *utf8, int px, int *outW, int *outH)>;
  void SetRasterizer(Rasterizer fn);

  // True once a usable backend (OS rasterizer or bundled CBDT) is resolved.
  bool Available();

  struct Segment {
    bool isEmoji = false;
    std::size_t byteStart = 0;
    std::size_t byteEnd = 0;
  };

  // Split UTF-8 text into alternating plain-text / emoji segments. Emoji
  // clusters collapse to a single segment. Font-independent (Unicode rules).
  std::vector<Segment> SegmentText(std::string_view utf8);

  struct Glyph {
    Texture2D texture = {0};
    int pngWidth = 0;
    int pngHeight = 0;
    bool failed = false;
  };

  // Lazily render + upload the bitmap for an emoji cluster (the cluster's UTF-8
  // bytes). Returns nullptr if unavailable. Render-thread only (creates a GPU
  // texture). Cached by cluster string.
  const Glyph *GetCluster(std::string_view clusterUtf8);

  void ResetTextureCache();

  // Register (or replace) a CBDT/CBLC emoji font from bytes instead of scanning
  // for resources/fonts/NotoColorEmoji.ttf on disk — lets JS ship a
  // smaller/newer/custom emoji font. Clears any OS rasterizer so the custom
  // font wins on every platform. Resets the resolved backend so the next
  // Available()/GetCluster() call re-picks with the new bytes.
  void RegisterCustomEmojiFont(std::vector<std::uint8_t> bytes);

  enum class BackendKind { None, OsRasterizer, Bundled, Unresolved };
  // Which backend is currently active (or would be chosen next) — useful for
  // JS-side diagnostics / the loadEmoji() resolution value.
  BackendKind ActiveBackend();

private:
  EmojiFont() = default;
  void EnsureBackend();
  // One-shot check that the injected rasterizer actually produces ink.
  bool ProbeRasterizer();

  enum class Backend { Unresolved, None, Raster, Cbdt };
  Backend backend_ = Backend::Unresolved;
  Rasterizer rasterizer_;

  std::unordered_map<std::string, Glyph> clusterCache_;

  // ── bundled CBDT backend (parsed only when selected) ──────────────────
  bool LoadCbdtFont();
  // Windows OS-rasterizer path: load the flags-only CBDT subset for RI pairs.
  bool LoadFlagsCbdtFont();
  bool ParseLoadedCbdtFont(const std::string &pathForLog);
  bool fontParsed_ = false;
  bool flagsFontResolved_ = false;
  bool flagsFontOk_ = false;
  std::vector<std::uint8_t> data_;
  std::vector<std::uint8_t> customFontBytes_; // pre-seeded via RegisterCustomEmojiFont

  struct CmapRange {
    std::uint32_t start, end, startGlyph;
  };
  std::vector<CmapRange> cmap_;

  struct Ligature {
    std::vector<std::uint16_t> components;
    std::uint16_t ligatureGlyph;
  };
  std::unordered_map<std::uint16_t, std::vector<Ligature>> ligatures_;

  struct IndexSubTable {
    std::uint16_t firstGlyph, lastGlyph;
    std::uint16_t indexFormat, imageFormat;
    std::uint32_t imageDataOffset;
    std::vector<std::uint32_t> offsets;
    std::uint32_t imageSize = 0;
    std::vector<std::uint16_t> glyphArray;
  };
  std::uint32_t cbdtOffset_ = 0, cbdtLength_ = 0;
  float strikePpem_ = 1.0f;
  std::vector<IndexSubTable> strike_;

  std::uint16_t GlyphForCodepoint(std::uint32_t cp) const;
  std::uint16_t ShapeCluster(std::string_view clusterUtf8) const;
  std::size_t MatchLigature(const std::vector<std::uint16_t> &glyphs,
                            std::size_t pos, std::uint16_t &outGlyph) const;
  bool DecodeGlyphBitmap(std::uint16_t glyphId, Glyph &out) const;
};

// Platform emoji rasterizer (defined per-OS: EmojiRasterizerApple.mm,
// EmojiRasterizerWindows.cpp). Declared here so EmojiFont can auto-wire it.
unsigned char *PlatformRasterizeEmoji(const char *utf8, int px, int *outW,
                                      int *outH);

// ── High-level draw / measure helpers ─────────────────────────────────────
// fontSize/spacing are in the caller's own coordinate unit (px in the v2 text
// path, dp in the legacy Renderer path). Emoji use a fixed em-square advance so
// measure and draw stay in lock-step. Falls back to plain raylib when no emoji
// are present or no backend is available.
void DrawTextWithEmoji(Font font, std::string_view utf8, Vector2 pos,
                       float fontSize, float spacing, Color tint);
Vector2 MeasureTextWithEmoji(Font font, std::string_view utf8, float fontSize,
                             float spacing);

// Grapheme cluster boundaries with emoji clusters kept whole (used by the
// TextEngine / TextField so emoji aren't split by cursor, wrapping or
// selection). Returns byte offsets [0 .. text.size()].
std::vector<std::size_t> EmojiAwareGraphemeBoundaries(std::string_view utf8);

} // namespace raym3::v2
