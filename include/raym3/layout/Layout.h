#pragma once

#include <memory>
#include <raylib.h>
#include <vector>

// Forward declare Yoga types to avoid exposing Yoga headers in our public API
// if possible, but for simplicity in this wrapper we might need some enums. For
// now, we'll wrap the style in our own structs or use simple params.

namespace raym3 {

struct LayoutStyle {
  float width = -1.0f; // -1 means auto/undefined
  float height = -1.0f;
  float flexGrow = 0.0f;
  float flexShrink = 1.0f; // Default shrink
  // Add more as needed: padding, margin, etc.
  float padding = 0.0f;
  float gap = 0.0f;

  // Simple enums for direction/align
  int direction = 0; // 0: Row, 1: Column
  int justify = 0;   // 0: Start, 1: Center, 2: End, 3: SpaceBetween...
  int align = 0;     // 0: Auto, 1: FlexStart, 2: Center, 3: FlexEnd, 4: Stretch
  int flexWrap = 0;  // 0: NoWrap, 1: Wrap, 2: WrapReverse
};

class Layout {
public:
  // Initialize the layout system for a new frame
  static void Begin(Rectangle rootBounds);

  // Finalize layout calculation for the current frame
  static void End();

  // Start a new container node (Row/Column)
  // Returns the bounds calculated from the PREVIOUS frame (or 0 if first time)
  static Rectangle BeginContainer(LayoutStyle style);

  // End the current container
  static void EndContainer();

  // Allocate a leaf node (e.g. for a button)
  // Returns the bounds calculated from the PREVIOUS frame
  static Rectangle Alloc(LayoutStyle style);

  // Start a scrollable container
  // scrollX/scrollY enable scrolling on each axis
  static Rectangle BeginScrollContainer(LayoutStyle style, bool scrollX = false,
                                        bool scrollY = true);

  // Get current scroll offset for the active scroll container
  static Vector2 GetScrollOffset();

  // Set scroll offset for the active scroll container
  static void SetScrollOffset(Vector2 offset);

  // Check if a rectangle is visible within the current scroll container's
  // scissor bounds Returns true if the rectangle intersects with the visible
  // scissor area
  static bool IsRectVisibleInScrollContainer(Rectangle rect);

  // Helpers
  static LayoutStyle Row();
  static LayoutStyle Column();
  static LayoutStyle Flex(float grow = 1.0f);
  static LayoutStyle Fixed(float width, float height);

  // Debug visualization
  static void SetDebug(bool enabled);
  static void DrawDebug();
  // Register manual layout bounds for debug visualization
  static void RegisterDebugRect(Rectangle rect);

private:
  struct Impl;
  static std::unique_ptr<Impl> impl_;
};

} // namespace raym3
