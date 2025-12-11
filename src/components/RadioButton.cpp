#include "raym3/components/RadioButton.h"
#include "raym3/components/Dialog.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raymath.h"

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool RadioButtonComponent::Render(const char *label, Rectangle bounds,
                                  bool selected) {
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

  ComponentState state = ComponentState::Default;
  if (isHovered) {
    state = isPressed ? ComponentState::Pressed : ComponentState::Hovered;
  }

  // Colors
  Color outerColor = selected ? scheme.primary : scheme.onSurfaceVariant;
  Color innerColor = scheme.primary;
  Color stateLayerColor = selected ? scheme.primary : scheme.onSurfaceVariant;

  // Layout
  // Layout
  float iconSize = 20.0f;
  // Make hover subtle like checkbox (size + padding)
  float stateLayerSize = iconSize + 8.0f;

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

  // Draw Outer Ring
  float outerRadius = iconSize / 2.0f;
  float strokeWidth = 2.0f;

  if (selected) {
    // Selected: Outer ring is primary, filled with primary?
    // MD3: Selected = Outer ring primary (2dp), Inner dot primary (10dp).
    // Actually, looking at specs:
    // Selected: Outer stroke 2dp Primary. Inner dot 10dp Primary.
    // Unselected: Outer stroke 2dp OnSurfaceVariant. No inner dot.

    DrawRing(center, outerRadius - strokeWidth, outerRadius, 0, 360, 32,
             outerColor);

    // Inner dot
    float dotRadius = 5.0f; // 10dp diameter
    DrawCircleV(center, dotRadius, innerColor);
  } else {
    // Unselected
    DrawRing(center, outerRadius - strokeWidth, outerRadius, 0, 360, 32,
             outerColor);
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

  return isClicked;
}

} // namespace raym3
