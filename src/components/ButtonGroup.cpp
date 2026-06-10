#include "raym3/components/ButtonGroup.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <cmath>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static int bgCurrentId_ = 0;

void ButtonGroupComponent::ResetIds() { bgCurrentId_ = 0; }

int ButtonGroupComponent::Render(Rectangle bounds, const ButtonGroupItem *items,
                                 int count, ButtonVariant variant) {
  if (count <= 0)
    return -1;

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();

  ColorScheme &scheme   = Theme::GetColorScheme();
  float segmentWidth    = bounds.width / (float)count;
  float cornerRadius    = bounds.height / 2.0f;
  int clicked           = -1;

  // Resolve content/bg colors from variant
  auto GetColors = [&](ButtonVariant v, ComponentState st, bool disabled,
                       Color &outBg, Color &outContent, Color &outBorder) {
    if (disabled) {
      outBg      = ColorAlpha(scheme.surface, 0.0f);
      outContent = ColorAlpha(scheme.onSurface, 0.38f);
      outBorder  = ColorAlpha(scheme.onSurface, 0.12f);
      return;
    }
    switch (v) {
    case ButtonVariant::Filled:
      outBg      = scheme.primary;
      outContent = scheme.onPrimary;
      outBorder  = {0, 0, 0, 0};
      break;
    case ButtonVariant::Tonal:
      outBg      = scheme.secondaryContainer;
      outContent = scheme.onSecondaryContainer;
      outBorder  = {0, 0, 0, 0};
      break;
    case ButtonVariant::Elevated:
      outBg      = scheme.surfaceContainerLow;
      outContent = scheme.primary;
      outBorder  = {0, 0, 0, 0};
      break;
    case ButtonVariant::Text:
      outBg      = {0, 0, 0, 0};
      outContent = scheme.primary;
      outBorder  = {0, 0, 0, 0};
      break;
    default: // Outlined
      outBg      = {0, 0, 0, 0};
      outContent = scheme.onSurface;
      outBorder  = scheme.outline;
      break;
    }
  };

  // Draw segment background/state shape matching position in group
  auto DrawSegmentShape = [&](int i, Rectangle r, Color c) {
    if (i == 0 && count == 1) {
      Renderer::DrawRoundedRectangle(r, cornerRadius, c);
    } else if (i == 0) {
      Rectangle rightRect = {r.x + cornerRadius, r.y, r.width - cornerRadius,
                             r.height};
      if (rightRect.width > 0)
        DrawRectangleRec(rightRect, c);
      Vector2 center = {r.x + cornerRadius, r.y + cornerRadius};
      DrawCircleSector(center, cornerRadius, 90, 270, 24, c);
    } else if (i == count - 1) {
      Rectangle leftRect = {r.x, r.y, r.width - cornerRadius, r.height};
      if (leftRect.width > 0)
        DrawRectangleRec(leftRect, c);
      Vector2 center = {r.x + r.width - cornerRadius, r.y + cornerRadius};
      DrawCircleSector(center, cornerRadius, 270, 450, 24, c);
    } else {
      DrawRectangleRec(r, c);
    }
  };

  for (int i = 0; i < count; ++i) {
    Rectangle segmentBounds = {bounds.x + i * segmentWidth, bounds.y,
                               segmentWidth, bounds.height};

    bool isDisabled = items[i].disabled || inputBlocked;
    ComponentState state = isDisabled ? ComponentState::Disabled
                                      : GetState(segmentBounds);

    Color bgColor, contentColor, borderColor;
    GetColors(variant, state, isDisabled, bgColor, contentColor, borderColor);

    // Draw background (opaque variants only)
    if (bgColor.a > 0) {
      DrawSegmentShape(i, segmentBounds, bgColor);
    }

    // State layer
    if (!isDisabled) {
      Color stateLayerColor = Theme::GetStateLayerColor(contentColor, state);
      if (stateLayerColor.a > 0) {
        DrawSegmentShape(i, segmentBounds, stateLayerColor);
      }
    }

    // Cursor feedback
    if (!isDisabled &&
        CheckCollisionPointRec(GetMousePosition(), segmentBounds)) {
      RequestCursor(MOUSE_CURSOR_POINTING_HAND);
    }

    // Content
    const char *iconName = items[i].icon;
    const char *label    = items[i].label;

    float cx = segmentBounds.x + segmentWidth / 2.0f;
    float cy = segmentBounds.y + segmentBounds.height / 2.0f;

    if (iconName && label) {
      float iconSize   = 18.0f;
      float gap        = 8.0f;
      Font font        = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, label, 14, 1.0f);
      float totalWidth = iconSize + gap + textSize.x;
      float startX     = cx - totalWidth / 2.0f;

      Rectangle iconRect = {startX, cy - iconSize / 2.0f, iconSize, iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            contentColor);
      Vector2 textPos = {startX + iconSize + gap, cy - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);

    } else if (iconName) {
      float iconSize     = 24.0f;
      Rectangle iconRect = {cx - iconSize / 2.0f, cy - iconSize / 2.0f,
                            iconSize, iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            contentColor);

    } else if (label) {
      Font font        = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, label, 14, 1.0f);
      Vector2 textPos  = {cx - textSize.x / 2.0f, cy - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);
    }

    // Click input
    if (!isDisabled && !inputBlocked) {
      Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
      bool canProcess =
          InputLayerManager::ShouldProcessMouseInput(segmentBounds);
      bool isHovered  = canProcess && CheckCollisionPointRec(mousePos, segmentBounds);
      bool isClicked  = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
      bool isHovered = CheckCollisionPointRec(mousePos, segmentBounds);
      bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
      if (isClicked) {
        clicked = i;
#if RAYM3_USE_INPUT_LAYERS
        InputLayerManager::ConsumeInput();
#endif
      }
    }
  }

  // Outer container outline
  Color outerBorder = {0, 0, 0, 0};
  Color dummy1, dummy2;
  GetColors(variant, ComponentState::Default, false, dummy1, dummy2,
            outerBorder);
  if (variant == ButtonVariant::Outlined || outerBorder.a > 0) {
    Renderer::DrawRoundedRectangleEx(bounds, cornerRadius, scheme.outline, 1.0f);
  }

  // Dividers
  for (int i = 1; i < count; ++i) {
    float x = bounds.x + i * segmentWidth;
    DrawLineEx({x, bounds.y}, {x, bounds.y + bounds.height}, 1.0f,
               scheme.outline);
  }

  return clicked;
}

ComponentState ButtonGroupComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered       = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = CheckCollisionPointRec(mousePos, bounds);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  if (isPressed)
    return ComponentState::Pressed;
  if (isHovered)
    return ComponentState::Hovered;
  return ComponentState::Default;
}

} // namespace raym3
