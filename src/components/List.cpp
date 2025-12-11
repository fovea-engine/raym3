#include "raym3/components/List.h"
#include "raym3/components/Icon.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/rendering/SvgRenderer.h"
#include "raym3/styles/Theme.h"
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static ListSelectionCallback s_selectionCallback = nullptr;

static float RenderListItems(Rectangle bounds, ListItem *items, int itemCount,
                             int depth, float currentY) {
  if (!items || itemCount <= 0)
    return currentY;

#if RAYM3_USE_INPUT_LAYERS
  int listLayerId = InputLayerManager::GetCurrentLayerId();
#endif

  ColorScheme &scheme = Theme::GetColorScheme();
  float itemHeight = 48.0f;
  float indentPerLevel = 16.0f;
  float basePadding = 16.0f;

  for (int i = 0; i < itemCount; ++i) {
    ListItem &item = items[i];

    Rectangle itemBounds = {bounds.x, currentY, bounds.width, itemHeight};

    bool isVisible = Layout::IsRectVisibleInScrollContainer(itemBounds);

    Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
    bool canProcessInput =
        isVisible &&
        InputLayerManager::ShouldProcessMouseInput(itemBounds, listLayerId);
    bool isHovered =
        canProcessInput && CheckCollisionPointRec(mousePos, itemBounds);
#else
    bool isHovered = isVisible && CheckCollisionPointRec(mousePos, itemBounds);
#endif
    bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool isClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    ComponentState state = ComponentState::Default;
    if (item.disabled) {
      state = ComponentState::Disabled;
    } else if (isPressed) {
      state = ComponentState::Pressed;
    } else if (isHovered) {
      state = ComponentState::Hovered;
    }

    if (isVisible) {
      if (item.selected) {
        Renderer::DrawRoundedRectangle(itemBounds,
                                       Theme::GetShapeTokens().cornerSmall,
                                       scheme.secondaryContainer);
      } else if (item.backgroundColor.a > 0) {
        Renderer::DrawRoundedRectangle(itemBounds,
                                       Theme::GetShapeTokens().cornerSmall,
                                       item.backgroundColor);
      }

      if (!item.disabled) {
        Color stateBaseColor =
            item.selected ? scheme.onSecondaryContainer : scheme.onSurface;
        if (item.textColor.a > 0)
          stateBaseColor = item.textColor;

        Renderer::DrawStateLayer(itemBounds,
                                 Theme::GetShapeTokens().cornerSmall,
                                 stateBaseColor, state);
      }

      float contentX = itemBounds.x + basePadding + (depth * indentPerLevel);
      float centerY = itemBounds.y + itemHeight / 2.0f;

      Color contentColor =
          item.selected ? scheme.onSecondaryContainer : scheme.onSurface;
      if (item.disabled) {
        contentColor = ColorAlpha(contentColor, 0.38f);
      } else {
        if (item.textColor.a > 0)
          contentColor = item.textColor;
      }

      if (item.leadingIcon) {
        Rectangle iconRect = {contentX, centerY - 12.0f, 24.0f, 24.0f};
        Color currentIconColor = contentColor;
        if (!item.disabled && item.iconColor.a > 0)
          currentIconColor = item.iconColor;

        SvgRenderer::DrawIcon(item.leadingIcon, iconRect, IconVariation::Filled,
                              currentIconColor);
        contentX += 24.0f + 16.0f;
      }

      if (item.text) {
        Vector2 textPos = {contentX, centerY - 7.0f};
        Renderer::DrawText(item.text, textPos, 14.0f, contentColor,
                           FontWeight::Regular);
      }

      if (item.secondaryActionIcon || (item.childCount > 0)) {
        const char *iconName = item.secondaryActionIcon;
        if (!iconName && item.childCount > 0) {
          iconName = item.expanded ? "expand_less" : "expand_more";
        }

        if (iconName) {
          float iconSize = 24.0f;
          float buttonSize = 48.0f;
          Rectangle actionBounds = {itemBounds.x + itemBounds.width -
                                        buttonSize,
                                    itemBounds.y, buttonSize, buttonSize};

#if RAYM3_USE_INPUT_LAYERS
          bool actionCanProcessInput =
              isVisible && InputLayerManager::ShouldProcessMouseInput(
                               actionBounds, listLayerId);
          bool actionHovered = actionCanProcessInput &&
                               CheckCollisionPointRec(mousePos, actionBounds);
#else
          bool actionHovered =
              isVisible && CheckCollisionPointRec(mousePos, actionBounds);
#endif
          bool actionClicked =
              actionHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

          Rectangle iconRect = {actionBounds.x + (buttonSize - iconSize) / 2.0f,
                                actionBounds.y + (buttonSize - iconSize) / 2.0f,
                                iconSize, iconSize};

          if (actionHovered && !item.disabled) {
            Renderer::DrawStateLayer(actionBounds, 24.0f, contentColor,
                                     ComponentState::Hovered);
          }

          SvgRenderer::DrawIcon(iconName, iconRect, IconVariation::Filled,
                                contentColor);

          if (actionClicked && !item.disabled) {
            if (item.childCount > 0) {
              item.expanded = !item.expanded;
            }
            isClicked = false;
          }
        }
      }
    }

    if (isClicked && !item.disabled) {
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
      if (s_selectionCallback) {
        s_selectionCallback(&item, i);
      } else {
        item.selected = !item.selected;
      }
    }

#if RAYM3_USE_INPUT_LAYERS
    if (isHovered || isPressed) {
      InputLayerManager::RegisterBlockingRegion(itemBounds, true);
    }
#endif

    currentY += itemHeight;

    // Recursively render children if expanded
    if (item.expanded && item.children && item.childCount > 0) {
      currentY = RenderListItems(bounds, item.children, item.childCount,
                                 depth + 1, currentY);
    }
  }
  return currentY;
}

void List(Rectangle bounds, ListItem *items, int itemCount, float *outHeight,
          ListSelectionCallback onSelectionChange) {
#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::RegisterBlockingRegion(bounds, true);
#endif

  s_selectionCallback = onSelectionChange;
  float endY = RenderListItems(bounds, items, itemCount, 0, bounds.y);
  if (outHeight) {
    *outHeight = endY - bounds.y;
  }
  s_selectionCallback = nullptr;
}

} // namespace raym3
