#include "raym3/rendering/Renderer.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/EmojiFont.h"
#include <cmath>

namespace raym3 {
namespace {
constexpr int kRoundedRectSegments = 16;

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

  // Optical vertical centering. raylib anchors text at the ascender line and the
  // measured height (textSize.y == fontSize) spans ascent+descent, so centering
  // the full box leaves the descender's blank space skewing the visible glyphs
  // off-centre (text looks bottom-heavy in fixed-height chrome like buttons).
  // Centre the ascent box instead — lift by half the descender — so the cap/x
  // height band sits on the true centre, matching web/RN button text.
  // Roboto (and the M3 label fonts): ascent/(ascent-descent) ≈ 0.79, i.e. the
  // descender is ~0.21·fontSize; shifting up by descent/2 centres the caps.
  const float kAscentFraction = 0.79f;
  const float ascent = fontSize * kAscentFraction;
  Vector2 position = {bounds.x + (bounds.width - textSize.x) / 2.0f,
                      bounds.y + (bounds.height - ascent) / 2.0f};

  v2::DrawTextWithEmoji(font, text ? text : "", position, fontSize, 0, color);
}

Vector2 Renderer::MeasureText(const char *text, float fontSize,
                              FontWeight weight) {
  Font font = Theme::GetFont(fontSize, weight);
  return v2::MeasureTextWithEmoji(font, text ? text : "", fontSize, 0);
}

} // namespace raym3
