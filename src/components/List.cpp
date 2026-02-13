#include "raym3/components/List.h"
#include "raym3/components/Icon.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/rendering/SvgRenderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/ClipScope.h"
#include <raylib.h>
#include <raymath.h>
#include <vector>
#include <string>
#include <algorithm>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

static ListSelectionCallback s_selectionCallback = nullptr;
static ListDragCallback s_dragCallback = nullptr;

// Drag state
static int s_draggingIndex = -1;
static int s_dragTargetIndex = -1;
static bool s_dragStarted = false;
static Vector2 s_dragStartPos = {0, 0};

// Keyboard navigation state
static int s_focusedIndex = -1;
static int s_anchorIndex = -1;
static bool s_listHasFocus = false;
static std::string s_typeaheadBuffer;
static float s_typeaheadTime = 0.0f;
static const float kTypeaheadTimeout = 0.5f;

struct FlatItem {
  ListItem *item;
  int depth;
};

static void FlattenVisibleItems(ListItem *items, int count, int depth, std::vector<FlatItem> &out) {
  for (int i = 0; i < count; i++) {
    out.push_back({&items[i], depth});
    if (items[i].expanded && items[i].children && items[i].childCount > 0) {
      FlattenVisibleItems(items[i].children, items[i].childCount, depth + 1, out);
    }
  }
}

bool ListIsDragging() { return s_draggingIndex != -1; }
int ListGetDragSourceIndex() { return s_draggingIndex; }
int ListGetDragTargetIndex() { return s_dragTargetIndex; }

static float RenderListItems(Rectangle bounds, ListItem *items, int itemCount,
                             int depth, float currentY, std::vector<Rectangle>* itemBoundsOut = nullptr) {
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
    
    // Store bounds for drag target calculation
    if (itemBoundsOut) {
      itemBoundsOut->push_back(itemBounds);
    }

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
    
    if (CheckCollisionPointRec(GetMousePosition(), itemBounds) && !item.disabled) {
      RequestCursor(MOUSE_CURSOR_POINTING_HAND);
    }

    ComponentState state = ComponentState::Default;
    if (item.disabled) {
      state = ComponentState::Disabled;
    } else if (isPressed) {
      state = ComponentState::Pressed;
    } else if (isHovered) {
      state = ComponentState::Hovered;
    }

    // Drag start detection
    if (item.enableDrag && isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      s_dragStartPos = mousePos;
      s_dragStarted = false;
    }
    
    // Check if drag should start (moved enough distance)
    if (item.enableDrag && s_draggingIndex == -1 && IsMouseButtonDown(MOUSE_BUTTON_LEFT) && isHovered) {
      float dragDist = Vector2Distance(mousePos, s_dragStartPos);
      if (dragDist > 5.0f) {
        s_draggingIndex = i;
        s_dragStarted = true;
      }
    }

    if (isVisible) {
      // Skip rendering the dragged item at its original position (show ghost instead)
      bool isDragSource = (s_draggingIndex == i);
      
      if (isDragSource) {
        // Draw semi-transparent version
        DrawRectangleRec(itemBounds, ColorAlpha(scheme.surfaceContainerHigh, 0.5f));
      } else {
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
      
      if (isDragSource) {
        contentColor = ColorAlpha(contentColor, 0.5f);
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

    // Don't process click if dragging
    if (isClicked && !item.disabled && s_draggingIndex == -1) {
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
      if (s_selectionCallback) {
        s_selectionCallback(&item, i);
      } else {
        item.selected = !item.selected;
      }
    }
    
    // Tooltip support
    if (item.tooltip && isHovered) {
      TooltipOptions tooltipOpts;
      tooltipOpts.placement = item.tooltipPlacement;
      Tooltip(itemBounds, item.tooltip, tooltipOpts);
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
                                 depth + 1, currentY, nullptr);
    }
  }
  return currentY;
}

void List(Rectangle bounds, ListItem *items, int itemCount, float *outHeight,
          ListSelectionCallback onSelectionChange, ListDragCallback onDragReorder) {
#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::RegisterBlockingRegion(bounds, true);
#endif

  s_selectionCallback = onSelectionChange;
  s_dragCallback = onDragReorder;
  
  // Focus management: click inside list to focus, click outside to blur
  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
      s_listHasFocus = true;
    } else {
      s_listHasFocus = false;
    }
  }
  
  // Update typeahead timeout
  if (s_typeaheadTime > 0.0f) {
    s_typeaheadTime -= GetFrameTime();
    if (s_typeaheadTime <= 0.0f) {
      s_typeaheadBuffer.clear();
    }
  }
  
  // Keyboard navigation (only when list has focus)
  bool isCtrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  bool isShiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  bool isSuperDown = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
  bool isCmdDown = isCtrlDown || isSuperDown;
  
  // Build flat list of all visible items (including expanded children)
  std::vector<FlatItem> flatItems;
  FlattenVisibleItems(items, itemCount, 0, flatItems);
  int flatCount = (int)flatItems.size();
  
  // Helper to clear selection across all visible items
  auto clearAllSelections = [&]() {
    for (auto &fi : flatItems) fi.item->selected = false;
  };
  
  // Initialize focus index when list gains focus
  if (s_listHasFocus && s_focusedIndex == -1 && flatCount > 0) {
    for (int i = 0; i < flatCount; i++) {
      if (flatItems[i].item->selected) {
        s_focusedIndex = i;
        break;
      }
    }
    if (s_focusedIndex == -1) s_focusedIndex = 0;
  }
  
  // Clamp focus index to flat list
  if (s_focusedIndex >= flatCount) s_focusedIndex = flatCount - 1;
  if (s_focusedIndex < 0 && flatCount > 0) s_focusedIndex = 0;
  
  if (s_listHasFocus && flatCount > 0) {
  ListItem *focused = flatItems[s_focusedIndex].item;
  
  // Up/Down navigation through flat visible list
  if (IsKeyPressed(KEY_UP) && s_focusedIndex > 0) {
    s_focusedIndex--;
    focused = flatItems[s_focusedIndex].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = s_focusedIndex;
      if (onSelectionChange) onSelectionChange(focused, s_focusedIndex);
    } else {
      if (s_anchorIndex == -1) s_anchorIndex = s_focusedIndex + 1;
      int start = std::min(s_anchorIndex, s_focusedIndex);
      int end = std::max(s_anchorIndex, s_focusedIndex);
      for (int i = 0; i < flatCount; i++) {
        flatItems[i].item->selected = (i >= start && i <= end);
      }
    }
  }
  
  if (IsKeyPressed(KEY_DOWN) && s_focusedIndex < flatCount - 1) {
    s_focusedIndex++;
    focused = flatItems[s_focusedIndex].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = s_focusedIndex;
      if (onSelectionChange) onSelectionChange(focused, s_focusedIndex);
    } else {
      if (s_anchorIndex == -1) s_anchorIndex = s_focusedIndex - 1;
      int start = std::min(s_anchorIndex, s_focusedIndex);
      int end = std::max(s_anchorIndex, s_focusedIndex);
      for (int i = 0; i < flatCount; i++) {
        flatItems[i].item->selected = (i >= start && i <= end);
      }
    }
  }
  
  // Home/End
  if (IsKeyPressed(KEY_HOME)) {
    s_focusedIndex = 0;
    focused = flatItems[0].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = 0;
    }
  }
  
  if (IsKeyPressed(KEY_END)) {
    s_focusedIndex = flatCount - 1;
    focused = flatItems[s_focusedIndex].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = s_focusedIndex;
    }
  }
  
  // Page Up/Down (10 items)
  if (IsKeyPressed(KEY_PAGE_UP) && s_focusedIndex > 0) {
    s_focusedIndex = std::max(0, s_focusedIndex - 10);
    focused = flatItems[s_focusedIndex].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = s_focusedIndex;
    }
  }
  
  if (IsKeyPressed(KEY_PAGE_DOWN) && s_focusedIndex < flatCount - 1) {
    s_focusedIndex = std::min(flatCount - 1, s_focusedIndex + 10);
    focused = flatItems[s_focusedIndex].item;
    if (!isShiftDown) {
      clearAllSelections();
      focused->selected = true;
      s_anchorIndex = s_focusedIndex;
    }
  }
  
  // Right arrow to expand
  if (IsKeyPressed(KEY_RIGHT)) {
    if (focused->childCount > 0 && !focused->expanded) {
      focused->expanded = true;
    }
  }
  
  // Left arrow to collapse, or jump to parent
  if (IsKeyPressed(KEY_LEFT)) {
    if (focused->childCount > 0 && focused->expanded) {
      focused->expanded = false;
    }
  }
  
  // Enter to toggle expand or activate
  if (IsKeyPressed(KEY_ENTER)) {
    if (focused->childCount > 0) {
      focused->expanded = !focused->expanded;
    } else if (onSelectionChange) {
      onSelectionChange(focused, s_focusedIndex);
    }
  }
  
  // Space to toggle selection
  if (IsKeyPressed(KEY_SPACE)) {
    focused->selected = !focused->selected;
  }
  
  // Ctrl+A to select all visible
  if (isCmdDown && IsKeyPressed(KEY_A)) {
    for (auto &fi : flatItems) fi.item->selected = true;
  }
  
  // Escape to clear selection
  if (IsKeyPressed(KEY_ESCAPE)) {
    clearAllSelections();
    s_anchorIndex = -1;
  }
  
  // Typeahead search across all visible items
  int key = GetCharPressed();
  while (key > 0) {
    if (key >= 32 && key <= 126) {
      s_typeaheadBuffer += (char)key;
      s_typeaheadTime = kTypeaheadTimeout;
      
      for (int i = 0; i < flatCount; i++) {
        if (flatItems[i].item->text) {
          std::string itemText = flatItems[i].item->text;
          std::string searchText = s_typeaheadBuffer;
          std::transform(itemText.begin(), itemText.end(), itemText.begin(), ::tolower);
          std::transform(searchText.begin(), searchText.end(), searchText.begin(), ::tolower);
          
          if (itemText.find(searchText) == 0) {
            s_focusedIndex = i;
            clearAllSelections();
            flatItems[i].item->selected = true;
            s_anchorIndex = i;
            if (onSelectionChange) onSelectionChange(flatItems[i].item, i);
            break;
          }
        }
      }
    }
    key = GetCharPressed();
  }
  } // end s_listHasFocus
  
  std::vector<Rectangle> itemBounds;
  float endY = RenderListItems(bounds, items, itemCount, 0, bounds.y, &itemBounds);
  
  // Handle drag target calculation and ghost line
  if (s_draggingIndex != -1) {
    Vector2 mousePos = GetMousePosition();
    ColorScheme &scheme = Theme::GetColorScheme();
    
    // Find target index based on mouse Y
    s_dragTargetIndex = -1;
    for (int i = 0; i < (int)itemBounds.size(); i++) {
      float midY = itemBounds[i].y + itemBounds[i].height / 2.0f;
      if (mousePos.y < midY) {
        s_dragTargetIndex = i;
        break;
      }
    }
    if (s_dragTargetIndex == -1) {
      s_dragTargetIndex = itemCount; // Insert at end
    }
    
    // Draw ghost line at target position
    float lineY = bounds.y;
    if (s_dragTargetIndex < (int)itemBounds.size()) {
      lineY = itemBounds[s_dragTargetIndex].y;
    } else if (!itemBounds.empty()) {
      lineY = itemBounds.back().y + itemBounds.back().height;
    }
    
    DrawRectangle((int)bounds.x, (int)lineY - 2, (int)bounds.width, 4, scheme.primary);
    
    // Handle drop
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      if (s_dragTargetIndex != s_draggingIndex && s_dragTargetIndex != s_draggingIndex + 1) {
        if (s_dragCallback) {
          s_dragCallback(s_draggingIndex, s_dragTargetIndex);
        }
      }
      s_draggingIndex = -1;
      s_dragTargetIndex = -1;
    }
  }
  
  if (outHeight) {
    *outHeight = endY - bounds.y;
  }
  s_selectionCallback = nullptr;
  s_dragCallback = nullptr;
}

} // namespace raym3

