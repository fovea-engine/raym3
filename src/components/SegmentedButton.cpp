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

static int focusedSegmentGroupId_ = -1;
static int focusedSegmentIndex_   = -1;
static int currentGroupId_        = 0;

void SegmentedButtonComponent::ResetIds() { currentGroupId_ = 0; }

bool SegmentedButtonComponent::Render(Rectangle bounds,
                                      const SegmentedButtonItem *items,
                                      int itemCount, int *selectedIndex,
                                      bool multiSelect) {
  if (itemCount <= 0)
    return false;

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();

  ColorScheme &scheme = Theme::GetColorScheme();
  float segmentWidth  = bounds.width / (float)itemCount;
  float cornerRadius  = bounds.height / 2.0f;

  bool changed  = false;
  int thisGroup = currentGroupId_++;
  bool isFocusedGroup = (focusedSegmentGroupId_ == thisGroup);

  // Keyboard navigation within focused group
  if (isFocusedGroup && !inputBlocked) {
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_TAB)) {
      focusedSegmentIndex_ = (focusedSegmentIndex_ + 1) % itemCount;
      // Skip disabled items
      int tries = 0;
      while (items[focusedSegmentIndex_].disabled && tries++ < itemCount)
        focusedSegmentIndex_ = (focusedSegmentIndex_ + 1) % itemCount;
    }
    if (IsKeyPressed(KEY_LEFT)) {
      focusedSegmentIndex_ = (focusedSegmentIndex_ - 1 + itemCount) % itemCount;
      int tries = 0;
      while (items[focusedSegmentIndex_].disabled && tries++ < itemCount)
        focusedSegmentIndex_ = (focusedSegmentIndex_ - 1 + itemCount) % itemCount;
    }
    if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
      int i = focusedSegmentIndex_;
      if (i >= 0 && i < itemCount && !items[i].disabled) {
        if (multiSelect) {
          *selectedIndex ^= (1 << i);
        } else {
          *selectedIndex = i;
        }
        changed = true;
      }
    }
  }

  // Lose group focus when clicking outside
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
      !CheckCollisionPointRec(GetMousePosition(), bounds)) {
    if (isFocusedGroup) {
      focusedSegmentGroupId_ = -1;
      isFocusedGroup = false;
    }
  }

  for (int i = 0; i < itemCount; ++i) {
    Rectangle segmentBounds = {bounds.x + i * segmentWidth, bounds.y,
                               segmentWidth, bounds.height};

    bool isDisabled = items[i].disabled || inputBlocked;

    bool isSelected = multiSelect ? ((*selectedIndex >> i) & 1)
                                  : (*selectedIndex == i);

    ComponentState state = ComponentState::Default;
    if (!isDisabled) {
      state = GetState(segmentBounds);
    }

    bool isFocused = isFocusedGroup && (focusedSegmentIndex_ == i);
    if (isFocused && state == ComponentState::Default)
      state = ComponentState::Focused;

    // Colors
    Color bgColor, contentColor;
    if (isDisabled) {
      bgColor      = isSelected ? ColorAlpha(scheme.onSurface, 0.12f)
                                : ColorAlpha(scheme.surface, 0.0f);
      contentColor = ColorAlpha(scheme.onSurface, 0.38f);
    } else {
      bgColor      = isSelected ? scheme.secondaryContainer
                                : ColorAlpha(scheme.surface, 0.0f);
      contentColor = isSelected ? scheme.onSecondaryContainer : scheme.onSurface;
    }

    // Cursor feedback
    if (!isDisabled && CheckCollisionPointRec(GetMousePosition(), segmentBounds)) {
      RequestCursor(MOUSE_CURSOR_POINTING_HAND);
    }

    // Helper: draw shape clipped to this segment's position in the group
    auto DrawSegmentShape = [&](Rectangle r, Color c) {
      if (i == 0 && itemCount == 1) {
        Renderer::DrawRoundedRectangle(r, cornerRadius, c);
      } else if (i == 0) {
        Rectangle rightRect = {r.x + cornerRadius, r.y, r.width - cornerRadius,
                               r.height};
        if (rightRect.width > 0)
          DrawRectangleRec(rightRect, c);
        Vector2 center = {r.x + cornerRadius, r.y + cornerRadius};
        DrawCircleSector(center, cornerRadius, 90, 270, 24, c);
      } else if (i == itemCount - 1) {
        Rectangle leftRect = {r.x, r.y, r.width - cornerRadius, r.height};
        if (leftRect.width > 0)
          DrawRectangleRec(leftRect, c);
        Vector2 center = {r.x + r.width - cornerRadius, r.y + cornerRadius};
        DrawCircleSector(center, cornerRadius, 270, 450, 24, c);
      } else {
        DrawRectangleRec(r, c);
      }
    };

    if (isSelected || (isDisabled && isSelected) ||
        state != ComponentState::Default) {
      DrawSegmentShape(segmentBounds, bgColor);
    }

    if (!isDisabled) {
      Color stateLayerColor = Theme::GetStateLayerColor(contentColor, state);
      if (stateLayerColor.a > 0) {
        DrawSegmentShape(segmentBounds, stateLayerColor);
      }
    }

    // Focused ring (thin outline on focused segment)
    if (isFocused && !isDisabled) {
      Renderer::DrawRoundedRectangleEx(segmentBounds, 4.0f, scheme.secondary,
                                       2.0f);
    }

    // Content
    const char *iconName = items[i].icon;
    const char *label    = items[i].label;

    if (isSelected) {
      iconName = "check"; // Checkmark replaces or prepends icon per M3 spec
    }

    float contentX = segmentBounds.x + segmentWidth / 2.0f;
    float contentY = segmentBounds.y + segmentBounds.height / 2.0f;

    if (iconName && label) {
      float iconSize = 18.0f;
      float gap      = 8.0f;
      Font font      = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize = MeasureTextEx(font, label, 14, 1.0f);
      float totalWidth = iconSize + gap + textSize.x;
      float startX     = contentX - totalWidth / 2.0f;

      Rectangle iconRect = {startX, contentY - iconSize / 2.0f, iconSize, iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled, contentColor);

      Vector2 textPos = {startX + iconSize + gap, contentY - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);

    } else if (iconName) {
      float iconSize = 24.0f;
      Rectangle iconRect = {contentX - iconSize / 2.0f,
                            contentY - iconSize / 2.0f, iconSize, iconSize};
      IconComponent::Render(iconName, iconRect, IconVariation::Filled, contentColor);

    } else if (label) {
      Font font          = Theme::GetFont(14, FontWeight::Medium);
      Vector2 textSize   = MeasureTextEx(font, label, 14, 1.0f);
      Vector2 textPos    = {contentX - textSize.x / 2.0f,
                            contentY - textSize.y / 2.0f};
      DrawTextEx(font, label, textPos, 14, 1.0f, contentColor);
    }

    // Input
    if (!isDisabled && !inputBlocked) {
      Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
      bool canProcessInput =
          InputLayerManager::ShouldProcessMouseInput(segmentBounds);
      bool isHovered = canProcessInput &&
                       CheckCollisionPointRec(mousePos, segmentBounds);
      bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
      bool isHovered = CheckCollisionPointRec(mousePos, segmentBounds);
      bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && isHovered) {
        focusedSegmentGroupId_ = thisGroup;
        focusedSegmentIndex_   = i;
      }
      if (isClicked) {
        if (multiSelect) {
          *selectedIndex ^= (1 << i);
        } else {
          *selectedIndex = i;
        }
        changed = true;
#if RAYM3_USE_INPUT_LAYERS
        InputLayerManager::ConsumeInput();
#endif
      }
    }
  }

  // Outer container outline
  Renderer::DrawRoundedRectangleEx(bounds, cornerRadius, scheme.outline, 1.0f);

  // Dividers between segments
  for (int i = 1; i < itemCount; ++i) {
    float x = bounds.x + i * segmentWidth;
    DrawLineEx({x, bounds.y}, {x, bounds.y + bounds.height}, 1.0f, scheme.outline);
  }

  return changed;
}

ComponentState SegmentedButtonComponent::GetState(Rectangle bounds) {
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
