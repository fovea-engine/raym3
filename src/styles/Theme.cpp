#include "raym3/styles/Theme.h"
#include "raym3/platform/SystemAppearance.h"

namespace raym3 {

ColorScheme Theme::colorScheme_ = ColorScheme::Light();
TypographyScale Theme::typographyScale_ = {
    57.0f, 45.0f, 36.0f, 32.0f, 28.0f, 24.0f, 22.0f,
    16.0f, 14.0f, 14.0f, 12.0f, 11.0f, 16.0f, 14.0f, 12.0f};
ShapeTokens Theme::shapeTokens_ = {0.0f, 4.0f, 8.0f, 12.0f,
                                   16.0f, 28.0f, 9999.0f};
bool Theme::darkMode_ = false;
ColorSchemePreference Theme::preference_ = ColorSchemePreference::System;
bool Theme::initialized_ = false;

void Theme::ApplyEffectiveColorScheme() {
  bool isDark = false;
  switch (preference_) {
  case ColorSchemePreference::System:
    isDark = SystemAppearance::IsDarkMode();
    break;
  case ColorSchemePreference::Light:
    isDark = false;
    break;
  case ColorSchemePreference::Dark:
    isDark = true;
    break;
  }
  darkMode_ = isDark;
  colorScheme_ = isDark ? ColorScheme::Dark() : ColorScheme::Light();
}

void Theme::Initialize() {
  if (initialized_)
    return;

  FontManager::Initialize();
  InitializeTypographyScale();
  InitializeShapeTokens();
  preference_ = ColorSchemePreference::System;
  ApplyEffectiveColorScheme();

  initialized_ = true;
}

void Theme::Shutdown() {
  if (!initialized_)
    return;

  FontManager::Shutdown();
  initialized_ = false;
}

void Theme::SetColorSchemePreference(ColorSchemePreference preference) {
  preference_ = preference;
  ApplyEffectiveColorScheme();
}

ColorSchemePreference Theme::GetColorSchemePreference() { return preference_; }

void Theme::SetDarkMode(bool isDarkMode) {
  SetColorSchemePreference(isDarkMode ? ColorSchemePreference::Dark
                                       : ColorSchemePreference::Light);
}

bool Theme::IsDarkMode() { return darkMode_; }

ColorScheme &Theme::GetColorScheme() { return colorScheme_; }

TypographyScale &Theme::GetTypographyScale() { return typographyScale_; }

ShapeTokens &Theme::GetShapeTokens() { return shapeTokens_; }

Color Theme::GetColor(const char *role) {
  ColorScheme &scheme = GetColorScheme();

  if (strcmp(role, "primary") == 0)
    return scheme.primary;
  if (strcmp(role, "onPrimary") == 0)
    return scheme.onPrimary;
  if (strcmp(role, "primaryContainer") == 0)
    return scheme.primaryContainer;
  if (strcmp(role, "onPrimaryContainer") == 0)
    return scheme.onPrimaryContainer;
  if (strcmp(role, "secondary") == 0)
    return scheme.secondary;
  if (strcmp(role, "onSecondary") == 0)
    return scheme.onSecondary;
  if (strcmp(role, "secondaryContainer") == 0)
    return scheme.secondaryContainer;
  if (strcmp(role, "onSecondaryContainer") == 0)
    return scheme.onSecondaryContainer;
  if (strcmp(role, "tertiary") == 0)
    return scheme.tertiary;
  if (strcmp(role, "onTertiary") == 0)
    return scheme.onTertiary;
  if (strcmp(role, "tertiaryContainer") == 0)
    return scheme.tertiaryContainer;
  if (strcmp(role, "onTertiaryContainer") == 0)
    return scheme.onTertiaryContainer;
  if (strcmp(role, "error") == 0)
    return scheme.error;
  if (strcmp(role, "onError") == 0)
    return scheme.onError;
  if (strcmp(role, "errorContainer") == 0)
    return scheme.errorContainer;
  if (strcmp(role, "onErrorContainer") == 0)
    return scheme.onErrorContainer;
  if (strcmp(role, "surface") == 0)
    return scheme.surface;
  if (strcmp(role, "onSurface") == 0)
    return scheme.onSurface;
  if (strcmp(role, "surfaceVariant") == 0)
    return scheme.surfaceVariant;
  if (strcmp(role, "onSurfaceVariant") == 0)
    return scheme.onSurfaceVariant;
  if (strcmp(role, "outline") == 0)
    return scheme.outline;
  if (strcmp(role, "outlineVariant") == 0)
    return scheme.outlineVariant;

  return scheme.surface;
}

Color Theme::GetStateLayerColor(Color baseColor, ComponentState state) {
  float opacity = 0.0f;

  switch (state) {
  case ComponentState::Hovered:
    opacity = 0.08f;
    break;
  case ComponentState::Pressed:
    opacity = 0.12f;
    break;
  case ComponentState::Focused:
    opacity = 0.12f;
    break;
  case ComponentState::Disabled:
    opacity = 0.38f;
    break;
  default:
    return ColorAlpha(baseColor,
                      0.0f); // Default state has no state layer (transparent)
  }

  // MD3: State layer is an overlay using the content color with specific
  // opacity. It should NOT be blended with white/black to make it opaque. It
  // should be transparent.
  return ColorAlpha(baseColor, opacity);
}

Font Theme::GetFont(float size, FontWeight weight, FontStyle style) {
  return FontManager::LoadFont(weight, style, static_cast<int>(size));
}

float Theme::GetElevationShadow(int elevation) {
  const float shadows[] = {0.0f, 1.0f, 3.0f, 6.0f, 8.0f, 12.0f};
  if (elevation >= 0 && elevation < 6) {
    return shadows[elevation];
  }
  return 0.0f;
}

Color Theme::GetElevationColor(int elevation) {
  const float opacities[] = {0.0f, 0.05f, 0.08f, 0.11f, 0.12f, 0.14f};
  if (elevation >= 0 && elevation < 6) {
    unsigned char alpha =
        static_cast<unsigned char>(opacities[elevation] * 255.0f);
    return {0, 0, 0, alpha};
  }
  return {0, 0, 0, 0};
}

void Theme::InitializeTypographyScale() {
  typographyScale_.displayLarge = 57.0f;
  typographyScale_.displayMedium = 45.0f;
  typographyScale_.displaySmall = 36.0f;
  typographyScale_.headlineLarge = 32.0f;
  typographyScale_.headlineMedium = 28.0f;
  typographyScale_.headlineSmall = 24.0f;
  typographyScale_.titleLarge = 22.0f;
  typographyScale_.titleMedium = 16.0f;
  typographyScale_.titleSmall = 14.0f;
  typographyScale_.labelLarge = 14.0f;
  typographyScale_.labelMedium = 12.0f;
  typographyScale_.labelSmall = 11.0f;
  typographyScale_.bodyLarge = 16.0f;
  typographyScale_.bodyMedium = 14.0f;
  typographyScale_.bodySmall = 12.0f;
}

void Theme::InitializeShapeTokens() {
  shapeTokens_.cornerNone = 0.0f;
  shapeTokens_.cornerExtraSmall = 4.0f;
  shapeTokens_.cornerSmall = 8.0f;
  shapeTokens_.cornerMedium = 12.0f;
  shapeTokens_.cornerLarge = 16.0f;
  shapeTokens_.cornerExtraLarge = 28.0f;
  shapeTokens_.cornerFull = 9999.0f;
}

} // namespace raym3
