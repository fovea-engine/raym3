#pragma once

#include <raylib.h>
#include "raym3/types.h"
#include "raym3/v2/TextEngine.h"
#include "raym3/styles/Theme.h"

namespace raym3 {

class Renderer {
public:
    static void DrawRoundedRectangle(Rectangle bounds, float cornerRadius, Color color);
    static void DrawRoundedRectangleEx(Rectangle bounds, float cornerRadius, Color color, float lineWidth);
    // Rounded border frame with a gap cut from the top edge between
    // notchStart..notchEnd (screen x-coords) — the M3 outlined text field
    // label notch. notchEnd <= notchStart draws an uninterrupted frame.
    static void DrawRoundedRectangleNotched(Rectangle bounds, float cornerRadius, Color color, float lineWidth, float notchStart, float notchEnd);
    static void DrawElevatedRectangle(Rectangle bounds, float cornerRadius, int elevation, Color color);
    static void DrawStateLayer(Rectangle bounds, float cornerRadius, Color baseColor, ComponentState state);
    // Paint one resolved CSS box-shadow layer (outer or inset). The host owns
    // CSS parsing; RayM3 owns the rounded, alpha-composited shadow geometry.
    static void DrawCssBoxShadow(Rectangle bounds, float cornerRadius,
                                 float offsetX, float offsetY, float blurRadius,
                                 float spreadRadius, Color color, bool inset,
                                 float opacity = 1.0f);
    static void DrawText(const char* text, Vector2 position, float fontSize, Color color, FontWeight weight = FontWeight::Regular);
    static void DrawTextCentered(const char* text, Rectangle bounds, float fontSize, Color color, FontWeight weight = FontWeight::Regular);
    // Draw a named font family with CSS-like spacing and alignment. The family
    // must be registered with FontManager before drawing.
    static void DrawTextFamily(const char* text, Rectangle bounds, float fontSize,
                               Color color, const char* family,
                               int fontWeight,
                               float letterSpacing, TextAlignment alignment,
                               float lineHeight = 1.0f);
    static Vector2 MeasureTextFamily(const char* text, float fontSize,
                                     const char* family, int fontWeight,
                                     float letterSpacing = 0.0f);
    
    static Vector2 MeasureText(const char* text, float fontSize, FontWeight weight = FontWeight::Regular);
    
private:
    static void DrawShadow(Rectangle bounds, float cornerRadius, int elevation);
};

} // namespace raym3

