#include "raym3/components/Navigation.h"
#include "raym3/components/Badge.h"
#include "raym3/components/Icon.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static ComponentState NavigationItemState(Rectangle bounds, bool disabled) {
  if (disabled)
    return ComponentState::Disabled;
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  bool hovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool hovered = CheckCollisionPointRec(mousePos, bounds);
#endif
  if (hovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    return ComponentState::Pressed;
  return hovered ? ComponentState::Hovered : ComponentState::Default;
}

static bool NavigationItemClicked(Rectangle bounds, bool disabled) {
  if (disabled)
    return false;
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  return canProcessInput && CheckCollisionPointRec(mousePos, bounds) &&
         IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  return CheckCollisionPointRec(mousePos, bounds) &&
         IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
}

bool NavigationComponent::Bar(Rectangle bounds, const NavigationItem *items,
                              int itemCount, int *selectedIndex,
                              const NavigationOptions &options) {
  if (!items || itemCount <= 0 || !selectedIndex)
    return false;

  ColorScheme &scheme = Theme::GetColorScheme();
  Renderer::DrawElevatedRectangle(bounds, 0.0f, 2, scheme.surfaceContainer);

  const float itemWidth = bounds.width / static_cast<float>(itemCount);
  const float indicatorW = 64.0f;
  const float indicatorH = 32.0f;
  const float iconSize = 24.0f;
  bool changed = false;

  for (int i = 0; i < itemCount; ++i) {
    Rectangle itemBounds = {bounds.x + itemWidth * i, bounds.y, itemWidth,
                            bounds.height};
    const bool selected = *selectedIndex == i;
    ComponentState state = NavigationItemState(itemBounds, items[i].disabled);
    Rectangle indicator = {itemBounds.x + (itemBounds.width - indicatorW) / 2.0f,
                           itemBounds.y + 12.0f, indicatorW, indicatorH};

    Color iconColor =
        selected ? scheme.onSecondaryContainer : scheme.onSurfaceVariant;
    Color labelColor = selected ? scheme.onSurface : scheme.onSurfaceVariant;
    if (items[i].disabled) {
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
      labelColor = ColorAlpha(scheme.onSurface, 0.38f);
    }

    if (selected) {
      Renderer::DrawRoundedRectangle(indicator, indicatorH / 2.0f,
                                     scheme.secondaryContainer);
      Renderer::DrawStateLayer(indicator, indicatorH / 2.0f, scheme.onSurface,
                               state);
    } else if (state != ComponentState::Default) {
      Renderer::DrawStateLayer(indicator, indicatorH / 2.0f, scheme.onSurface,
                               state);
    }

    Rectangle iconRect = {itemBounds.x + (itemBounds.width - iconSize) / 2.0f,
                          indicator.y + (indicator.height - iconSize) / 2.0f,
                          iconSize, iconSize};
    const char *iconName =
        selected && items[i].selectedIcon ? items[i].selectedIcon : items[i].icon;
    if (iconName) {
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            iconColor);
    }
    if (items[i].showBadge || (items[i].badgeLabel && items[i].badgeLabel[0])) {
      BadgeOptions badgeOptions;
      badgeOptions.horizontalOffset = -2.0f;
      badgeOptions.verticalOffset = -2.0f;
      BadgeComponent::Render(iconRect, items[i].badgeLabel, badgeOptions);
    }

    if (options.showLabels && items[i].label) {
      Font font = Theme::GetFont(Theme::GetTypographyScale().labelMedium,
                                 selected ? FontWeight::Bold
                                          : FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, items[i].label,
                                       Theme::GetTypographyScale().labelMedium,
                                       0.0f);
      DrawTextEx(font, items[i].label,
                 {itemBounds.x + (itemBounds.width - textSize.x) / 2.0f,
                  itemBounds.y + 52.0f},
                 Theme::GetTypographyScale().labelMedium, 0.0f, labelColor);
    }

    if (NavigationItemClicked(itemBounds, items[i].disabled)) {
      *selectedIndex = i;
      changed = true;
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
    }
    if (!items[i].disabled && CheckCollisionPointRec(GetMousePosition(), itemBounds))
      RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }

  return changed;
}

bool NavigationComponent::Rail(Rectangle bounds, const NavigationItem *items,
                               int itemCount, int *selectedIndex,
                               const NavigationOptions &options) {
  if (!items || itemCount <= 0 || !selectedIndex)
    return false;

  ColorScheme &scheme = Theme::GetColorScheme();
  int elevation = options.expressiveRail ? 3 : 0;
  Renderer::DrawElevatedRectangle(bounds, 0.0f, elevation, scheme.surface);

  const float itemHeight = options.expressiveRail ? 64.0f : 60.0f;
  const float indicatorW = options.expressiveRail ? 64.0f : 56.0f;
  const float indicatorH = 32.0f;
  const float iconSize = 24.0f;
  float y = bounds.y + (options.expressiveRail ? 44.0f : 8.0f);

  if (options.headerIcon) {
    Rectangle header = {bounds.x + (bounds.width - 48.0f) / 2.0f, y, 48.0f,
                        48.0f};
    IconComponent::Render(options.headerIcon,
                          {header.x + 12.0f, header.y + 12.0f, 24.0f, 24.0f},
                          IconVariation::Filled, scheme.onSurfaceVariant);
    y += header.height + (options.expressiveRail ? 40.0f : 8.0f);
  }

  bool changed = false;
  for (int i = 0; i < itemCount; ++i) {
    Rectangle itemBounds = {bounds.x, y, bounds.width, itemHeight};
    const bool selected = *selectedIndex == i;
    ComponentState state = NavigationItemState(itemBounds, items[i].disabled);

    Color iconColor =
        selected ? scheme.onSecondaryContainer : scheme.onSurfaceVariant;
    Color labelColor = selected ? scheme.onSurface : scheme.onSurfaceVariant;
    if (items[i].disabled) {
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
      labelColor = ColorAlpha(scheme.onSurface, 0.38f);
    }

    Rectangle indicator = {bounds.x + (bounds.width - indicatorW) / 2.0f,
                           itemBounds.y + 4.0f, indicatorW, indicatorH};
    if (selected) {
      Renderer::DrawRoundedRectangle(indicator, indicatorH / 2.0f,
                                     scheme.secondaryContainer);
      Renderer::DrawStateLayer(indicator, indicatorH / 2.0f, scheme.onSurface,
                               state);
    } else if (state != ComponentState::Default) {
      Renderer::DrawStateLayer(indicator, indicatorH / 2.0f, scheme.onSurface,
                               state);
    }

    Rectangle iconRect = {bounds.x + (bounds.width - iconSize) / 2.0f,
                          indicator.y + (indicator.height - iconSize) / 2.0f,
                          iconSize, iconSize};
    const char *iconName =
        selected && items[i].selectedIcon ? items[i].selectedIcon : items[i].icon;
    if (iconName) {
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            iconColor);
    }
    if (items[i].showBadge || (items[i].badgeLabel && items[i].badgeLabel[0])) {
      BadgeOptions badgeOptions;
      badgeOptions.horizontalOffset = -2.0f;
      badgeOptions.verticalOffset = -2.0f;
      BadgeComponent::Render(iconRect, items[i].badgeLabel, badgeOptions);
    }

    if (options.showLabels && items[i].label) {
      Font font = Theme::GetFont(Theme::GetTypographyScale().labelMedium,
                                 FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, items[i].label,
                                       Theme::GetTypographyScale().labelMedium,
                                       0.0f);
      DrawTextEx(font, items[i].label,
                 {itemBounds.x + (itemBounds.width - textSize.x) / 2.0f,
                  itemBounds.y + 40.0f},
                 Theme::GetTypographyScale().labelMedium, 0.0f, labelColor);
    }

    if (NavigationItemClicked(itemBounds, items[i].disabled)) {
      *selectedIndex = i;
      changed = true;
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
    }
    if (!items[i].disabled && CheckCollisionPointRec(GetMousePosition(), itemBounds))
      RequestCursor(MOUSE_CURSOR_POINTING_HAND);

    y += itemHeight + (options.expressiveRail ? 4.0f : 0.0f);
  }

  return changed;
}

} // namespace raym3
