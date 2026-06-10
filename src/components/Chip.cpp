#include "raym3/components/Chip.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cstring>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedChipId_ = -1;
static int currentChipId_ = 0;

static Color DisabledColor(Color color, float alpha) {
  return ColorAlpha(color, alpha);
}

static ComponentState GetChipState(Rectangle bounds, bool disabled) {
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

void ChipComponent::ResetIds() { currentChipId_ = 0; }

bool ChipComponent::Render(const char *label, Rectangle bounds,
                           const ChipOptions &options) {
  return Render(label, bounds, nullptr, options);
}

bool ChipComponent::Render(const char *label, Rectangle bounds, bool *selected,
                           const ChipOptions &options) {
  if (!label)
    label = "";

  ChipOptions resolved = options;
  if (selected)
    resolved.selected = *selected;

  const bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  ComponentState state =
      inputBlocked ? ComponentState::Default
                   : GetChipState(bounds, resolved.disabled);

  ColorScheme &scheme = Theme::GetColorScheme();
  const bool isSelectable = resolved.variant == ChipVariant::Filter ||
                            resolved.variant == ChipVariant::Input ||
                            selected != nullptr;
  const bool isSelected = resolved.selected && isSelectable;
  const float availableHeight = bounds.height > 0.0f ? bounds.height : 48.0f;
  const float containerHeight = std::min(32.0f, availableHeight);
  Rectangle container = {bounds.x, bounds.y + (availableHeight - containerHeight) / 2.0f,
                         bounds.width, containerHeight};

  Color containerColor = ColorAlpha(scheme.surface, 0.0f);
  Color outlineColor = scheme.outline;
  float outlineWidth = 1.0f;
  int elevation = 0;
  if (resolved.elevated) {
    containerColor = scheme.surfaceContainerLow;
    elevation = state == ComponentState::Hovered ? 2 : 1;
  }
  if (isSelected) {
    containerColor = scheme.secondaryContainer;
    outlineWidth = 0.0f;
  }

  Color contentColor =
      isSelected ? scheme.onSecondaryContainer : scheme.onSurfaceVariant;
  Color iconColor = isSelected ? scheme.onSecondaryContainer : scheme.primary;
  Color trailingIconColor =
      isSelected ? scheme.onSecondaryContainer : scheme.onSurfaceVariant;

  if (resolved.disabled) {
    contentColor = DisabledColor(scheme.onSurface, 0.38f);
    iconColor = DisabledColor(scheme.onSurface, 0.38f);
    trailingIconColor = DisabledColor(scheme.onSurface, 0.38f);
    outlineColor = DisabledColor(scheme.onSurface, 0.12f);
    containerColor = resolved.elevated
                         ? DisabledColor(scheme.onSurface, 0.12f)
                         : ColorAlpha(scheme.surface, 0.0f);
    elevation = 0;
  }

  const float radius = Theme::GetShapeTokens().cornerSmall;
  if (containerColor.a > 0) {
    if (elevation > 0)
      Renderer::DrawElevatedRectangle(container, radius, elevation,
                                      containerColor);
    else
      Renderer::DrawRoundedRectangle(container, radius, containerColor);
  }

  if (outlineWidth > 0.0f) {
    Renderer::DrawRoundedRectangleEx(container, radius, outlineColor,
                                     outlineWidth);
  }

  Color stateLayerBase = isSelected ? scheme.onSecondaryContainer
                                    : scheme.onSurfaceVariant;
  Renderer::DrawStateLayer(container, radius, stateLayerBase, state);

  float x = container.x + 16.0f;
  const float centerY = container.y + container.height / 2.0f;
  const float iconSize = 18.0f;

  const char *leadingIcon = resolved.leadingIcon;
  if (isSelected && resolved.showSelectedIcon &&
      (resolved.variant == ChipVariant::Filter ||
       resolved.variant == ChipVariant::Input)) {
    leadingIcon = "check";
  }

  if (leadingIcon) {
    Rectangle iconRect = {x - (resolved.leadingIcon ? 8.0f : 0.0f),
                          centerY - iconSize / 2.0f, iconSize, iconSize};
    IconComponent::Render(leadingIcon, iconRect, IconVariation::Filled,
                          isSelected ? contentColor : iconColor);
    x = iconRect.x + iconSize + 8.0f;
  }

  Font font = Theme::GetFont(Theme::GetTypographyScale().labelLarge,
                             FontWeight::Medium);
  Vector2 labelSize =
      MeasureTextEx(font, label, Theme::GetTypographyScale().labelLarge, 0.0f);
  DrawTextEx(font, label, {x, centerY - labelSize.y / 2.0f},
             Theme::GetTypographyScale().labelLarge, 0.0f, contentColor);

  const bool drawCloseIcon = resolved.showCloseIcon ||
                             resolved.variant == ChipVariant::Input;
  const char *trailingIcon = drawCloseIcon ? "close" : resolved.trailingIcon;
  if (trailingIcon) {
    Rectangle trailingRect = {container.x + container.width - 16.0f - iconSize,
                              centerY - iconSize / 2.0f, iconSize, iconSize};
    IconComponent::Render(trailingIcon, trailingRect, IconVariation::Filled,
                          trailingIconColor);
  }

  int thisId = currentChipId_++;
  bool isFocused = focusedChipId_ == thisId;
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
  bool wasClicked =
      !resolved.disabled && !inputBlocked && isHovered &&
      IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedChipId_ = thisId;
    isFocused = true;
  }
  if (isFocused && !resolved.disabled &&
      (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    wasClicked = true;
  }
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      !CheckCollisionPointRec(mousePos, bounds)) {
    focusedChipId_ = -1;
  }

  if (isHovered && !resolved.disabled && !inputBlocked)
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);

  if (wasClicked && selected) {
    *selected = !*selected;
  }

#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || wasClicked) {
    InputLayerManager::ConsumeInput();
  }
#endif

  if (resolved.tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = resolved.tooltipPlacement;
    Tooltip(bounds, resolved.tooltip, tooltipOpts);
  }

  return wasClicked;
}

} // namespace raym3
