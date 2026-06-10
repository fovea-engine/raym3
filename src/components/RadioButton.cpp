#include "raym3/components/RadioButton.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/IconRenderer.h"
#include "raymath.h"
#include <map>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedRadioId_ = -1;
static int currentRadioId_ = 0;

void RadioButtonComponent::ResetIds() { currentRadioId_ = 0; }

bool RadioButtonComponent::Render(const char *label, Rectangle bounds,
                                  bool selected, const RadioButtonOptions* options) {
  ColorScheme &scheme = Theme::GetColorScheme();

  // MD3 Specs:
  // Target size: 48x48 (minimum touch target)
  // Icon size: 20x20
  // Outer stroke: 2dp
  // Inner dot: 10dp
  // State layer: 40x40 (circular)

  // Interaction logic
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  int layerId = InputLayerManager::GetCurrentLayerId();
  // High-layer overlays bypass scroll container clipping
  bool isVisible =
      (layerId >= 100) ? true : Layout::IsRectVisibleInScrollContainer(bounds);

  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif

  // Modal check
  if (DialogComponent::IsActive() && !DialogComponent::IsRendering()) {
    isHovered = false;
    isPressed = false;
    isClicked = false;
  }
  
  int thisId = currentRadioId_++;
  bool isFocused = (focusedRadioId_ == thisId);
  
  // Keyboard navigation
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedRadioId_ = thisId;
    isFocused = true;
  }
  
  if (isFocused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER))) {
    isClicked = true;
  }
  
  if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }
  
  // Lose focus when clicking anywhere outside (raw check, bypass input layers)
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), bounds)) {
    focusedRadioId_ = -1;
    isFocused = false;
  }

  ComponentState state = ComponentState::Default;
  if (isHovered) {
    state = isPressed ? ComponentState::Pressed : ComponentState::Hovered;
  }

  // Animation progress: 0 = unselected, 1 = selected.
  float t = (options && options->animProgress >= 0.0f) ? options->animProgress
                                                       : (selected ? 1.0f : 0.0f);
  auto lerpColor = [](Color a, Color b, float f) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * f),
                 (unsigned char)(a.g + (b.g - a.g) * f),
                 (unsigned char)(a.b + (b.b - a.b) * f),
                 (unsigned char)(a.a + (b.a - a.a) * f)};
  };
  // Colors — outer ring cross-fades onSurfaceVariant→primary with t.
  Color outerColor = lerpColor(scheme.onSurfaceVariant, scheme.primary, t);
  Color innerColor = scheme.primary;
  Color stateLayerColor = selected ? scheme.primary : scheme.onSurfaceVariant;

  // Layout
  // Layout
  float iconSize = 20.0f;
  float stateLayerSize = 40.0f;

  // Center vertically
  float centerY = bounds.y + bounds.height / 2.0f;
  // Align to left side of bounds with padding.
  // Note: The touch target is 48x48, but visual is smaller.
  // We want the visual icon to be centered within the touch target area if
  // possible, or just left aligned. Current logic: centerX = bounds.x +
  // stateLayerSize / 2.0f If stateLayerSize is small (28), centerX = x + 14. If
  // we want it to align with the 48dp touch target center:
  float touchTargetSize = 48.0f;
  float centerX = bounds.x + touchTargetSize / 2.0f;

  Vector2 center = {centerX, centerY};

  // Draw State Layer
  if (state != ComponentState::Default) {
    Rectangle stateLayerRect = {center.x - stateLayerSize / 2.0f,
                                center.y - stateLayerSize / 2.0f,
                                stateLayerSize, stateLayerSize};
    Renderer::DrawStateLayer(stateLayerRect, stateLayerSize / 2.0f,
                             stateLayerColor, state);
  }

  Rectangle iconBounds = {center.x - iconSize / 2.0f,
                          center.y - iconSize / 2.0f, iconSize, iconSize};
  if (t < 0.99f) {
    raym3::v2::DrawMaterialIcon(0xe836, iconBounds,
                                ColorAlpha(outerColor, 1.0f - t), 20, true);
  }
  if (t > 0.01f) {
    raym3::v2::DrawMaterialIcon(0xe837, iconBounds,
                                ColorAlpha(innerColor, t), 20, true);
  }

  // Draw Label
  if (label) {
    float touchTargetSize = 48.0f;
    float labelX = bounds.x + touchTargetSize +
                   4.0f; // Padding after the touch target area
    // Vertical center
    // Assuming standard body font size 16?
    float fontSize = Theme::GetTypographyScale().bodyLarge;

    // Measure text to center vertically perfectly?
    // For now, simple offset.
    Vector2 textPos = {labelX, centerY - fontSize / 2.0f};

    Renderer::DrawText(label, textPos, fontSize, scheme.onSurface,
                       FontWeight::Regular);
  }

#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || isClicked) {
    InputLayerManager::ConsumeInput();
  }
#endif

  // Tooltip
  if (options && options->tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options->tooltipPlacement;
    Tooltip(bounds, options->tooltip, tooltipOpts);
  }

  return isClicked;
}

} // namespace raym3
