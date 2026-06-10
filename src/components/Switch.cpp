#include "raym3/components/Switch.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/IconRenderer.h"
#include <algorithm>
#include <map>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedSwitchId_ = -1;
static int currentSwitchId_ = 0;

void SwitchComponent::ResetIds() { currentSwitchId_ = 0; }

bool SwitchComponent::Render(const char *label, Rectangle bounds,
                             bool *checked, const SwitchOptions* options) {
  if (!checked)
    return false;

  ComponentState state = GetState(bounds);

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  if (inputBlocked) {
    state = ComponentState::Default;
  }

  ColorScheme &scheme = Theme::GetColorScheme();

  // MD3 dimensions: 52dp track, 32dp height, 24dp handle, 28dp pressed handle.
  float scale = 1.0f;
  float trackWidth = 52.0f * scale;
  float trackHeight = 32.0f * scale;
  float thumbSizeChecked = 24.0f * scale;
  float thumbSizeUnchecked = 24.0f * scale; // Keeping consistent size with icon
  float thumbSizePressed = 28.0f * scale;

  Rectangle switchBounds = GetSwitchBounds(bounds);

  float trackX = switchBounds.x;
  if (!label) {
    trackX += (switchBounds.width - trackWidth) / 2.0f;
  }

  // Align track vertically in bounds
  Rectangle trackRect = {
      trackX, switchBounds.y + (switchBounds.height - trackHeight) / 2.0f,
      trackWidth, trackHeight};

  // Animation progress: 0 = off, 1 = on. Snaps when animProgress < 0.
  float t = (options && options->animProgress >= 0.0f)
                ? options->animProgress
                : (*checked ? 1.0f : 0.0f);
  bool isChecked = t >= 0.5f;
  // M3: unchecked thumb 16dp, checked 24dp — grows as it slides on.
  thumbSizeUnchecked = 16.0f * scale;
  float currentThumbSize =
      thumbSizeUnchecked + (thumbSizeChecked - thumbSizeUnchecked) * t;

  // Handle Pressed State Scaling
  if (state == ComponentState::Pressed) {
    currentThumbSize = thumbSizePressed;
  }

  // Colors — cross-fade between off and on palettes by t (M3 motion).
  auto lerp = [](Color a, Color b, float f) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * f),
                 (unsigned char)(a.g + (b.g - a.g) * f),
                 (unsigned char)(a.b + (b.b - a.b) * f),
                 (unsigned char)(a.a + (b.a - a.a) * f)};
  };
  Color trackColor;
  Color thumbColor;
  Color outlineColor = scheme.outline;
  Color iconColor;
  bool hasOutline = t < 0.5f;

  if (state == ComponentState::Disabled) {
    if (isChecked) {
      trackColor = ColorAlpha(scheme.onSurface, 0.12f);
      thumbColor = scheme.surface;
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
      hasOutline = false;
    } else {
      trackColor = ColorAlpha(scheme.surfaceVariant, 0.12f);
      outlineColor = ColorAlpha(scheme.onSurface, 0.12f);
      thumbColor = ColorAlpha(scheme.onSurface, 0.38f);
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
    }
  } else {
    trackColor = lerp(scheme.surfaceContainerHighest, scheme.primary, t);
    thumbColor = lerp(scheme.outline, scheme.onPrimary, t);
    iconColor = isChecked ? scheme.onPrimaryContainer
                          : scheme.surfaceContainerHighest;
  }

  // Draw Track
  Renderer::DrawRoundedRectangle(trackRect, trackHeight / 2.0f, trackColor);
  if (hasOutline) {
    Renderer::DrawRoundedRectangleEx(trackRect, trackHeight / 2.0f,
                                     outlineColor, 2.0f);
  }

  // Draw Thumb — center travels from the left inset to the right inset; the
  // thumb both slides and grows (16→24dp) with t.
  float thumbSize = (state == ComponentState::Pressed) ? thumbSizePressed
                                                       : currentThumbSize;
  float cy = trackRect.y + trackHeight / 2.0f;
  float centerOff = trackRect.x + trackHeight / 2.0f;
  float centerOn = trackRect.x + trackRect.width - trackHeight / 2.0f;
  float cx = centerOff + (centerOn - centerOff) * t;
  Rectangle thumbRect = {cx - thumbSize / 2.0f, cy - thumbSize / 2.0f, thumbSize,
                         thumbSize};

  Renderer::DrawRoundedRectangle(thumbRect, thumbSize / 2.0f, thumbColor);

  // Draw Icon
  Vector2 center = {thumbRect.x + thumbRect.width / 2.0f,
                    thumbRect.y + thumbRect.height / 2.0f};
  float iconSize = 16.0f * scale;

  raym3::v2::DrawMaterialIcon(isChecked ? 0xe5ca : 0xe5cd, thumbRect,
                              iconColor, (int)iconSize, true);

  // Draw State Layer
  if (state != ComponentState::Default && state != ComponentState::Disabled) {
    float stateLayerSize = 40.0f * scale;
    Rectangle stateLayerRect = {center.x - stateLayerSize / 2.0f,
                                center.y - stateLayerSize / 2.0f,
                                stateLayerSize, stateLayerSize};
    Renderer::DrawStateLayer(stateLayerRect, stateLayerSize / 2.0f,
                             isChecked ? scheme.primary : scheme.onSurface,
                             state);
  }

  // Label
  if (label) {
    Vector2 labelPos = {bounds.x + trackWidth + 12.0f,
                        bounds.y + (bounds.height - 14.0f) / 2.0f};
    Renderer::DrawText(label, labelPos, 14.0f, scheme.onSurface,
                       FontWeight::Regular);
  }

  // Interaction
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  Vector2 mousePos = GetMousePosition();
  
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
  bool clicked = canProcessInput &&
                 CheckCollisionPointRec(mousePos, bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
  bool clicked = isVisible &&
                 CheckCollisionPointRec(mousePos, bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif

  int thisId = currentSwitchId_++;
  bool isFocused = (focusedSwitchId_ == thisId);
  
  // Keyboard navigation
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedSwitchId_ = thisId;
    isFocused = true;
  }
  
  if (isFocused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    clicked = true;
  }
  
  if (CheckCollisionPointRec(mousePos, bounds) && !inputBlocked) {
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }
  
  // Lose focus when clicking anywhere outside (raw check, bypass input layers)
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), bounds)) {
    focusedSwitchId_ = -1;
    isFocused = false;
  }

  if (!inputBlocked && clicked && state != ComponentState::Disabled) {
    *checked = !*checked;
#if RAYM3_USE_INPUT_LAYERS
    InputLayerManager::ConsumeInput();
#endif
    return true;
  }
  
  // Tooltip
  if (options && options->tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options->tooltipPlacement;
    Tooltip(bounds, options->tooltip, tooltipOpts);
  }

  return false;
}

ComponentState SwitchComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
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

Rectangle SwitchComponent::GetSwitchBounds(Rectangle bounds) { return bounds; }

} // namespace raym3
