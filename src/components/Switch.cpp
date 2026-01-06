#include "raym3/components/Switch.h"
#include "raym3/components/Dialog.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool SwitchComponent::Render(const char *label, Rectangle bounds,
                             bool *checked) {
  if (!checked)
    return false;

  ComponentState state = GetState(bounds);

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  if (inputBlocked) {
    state = ComponentState::Default;
  }

  ColorScheme &scheme = Theme::GetColorScheme();

  // MD3 Dimensions (Scaled to 75% per user request)
  float scale = 0.75f;
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

  bool isChecked = *checked;
  float currentThumbSize = isChecked ? thumbSizeChecked : thumbSizeUnchecked;

  // Handle Pressed State Scaling
  if (state == ComponentState::Pressed) {
    currentThumbSize = thumbSizePressed;
  }

  // Colors
  Color trackColor;
  Color thumbColor;
  Color outlineColor;
  Color iconColor;
  bool hasOutline = false;

  if (isChecked) {
    // Checked: Track Primary, Thumb OnPrimary, Icon Primary (if on thumb)
    // Actually MD3 spec:
    // Thumb: OnPrimary. Icon: OnPrimaryContainer (or Primary if high contrast
    // needed). Track: Primary.
    trackColor = scheme.primary;
    thumbColor = scheme.onPrimary;
    iconColor = scheme.onPrimaryContainer; // Darker color on white thumb

    if (state == ComponentState::Disabled) {
      trackColor = ColorAlpha(scheme.onSurface, 0.12f);
      thumbColor = scheme.surface;
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
    }
  } else {
    // Unchecked: Track SurfaceContainerHighest, Border Outline.
    // Thumb: Outline. Icon: SurfaceContainerHighest.
    trackColor = scheme.surfaceContainerHighest;
    outlineColor = scheme.outline;
    hasOutline = true;
    thumbColor = scheme.outline;
    iconColor = scheme.surfaceContainerHighest;

    if (state == ComponentState::Disabled) {
      trackColor = ColorAlpha(scheme.surfaceVariant, 0.12f);
      outlineColor = ColorAlpha(scheme.onSurface, 0.12f);
      thumbColor = ColorAlpha(scheme.onSurface, 0.38f);
      iconColor = ColorAlpha(scheme.onSurface, 0.38f);
    }
  }

  // Draw Track
  Renderer::DrawRoundedRectangle(trackRect, trackHeight / 2.0f, trackColor);
  if (hasOutline) {
    Renderer::DrawRoundedRectangleEx(trackRect, trackHeight / 2.0f,
                                     outlineColor, 2.0f);
  }

  // Draw Thumb
  // Unchecked: Thumb is smaller (if 16) or same (if 24).
  // Padding calculation needs to handle the visual centering.
  float padding = (trackHeight - currentThumbSize) / 2.0f;

  // MD3: Unchecked thumb is usually offset more to the left?
  // No, just centered vertically, and padded from left.
  // If unchecked thumb is 16dp, padding is (32-16)/2 = 8.
  // If checked thumb is 24dp, padding is (32-24)/2 = 4.

  float thumbX;
  if (isChecked) {
    thumbX = trackRect.x + trackRect.width - currentThumbSize -
             4.0f * scale; // 4dp padding from right
  } else {
    thumbX = trackRect.x + 4.0f * scale; // 4dp padding from left?
    // If thumb is 24dp, 4dp padding makes it touch the border (32 height - 24
    // thumb = 8 space total, 4 top 4 bottom). If we want 4dp from left edge:
    // track x + 4.

    // Wait, if unchecked thumb is 24dp (with icon), it should be same padding
    // as checked. If unchecked thumb is 16dp (no icon), it has more padding.
    // Let's stick to calculated padding based on size for vertical centering,
    // but horizontal position needs to be explicit.

    // Let's use the calculated padding for X as well to keep it
    // circular/consistent? Or fixed margins? MD3 spec usually has fixed
    // margins. Let's use the calculated padding for now as it centers it in the
    // available height.
    thumbX = trackRect.x + padding;

    // Correction: If thumb is 24dp, padding is 4. So x + 4.
    // If thumb is 16dp, padding is 8. So x + 8.
  }

  // Override for Pressed state to ensure it expands from center
  if (state == ComponentState::Pressed) {
    // Center of where the thumb WOULD be
    float normalSize = isChecked ? thumbSizeChecked : thumbSizeUnchecked;
    float normalPadding = (trackHeight - normalSize) / 2.0f;
    float normalX =
        isChecked ? (trackRect.x + trackRect.width - normalSize - normalPadding)
                  : (trackRect.x + normalPadding);
    float centerX = normalX + normalSize / 2.0f;

    thumbX = centerX - currentThumbSize / 2.0f;
    padding = (trackHeight - currentThumbSize) / 2.0f; // Vertical centering
  }

  Rectangle thumbRect = {thumbX, trackRect.y + padding, currentThumbSize,
                         currentThumbSize};

  Renderer::DrawRoundedRectangle(thumbRect, currentThumbSize / 2.0f,
                                 thumbColor);

  // Draw Icon
  Vector2 center = {thumbRect.x + thumbRect.width / 2.0f,
                    thumbRect.y + thumbRect.height / 2.0f};
  float iconSize = 16.0f * scale;

  if (isChecked) {
    // Checkmark Geometry
    Vector2 p1 = {center.x - 4.0f * scale, center.y};
    Vector2 p2 = {center.x - 1.0f * scale, center.y + 3.0f * scale};
    Vector2 p3 = {center.x + 5.0f * scale, center.y - 5.0f * scale};

    DrawLineEx(p1, p2, 2.0f * scale, iconColor);
    DrawLineEx(p2, p3, 2.0f * scale, iconColor);
  } else {
    // 'X' Icon
    float half = 4.0f * scale;
    Vector2 p1 = {center.x - half, center.y - half};
    Vector2 p2 = {center.x + half, center.y + half};
    Vector2 p3 = {center.x - half, center.y + half};
    Vector2 p4 = {center.x + half, center.y - half};

    DrawLineEx(p1, p2, 2.0f * scale, iconColor);
    DrawLineEx(p3, p4, 2.0f * scale, iconColor);
  }

  // Draw State Layer
  if (state != ComponentState::Default && state != ComponentState::Disabled) {
    float stateLayerSize = 40.0f * scale; // Standard touch target size scaled
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
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
  bool clicked = canProcessInput &&
                 CheckCollisionPointRec(GetMousePosition(), bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool clicked = isVisible &&
                 CheckCollisionPointRec(GetMousePosition(), bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
  if (!inputBlocked && clicked && state != ComponentState::Disabled) {
    *checked = !*checked;
#if RAYM3_USE_INPUT_LAYERS
    InputLayerManager::ConsumeInput();
#endif
    return true;
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
