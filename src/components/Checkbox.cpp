#include "raym3/components/Checkbox.h"
#include "raym3/components/Dialog.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool CheckboxComponent::Render(const char *label, Rectangle bounds,
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

  // MD3 Specs
  float size = 18.0f;
  // User requested "round but less circular".
  // MD3 Checkbox radius is typically 2dp.
  // User requested "a tiny bit rounder" than 2.0f.
  float cornerRadius = 4.0f;

  Rectangle checkboxBounds = GetCheckboxBounds(bounds);
  // Ensure bounds match size exactly
  checkboxBounds.width = size;
  checkboxBounds.height = size;
  checkboxBounds.y = bounds.y + (bounds.height - size) / 2.0f;

  // State Layer (Halo)
  // User requested "only 3 px bigger than the checkbox".
  // Assuming 3px padding on all sides -> size + 6.
  float stateLayerSize = size + 6.0f;
  if (state != ComponentState::Default) {
    Rectangle stateLayerRect = {
        checkboxBounds.x + checkboxBounds.width / 2.0f - stateLayerSize / 2.0f,
        checkboxBounds.y + checkboxBounds.height / 2.0f - stateLayerSize / 2.0f,
        stateLayerSize, stateLayerSize};
    // User requested "overlay should be onPrimary"
    Renderer::DrawStateLayer(stateLayerRect, stateLayerSize / 2.0f,
                             scheme.onPrimary, state);
  }

  if (*checked) {
    // Filled
    Renderer::DrawRoundedRectangle(checkboxBounds, cornerRadius,
                                   scheme.primary);

    // Checkmark
    // Icon area is 24x24 usually scaled to 18x18.
    // Simple checkmark:
    float thickness = 2.0f;
    Vector2 center = {checkboxBounds.x + size / 2.0f,
                      checkboxBounds.y + size / 2.0f};

    // Points relative to center
    Vector2 p1 = {center.x - 5.0f, center.y - 1.0f};
    Vector2 p2 = {center.x - 1.0f, center.y + 3.0f};
    Vector2 p3 = {center.x + 5.0f, center.y - 3.0f};

    // Adjust for 18px size (smaller than 24px icon space)
    // Scale by 18/24 = 0.75
    p1 = {center.x - 4.0f, center.y - 0.5f};
    p2 = {center.x - 1.0f, center.y + 2.5f};
    p3 = {center.x + 3.5f, center.y - 3.0f};

    DrawLineEx(p1, p2, thickness, scheme.onPrimary);
    DrawLineEx(p2, p3, thickness, scheme.onPrimary);
  } else {
    // Outline
    // MD3 Unchecked: onSurfaceVariant border
    Renderer::DrawRoundedRectangleEx(checkboxBounds, cornerRadius,
                                     scheme.onSurfaceVariant, 2.0f);
  }

  if (label) {
    Vector2 labelPos = {bounds.x + checkboxBounds.width + 12.0f,
                        bounds.y + (bounds.height - 14.0f) / 2.0f};
    Renderer::DrawText(label, labelPos, 14.0f, scheme.onSurface,
                       FontWeight::Regular);
  }

  // Interaction
  // Touch target should be larger (48dp)
  Rectangle touchTarget = {checkboxBounds.x + size / 2.0f - 24.0f,
                           checkboxBounds.y + size / 2.0f - 24.0f, 48.0f,
                           48.0f};

  // If label exists, include it in hit area?
  // Standard implementation often separates them or includes both.
  // Current bounds passed in likely include label area.

  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  // High-layer overlays bypass scroll container clipping
  if (layerId >= 100)
    isVisible = true;

  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool clicked = canProcessInput &&
                 CheckCollisionPointRec(GetMousePosition(), bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool clicked = isVisible &&
                 CheckCollisionPointRec(GetMousePosition(), bounds) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
  if (!inputBlocked && clicked) {
    *checked = !*checked;
#if RAYM3_USE_INPUT_LAYERS
    InputLayerManager::ConsumeInput();
#endif
    return true;
  }

  return false;
}

ComponentState CheckboxComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  // High-layer overlays bypass scroll container clipping
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

Rectangle CheckboxComponent::GetCheckboxBounds(Rectangle bounds) {
  float size = 18.0f;
  return {bounds.x, bounds.y + (bounds.height - size) / 2.0f, size, size};
}

} // namespace raym3
