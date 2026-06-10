#include "raym3/components/FloatingActionButton.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedFabId_ = -1;
static int currentFabId_ = 0;

struct FabMetrics {
  float height;
  float radius;
  float iconSize;
  float labelSize;
};

static FabMetrics MetricsFor(FabSize size) {
  switch (size) {
  case FabSize::Medium:
    return {80.0f, 20.0f, 28.0f, 22.0f};
  case FabSize::Large:
    return {96.0f, 28.0f, 36.0f, 24.0f};
  case FabSize::Regular:
  default:
    return {56.0f, 16.0f, 24.0f, 14.0f};
  }
}

static void ColorsFor(FabColor color, Color &container, Color &content) {
  ColorScheme &scheme = Theme::GetColorScheme();
  switch (color) {
  case FabColor::Primary:
    container = scheme.primary;
    content = scheme.onPrimary;
    break;
  case FabColor::SecondaryContainer:
    container = scheme.secondaryContainer;
    content = scheme.onSecondaryContainer;
    break;
  case FabColor::Secondary:
    container = scheme.secondary;
    content = scheme.onSecondary;
    break;
  case FabColor::TertiaryContainer:
    container = scheme.tertiaryContainer;
    content = scheme.onTertiaryContainer;
    break;
  case FabColor::Tertiary:
    container = scheme.tertiary;
    content = scheme.onTertiary;
    break;
  case FabColor::Surface:
    container = scheme.surfaceContainerHigh;
    content = scheme.primary;
    break;
  case FabColor::PrimaryContainer:
  default:
    container = scheme.primaryContainer;
    content = scheme.onPrimaryContainer;
    break;
  }
}

static Rectangle VisualBounds(Rectangle bounds, float preferredHeight,
                              bool extended) {
  float height = std::min(bounds.height, preferredHeight);
  if (height <= 0.0f)
    height = preferredHeight;
  float width = extended ? bounds.width : std::min(bounds.width, height);
  if (width <= 0.0f)
    width = extended ? std::max(80.0f, preferredHeight * 1.8f) : height;
  return {bounds.x + (bounds.width - width) / 2.0f,
          bounds.y + (bounds.height - height) / 2.0f, width, height};
}

static ComponentState GetFabState(Rectangle bounds, bool disabled) {
  if (disabled)
    return ComponentState::Disabled;
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  if (layerId >= 100)
    isVisible = true;
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  if (isPressed)
    return ComponentState::Pressed;
  if (isHovered)
    return ComponentState::Hovered;
  return ComponentState::Default;
}

void FloatingActionButtonComponent::ResetIds() { currentFabId_ = 0; }

bool FloatingActionButtonComponent::Render(const char *iconName, Rectangle bounds,
                                           const FabOptions &options) {
  const FabMetrics metrics = MetricsFor(options.size);
  Rectangle container = VisualBounds(bounds, metrics.height, false);

  Color containerColor;
  Color contentColor;
  ColorsFor(options.color, containerColor, contentColor);
  if (options.disabled) {
    ColorScheme &scheme = Theme::GetColorScheme();
    containerColor = ColorAlpha(scheme.onSurface, 0.12f);
    contentColor = ColorAlpha(scheme.onSurface, 0.38f);
  }

  const bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  ComponentState state =
      inputBlocked ? ComponentState::Default
                   : GetFabState(bounds, options.disabled);
  int elevation = options.lowered ? 1 : 3;
  if (state == ComponentState::Hovered)
    elevation = options.lowered ? 2 : 4;
  if (state == ComponentState::Pressed)
    elevation = options.lowered ? 1 : 3;
  if (options.disabled)
    elevation = 0;

  Renderer::DrawElevatedRectangle(container, metrics.radius, elevation,
                                  containerColor);
  Renderer::DrawStateLayer(container, metrics.radius, contentColor, state);

  if (iconName) {
    Rectangle iconRect = {container.x + (container.width - metrics.iconSize) / 2.0f,
                          container.y + (container.height - metrics.iconSize) / 2.0f,
                          metrics.iconSize, metrics.iconSize};
    IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                          contentColor);
  }

  int thisId = currentFabId_++;
  bool isFocused = focusedFabId_ == thisId;
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  if (layerId >= 100)
    isVisible = true;
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif
  bool wasClicked = !options.disabled && !inputBlocked && isHovered &&
                    IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedFabId_ = thisId;
    isFocused = true;
  }
  if (isFocused && !options.disabled &&
      (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    wasClicked = true;
  }
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      !CheckCollisionPointRec(mousePos, bounds)) {
    focusedFabId_ = -1;
  }
  if (isHovered && !options.disabled && !inputBlocked)
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);

#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || wasClicked)
    InputLayerManager::ConsumeInput();
#endif

  if (options.tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options.tooltipPlacement;
    Tooltip(bounds, options.tooltip, tooltipOpts);
  }

  return wasClicked;
}

bool FloatingActionButtonComponent::RenderExtended(
    const char *label, const char *iconName, Rectangle bounds,
    const FabOptions &options) {
  const FabMetrics metrics = MetricsFor(options.size);
  Rectangle container = VisualBounds(bounds, metrics.height, true);

  Color containerColor;
  Color contentColor;
  ColorsFor(options.color, containerColor, contentColor);
  if (options.disabled) {
    ColorScheme &scheme = Theme::GetColorScheme();
    containerColor = ColorAlpha(scheme.onSurface, 0.12f);
    contentColor = ColorAlpha(scheme.onSurface, 0.38f);
  }

  const bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  ComponentState state =
      inputBlocked ? ComponentState::Default
                   : GetFabState(bounds, options.disabled);
  int elevation = options.lowered ? 1 : 3;
  if (state == ComponentState::Hovered)
    elevation = options.lowered ? 2 : 4;
  if (state == ComponentState::Pressed)
    elevation = options.lowered ? 1 : 3;
  if (options.disabled)
    elevation = 0;

  Renderer::DrawElevatedRectangle(container, metrics.radius, elevation,
                                  containerColor);
  Renderer::DrawStateLayer(container, metrics.radius, contentColor, state);

  Font font = Theme::GetFont(metrics.labelSize, FontWeight::Medium);
  Vector2 labelSize = MeasureTextEx(font, label ? label : "", metrics.labelSize,
                                    0.0f);
  const float iconGap = iconName ? 12.0f : 0.0f;
  const float totalWidth = (iconName ? metrics.iconSize : 0.0f) + iconGap +
                           labelSize.x;
  float x = container.x + (container.width - totalWidth) / 2.0f;
  float centerY = container.y + container.height / 2.0f;

  if (iconName) {
    Rectangle iconRect = {x, centerY - metrics.iconSize / 2.0f,
                          metrics.iconSize, metrics.iconSize};
    IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                          contentColor);
    x += metrics.iconSize + iconGap;
  }
  DrawTextEx(font, label ? label : "", {x, centerY - labelSize.y / 2.0f},
             metrics.labelSize, 0.0f, contentColor);

  int thisId = currentFabId_++;
  bool isFocused = focusedFabId_ == thisId;
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  if (layerId >= 100)
    isVisible = true;
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif
  bool wasClicked = !options.disabled && !inputBlocked && isHovered &&
                    IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedFabId_ = thisId;
    isFocused = true;
  }
  if (isFocused && !options.disabled &&
      (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    wasClicked = true;
  }
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      !CheckCollisionPointRec(mousePos, bounds)) {
    focusedFabId_ = -1;
  }
  if (isHovered && !options.disabled && !inputBlocked)
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);

#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || wasClicked)
    InputLayerManager::ConsumeInput();
#endif

  if (options.tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options.tooltipPlacement;
    Tooltip(bounds, options.tooltip, tooltipOpts);
  }

  return wasClicked;
}

} // namespace raym3
