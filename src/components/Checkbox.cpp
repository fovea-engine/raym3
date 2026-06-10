#include "raym3/components/Checkbox.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/IconRenderer.h"
#include <raylib.h>
#include <algorithm>
#include <map>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedCheckboxId_ = -1;
static int currentCheckboxId_ = 0;
static std::map<int, Rectangle> checkboxBounds_;

void CheckboxComponent::ResetIds() { currentCheckboxId_ = 0; }

bool CheckboxComponent::Render(const char *label, Rectangle bounds,
                               bool *checked, const CheckboxOptions* options) {
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

  // MD3 state layer is 40dp, while the minimum touch target is 48dp.
  float stateLayerSize = 40.0f;
  if (state != ComponentState::Default) {
    Rectangle stateLayerRect = {
        checkboxBounds.x + checkboxBounds.width / 2.0f - stateLayerSize / 2.0f,
        checkboxBounds.y + checkboxBounds.height / 2.0f - stateLayerSize / 2.0f,
        stateLayerSize, stateLayerSize};
    // User requested "overlay should be onPrimary"
    Renderer::DrawStateLayer(stateLayerRect, stateLayerSize / 2.0f,
                             scheme.onPrimary, state);
  }

  // Animation progress: 0 = unchecked, 1 = checked.
  float t = (options && options->animProgress >= 0.0f) ? options->animProgress
                                                       : (*checked ? 1.0f : 0.0f);
  // Outline is always present; the primary fill scales in from the center as t
  // rises so the box "fills" rather than popping.
  Renderer::DrawRoundedRectangleEx(checkboxBounds, cornerRadius,
                                   scheme.onSurfaceVariant, 2.0f);
  if (t > 0.01f) {
    float fillSize = size * t;
    Rectangle fill = {checkboxBounds.x + (size - fillSize) / 2.0f,
                      checkboxBounds.y + (size - fillSize) / 2.0f, fillSize,
                      fillSize};
    Renderer::DrawRoundedRectangle(fill, cornerRadius * t, scheme.primary);

    raym3::v2::DrawMaterialIcon(0xe5ca, checkboxBounds,
                                ColorAlpha(scheme.onPrimary, t), 18, true);
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
      isVisible && InputLayerManager::ShouldProcessMouseInput(touchTarget, layerId);
  bool clicked = canProcessInput &&
                 CheckCollisionPointRec(GetMousePosition(), touchTarget) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool clicked = isVisible &&
                 CheckCollisionPointRec(GetMousePosition(), touchTarget) &&
                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
  
  int thisId = currentCheckboxId_++;
  checkboxBounds_[thisId] = bounds;
  bool isFocused = (focusedCheckboxId_ == thisId);
  bool isHovered = CheckCollisionPointRec(GetMousePosition(), touchTarget);
  
  // Keyboard navigation
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedCheckboxId_ = thisId;
    isFocused = true;
  }
  
  if (isFocused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    clicked = true;
  }
  
  if (CheckCollisionPointRec(GetMousePosition(), touchTarget) && !inputBlocked) {
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }
  
  // Lose focus when clicking anywhere outside (raw check, bypass input layers)
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), touchTarget)) {
    focusedCheckboxId_ = -1;
    isFocused = false;
  }

  if (!inputBlocked && clicked) {
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

ComponentState CheckboxComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  Rectangle checkboxBounds = GetCheckboxBounds(bounds);
  Rectangle touchTarget = {checkboxBounds.x + checkboxBounds.width / 2.0f - 24.0f,
                           checkboxBounds.y + checkboxBounds.height / 2.0f - 24.0f, 48.0f,
                           48.0f};
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  // High-layer overlays bypass scroll container clipping
  if (layerId >= 100)
    isVisible = true;

  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, touchTarget);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, touchTarget);
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
  float visualSlot = std::min(48.0f, std::max(size, bounds.width));
  return {bounds.x + (visualSlot - size) / 2.0f,
          bounds.y + (bounds.height - size) / 2.0f, size, size};
}

} // namespace raym3
