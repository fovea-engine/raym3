#include "raym3/rendering/Renderer.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/fonts/SystemUiFont.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/EmojiFont.h"
#include "rlgl.h"
#include <algorithm>   // std::clamp — libc++ leaks it via other headers, MSVC's STL does not
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace raym3 {
namespace {
constexpr int kRoundedRectSegments = 16;

static FontWeight CssFontWeight(int weight) {
  if (weight >= 850)
    return FontWeight::Black;
  if (weight >= 650)
    return FontWeight::Bold;
  if (weight >= 550)
    return FontWeight::Medium;
  if (weight <= 300)
    return FontWeight::Light;
  return FontWeight::Regular;
}

static std::string TrimFontFamily(std::string value) {
  while (!value.empty() && std::isspace((unsigned char)value.front()))
    value.erase(value.begin());
  while (!value.empty() && std::isspace((unsigned char)value.back()))
    value.pop_back();
  if (value.size() >= 2 &&
      ((value.front() == '\'' && value.back() == '\'') ||
       (value.front() == '"' && value.back() == '"')))
    value = value.substr(1, value.size() - 2);
  return value;
}

static std::string ResolveTextFamily(std::string_view familyStack, int weight) {
  const FontWeight cssWeight = CssFontWeight(weight);
  const std::string stack(familyStack);
  size_t start = 0;
  while (start <= stack.size()) {
    const size_t comma = stack.find(',', start);
    const size_t end = comma == std::string::npos ? stack.size() : comma;
    const std::string family = TrimFontFamily(stack.substr(start, end - start));
    if (!family.empty()) {
      if (FontManager::HasFont(family))
        return family;
      std::string path;
      if (ResolveCssFontPath(family, cssWeight, FontStyle::Normal, path)) {
        const std::string key = "__css_family__" + family + "_" +
            std::to_string(weight);
        if (!FontManager::HasFont(key))
          FontManager::RegisterFont(key, path);
        return key;
      }
    }
    if (comma == std::string::npos)
      break;
    start = comma + 1;
  }
  return {};
}

// Uniform border thickness: raylib's DrawRectangleRoundedLinesEx uses 1px line
// arcs when lineThick <= 1, so corners look thinner than straight edges.
// notchStart/notchEnd cut a gap out of the top edge (M3 outlined text field
// label notch); pass notchEnd <= notchStart for an uninterrupted frame.
static void DrawRoundedBorderFrame(Rectangle bounds, float cornerRadius,
                                   float lineWidth, Color color,
                                   float notchStart = 0.0f,
                                   float notchEnd = 0.0f) {
  if (lineWidth <= 0.0f)
    return;

  const float w = lineWidth;
  if (bounds.width <= 2.0f * w || bounds.height <= 2.0f * w) {
    Renderer::DrawRoundedRectangle(bounds, cornerRadius, color);
    return;
  }

  const float r =
      std::min(cornerRadius, std::min(bounds.width, bounds.height) * 0.5f);
  const float innerR = std::max(0.0f, r - w);
  const int segs = kRoundedRectSegments;

  // Use the float-coord DrawRectangleRec, NOT DrawRectangle(int,int,...): the
  // latter truncates to integer pixels, so at fractional positions (e.g. a
  // momentum-scrolled offset) the straight edges snap to whole pixels while the
  // corner arcs (DrawRing) and the fill (DrawRectangleRounded) stay on the float
  // position — the border then misaligns/shimmers against the fill while scrolling.
  if (bounds.width > 2.0f * r) {
    const float topLeft = bounds.x + r;
    const float topRight = bounds.x + bounds.width - r;
    if (notchEnd > notchStart) {
      const float leftSpanEnd = std::clamp(notchStart, topLeft, topRight);
      const float rightSpanStart = std::clamp(notchEnd, topLeft, topRight);
      if (leftSpanEnd > topLeft)
        DrawRectangleRec({topLeft, bounds.y, leftSpanEnd - topLeft, w}, color);
      if (topRight > rightSpanStart)
        DrawRectangleRec({rightSpanStart, bounds.y, topRight - rightSpanStart, w},
                         color);
    } else {
      DrawRectangleRec({topLeft, bounds.y, topRight - topLeft, w}, color);
    }
    DrawRectangleRec({bounds.x + r, bounds.y + bounds.height - w,
                      bounds.width - 2.0f * r, w}, color);
  }
  if (bounds.height > 2.0f * r) {
    DrawRectangleRec({bounds.x + bounds.width - w, bounds.y + r, w,
                      bounds.height - 2.0f * r}, color);
    DrawRectangleRec({bounds.x, bounds.y + r, w, bounds.height - 2.0f * r}, color);
  }

  if (r <= 0.0f)
    return;

  DrawRing({bounds.x + r, bounds.y + r}, innerR, r, 180.0f, 270.0f, segs,
           color);
  DrawRing({bounds.x + bounds.width - r, bounds.y + r}, innerR, r, 270.0f,
           360.0f, segs, color);
  DrawRing({bounds.x + bounds.width - r, bounds.y + bounds.height - r}, innerR,
           r, 0.0f, 90.0f, segs, color);
  DrawRing({bounds.x + r, bounds.y + bounds.height - r}, innerR, r, 90.0f,
           180.0f, segs, color);
}
} // namespace

void Renderer::DrawRoundedRectangle(Rectangle bounds, float cornerRadius,
                                    Color color) {
  float minDim = std::min(bounds.width, bounds.height);
  // Raylib expects 1.0 for full rounding (radius = minDim/2).
  // So we need to normalize cornerRadius against minDim/2.
  float roundness = (minDim > 0) ? (2.0f * cornerRadius) / minDim : 0.0f;
  roundness = std::clamp(roundness, 0.0f, 1.0f);
  DrawRectangleRounded(bounds, roundness, kRoundedRectSegments, color);
}

void Renderer::DrawRoundedRectangleEx(Rectangle bounds, float cornerRadius,
                                      Color color, float lineWidth) {
  DrawRoundedBorderFrame(bounds, cornerRadius, lineWidth, color);
}

void Renderer::DrawRoundedRectangleNotched(Rectangle bounds, float cornerRadius,
                                           Color color, float lineWidth,
                                           float notchStart, float notchEnd) {
  DrawRoundedBorderFrame(bounds, cornerRadius, lineWidth, color, notchStart,
                         notchEnd);
}

void Renderer::DrawElevatedRectangle(Rectangle bounds, float cornerRadius,
                                     int elevation, Color color) {
  if (elevation > 0) {
    DrawShadow(bounds, cornerRadius, elevation);
  }
  DrawRoundedRectangle(bounds, cornerRadius, color);
}

namespace {
// ---------------------------------------------------------------------------
// Cached gaussian shadow field textures. One raster per unique
// (pixel size, sigma, color, alpha) spec; game boards reuse ~a dozen specs,
// so the cache holds a handful of small surfaces.
// ---------------------------------------------------------------------------
struct ShadowFieldTexture {
  Texture2D texture{};
  unsigned long long lastUse = 0;
};

std::unordered_map<std::string, ShadowFieldTexture> g_shadowFields;
unsigned long long g_shadowFieldClock = 0;
constexpr size_t kShadowFieldLimit = 128;

// Φ(z) = std-normal CDF = 0.5 erfc(-z/√2), via the Abramowitz-Stegun 7.1.26
// erfc approximation (max error 1.5e-7) evaluated at |z|/√2 with its
// alternating-sign polynomial; erfc(t) = t·e^(-t²)·P(t).
float PhiCoverage(float z) {
  const bool negative = z < 0.0f;
  const float w = 0.70710678f * (negative ? -z : z);
  const float t = 1.0f / (1.0f + 0.3275911f * w);
  const float erfc = t * std::exp(-(w * w)) *
      ((((1.061405429f * t - 1.453152027f) * t + 1.421413741f) * t -
        0.284496736f) * t + 0.254829592f);
  return negative ? 0.5f * erfc : 1.0f - 0.5f * erfc;
}

// The raster stores only the NORMALIZED field alpha — the blur of the
// (rounded) shadow rect mapped to 0..255. The shadow's own opacity is
// applied at draw time as tint alpha, so the cache key is opacity-
// independent: an element animating its opacity (e.g. popIn transitions
// changing alpha every frame) reuses the same texture instead of evicting
// the whole LRU each frame.
Texture2D BuildShadowFieldTexture(int w, int h, float pad, float spanW,
                                  float spanH, float sigma, Color color,
                                  float cornerRadius) {
  char key[112];
  std::snprintf(key, sizeof(key), "%dx%d|%f|c%02x%02x%02x|r%.2f", w, h, sigma,
                color.r, color.g, color.b,
                std::max(0.0f, cornerRadius));
  auto it = g_shadowFields.find(key);
  if (it != g_shadowFields.end()) {
    it->second.lastUse = ++g_shadowFieldClock;
    return it->second.texture;
  }
  while (g_shadowFields.size() >= kShadowFieldLimit &&
         !g_shadowFields.empty()) {
    auto oldest = g_shadowFields.begin();
    for (auto candidate = g_shadowFields.begin();
         candidate != g_shadowFields.end(); ++candidate) {
      if (candidate->second.lastUse < oldest->second.lastUse)
        oldest = candidate;
    }
    if (oldest->second.texture.id != 0) {
      // Flush pending batches so no recorded batch still references the
      // texture id we are about to unload (ids get recycled).
      rlDrawRenderBatchActive();
      UnloadTexture(oldest->second.texture);
    }
    g_shadowFields.erase(oldest);
  }

  const size_t byteCount = (size_t)w * (size_t)h * 4;
  unsigned char *pixels = (unsigned char *)MemAlloc((unsigned int)byteCount);
  if (!pixels)
    return Texture2D{0};
  std::memset(pixels, 0, byteCount);

  if (cornerRadius <= 0.35f) {
    // Per-axis interval coverage of the shadow span, sampled at pixel centers
    // (px + 0.5). Column vector Gx, row vector Gy; the alpha is their
    // product — the exact separable field of a rect blurred with sigma.
    std::vector<float> colCoverage((size_t)w);
    const float u0 = pad;
    const float u1 = pad + spanW;
    const float v0 = pad;
    const float v1 = pad + spanH;
    const float s2 = sigma;
    for (int x = 0; x < w; ++x)
      colCoverage[(size_t)x] = PhiCoverage(((float)x + 0.5f - u0) / s2) -
                               PhiCoverage(((float)x + 0.5f - u1) / s2);
    for (int y = 0; y < h; ++y) {
      const float rowCoverage =
          PhiCoverage(((float)y + 0.5f - v0) / s2) -
          PhiCoverage(((float)y + 0.5f - v1) / s2);
      unsigned char *out = pixels + (size_t)y * (size_t)w * 4;
      for (int x = 0; x < w; ++x) {
        const float a = rowCoverage * colCoverage[(size_t)x];
        out[x * 4 + 0] = color.r;
        out[x * 4 + 1] = color.g;
        out[x * 4 + 2] = color.b;
        out[x * 4 + 3] = (unsigned char)std::clamp(
            (int)std::lround(a * 255.0f), 0, 255);
      }
    }
  } else {
    // Rounded rect: rasterize an SDF coverage indicator and approximate the
    // gaussian with three separable box blurs (box radius from sigma; the
    // classic Skia-style triple-box approximation). Mass is preserved per
    // pass, so the field sums like an equivalent gaussian blur.
    auto field = std::vector<float>((size_t)w * (size_t)h);
    const float hx = spanW * 0.5f;
    const float hy = spanH * 0.5f;
    const float cx = pad + hx;
    const float cy = pad + hy;
    const float radius = std::min(cornerRadius, std::min(hx, hy));
    for (int y = 0; y < h; ++y) {
      const float py = ((float)y + 0.5f) - cy;
      if (std::abs(py) > hy + sigma * 3.2f) continue;
      for (int x = 0; x < w; ++x) {
        const float px = ((float)x + 0.5f) - cx;
        const float qx = std::fabs(px) - (hx - radius);
        const float qy = std::fabs(py) - (hy - radius);
        const float ax = std::max(qx, 0.0f);
        const float ay = std::max(qy, 0.0f);
        const float d = std::min(std::max(qx, qy), 0.0f) +
                        std::sqrt(ax * ax + ay * ay) - radius;
        field[(size_t)y * (size_t)w + (size_t)x] = std::clamp(0.5f - d, 0.0f,
                                                              1.0f);
      }
    }
    // Box radius: sum of 3 boxes of (2r+1) matches var 6·(r²-1)/12 = σ².
    const int boxR = std::max(1, (int)std::lround(std::sqrt(sigma * sigma + 1.0f)));
    std::vector<float> tmp((size_t)w * (size_t)h);
    for (int pass = 0; pass < 3; ++pass) {
      // Horizontal
      for (int y = 0; y < h; ++y) {
        const float *row = field.data() + (size_t)y * (size_t)w;
        double acc = 0.0;
        const int win = 2 * boxR + 1;
        for (int i = -boxR; i <= boxR; ++i) {
          const int cx2 = std::clamp(i, 0, w - 1);
          acc += row[(size_t)cx2];
        }
        for (int x = 0; x < w; ++x) {
          tmp[(size_t)y * (size_t)w + (size_t)x] = (float)(acc / win);
          const int addX = std::clamp(x + boxR + 1, 0, w - 1);
          const int subX = std::clamp(x - boxR, 0, w - 1);
          acc += row[(size_t)addX] - row[(size_t)subX];
        }
      }
      // Vertical
      for (int x = 0; x < w; ++x) {
        double acc = 0.0;
        const int win = 2 * boxR + 1;
        for (int i = -boxR; i <= boxR; ++i) {
          const int cy2 = std::clamp(i, 0, h - 1);
          acc += tmp[(size_t)cy2 * (size_t)w + (size_t)x];
        }
        for (int y = 0; y < h; ++y) {
          field[(size_t)y * (size_t)w + (size_t)x] = (float)(acc / win);
          const int addY = std::clamp(y + boxR + 1, 0, h - 1);
          const int subY = std::clamp(y - boxR, 0, h - 1);
          acc += tmp[(size_t)addY * (size_t)w + (size_t)x] -
                 tmp[(size_t)subY * (size_t)w + (size_t)x];
        }
      }
    }
    for (size_t i = 0; i < (size_t)w * (size_t)h; ++i) {
      unsigned char *out = pixels + i * 4;
      const float a = field[i];
      out[0] = color.r;
      out[1] = color.g;
      out[2] = color.b;
      out[3] = (unsigned char)std::clamp((int)std::lround(a * 255.0f), 0, 255);
    }
  }

  Image raster = {.data = pixels,
                  .width = w,
                  .height = h,
                  .mipmaps = 1,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  Texture2D texture = LoadTextureFromImage(raster);
  UnloadImage(raster);
  if (texture.id == 0)
    return Texture2D{0};
  SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
  g_shadowFields.emplace(key, ShadowFieldTexture{texture, ++g_shadowFieldClock});
  return texture;
}
} // namespace
void Renderer::DrawCssBoxShadow(Rectangle bounds, float cornerRadius,
                                float offsetX, float offsetY, float blurRadius,
                                float spreadRadius, Color color, bool inset,
                                float opacity) {
  if (color.a == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f)
    return;
  const float alpha = std::clamp(opacity, 0.0f, 1.0f);
  if (alpha <= 0.004f)
    return;
  if (inset) {
    // The retained CSS painter uses an inner rounded frame as the portable
    // approximation for inset blur; keep its geometry and alpha in RayM3.
    const float insetAmount = std::max(1.0f, blurRadius + spreadRadius);
    Rectangle inner{bounds.x + offsetX + insetAmount,
                    bounds.y + offsetY + insetAmount,
                    std::max(0.0f, bounds.width - 2.0f * insetAmount),
                    std::max(0.0f, bounds.height - 2.0f * insetAmount)};
    const float thickness = std::max(1.0f, blurRadius + std::abs(spreadRadius));
    DrawRoundedBorderFrame(inner, std::max(0.0f, cornerRadius - insetAmount),
                           thickness, ColorAlpha(color, alpha));
    return;
  }

  // Outset: the analytic gaussian field of the shadow rect, matching what
  // the browser composites for box-shadow blur. A separable rect blurred by
  // a gaussian kernel has pixel alpha
  //   A(x,y) = A * (Φ((x-u0)/σ) - Φ((x-u1)/σ)) * (Φ((y-v0)/σ) - Φ((y-v1)/σ))
  // (interval coverage per axis; σ = blurRadius/2, Φ = normal CDF). Rasterize
  // that field once per unique shadow spec into a cached texture and draw it
  // like the browser's cached blur surface — exact alpha everywhere (no
  // ring-union over-darkening, no stroke corner over-coverage), and one
  // texture draw per shadow per frame.
  const float sigma = std::max(0.125f, blurRadius * 0.5f);
  Rectangle core{bounds.x + offsetX - spreadRadius,
                 bounds.y + offsetY - spreadRadius,
                 bounds.width + 2.0f * spreadRadius,
                 bounds.height + 2.0f * spreadRadius};
  if (core.width <= 0.0f || core.height <= 0.0f)
    return;
  const int pad = (int)std::ceil(2.8f * sigma) + 1;
  const int texW = (int)std::ceil(core.width) + 2 * pad;
  const int texH = (int)std::ceil(core.height) + 2 * pad;
  Texture2D field = BuildShadowFieldTexture(texW, texH,
      (float)pad, core.width, core.height, sigma, color,
      std::max(0.0f, cornerRadius + spreadRadius));
  if (field.id == 0)
    return;
  Rectangle dest{core.x - (float)pad, core.y - (float)pad, (float)texW,
                 (float)texH};
  const unsigned char tintAlpha =
      (unsigned char)std::clamp((int)std::lround(alpha * 255.0f), 0, 255);
  DrawTexturePro(field, Rectangle{0, 0, (float)texW, (float)texH}, dest,
                 {0.0f, 0.0f}, 0.0f, Color{255, 255, 255, tintAlpha});
}


void Renderer::DrawShadow(Rectangle bounds, float cornerRadius, int elevation) {
  Color shadowColor = Theme::GetElevationColor(elevation);
  float shadowOffset = Theme::GetElevationShadow(elevation);

  // User request: "shadownshould be more visible"
  // The current implementation is a simple offset rectangle which looks flat.
  // To make it more visible/realistic without shaders, we can:
  // 1. Draw multiple layers with decreasing opacity (fake blur)
  // 2. Increase offset or opacity (currently controlled by
  // Theme::GetElevationColor/Shadow) Let's try drawing a few layers for a
  // softer look, or just darker/bigger offset.

  // Simple improvement: multiple passes
  int layers = 3;
  for (int i = 0; i < layers; i++) {
    float scale = 1.0f + (float)(i + 1) * 0.02f;
    float offset = shadowOffset * (float)(i + 1) / layers;
    // Just simple offset for now to avoid complex scaling math on rect

    Rectangle shadowBounds = {bounds.x + offset, bounds.y + offset,
                              bounds.width, bounds.height};

    Color layerColor =
        ColorAlpha(shadowColor, 0.3f / layers); // Distribute opacity
    DrawRoundedRectangle(shadowBounds, cornerRadius, layerColor);
  }
}

void Renderer::DrawStateLayer(Rectangle bounds, float cornerRadius,
                              Color baseColor, ComponentState state) {
  Color layerColor = Theme::GetStateLayerColor(baseColor, state);
  if (layerColor.a > 0) {
    DrawRoundedRectangle(bounds, cornerRadius, layerColor);
  }
}

void Renderer::DrawText(const char *text, Vector2 position, float fontSize,
                        Color color, FontWeight weight) {
  Font font = Theme::GetFont(fontSize, weight);
  v2::DrawTextWithEmoji(font, text ? text : "", position, fontSize, 0, color);
}

void Renderer::DrawTextCentered(const char *text, Rectangle bounds,
                                float fontSize, Color color,
                                FontWeight weight) {
  Font font = Theme::GetFont(fontSize, weight);
  Vector2 textSize = v2::MeasureTextWithEmoji(font, text ? text : "", fontSize, 0);

  // Optical vertical centering. raylib anchors text at the ascender line, so
  // centering the whole glyph box leaves the descender's blank space skewing the
  // visible glyphs off-centre (text looks bottom-heavy in fixed-height chrome
  // like buttons). Centre the ascent box instead — lift by half the descender —
  // so the cap/x-height band sits on the true centre, matching web/RN.
  // The ascent is read from the face: fontSize is an em (FontManager bakes it
  // that way), and ascent is ~0.93 em on Roboto, ~0.97 em on SF, so it is NOT a
  // fraction of the drawn box that can be hardcoded across faces.
  const FontVMetrics metrics = FontManager::MetricsFor(weight, FontStyle::Normal);
  const float ascent = fontSize * metrics.ascent;
  Vector2 position = {bounds.x + (bounds.width - textSize.x) / 2.0f,
                      bounds.y + (bounds.height - ascent) / 2.0f};

  v2::DrawTextWithEmoji(font, text ? text : "", position, fontSize, 0, color);
}

void Renderer::DrawTextFamily(const char *text, Rectangle bounds, float fontSize,
                              Color color, const char *family,
                              int fontWeight,
                              float letterSpacing, TextAlignment alignment,
                              float lineHeight) {
  const std::string_view safeText = text ? text : "";
  const std::string safeFamily = family ? family : "";
  const FontWeight fallbackWeight = CssFontWeight(fontWeight);
  std::string faceFamily = ResolveTextFamily(safeFamily, fontWeight);
  const bool registeredFamily = !faceFamily.empty();
  if (faceFamily == "Outfit" || faceFamily == "Fraunces") {
    const int weight = std::clamp(fontWeight, 100, 900);
    int faceWeight = 400;
    if (weight >= 850) faceWeight = 800;
    else if (weight >= 750) faceWeight = 800;
    else if (weight >= 650) faceWeight = 700;
    else if (weight >= 550) faceWeight = 600;
    else faceWeight = 400;
    if (faceFamily == "Fraunces" && faceWeight == 400) faceWeight = 600;
    // ResolveTextFamily returns the registered base family name from the
    // first usable entry in a CSS stack (e.g. "Outfit" from
    // "Outfit, sans-serif"). Use that name for the weighted face lookup;
    // the complete stack is not a registered family key.
    const std::string candidate = faceFamily + "-" + std::to_string(faceWeight);
    if (FontManager::HasFont(candidate)) faceFamily = candidate;
  }
  if (registeredFamily) {
    FontManager::EnsureGlyphsForFamily(faceFamily, (int)fontSize, safeText);
  } else {
    FontManager::EnsureGlyphsForText(fallbackWeight, FontStyle::Normal,
                                     (int)fontSize, safeText);
  }
  Font font = registeredFamily
      ? FontManager::LoadFontByFamily(faceFamily, (int)fontSize)
      : FontManager::LoadFont(fallbackWeight, FontStyle::Normal, (int)fontSize);
  if (family && std::string_view(family).find("Symbols") != std::string_view::npos)
    font = FontManager::LoadFontByFamily("Noto Sans Symbols", (int)fontSize);
  Vector2 size = v2::MeasureTextWithEmoji(font, safeText, fontSize, letterSpacing);
  float x = bounds.x;
  if (alignment == TextAlignment::Center)
    x += (bounds.width - size.x) / 2.0f;
  else if (alignment == TextAlignment::Right)
    x += bounds.width - size.x;
  const auto metrics = registeredFamily
      ? FontManager::MetricsForFamily(faceFamily)
      : FontManager::MetricsFor(fallbackWeight, FontStyle::Normal);
  const float ascent = fontSize * metrics.ascent;
  const float lineBox = fontSize * lineHeight;
  const float y = bounds.y + std::max(0.0f, (bounds.height - lineBox) / 2.0f)
      + (lineBox - ascent) / 2.0f;
  v2::DrawTextWithEmoji(font, safeText, {x, y}, fontSize, letterSpacing, color);
}

Vector2 Renderer::MeasureTextFamily(const char *text, float fontSize,
                                    const char *family, int fontWeight,
                                    float letterSpacing) {
  const std::string_view safeText = text ? text : "";
  const std::string safeFamily = family ? family : "";
  const FontWeight fallbackWeight = CssFontWeight(fontWeight);
  std::string faceFamily = ResolveTextFamily(safeFamily, fontWeight);
  const bool registeredFamily = !faceFamily.empty();
  if (faceFamily == "Outfit" || faceFamily == "Fraunces") {
    const int weight = std::clamp(fontWeight, 100, 900);
    int faceWeight = 400;
    if (weight >= 750) faceWeight = 800;
    else if (weight >= 650) faceWeight = 700;
    else if (weight >= 550) faceWeight = 600;
    if (faceFamily == "Fraunces" && faceWeight == 400) faceWeight = 600;
    const std::string candidate = faceFamily + "-" + std::to_string(faceWeight);
    if (FontManager::HasFont(candidate)) faceFamily = candidate;
  }
  if (family && std::string_view(family).find("Symbols") != std::string_view::npos) {
    FontManager::EnsureGlyphsForFamily("Noto Sans Symbols", (int)fontSize,
                                       safeText);
    faceFamily = "Noto Sans Symbols";
  }
  if (registeredFamily || faceFamily == "Noto Sans Symbols") {
    FontManager::EnsureGlyphsForFamily(faceFamily, (int)fontSize, safeText);
  } else {
    FontManager::EnsureGlyphsForText(fallbackWeight, FontStyle::Normal,
                                     (int)fontSize, safeText);
  }
  Font font = registeredFamily || faceFamily == "Noto Sans Symbols"
      ? FontManager::LoadFontByFamily(faceFamily, (int)fontSize)
      : FontManager::LoadFont(fallbackWeight, FontStyle::Normal, (int)fontSize);
  return v2::MeasureTextWithEmoji(font, safeText, fontSize, letterSpacing);
}

Vector2 Renderer::MeasureText(const char *text, float fontSize,
                              FontWeight weight) {
  Font font = Theme::GetFont(fontSize, weight);
  return v2::MeasureTextWithEmoji(font, text ? text : "", fontSize, 0);
}

} // namespace raym3
