#include "raym3/components/Menu.h"
#include "raym3/components/Dialog.h"
#include "raym3/layout/Layout.h"
#include "raym3/raym3.h" // For MenuItem definition
#include "raym3/rendering/Renderer.h"
#include "raym3/rendering/SvgRenderer.h" // For icons
#include "raym3/styles/Theme.h"
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

void MenuComponent::Render(Rectangle bounds, const MenuItem *items,
                           int itemCount, int *selected) {
  if (!items || itemCount <= 0)
    return;

  ColorScheme &scheme = Theme::GetColorScheme();
  float cornerRadius = Theme::GetShapeTokens().cornerMedium;

  float itemHeight = 48.0f;        // Min height 48dp
  float verticalPadding = 8.0f;    // Padding at top/bottom of menu list
  float horizontalPadding = 12.0f; // Padding for items inside menu
  float gapHeight = 8.0f;          // Height of the gap between sections

  // 1. Calculate layout and identify sections
  struct MenuSection {
    int startIndex;
    int count;
    float height;
    float yOffset;
  };

  // We can't use std::vector, so we'll do two passes or just use a fixed max
  // size if possible, or just iterate carefully. Let's do a pre-pass to
  // calculate total height and section bounds.

  float currentY = bounds.y;
  float sectionY = currentY;
  float sectionHeight = verticalPadding * 2; // Start with padding
  bool inSection = true;

  // We need to draw backgrounds FIRST, before any items.
  // So we need to know the bounds of each section.

  int sectionStart = 0;
  float currentSectionY = bounds.y;

  for (int i = 0; i < itemCount; i++) {
    if (items[i].isGap) {
      // End of current section
      Rectangle sectionBounds = {bounds.x, currentSectionY, bounds.width,
                                 sectionHeight};
      Renderer::DrawElevatedRectangle(sectionBounds, cornerRadius, 2,
                                      scheme.surfaceContainer);

      // Move to next section
      currentSectionY += sectionHeight + gapHeight;
      sectionHeight = verticalPadding * 2; // Reset for next section
      sectionStart = i + 1;
      continue;
    }

    if (items[i].isDivider) {
      sectionHeight += 1.0f + 16.0f;
    } else {
      sectionHeight += itemHeight;
    }

    // If this is the last item, draw the last section
    if (i == itemCount - 1) {
      Rectangle sectionBounds = {bounds.x, currentSectionY, bounds.width,
                                 sectionHeight};
      Renderer::DrawElevatedRectangle(sectionBounds, cornerRadius, 2,
                                      scheme.surfaceContainer);
    }
  }

  // 2. Render Items
  currentY = bounds.y + verticalPadding;
  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();

  for (int i = 0; i < itemCount; i++) {
    if (items[i].isGap) {
      currentY += verticalPadding; // Finish previous section padding
      currentY += gapHeight;       // Add gap
      currentY += verticalPadding; // Start next section padding
      continue;
    }

    if (items[i].isDivider) {
      // Draw Divider
      float dividerY = currentY + 8.0f; // Center in padding
      Rectangle dividerBounds = {bounds.x, dividerY, bounds.width, 1.0f};
      DrawLineEx({dividerBounds.x, dividerBounds.y},
                 {dividerBounds.x + dividerBounds.width, dividerBounds.y}, 1.0f,
                 scheme.outlineVariant);
      currentY += 17.0f; // 1px + 16px padding
      continue;
    }

    Rectangle itemBounds = {bounds.x + horizontalPadding, currentY,
                            bounds.width - horizontalPadding * 2, itemHeight};
    float itemCornerRadius = Theme::GetShapeTokens().cornerSmall;

    ComponentState state = items[i].disabled
                               ? ComponentState::Disabled
                               : GetItemState(itemBounds, i, selected);

    if (inputBlocked && state != ComponentState::Disabled) {
      state = ComponentState::Default;
    }

    bool isSelected = (selected && *selected == i);

    // Background for selected item
    if (isSelected) {
      Renderer::DrawRoundedRectangle(itemBounds, itemCornerRadius,
                                     scheme.secondaryContainer);
    }

    // Text Color
    Color textColor = items[i].disabled ? ColorAlpha(scheme.onSurface, 0.38f)
                                        : scheme.onSurface;
    if (isSelected)
      textColor = scheme.onSecondaryContainer;

    // State Layer
    if (!items[i].disabled) {
      Color stateBaseColor =
          isSelected ? scheme.onSecondaryContainer : scheme.onSurface;
      Renderer::DrawStateLayer(itemBounds, itemCornerRadius, stateBaseColor,
                               state);
    }

    // Content Layout
    float contentX = itemBounds.x + 12.0f;
    float centerY = itemBounds.y + itemHeight / 2.0f;

    // Leading Icon
    if (items[i].leadingIcon) {
      Rectangle iconRect = {contentX, centerY - 12.0f, 24.0f, 24.0f};
      Color iconColor = items[i].disabled ? ColorAlpha(scheme.onSurface, 0.38f)
                                          : scheme.onSurfaceVariant;
      if (isSelected)
        iconColor = scheme.onSecondaryContainer;

      SvgRenderer::DrawIcon(items[i].leadingIcon, iconRect,
                            IconVariation::Filled, iconColor);
      contentX += 24.0f + 12.0f; // Icon size + gap
    }

    // Text
    if (items[i].text) {
      Vector2 textPos = {contentX,
                         centerY - 7.0f}; // Centered vertically (approx)
      Renderer::DrawText(items[i].text, textPos, 14.0f, textColor,
                         FontWeight::Regular);
    }

    // Trailing Text / Icon
    if (items[i].trailingText) {
      Vector2 textSize =
          MeasureTextEx(GetFontDefault(), items[i].trailingText, 14.0f, 1.0f);
      Vector2 textPos = {itemBounds.x + itemBounds.width - textSize.x - 12.0f,
                         centerY - 7.0f};
      Renderer::DrawText(items[i].trailingText, textPos, 14.0f, textColor,
                         FontWeight::Regular);
    } else if (items[i].trailingIcon) {
      Rectangle iconRect = {itemBounds.x + itemBounds.width - 24.0f - 12.0f,
                            centerY - 12.0f, 24.0f, 24.0f};
      Color iconColor = items[i].disabled ? ColorAlpha(scheme.onSurface, 0.38f)
                                          : scheme.onSurfaceVariant;
      if (isSelected)
        iconColor = scheme.onSecondaryContainer;
      SvgRenderer::DrawIcon(items[i].trailingIcon, iconRect,
                            IconVariation::Filled, iconColor);
    }

    // Interaction
    bool canInteract = !items[i].disabled && !inputBlocked;

#if RAYM3_USE_INPUT_LAYERS
    bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(itemBounds);
    if (canInteract && canProcessInput && CheckCollisionPointRec(GetMousePosition(), itemBounds) &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      if (selected) {
        *selected = i;
      }
      InputLayerManager::ConsumeInput();
    }
#else
    if (canInteract && CheckCollisionPointRec(GetMousePosition(), itemBounds) &&
        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      if (selected) {
        *selected = i;
      }
    }
#endif

    currentY += itemHeight;
  }
}

ComponentState MenuComponent::GetItemState(Rectangle itemBounds, int index,
                                           int *selected) {
  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  int menuLayerId = InputLayerManager::GetCurrentLayerId();
  bool isVisible = (menuLayerId >= 100) ? true : Layout::IsRectVisibleInScrollContainer(itemBounds);
  bool canProcessInput = isVisible && InputLayerManager::ShouldProcessMouseInput(itemBounds, menuLayerId);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, itemBounds);
#else
  bool isHovered = CheckCollisionPointRec(mousePos, itemBounds);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  if (isPressed)
    return ComponentState::Pressed;
  if (isHovered)
    return ComponentState::Hovered;
  return ComponentState::Default;
}

} // namespace raym3
