#include "raym3/components/IconButton.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <map>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int focusedIconButtonId_ = -1;
static int currentIconButtonId_ = 0;

void IconButtonComponent::ResetIds() { currentIconButtonId_ = 0; }

bool IconButtonComponent::Render(const char *iconName, Rectangle bounds,
                                 ButtonVariant variant,
                                 IconVariation iconVariation,
                                 Color iconColorOverride,
                                 const IconButtonOptions* options) {
  // Interaction
  Vector2 mousePos = GetMousePosition();
  
  // Check if element is visible in scroll container
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  // Modal check
  if (DialogComponent::IsActive() && !DialogComponent::IsRendering()) {
    isHovered = false;
    isPressed = false;
  }
  
  int thisId = currentIconButtonId_++;
  bool isFocused = (focusedIconButtonId_ == thisId);
  
  // Keyboard navigation
  if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    focusedIconButtonId_ = thisId;
    isFocused = true;
  }
  
  bool keyActivated = isFocused && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER));
  
  if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }
  
  // Lose focus when clicking anywhere outside (raw check, bypass input layers)
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(GetMousePosition(), bounds)) {
    focusedIconButtonId_ = -1;
    isFocused = false;
  }

  ComponentState state = GetState(bounds);
  // Override state
  if (isPressed)
    state = ComponentState::Pressed;
  else if (isHovered)
    state = ComponentState::Hovered;
  else
    state = ComponentState::Default;

  Color bgColor = GetBackgroundColor(variant, state);
  Color iconColor = (iconColorOverride.a > 0) ? iconColorOverride
                                              : GetIconColor(variant, state);

  // Icon buttons are circular
  float cornerRadius = bounds.height / 2.0f;

  ColorScheme &scheme = Theme::GetColorScheme();

  // Draw Container
  if (variant == ButtonVariant::Filled) {
    int elevation = (state == ComponentState::Pressed)
                        ? 1
                        : 0; // Filled icon button usually flat or low elevation
    Renderer::DrawElevatedRectangle(bounds, cornerRadius, elevation, bgColor);
  } else if (variant == ButtonVariant::Tonal) {
    Renderer::DrawRoundedRectangle(bounds, cornerRadius, bgColor);
  } else if (variant == ButtonVariant::Outlined) {
    Renderer::DrawRoundedRectangleEx(bounds, cornerRadius, scheme.outline,
                                     1.0f);
    // If hovered/pressed, maybe draw a state layer or background?
    // Standard outlined button has transparent bg, but state layer on top.
  } else if (variant == ButtonVariant::Text) { // Standard Icon Button
                                               // Transparent background
  } else if (variant == ButtonVariant::Elevated) {
    int elevation = (state == ComponentState::Pressed) ? 1 : 2;
    Renderer::DrawElevatedRectangle(bounds, cornerRadius, elevation, bgColor);
  }

  // State Layer
  // MD3: State layer uses the "Content" color (text/icon color)
  Color stateLayerBaseColor = iconColor;

  // Refine state layer for Text variant (standard icon button)
  // Make it subtle and circular, centered, 40dp (or smaller if bounds are
  // small)
  if (variant == ButtonVariant::Text) {
    float stateLayerSize = 40.0f;
    if (bounds.width < stateLayerSize)
      stateLayerSize = bounds.width;
    if (bounds.height < stateLayerSize)
      stateLayerSize = bounds.height;

    Rectangle stateLayerBounds = {
        bounds.x + (bounds.width - stateLayerSize) / 2.0f,
        bounds.y + (bounds.height - stateLayerSize) / 2.0f, stateLayerSize,
        stateLayerSize};
    Renderer::DrawStateLayer(stateLayerBounds, stateLayerSize / 2.0f,
                             stateLayerBaseColor, state);
  } else {
    // For container-based buttons, fill the bounds
    Renderer::DrawStateLayer(bounds, cornerRadius, stateLayerBaseColor, state);
  }

  // Draw Icon
  // Icon size is typically 24dp
  float iconSize = 24.0f;
  Rectangle iconBounds = {bounds.x + (bounds.width - iconSize) / 2.0f,
                          bounds.y + (bounds.height - iconSize) / 2.0f,
                          iconSize, iconSize};

  IconComponent::Render(iconName, iconBounds, iconVariation, iconColor);

  bool wasClicked = (isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) || keyActivated;
  
#if RAYM3_USE_INPUT_LAYERS
  if (isHovered || wasClicked) {
    InputLayerManager::ConsumeInput();
  }
#endif
  
  // Tooltip
  if (options && options->tooltip && isHovered) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options->tooltipPlacement;
    Tooltip(bounds, options->tooltip, tooltipOpts);
  }
  
  return wasClicked;
}

Color IconButtonComponent::GetBackgroundColor(ButtonVariant variant,
                                              ComponentState state) {
  ColorScheme &scheme = Theme::GetColorScheme();

  switch (variant) {
  case ButtonVariant::Filled:
    return scheme.primary;
  case ButtonVariant::Tonal:
    return scheme.secondaryContainer;
  case ButtonVariant::Elevated:
    return scheme.surfaceContainerLow; // Or similar
  case ButtonVariant::Outlined:
    return (state == ComponentState::Pressed)
               ? scheme.surface
               : scheme.surface; // Transparent-ish?
  case ButtonVariant::Text:
    return ColorAlpha(scheme.surface, 0);
  default:
    return scheme.surface;
  }
}

Color IconButtonComponent::GetIconColor(ButtonVariant variant,
                                        ComponentState state) {
  ColorScheme &scheme = Theme::GetColorScheme();

  switch (variant) {
  case ButtonVariant::Filled:
    return scheme.surface;
  case ButtonVariant::Tonal:
    return scheme.onSecondaryContainer;
  case ButtonVariant::Elevated:
    return scheme.primary;
  case ButtonVariant::Outlined:
    return (state == ComponentState::Pressed) ? scheme.inversePrimary
                                              : scheme.onSurfaceVariant;
    // Note: MD3 spec says onSurfaceVariant for standard/outlined, primary for
    // selected/pressed? Let's stick to onSurfaceVariant for default, maybe
    // primary for pressed.
  case ButtonVariant::Text:
    return (state == ComponentState::Pressed) ? scheme.primary
                                              : scheme.onSurfaceVariant;
  default:
    return scheme.onSurface;
  }
}

ComponentState IconButtonComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
  
  // Check if element is visible in scroll container
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
  
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
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

} // namespace raym3
