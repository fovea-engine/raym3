#include "raym3/components/TabBar.h"
#include "raym3/components/Icon.h"
#include "raym3/components/IconButton.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

//-----------------------------------------------------------------------------
// Static state for hover tracking
//-----------------------------------------------------------------------------
static int s_hoveredTabIndex = -1;
static Rectangle s_tabContentBounds = {0, 0, 0, 0};

//-----------------------------------------------------------------------------
// Helper: Calculate tab width based on available space
//-----------------------------------------------------------------------------
static float CalcTabWidth(float availableWidth, int tabCount, float minWidth,
                          float maxWidth) {
  if (tabCount <= 0)
    return 0.0f;
  float idealWidth = availableWidth / tabCount;
  return std::clamp(idealWidth, minWidth, maxWidth);
}

//-----------------------------------------------------------------------------
// Helper: Truncate text with ellipsis
//-----------------------------------------------------------------------------
std::string TabBarComponent::TruncateText(const std::string &text,
                                          float maxWidth, float fontSize) {
  Vector2 size = Renderer::MeasureText(text.c_str(), fontSize, FontWeight::Regular);
  if (size.x <= maxWidth) return text;

  std::string truncated = text;
  const std::string ellipsis = "...";
  Vector2 ellipsisSize = Renderer::MeasureText(ellipsis.c_str(), fontSize, FontWeight::Regular);

  while (!truncated.empty()) {
    truncated.pop_back();
    Vector2 truncSize = Renderer::MeasureText(truncated.c_str(), fontSize, FontWeight::Regular);
    if (truncSize.x + ellipsisSize.x <= maxWidth) {
      return truncated + ellipsis;
    }
  }

  return ellipsis;
}

//-----------------------------------------------------------------------------
// TabBarComponent::Render
//-----------------------------------------------------------------------------
int TabBarComponent::Render(Rectangle bounds, const std::vector<TabItem> &items,
                            int selectedIndex, const TabBarOptions &options,
                            int *closedTabIndex) {
  if (items.empty())
    return -1;

  ColorScheme &scheme = Theme::GetColorScheme();

  // Resolve colors
  Color activeTabColor = (options.activeTabColor.a == 0) 
      ? scheme.surface 
      : options.activeTabColor;
  Color inactiveTabColor = (options.inactiveTabColor.a == 0)
      ? scheme.surfaceContainerHighest
      : options.inactiveTabColor;
  Color activeTextColor = (options.activeTextColor.a == 0)
      ? scheme.onSurface
      : options.activeTextColor;
  Color inactiveTextColor = (options.inactiveTextColor.a == 0)
      ? scheme.onSurfaceVariant
      : options.inactiveTextColor;
  Color dividerColor = (options.dividerColor.a == 0)
      ? scheme.outlineVariant
      : options.dividerColor;

  int clickedIndex = -1;
  int closedIdx = -1;

  // Calculate tab dimensions
  int tabCount = static_cast<int>(items.size());
  float tabWidth = CalcTabWidth(bounds.width, tabCount, options.minTabWidth,
                                options.maxTabWidth);
  float tabHeight = options.tabHeight;

  // Draw tab strip background (inactive area)
  DrawRectangleRec(bounds, inactiveTabColor);

  // Get mouse state
  Vector2 mousePos = GetMousePosition();
  bool mouseInBounds = CheckCollisionPointRec(mousePos, bounds);

  // First pass: render inactive tabs
  for (int i = 0; i < tabCount; i++) {
    if (i == selectedIndex)
      continue; // Skip active tab for now

    Rectangle tabBounds = {
        bounds.x + i * tabWidth,
        bounds.y,
        tabWidth,
        tabHeight
    };

    bool isHovered = mouseInBounds && CheckCollisionPointRec(mousePos, tabBounds);
    if (isHovered)
      s_hoveredTabIndex = i;

    bool prevIsActive = (i > 0 && i - 1 == selectedIndex);
    bool nextIsActive = (i < tabCount - 1 && i + 1 == selectedIndex);
    bool showLeftDivider = options.showDividers && i > 0 && !prevIsActive;
    bool showRightDivider = options.showDividers && i < tabCount - 1 && !nextIsActive;

    // Draw inactive tab background
    if (isHovered) {
      // Hover state - slightly lighter
      Color hoverColor = ColorAlpha(scheme.onSurface, 0.08f);
      DrawRectangleRec(tabBounds, hoverColor);
    }

    // Draw dividers
    if (showLeftDivider) {
      float dividerHeight = 20.0f;
      float dividerY = tabBounds.y + (tabHeight - dividerHeight) / 2.0f;
      DrawLineEx({tabBounds.x, dividerY}, {tabBounds.x, dividerY + dividerHeight},
                 1.0f, dividerColor);
    }

    // Draw content
    float padding = 8.0f;
    float contentX = tabBounds.x + padding;
    float iconSize = 16.0f;
    float closeSize = 16.0f;
    float gap = 8.0f;

    // Icon/Favicon
    if (items[i].iconName) {
      Rectangle iconBounds = {contentX, tabBounds.y + (tabHeight - iconSize) / 2.0f,
                              iconSize, iconSize};
      IconComponent::Render(items[i].iconName, iconBounds, IconVariation::Filled,
                            inactiveTextColor);
      contentX += iconSize + gap;
    }

    // Calculate available width for text
    float closeButtonSpace = (items[i].closeable && (isHovered || options.showCloseOnHover == false)) 
        ? closeSize + gap : 0;
    float availableTextWidth = tabBounds.width - (contentX - tabBounds.x) - padding - closeButtonSpace;

    // Title
    std::string displayTitle = TruncateText(items[i].title, availableTextWidth, 12.0f);
    Vector2 textPos = {contentX, tabBounds.y + (tabHeight - 12.0f) / 2.0f};
    Renderer::DrawText(displayTitle.c_str(), textPos, 12.0f, inactiveTextColor,
                       FontWeight::Regular);

    // Close button
    if (items[i].closeable && (isHovered || !options.showCloseOnHover)) {
      Rectangle closeBtn = {
          tabBounds.x + tabBounds.width - padding - closeSize,
          tabBounds.y + (tabHeight - closeSize) / 2.0f,
          closeSize,
          closeSize
      };

      bool closeHovered = CheckCollisionPointRec(mousePos, closeBtn);
      if (closeHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        closedIdx = i;
      }

      Color closeColor = closeHovered ? scheme.error : inactiveTextColor;
      IconComponent::Render("close", closeBtn, IconVariation::Filled, closeColor);
    }

    // Click detection
    if (isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && closedIdx != i) {
      clickedIndex = i;
    }
  }

  // Second pass: render active tab on top
  if (selectedIndex >= 0 && selectedIndex < tabCount) {
    Rectangle tabBounds = {
        bounds.x + selectedIndex * tabWidth,
        bounds.y,
        tabWidth,
        tabHeight
    };

    bool isHovered = CheckCollisionPointRec(mousePos, tabBounds);

    // Draw active tab with rounded top corners
    // We draw a custom shape: rounded top, flat bottom
    float r = options.cornerRadius;
    
    // Main body (below the rounded corners)
    Rectangle bodyRect = {tabBounds.x, tabBounds.y + r, 
                          tabBounds.width, tabBounds.height - r};
    DrawRectangleRec(bodyRect, activeTabColor);

    // Top-left rounded corner
    DrawCircleSector({tabBounds.x + r, tabBounds.y + r}, r, 180, 270, 16, activeTabColor);
    // Top-right rounded corner  
    DrawCircleSector({tabBounds.x + tabBounds.width - r, tabBounds.y + r}, r, 270, 360, 16, activeTabColor);
    // Top middle rectangle
    Rectangle topRect = {tabBounds.x + r, tabBounds.y, 
                         tabBounds.width - 2 * r, r};
    DrawRectangleRec(topRect, activeTabColor);

    // Draw content
    float padding = 8.0f;
    float contentX = tabBounds.x + padding;
    float iconSize = 16.0f;
    float closeSize = 16.0f;
    float gap = 8.0f;

    // Icon/Favicon
    if (items[selectedIndex].iconName) {
      Rectangle iconBounds = {contentX, tabBounds.y + (tabHeight - iconSize) / 2.0f,
                              iconSize, iconSize};
      IconComponent::Render(items[selectedIndex].iconName, iconBounds, 
                            IconVariation::Filled, activeTextColor);
      contentX += iconSize + gap;
    }

    // Calculate available width for text
    float closeButtonSpace = items[selectedIndex].closeable ? closeSize + gap : 0;
    float availableTextWidth = tabBounds.width - (contentX - tabBounds.x) - padding - closeButtonSpace;

    // Title (use medium weight for active)
    std::string displayTitle = TruncateText(items[selectedIndex].title, availableTextWidth, 12.0f);
    Vector2 textPos = {contentX, tabBounds.y + (tabHeight - 12.0f) / 2.0f};
    Renderer::DrawText(displayTitle.c_str(), textPos, 12.0f, activeTextColor,
                       FontWeight::Medium);

    // Close button (always visible for active tab)
    if (items[selectedIndex].closeable) {
      Rectangle closeBtn = {
          tabBounds.x + tabBounds.width - padding - closeSize,
          tabBounds.y + (tabHeight - closeSize) / 2.0f,
          closeSize,
          closeSize
      };

      bool closeHovered = CheckCollisionPointRec(mousePos, closeBtn);
      if (closeHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        closedIdx = selectedIndex;
      }

      Color closeColor = closeHovered ? scheme.error : activeTextColor;
      IconComponent::Render("close", closeBtn, IconVariation::Filled, closeColor);
    }
  }

  // Draw trailing divider if there's space after the last tab
  if (options.showDividers) {
    float totalTabsWidth = tabCount * tabWidth;
    if (totalTabsWidth < bounds.width) {
      float dividerX = bounds.x + totalTabsWidth;
      float dividerHeight = 20.0f;
      float dividerY = bounds.y + (tabHeight - dividerHeight) / 2.0f;
      DrawLineEx({dividerX, dividerY}, {dividerX, dividerY + dividerHeight},
                 1.0f, dividerColor);
    }
  }

  if (closedTabIndex)
    *closedTabIndex = closedIdx;

  return clickedIndex;
}

//-----------------------------------------------------------------------------
// Tab Content Container
//-----------------------------------------------------------------------------

void TabContentBegin(Rectangle bounds, Color backgroundColor) {
  s_tabContentBounds = bounds;
  
  // Draw background with top corners flat (matches tab bottom)
  DrawRectangleRec(bounds, backgroundColor);
  
  // Begin scissor for content clipping
  BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
}

void TabContentEnd() {
  EndScissorMode();
}

//-----------------------------------------------------------------------------
// Convenience API
//-----------------------------------------------------------------------------

int TabBar(Rectangle bounds, const std::vector<TabItem> &items,
           int selectedIndex, const TabBarOptions &options,
           int *closedTabIndex) {
  return TabBarComponent::Render(bounds, items, selectedIndex, options, closedTabIndex);
}

} // namespace raym3
