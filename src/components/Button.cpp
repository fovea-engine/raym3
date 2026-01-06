#include "raym3/components/Button.h"
#include "raym3/components/Dialog.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool ButtonComponent::Render(const char *text, Rectangle bounds,
                             ButtonVariant variant,
                             const ButtonOptions &options) {
  // Interaction
  Vector2 mousePos = GetMousePosition();

#if RAYM3_USE_INPUT_LAYERS
  // Get the current layer ID (buttons should be on the same layer as their
  // parent)
  int buttonLayerId = InputLayerManager::GetCurrentLayerId();

  // High-layer overlays (dialogs at 9999, context menus at 100) bypass scroll
  // container clipping since they're absolutely positioned outside the layout
  // flow
  bool isVisible = (buttonLayerId >= 100)
                       ? true
                       : Layout::IsRectVisibleInScrollContainer(bounds);

  bool canProcessInput =
      isVisible &&
      InputLayerManager::ShouldProcessMouseInput(bounds, buttonLayerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  // Modal check
  if (DialogComponent::IsActive() && !DialogComponent::IsRendering()) {
    isHovered = false;
    isPressed = false;
  }

  ComponentState state = GetState(bounds);
  // Override state based on our local isHovered/isPressed which might be
  // modified by modal check
  if (isPressed)
    state = ComponentState::Pressed;
  else if (isHovered)
    state = ComponentState::Hovered;
  else
    state = ComponentState::Default;

  Color bgColor = (options.backgroundColor.a > 0)
                      ? options.backgroundColor
                      : GetBackgroundColor(variant, state);
  Color textColor = (options.textColor.a > 0) ? options.textColor
                                              : GetTextColor(variant, state);
  // MD3 Buttons are typically Pill shaped (fully rounded)
  float cornerRadius = bounds.height / 2.0f;

  ColorScheme &scheme = Theme::GetColorScheme();

  if (variant == ButtonVariant::Elevated) {
    // Elevated has shadow (level 1 usually, increases on press/hover)
    int elevation = (state == ComponentState::Pressed)
                        ? 1
                        : 1; // MD3: Enabled 1dp, Hover 3dp/Pressed 1dp?
    if (state == ComponentState::Hovered)
      elevation = 2;

    Renderer::DrawElevatedRectangle(bounds, cornerRadius, elevation, bgColor);
  } else if (variant == ButtonVariant::Filled) {
    // Filled button: Use bgColor which handles hover state (inversion)
    int elevation = (state == ComponentState::Pressed) ? 1 : 2;
    Renderer::DrawElevatedRectangle(bounds, cornerRadius, elevation, bgColor);
  } else if (options.drawBackground && variant != ButtonVariant::Text) {
    Renderer::DrawRoundedRectangle(bounds, cornerRadius, bgColor);
  }

  if (options.drawOutline && variant == ButtonVariant::Outlined) {
    // Outlined button user requested white bg and shadow -> User now requested
    // NO shadow Re-using bgColor which we will set to white/surface below
    // Renderer::DrawRoundedRectangle(bounds, cornerRadius, bgColor); // This
    // was moved to the drawBackground check

    // Draw outline on top
    Color borderColor = (state == ComponentState::Disabled)
                            ? ColorAlpha(scheme.onSurface, 0.12f)
                            : scheme.outline;
    if (state == ComponentState::Pressed)
      borderColor = scheme.outline;

    // Use primary color outline as per user request
    borderColor =
        (options.outlineColor.a > 0) ? options.outlineColor : scheme.primary;

    Renderer::DrawRoundedRectangleEx(bounds, cornerRadius, borderColor, 1.0f);
  } else if (variant == ButtonVariant::Text) {
    // Text button in MD3 has transparent background (no fill)
    // Only state layer and text are drawn
  }

  // State Layer (on top of everything but text)
  // MD3: State layer uses the "Content" color (text/icon color)
  Color stateLayerBaseColor = textColor;

  // Special case for Filled button?
  // Filled: Content is onPrimary. State layer is onPrimary. Correct.
  // Tonal: Content is onSecondaryContainer. State layer is
  // onSecondaryContainer. Correct. Outlined/Text/Elevated: Content is Primary.
  // State layer is Primary. Correct.

  Renderer::DrawStateLayer(bounds, cornerRadius, stateLayerBaseColor, state);

  // Use the calculated textColor which handles hover/inversion logic correctly
  Renderer::DrawTextCentered(text, bounds, 14.0f, textColor,
                             FontWeight::Medium);

  // Fix: Check for release independently of current frame's "Pressed" state
  // (which requires mouse down)
  bool wasClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || wasClicked) {
    InputLayerManager::ConsumeInput();
  }
#endif

  return wasClicked;
}

ComponentState ButtonComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();

  // Check if element is visible in scroll container
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);

#if RAYM3_USE_INPUT_LAYERS
  // Get the current layer ID (buttons should be on the same layer as their
  // parent)
  int buttonLayerId = InputLayerManager::GetCurrentLayerId();
  bool canProcessInput =
      isVisible &&
      InputLayerManager::ShouldProcessMouseInput(bounds, buttonLayerId);
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

Color ButtonComponent::GetBackgroundColor(ButtonVariant variant,
                                          ComponentState state) {
  ColorScheme &scheme = Theme::GetColorScheme();

  // Handle Disabled state if needed (assuming state includes disabled logic
  // externally or here) For now assuming enabled states.

  switch (variant) {
  case ButtonVariant::Filled:
    return scheme.primary;
  case ButtonVariant::Tonal:
    return scheme.secondaryContainer;
  case ButtonVariant::Elevated:
    return scheme.surfaceContainerLow; // Or surface + elevation
  case ButtonVariant::Outlined:
    return ColorAlpha(scheme.surface, 0); // Transparent
  case ButtonVariant::Text:
    return ColorAlpha(scheme.surface, 0); // Transparent
  default:
    return scheme.surface;
  }
}

Color ButtonComponent::GetTextColor(ButtonVariant variant,
                                    ComponentState state) {
  ColorScheme &scheme = Theme::GetColorScheme();

  switch (variant) {
  case ButtonVariant::Filled:
    return scheme.onPrimary;
  case ButtonVariant::Tonal:
    return scheme.onSecondaryContainer;
  case ButtonVariant::Elevated:
    return scheme.primary;
  case ButtonVariant::Outlined:
    return scheme.primary;
  case ButtonVariant::Text:
    return scheme.primary;
  default:
    return scheme.onSurface;
  }
}

float ButtonComponent::GetCornerRadius() {
  return 20.0f; // Standard pill radius approximation
}

} // namespace raym3
