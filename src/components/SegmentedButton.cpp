#include "raym3/components/SegmentedButton.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <cmath>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool SegmentedButtonComponent::Render(Rectangle bounds,
                                      const SegmentedButtonItem *items,
                                      int itemCount, int *selectedIndex,
                                      bool multiSelect) {
  if (itemCount <= 0)
    return false;

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();

  ColorScheme &scheme = Theme::GetColorScheme();
  float segmentWidth = bounds.width / (float)itemCount;
  float cornerRadius = bounds.height / 2.0f; // Pill shape

  bool changed = false;

  // Draw Outline for the whole container first (or per segment?)
  // MD3: "Outlined container". But segments have dividers.
  // Strategy: Draw each segment.
  // First and last segments have rounded corners on outer side.
  // Middle segments are rectangular.

  for (int i = 0; i < itemCount; ++i) {
    Rectangle segmentBounds = {bounds.x + i * segmentWidth, bounds.y,
                               segmentWidth, bounds.height};

    bool isSelected = false;
    if (multiSelect) {
      // TODO: Implement multi-select logic if needed (using bitmask or array?)
      // For now, let's assume selectedIndex is a bitmask if multiSelect is
      // true? Or just stick to single select for this iteration as per API
      // signature limitation. Let's treat selectedIndex as a single index for
      // now.
      isSelected = (*selectedIndex == i);
    } else {
      isSelected = (*selectedIndex == i);
    }

    ComponentState state = GetState(segmentBounds);
    if (inputBlocked) {
      state = ComponentState::Default;
    }

    // Background Color
    Color bgColor =
        isSelected ? scheme.secondaryContainer : ColorAlpha(scheme.surface, 0);

    // Content Color
    Color contentColor =
        isSelected ? scheme.onSecondaryContainer : scheme.onSurface;

    // Draw Segment Background
    // We need custom drawing for partial rounded corners.
    // Raylib doesn't support per-corner radius easily.
    // We can use clipping or draw specific shapes.
    // Or simpler: Draw rect, then mask?
    // Or: Draw rounded rect for the whole container, then draw dividers?
    // But selected segment has background.

    // Approach:
    // 1. Draw background for selected segment.
    //    If first/last, it needs rounded corners.
    //    If middle, rect.

    // Helper to draw segment background/state layer with correct shape
    auto DrawSegmentShape = [&](Rectangle r, Color c) {
      if (i == 0 && itemCount == 1) {
        // Single item: Full pill
        Renderer::DrawRoundedRectangle(r, cornerRadius, c);
      } else if (i == 0) {
        // First item: Left rounded, Right square
        // Draw rect on right (non-overlapping)
        Rectangle rightRect = {r.x + cornerRadius, r.y, r.width - cornerRadius,
                               r.height};
        if (rightRect.width > 0)
          DrawRectangleRec(rightRect, c);

        // Draw left semi-circle
        // Raylib angles: 90=Down, 180=Left, 270=Up
        Vector2 center = {r.x + cornerRadius, r.y + cornerRadius};
        DrawCircleSector(center, cornerRadius, 90, 270, 24, c);
      } else if (i == itemCount - 1) {
        // Last item: Left square, Right rounded
        // Draw rect on left (non-overlapping)
        Rectangle leftRect = {r.x, r.y, r.width - cornerRadius, r.height};
        if (leftRect.width > 0)
          DrawRectangleRec(leftRect, c);

        // Draw right semi-circle
        // Raylib angles: 270=Up, 360=Right, 450=Down
        Vector2 center = {r.x + r.width - cornerRadius, r.y + cornerRadius};
        DrawCircleSector(center, cornerRadius, 270, 450, 24, c);
      } else {
        // Middle item: Square
        DrawRectangleRec(r, c);
      }
    };

    if (isSelected || state != ComponentState::Default) {
      DrawSegmentShape(segmentBounds, bgColor);
    }

    // State Layer
    Color stateLayerColor = Theme::GetStateLayerColor(contentColor, state);
    if (stateLayerColor.a > 0) {
      DrawSegmentShape(segmentBounds, stateLayerColor);
    }

    // Content (Icon + Text)
    // If selected, MD3 often adds a checkmark.
    // Let's check if we have an icon.
    const char *iconName = items[i].icon;
    const char *label = items[i].label;

    if (isSelected && !iconName) {
      iconName = "check"; // Default checkmark if selected and no icon
    } else if (isSelected && iconName) {
      // If icon exists, maybe replace with check? Or just keep icon?
      // MD3: "Selected segments use a checkmark icon... replacing the icon if
      // one exists." Let's swap to checkmark for now to be distinct.
      iconName = "check";
    }

    float contentX = segmentBounds.x + segmentWidth / 2.0f;
    float contentY = segmentBounds.y + segmentBounds.height / 2.0f;

    if (iconName && label) {
      // Icon + Text
      float iconSize = 18.0f;
      float gap = 8.0f;
      // Measure text
      Font font = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, label, 14, 1.0f);
      float totalWidth = iconSize + gap + textSize.x;

      float startX = contentX - totalWidth / 2.0f;

      Rectangle iconRect = {startX, contentY - iconSize / 2.0f, iconSize,
                            iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            contentColor);

      Vector2 textPos = {startX + iconSize + gap, contentY - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);

    } else if (iconName) {
      // Icon Only
      float iconSize = 24.0f;
      Rectangle iconRect = {contentX - iconSize / 2.0f,
                            contentY - iconSize / 2.0f, iconSize, iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled,
                            contentColor);
    } else if (label) {
      // Text Only
      Font font = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, label, 14, 1.0f);
      Vector2 textPos = {contentX - textSize.x / 2.0f,
                         contentY - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);
    }

    // Input
    Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
    bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(segmentBounds);
    bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, segmentBounds);
    bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
    bool isHovered = CheckCollisionPointRec(mousePos, segmentBounds);
    bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
    if (isClicked && !inputBlocked) {
      *selectedIndex = i;
      changed = true;
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
    }
  }

  // Draw Outline (Border)
  // We can draw the full rounded rect outline on top.
  Renderer::DrawRoundedRectangleEx(bounds, cornerRadius, scheme.outline, 1.0f);

  // Draw Dividers
  for (int i = 1; i < itemCount; ++i) {
    float x = bounds.x + i * segmentWidth;
    DrawLineEx({x, bounds.y}, {x, bounds.y + bounds.height}, 1.0f,
               scheme.outline);
  }

  return changed;
}

ComponentState SegmentedButtonComponent::GetState(Rectangle bounds) {
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
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
