#pragma once

#include "raym3/styles/ColorScheme.h"
#include "raym3/types.h"
#include "raym3/fonts/FontManager.h"
#include <raylib.h>

namespace raym3 {

enum class ColorSchemePreference { System, Light, Dark };

class Theme {
public:
    static void Initialize();
    static void Shutdown();

    static void SetColorSchemePreference(ColorSchemePreference preference);
    static ColorSchemePreference GetColorSchemePreference();

    static void SetDarkMode(bool darkMode);
    static bool IsDarkMode();
    static void ApplyEffectiveColorScheme();
    
    static ColorScheme& GetColorScheme();
    static TypographyScale& GetTypographyScale();
    static ShapeTokens& GetShapeTokens();
    
    static Color GetColor(const char* role);
    static Color GetStateLayerColor(Color baseColor, ComponentState state);
    
    static Font GetFont(float size, FontWeight weight = FontWeight::Regular, FontStyle style = FontStyle::Normal);
    
    static float GetElevationShadow(int elevation);
    static Color GetElevationColor(int elevation);
    
private:
    static ColorScheme colorScheme_;
    static TypographyScale typographyScale_;
    static ShapeTokens shapeTokens_;
    static bool darkMode_;
    static ColorSchemePreference preference_;
    static bool initialized_;
    
    static void InitializeTypographyScale();
    static void InitializeShapeTokens();
};

} // namespace raym3

