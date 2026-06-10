#include "raym3/components/AppBar.h"
#include "raym3/components/IconButton.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>

namespace raym3 {

static float DefaultHeight(AppBarVariant variant) {
  switch (variant) {
  case AppBarVariant::Medium:
    return 112.0f;
  case AppBarVariant::Large:
    return 152.0f;
  case AppBarVariant::CenterAligned:
  case AppBarVariant::Small:
  default:
    return 64.0f;
  }
}

int AppBarComponent::Render(Rectangle bounds, const char *title,
                            const AppBarOptions &options) {
  ColorScheme &scheme = Theme::GetColorScheme();
  float preferredHeight = DefaultHeight(options.variant);
  if (bounds.height <= 0.0f)
    bounds.height = preferredHeight;

  Color bg = options.elevated ? scheme.surfaceContainer : scheme.surface;
  if (options.elevated)
    Renderer::DrawElevatedRectangle(bounds, 0.0f, 2, bg);
  else
    DrawRectangleRec(bounds, bg);

  const float touchSize = 48.0f;
  const float topRowY = bounds.y + std::max(0.0f, (64.0f - touchSize) / 2.0f);
  int clicked = -1;

  float leadingSpace = 16.0f;
  if (options.navigationIcon) {
    IconButtonOptions iconOptions;
    if (IconButtonComponent::Render(
            options.navigationIcon, {bounds.x + 4.0f, topRowY, touchSize, touchSize},
            ButtonVariant::Text, IconVariation::Filled, BLANK, &iconOptions)) {
      clicked = -2;
    }
    leadingSpace = 72.0f;
  }

  float trailingX = bounds.x + bounds.width - 4.0f - touchSize;
  for (int i = options.actionCount - 1; i >= 0; --i) {
    IconButtonOptions iconOptions;
    iconOptions.tooltip = options.actions ? options.actions[i].tooltip : nullptr;
    if (options.actions && options.actions[i].icon &&
        IconButtonComponent::Render(options.actions[i].icon,
                                    {trailingX, topRowY, touchSize, touchSize},
                                    ButtonVariant::Text, IconVariation::Filled,
                                    BLANK, &iconOptions)) {
      clicked = i;
    }
    trailingX -= touchSize;
  }

  float titleFontSize = Theme::GetTypographyScale().titleLarge;
  FontWeight titleWeight = FontWeight::Regular;
  float titleLineHeight = 28.0f;
  float titleY = bounds.y + (64.0f - titleLineHeight) / 2.0f;
  if (options.variant == AppBarVariant::Medium) {
    titleFontSize = Theme::GetTypographyScale().headlineSmall;
    titleLineHeight = 32.0f;
    titleY = bounds.y + bounds.height - 52.0f;
  } else if (options.variant == AppBarVariant::Large) {
    titleFontSize = Theme::GetTypographyScale().headlineMedium;
    titleLineHeight = 36.0f;
    titleY = bounds.y + bounds.height - 64.0f;
  }

  const char *safeTitle = title ? title : "";
  Font titleFont = Theme::GetFont(titleFontSize, titleWeight);
  Vector2 titleSize = MeasureTextEx(titleFont, safeTitle, titleFontSize, 0.0f);

  float titleX = bounds.x + leadingSpace;
  if (options.variant == AppBarVariant::CenterAligned) {
    titleX = bounds.x + (bounds.width - titleSize.x) / 2.0f;
    titleX = std::max(bounds.x + leadingSpace,
                      std::min(titleX, trailingX - titleSize.x));
  }

  DrawTextEx(titleFont, safeTitle, {titleX, titleY}, titleFontSize, 0.0f,
             scheme.onSurface);

  if (options.subtitle) {
    float subtitleSize = Theme::GetTypographyScale().titleMedium;
    Font subtitleFont = Theme::GetFont(subtitleSize, FontWeight::Regular);
    DrawTextEx(subtitleFont, options.subtitle, {titleX, titleY + titleLineHeight},
               subtitleSize, 0.0f, scheme.onSurfaceVariant);
  }

  return clicked;
}

} // namespace raym3
