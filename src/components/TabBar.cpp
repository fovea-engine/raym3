#include "raym3/components/TabBar.h"
#include "raym3/components/Icon.h"
#include "raym3/components/IconButton.h"
#include "raym3/components/Tooltip.h"
#include "raym3/ClipScope.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#include "raym3/input/RenderQueue.h"
#endif

namespace raym3 {

// Draw an inverted rounded corner (concave)
static void DrawInvertedCorner(Vector2 position, float radius, bool right, bool top, Color color) {
    // Fill the corner square
    // We want to fill the area "outside" the circle.
    // So we draw a rectangle of the *Tab* color, then subtract the circle?
    // No, we want to draw the *Inverse* shape. 
    // Actually, to make the tab flare into the background, we need to draw the *Tab Color*
    // in the concave shape.
    // The concave shape can be drawn by:
    // 1. Drawing a Rectangle of size (r,r)
    // 2. Drawing a Circle of size r in the *Background Color* (Transparent/Empty) on top?
    // If we can't subtract, we must draw the shape using a polygon/ring.
    
    // For a solid color tab against a solid color background, we can cheat:
    // Draw the tab rect.
    // Then draw the "flare" by drawing a rectangle of TAB color, 
    // then a circle of BACKGROUND color masking it?
    // But we don't know the background color easily (it's activeTabColor vs inactiveTabColor).
    // The "Gap" is InactiveTabColor (or Transparent). 
    // The Tab is ActiveTabColor.
    // We want the Tab to flare OUT.
    // So we draw a small rect of ActiveTabColor OUTSIDE the main tab rect.
    // Then we draw a circle of InactiveTabColor ON TOP of that small rect to cut it.
    
    // Wait, the "Line" below the tabs is usually ActiveTabColor (or divider color).
    // The user wants the selected tab to look like it rises from the content.
    
    // Let's assume InactiveTabColor is the background.
    // We need to pass the "Mask Color" (Background).
}

//-----------------------------------------------------------------------------
// Static state for hover tracking (per-instance)
//-----------------------------------------------------------------------------
static int s_hoveredTabIndex = -1;
static int s_hoveredInstanceId = -1;
static Rectangle s_tabContentBounds = {0, 0, 0, 0};
static bool s_tabContentClipEnabled = false;

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
//-----------------------------------------------------------------------------
// TabBarComponent::Render
//-----------------------------------------------------------------------------
int TabBarComponent::Render(Rectangle bounds, const std::vector<TabItem> &items,
                            int selectedIndex, const TabBarOptions &options,
                            int *closedTabIndex) {
  if (items.empty() && !options.onAddTab)
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
  bool addTabClicked = false;

  // Calculate tab dimensions
  int tabCount = static_cast<int>(items.size());
  
  // Reserve space for Add Button if present
  float addButtonWidth = (options.onAddTab) ? 32.0f : 0.0f;
  float availableWidth = bounds.width - addButtonWidth;

  float tabWidth = CalcTabWidth(availableWidth, tabCount, options.minTabWidth,
                                options.maxTabWidth);
  float tabHeight = options.tabHeight;

  // Track hover (Input Logic)
  Vector2 mousePos = GetMousePosition();
  bool mouseInBounds = CheckCollisionPointRec(mousePos, bounds);
  
  // Calculate Add Button Bounds
  Rectangle addButtonBounds = {
      bounds.x + (tabCount * tabWidth) + 4.0f,
      bounds.y + (tabHeight - 24.0f) / 2.0f,
      24.0f, 24.0f
  };
  
  // Check Add Button Click
  if (options.onAddTab) {
      if (CheckCollisionPointRec(mousePos, addButtonBounds)) {
          if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
              addTabClicked = true;
          }
      }
  }

  // Reset hover if mouse not in this TabBar's bounds or different instance
  if (!mouseInBounds || s_hoveredInstanceId != options.instanceId) {
    if (s_hoveredInstanceId == options.instanceId) {
      s_hoveredTabIndex = -1;
    }
  }
  
  // Check Tabs Input
  for (int i = 0; i < tabCount; i++) {
    Rectangle tabBounds = {
        bounds.x + i * tabWidth,
        bounds.y,
        tabWidth,
        tabHeight
    };

    bool isHovered = mouseInBounds && CheckCollisionPointRec(mousePos, tabBounds);
    if (isHovered) {
      s_hoveredTabIndex = i;
      s_hoveredInstanceId = options.instanceId;
      
      // Close button check
      float contentX = tabBounds.x + 8.0f;
      // ... simplified check logic ...
      float closeSize = 16.0f;
      Rectangle closeBtn = {
          tabBounds.x + tabBounds.width - 8.0f - closeSize,
          tabBounds.y + (tabHeight - closeSize) / 2.0f,
          closeSize, closeSize
      };
      
      if (items[i].closeable && CheckCollisionPointRec(mousePos, closeBtn)) {
          if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
              closedIdx = i;
          }
      } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && closedIdx != i && i != selectedIndex) {
          clickedIndex = i;
      }
    }
  }
  
  if (closedTabIndex) *closedTabIndex = closedIdx;
  if (addTabClicked && options.onAddTab) options.onAddTab();

  // Capture hover state for this instance
  int localHoveredTabIndex = (s_hoveredInstanceId == options.instanceId) ? s_hoveredTabIndex : -1;
  
  // Draw Function (Deferred or Immediate)
  auto drawFunc = [=]() {
      float r = options.cornerRadius;
      float topPadding = 5.0f;

      // PASS 1: Strip Background
      DrawRectangleRec(bounds, inactiveTabColor);

      // PASS 2: Inactive Tab Backgrounds/Hover (drawn BEFORE active tab flares)
      for (int i = 0; i < tabCount; i++) {
          if (i == selectedIndex) continue; // Skip active
          Rectangle tabBounds = {bounds.x + i * tabWidth, bounds.y, tabWidth, tabHeight};
          if (localHoveredTabIndex == i) {
              DrawRectangleRec(tabBounds, ColorAlpha(scheme.onSurface, 0.08f));
          }
      }

      // PASS 3: Active Tab Shape & Flares (drawn ON TOP of inactive bgs)
      if (selectedIndex >= 0 && selectedIndex < tabCount) {
          Rectangle tabBounds = {bounds.x + selectedIndex * tabWidth, bounds.y, tabWidth, tabHeight};
          
          float r = options.cornerRadius;
          float topPadding = 2.0f; 
          float x = floorf(tabBounds.x);
          float y = floorf(tabBounds.y + topPadding);
          float w = floorf(tabBounds.width);
          float h = floorf(tabBounds.height - topPadding);

          // Sequence: bottom horizontal line -> arch -> vertical line -> arch -> horizontal line -> arch -> vertical line -> arch -> horizontal line

          // --- 1. Main body rectangles (Center & Sides) ---
          // Center vertical strip
          DrawRectangleRec({x + r, y, w - 2*r, h}, activeTabColor);
          // Left vertical strip (connected to top-left arch and bottom-left flare)
          DrawRectangleRec({x, y + r, r, h - r + 0.5f}, activeTabColor);
          // Right vertical strip (connected to top-right arch and bottom-right flare)
          DrawRectangleRec({x + w - r, y + r, r, h - r + 0.5f}, activeTabColor);
          
          // --- 2. Top Rounded Corners (Convex Arches) ---
          DrawCircleSector({x + r, y + r}, r, 180, 270, 64, activeTabColor);
          DrawCircleSector({x + w - r, y + r}, r, 270, 360, 64, activeTabColor);
          
          // --- 3. Bottom Flares (Concave Arches) ---
          Color leftMaskColor = inactiveTabColor;
          if (selectedIndex > 0 && localHoveredTabIndex == selectedIndex - 1) {
              leftMaskColor = ColorAlphaBlend(inactiveTabColor, ColorAlpha(scheme.onSurface, 0.08f), WHITE);
          }
          
          Color rightMaskColor = inactiveTabColor;
          if (selectedIndex < tabCount - 1 && localHoveredTabIndex == selectedIndex + 1) {
              rightMaskColor = ColorAlphaBlend(inactiveTabColor, ColorAlpha(scheme.onSurface, 0.08f), WHITE);
          }

          // Left Flare
          // Draw a slightly larger base rectangle to ensure no gaps with the body
          DrawRectangleRec({x - r, y + h - r, r + 0.5f, r + 0.5f}, activeTabColor);
          // Draw the mask slightly larger to ensure it covers the edge completely
          DrawCircleSector({x - r, y + h - r}, r + 0.5f, 0, 90, 64, leftMaskColor);
          
          // Right Flare
          DrawRectangleRec({x + w - 0.5f, y + h - r, r + 0.5f, r + 0.5f}, activeTabColor);
          DrawCircleSector({x + w + r, y + h - r}, r + 0.5f, 90, 180, 64, rightMaskColor);
      }

      // PASS 4: All Tab Content (Text, Icons, Close Buttons)
      for (int i = 0; i < tabCount; i++) {
          Rectangle tabBounds = {bounds.x + i * tabWidth, bounds.y, tabWidth, tabHeight};
          bool isActive = (i == selectedIndex);
          bool prevIsActive = (i > 0 && i - 1 == selectedIndex);
          
          // Dividers
          if (options.showDividers && i > 0 && !isActive && !prevIsActive) {
              DrawLineEx({tabBounds.x, tabBounds.y + 7}, {tabBounds.x, tabBounds.y + tabHeight - 7}, 1.0f, dividerColor);
          }
          
          Color textColor = isActive ? activeTextColor : inactiveTextColor;
          FontWeight weight = isActive ? FontWeight::Medium : FontWeight::Regular;
          float contentX = tabBounds.x + 8.0f;
          float iconSize = 16.0f;
          
          if (items[i].iconName) {
              Rectangle iconBounds = {contentX, tabBounds.y + (tabHeight - iconSize) / 2.0f, iconSize, iconSize};
              IconComponent::Render(items[i].iconName, iconBounds, IconVariation::Filled, textColor);
              contentX += iconSize + 8.0f;
          }
          
          float availWidth = tabBounds.width - (contentX - tabBounds.x) - 8.0f;
          Vector2 textPos = {contentX, tabBounds.y + (tabHeight - 12.0f) / 2.0f};
          std::string title = TruncateText(items[i].title, availWidth, 12.0f);
          Renderer::DrawText(title.c_str(), textPos, 12.0f, textColor, weight);
          
          if (items[i].closeable && (isActive || localHoveredTabIndex == i || !options.showCloseOnHover)) {
              Rectangle closeBtn = {tabBounds.x + tabBounds.width - 8.0f - 16.0f, tabBounds.y + (tabHeight - 16.0f) / 2.0f, 16.0f, 16.0f};
              Color closeColor = (localHoveredTabIndex == i && CheckCollisionPointRec(GetMousePosition(), closeBtn)) ? scheme.error : textColor;
              IconComponent::Render("close", closeBtn, IconVariation::Filled, closeColor);
          }
      }
      
      // Draw Add Button
      if (options.onAddTab) {
          IconButtonComponent::Render(options.addTabIcon, addButtonBounds, ButtonVariant::Text, IconVariation::Filled);
      }
  };

  drawFunc();
#if 0
#if RAYM3_USE_INPUT_LAYERS
  // Defer drawing to ensure Z-order on top of content
  RenderQueue::RegisterComponent(ComponentType::Custom, [=](Rectangle r){ drawFunc(); }, 1, false);
#endif
#endif

  return clickedIndex;
}

//-----------------------------------------------------------------------------
// Tab Content Container
//-----------------------------------------------------------------------------

void TabContentBegin(Rectangle bounds, Color backgroundColor, bool clip) {
  s_tabContentBounds = bounds;
  s_tabContentClipEnabled = clip && bounds.width > 0 && bounds.height > 0;
  
  // Draw background with top corners flat (matches tab bottom)
  DrawRectangleRec(bounds, backgroundColor);
  
  // Begin scissor for content clipping (optional)
  if (s_tabContentClipEnabled) {
    PushClipRect(bounds);
  }
}

void TabContentEnd() {
  if (s_tabContentClipEnabled) {
    PopClipRect();
    s_tabContentClipEnabled = false;
  }
}

Rectangle GetTabContentScissorBounds() {
  if (s_tabContentClipEnabled) {
    return GetCurrentClipRect();
  }
  return {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
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
