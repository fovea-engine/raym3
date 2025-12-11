#include "raym3/layout/Layout.h"
#include <map>
#include <string>
#include <vector>

#if RAYM3_USE_YOGA
#include <yoga/Yoga.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

struct LayoutNodeInfo {
  YGNodeRef node = nullptr;
  Rectangle cachedBounds = {0, 0, 0, 0};
  int id = 0;
};

struct ScrollContainerState {
  Vector2 scrollOffset = {0, 0};
  Vector2 contentSize = {0, 0};
  Rectangle bounds = {0, 0, 0, 0};
  bool scrollX = false;
  bool scrollY = true;
  bool isDragging = false;
  Vector2 dragStart = {0, 0};
  int nodeId = -1;
};

struct Layout::Impl {
  // Current frame state
  YGNodeRef root = nullptr;
  std::vector<YGNodeRef> nodeStack;
  std::vector<bool> nodeIsScrollContainer;
  int currentNodeId = 0;

  // Scroll container stack
  std::vector<ScrollContainerState> scrollStack;

  // Persistent state (mapped by ID order for simplicity in immediate mode)
  // In a real immediate mode system, we might use a hash of the path or ID
  // stack. For this simple implementation, we'll assume deterministic call
  // order.
  std::vector<Rectangle> previousFrameBounds;
  std::vector<Rectangle> currentFrameBounds;
  std::map<int, ScrollContainerState> scrollStates;

  Impl() {
    // Initialize config if needed
  }

  ~Impl() {
    if (root) {
      YGNodeFreeRecursive(root);
    }
  }

  void Begin(Rectangle rootBounds) {
    // Clear current frame state
    if (root) {
      YGNodeFreeRecursive(root);
    }
    nodeStack.clear();
    nodeIsScrollContainer.clear();
    scrollStack.clear();
    currentFrameBounds.clear();
    currentNodeId = 0;

    // Create root
    root = YGNodeNew();
    YGNodeStyleSetWidth(root, rootBounds.width);
    YGNodeStyleSetHeight(root, rootBounds.height);

    // Default root style?
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionColumn);

    nodeStack.push_back(root);

    // Store root bounds as first entry
    currentFrameBounds.push_back(rootBounds);
    currentNodeId++;
  }

  void End() {
    if (!root)
      return;

    // Calculate layout
    YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);

    // Traverse and store bounds for next frame
    StoreCalculatedBounds(root, 0, 0);

    // Swap buffers
    previousFrameBounds = currentFrameBounds;
  }

  void StoreCalculatedBounds(YGNodeRef node, float parentX, float parentY) {
    float x = parentX + YGNodeLayoutGetLeft(node);
    float y = parentY + YGNodeLayoutGetTop(node);
    float w = YGNodeLayoutGetWidth(node);
    float h = YGNodeLayoutGetHeight(node);

    // We already pushed the bounds in Alloc/BeginContainer order?
    // No, we need to update them.
    // Actually, the 'currentFrameBounds' vector was populated with placeholders
    // or inputs. But we need to map the *calculated* bounds to the *call
    // order*. Since we traverse the tree recursively, and we built it
    // recursively, a preorder traversal should match the build order if we are
    // careful.

    // Wait, the build order is:
    // BeginContainer (Parent)
    //   Alloc (Child 1)
    //   Alloc (Child 2)
    // EndContainer

    // The tree structure is:
    // Parent -> [Child 1, Child 2]

    // If we traverse Parent, then Child 1, then Child 2, it matches call order.
    // So we can just iterate and fill?
    // But we need to write to the correct index.
    // Let's just rebuild the list during traversal?
    // No, 'currentFrameBounds' was used to store the *result*?
    // Let's clear 'currentFrameBounds' at End() and refill it.

    // Actually, let's use a separate vector for results.
  }

  // Revised strategy for bounds storage:
  // We need to store the bounds *as we calculate them* at the end of the frame,
  // so they are ready for the *next* frame's Begin/Alloc calls.
  // The Begin/Alloc calls read from 'previousFrameBounds' using an index.
};

// Static instance
std::unique_ptr<Layout::Impl> Layout::impl_ = std::make_unique<Layout::Impl>();

// We need to implement the traversal properly in End().
// But first, let's implement the helpers.

static void ApplyStyle(YGNodeRef node, LayoutStyle style) {
  if (style.width >= 0)
    YGNodeStyleSetWidth(node, style.width);
  if (style.height >= 0)
    YGNodeStyleSetHeight(node, style.height);

  YGNodeStyleSetFlexGrow(node, style.flexGrow);
  YGNodeStyleSetFlexShrink(node, style.flexShrink);

  if (style.padding > 0)
    YGNodeStyleSetPadding(node, YGEdgeAll, style.padding);
  if (style.gap > 0)
    YGNodeStyleSetGap(node, YGGutterAll, style.gap);

  // Direction
  if (style.direction == 0)
    YGNodeStyleSetFlexDirection(node, YGFlexDirectionRow);
  else
    YGNodeStyleSetFlexDirection(node, YGFlexDirectionColumn);

  // Justify
  switch (style.justify) {
  case 0:
    YGNodeStyleSetJustifyContent(node, YGJustifyFlexStart);
    break;
  case 1:
    YGNodeStyleSetJustifyContent(node, YGJustifyCenter);
    break;
  case 2:
    YGNodeStyleSetJustifyContent(node, YGJustifyFlexEnd);
    break;
  case 3:
    YGNodeStyleSetJustifyContent(node, YGJustifySpaceBetween);
    break;
  case 4:
    YGNodeStyleSetJustifyContent(node, YGJustifySpaceAround);
    break;
  case 5:
    YGNodeStyleSetJustifyContent(node, YGJustifySpaceEvenly);
    break;
  }

  // Align Items
  switch (style.align) {
  case 0:
    YGNodeStyleSetAlignItems(node, YGAlignStretch);
    break;
  case 1:
    YGNodeStyleSetAlignItems(node, YGAlignFlexStart);
    break;
  case 2:
    YGNodeStyleSetAlignItems(node, YGAlignCenter);
    break;
  case 3:
    YGNodeStyleSetAlignItems(node, YGAlignFlexEnd);
    break;
  case 4:
    YGNodeStyleSetAlignItems(node, YGAlignStretch);
    break;
  }

  // Flex Wrap
  switch (style.flexWrap) {
  case 0:
    YGNodeStyleSetFlexWrap(node, YGWrapNoWrap);
    break;
  case 1:
    YGNodeStyleSetFlexWrap(node, YGWrapWrap);
    break;
  case 2:
    YGNodeStyleSetFlexWrap(node, YGWrapWrapReverse);
    break;
  }
}

Rectangle Layout::BeginContainer(LayoutStyle style) {
  YGNodeRef node = YGNodeNew();
  ApplyStyle(node, style);

  // Add to current parent
  if (!impl_->nodeStack.empty()) {
    YGNodeRef parent = impl_->nodeStack.back();
    YGNodeInsertChild(parent, node, YGNodeGetChildCount(parent));
  }

  impl_->nodeStack.push_back(node);

  // Return bounds from previous frame
  int id = impl_->currentNodeId++;
  if (id < impl_->previousFrameBounds.size()) {
    return impl_->previousFrameBounds[id];
  }
  return {0, 0, 0, 0}; // Default if new
}

void Layout::EndContainer() {
  if (impl_->nodeStack.size() > 1) {
    // Check if this container was a scroll container
    if (!impl_->nodeIsScrollContainer.empty() &&
        impl_->nodeIsScrollContainer.back()) {
      // Pop the scroll stack
      if (!impl_->scrollStack.empty()) {
        impl_->scrollStack.pop_back();
      }
    }

    impl_->nodeStack.pop_back();
    if (!impl_->nodeIsScrollContainer.empty()) {
      impl_->nodeIsScrollContainer.pop_back();
    }
  }
}

Rectangle Layout::Alloc(LayoutStyle style) {
  YGNodeRef node = YGNodeNew();
  ApplyStyle(node, style);

  if (!impl_->nodeStack.empty()) {
    YGNodeRef parent = impl_->nodeStack.back();
    YGNodeInsertChild(parent, node, YGNodeGetChildCount(parent));
  }

  // Return bounds
  int id = impl_->currentNodeId++;
  if (id < impl_->previousFrameBounds.size()) {
    return impl_->previousFrameBounds[id];
  }
  return {0, 0, 0, 0};
}

// Helpers
LayoutStyle Layout::Row() { return LayoutStyle{.direction = 0}; }
LayoutStyle Layout::Column() { return LayoutStyle{.direction = 1}; }
LayoutStyle Layout::Flex(float grow) { return LayoutStyle{.flexGrow = grow}; }
LayoutStyle Layout::Fixed(float width, float height) {
  return LayoutStyle{
      .width = width, .height = height, .flexGrow = 0, .flexShrink = 0};
}

} // namespace raym3

// Re-implement End() with the traversal logic
namespace raym3 {

// We need to store root offset to add it to results
static float rootOffsetX = 0;
static float rootOffsetY = 0;

void Layout::Begin(Rectangle rootBounds) {
  rootOffsetX = rootBounds.x;
  rootOffsetY = rootBounds.y;
  impl_->Begin(rootBounds);
}

void Layout::End() {
  if (!impl_->root)
    return;

  YGNodeCalculateLayout(impl_->root, YGUndefined, YGUndefined, YGDirectionLTR);

  impl_->currentFrameBounds.clear();

  // Track node index for matching with scroll states
  int nodeIndex = 0;

  // Recursive lambda
  auto traverse = [&](auto &&self, YGNodeRef node, float x, float y,
                      int &idx) -> void {
    float left = YGNodeLayoutGetLeft(node);
    float top = YGNodeLayoutGetTop(node);
    float width = YGNodeLayoutGetWidth(node);
    float height = YGNodeLayoutGetHeight(node);

    float absX = x + left;
    float absY = y + top;

    int currentIdx = idx++;
    impl_->currentFrameBounds.push_back({absX, absY, width, height});

    uint32_t count = YGNodeGetChildCount(node); // Moved declaration up

    // Check if this node is a scroll container
    Vector2 scrollOffset = {0, 0};
    if (impl_->scrollStates.count(currentIdx)) {
      auto &scrollState = impl_->scrollStates[currentIdx];
      scrollOffset = scrollState.scrollOffset;

      // Calculate content size by measuring children
      if (count > 0) {
        float contentHeight = 0;
        float contentWidth = 0;
        for (uint32_t i = 0; i < count; ++i) {
          YGNodeRef child = YGNodeGetChild(node, i);
          float childHeight =
              YGNodeLayoutGetTop(child) + YGNodeLayoutGetHeight(child);
          float childWidth =
              YGNodeLayoutGetLeft(child) + YGNodeLayoutGetWidth(child);
          if (childHeight > contentHeight)
            contentHeight = childHeight;
          if (childWidth > contentWidth)
            contentWidth = childWidth;
        }

        // Clamp scroll offset based on content size
        // Only clamp if scrolled too far down (prevent scrolling past bottom)
        // Don't clamp on content size increase to avoid visual jumps
        if (scrollState.scrollY) {
          float maxScrollY = height - contentHeight;
          if (maxScrollY > 0)
            maxScrollY = 0;
          if (scrollState.scrollOffset.y < maxScrollY) {
            scrollState.scrollOffset.y = maxScrollY;
            scrollOffset.y = maxScrollY;
          }
          if (scrollState.scrollOffset.y > 0) {
            scrollState.scrollOffset.y = 0;
            scrollOffset.y = 0;
          }
        }
        if (scrollState.scrollX) {
          float maxScrollX = width - contentWidth;
          if (maxScrollX > 0)
            maxScrollX = 0;
          if (scrollState.scrollOffset.x < maxScrollX) {
            scrollState.scrollOffset.x = maxScrollX;
            scrollOffset.x = maxScrollX;
          }
          if (scrollState.scrollOffset.x > 0) {
            scrollState.scrollOffset.x = 0;
            scrollOffset.x = 0;
          }
        }
      }
    }

    for (uint32_t i = 0; i < count; ++i) {
      // Apply scroll offset to children
      self(self, YGNodeGetChild(node, i), absX + scrollOffset.x,
           absY + scrollOffset.y, idx);
    }
  };

  // Start traversal.
  traverse(traverse, impl_->root, rootOffsetX, rootOffsetY, nodeIndex);

  impl_->previousFrameBounds = impl_->currentFrameBounds;
}

Rectangle Layout::BeginScrollContainer(LayoutStyle style, bool scrollX,
                                       bool scrollY) {
  // Create the container node
  YGNodeRef node = YGNodeNew();
  ApplyStyle(node, style);

  // For scroll containers, we need to allow content to overflow
  // So we don't constrain the height (let children determine it)
  if (scrollX) {
    YGNodeStyleSetWidth(node, YGUndefined);
  }

  if (!impl_->nodeStack.empty()) {
    YGNodeRef parent = impl_->nodeStack.back();
    YGNodeInsertChild(parent, node, YGNodeGetChildCount(parent));
  }

  impl_->nodeStack.push_back(node);
  impl_->nodeIsScrollContainer.push_back(true);

  // Get bounds from previous frame
  int id = impl_->currentNodeId++;
  Rectangle bounds = {0, 0, 0, 0};
  if (id < impl_->previousFrameBounds.size()) {
    bounds = impl_->previousFrameBounds[id];
  }

  // If bounds are invalid (first frame), use screen bounds as fallback
  // This ensures scissor mode is always set up, and bounds will be corrected on
  // next frame
  if (bounds.width <= 0 || bounds.height <= 0) {
    bounds = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
  }

  // Setup scroll state
  ScrollContainerState scrollState;
  scrollState.nodeId = id;
  scrollState.bounds = bounds;
  scrollState.scrollX = scrollX;
  scrollState.scrollY = scrollY;

  // Restore scroll offset from previous frame if exists
  if (impl_->scrollStates.count(id)) {
    scrollState.scrollOffset = impl_->scrollStates[id].scrollOffset;
    scrollState.isDragging = impl_->scrollStates[id].isDragging;
    scrollState.dragStart = impl_->scrollStates[id].dragStart;
  }

  // Handle input
  Vector2 mousePos = GetMousePosition();
  bool mouseInBounds = CheckCollisionPointRec(mousePos, bounds);

#if RAYM3_USE_INPUT_LAYERS
  // Use input capture to ensure drags must start in bounds
  bool canProcessInput = InputLayerManager::BeginInputCapture(bounds, true);
  bool isHovered = canProcessInput && mouseInBounds;
#else
  bool isHovered = mouseInBounds;
#endif

  // Mouse wheel scrolling (passive input - works regardless of input capture)
  // Only check if mouse is in bounds
  if (mouseInBounds) {
    float wheelMove = GetMouseWheelMove();
    if (wheelMove != 0) {
      if (scrollY) {
        scrollState.scrollOffset.y += wheelMove * 20.0f;
      } else if (scrollX) {
        scrollState.scrollOffset.x += wheelMove * 20.0f;
      }
    }
  }

  // Drag scrolling (requires input capture)
  if (isHovered) {
    // Drag scrolling (only if drag started in bounds)
#if RAYM3_USE_INPUT_LAYERS
    if (canProcessInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
#else
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
#endif
      scrollState.isDragging = true;
      scrollState.dragStart = mousePos;
    }
  }

  if (scrollState.isDragging) {
#if RAYM3_USE_INPUT_LAYERS
    // Continue dragging only if we captured the input
    if (canProcessInput && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
#else
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
#endif
      Vector2 delta = {mousePos.x - scrollState.dragStart.x,
                       mousePos.y - scrollState.dragStart.y};
      if (scrollX)
        scrollState.scrollOffset.x += delta.x;
      if (scrollY)
        scrollState.scrollOffset.y += delta.y;
      scrollState.dragStart = mousePos;
    } else {
      scrollState.isDragging = false;
    }
  }

  // Clamp scroll offset (will be updated after layout calculation with actual
  // content size)
  if (scrollState.scrollOffset.x > 0)
    scrollState.scrollOffset.x = 0;
  if (scrollState.scrollOffset.y > 0)
    scrollState.scrollOffset.y = 0;

  impl_->scrollStack.push_back(scrollState);
  impl_->scrollStates[id] = scrollState;

  // Begin scissor mode for clipping
  if (bounds.width > 0 && bounds.height > 0) {
    BeginScissorMode((int)bounds.x, (int)bounds.y, (int)bounds.width,
                     (int)bounds.height);
  }

  return bounds;
}

Vector2 Layout::GetScrollOffset() {
  if (!impl_->scrollStack.empty()) {
    return impl_->scrollStack.back().scrollOffset;
  }
  return {0, 0};
}

void Layout::SetScrollOffset(Vector2 offset) {
  if (!impl_->scrollStack.empty()) {
    impl_->scrollStack.back().scrollOffset = offset;
    int id = impl_->scrollStack.back().nodeId;
    impl_->scrollStates[id].scrollOffset = offset;
  }
}

bool Layout::IsRectVisibleInScrollContainer(Rectangle rect) {
  if (impl_->scrollStack.empty()) {
    // No scroll container active, element is always visible
    return true;
  }

  // Get the current scroll container's bounds (scissor bounds)
  const ScrollContainerState &scrollState = impl_->scrollStack.back();
  Rectangle scissorBounds = scrollState.bounds;

  // Check if rect intersects with scissor bounds
  return CheckCollisionRecs(rect, scissorBounds);
}

static bool debugEnabled = false;

void Layout::SetDebug(bool enabled) { debugEnabled = enabled; }

void Layout::DrawDebug() {
  if (!debugEnabled || !impl_)
    return;

  Vector2 mousePos = GetMousePosition();

  // Iterate all bounds to draw them
  for (size_t i = 0; i < impl_->currentFrameBounds.size(); ++i) {
    Rectangle rect = impl_->currentFrameBounds[i];

    // Generate distinct color based on index
    // Using prime number steps to distribute colors across the hue spectrum
    float hue = (float)((i * 67) % 360);
    Color baseColor = ColorFromHSV(hue, 0.8f, 0.9f);

    bool isHovered = CheckCollisionPointRec(mousePos, rect);

    // Determine fill and outline colors based on hover state
    Color fillColor;
    Color outlineColor;

    if (isHovered) {
      // Hover: Darken the color (lower value in HSV)
      Color darkerColor = ColorFromHSV(hue, 0.8f, 0.6f); // Darker value
      fillColor = ColorAlpha(darkerColor, 0.15f);
      outlineColor =
          ColorAlpha(darkerColor, 1.0f); // Fully opaque darker outline
    } else {
      // Normal: Low opacity fill, dark outline
      fillColor = ColorAlpha(baseColor, 0.05f);
      // Darker shade for outline
      outlineColor = {(unsigned char)(baseColor.r * 0.5f),
                      (unsigned char)(baseColor.g * 0.5f),
                      (unsigned char)(baseColor.b * 0.5f), 255};
    }

    DrawRectangleRec(rect, fillColor);
    DrawRectangleLinesEx(rect, 1.0f, outlineColor);
  }
}

void Layout::RegisterDebugRect(Rectangle rect) {
  if (!impl_)
    return;
  impl_->currentFrameBounds.push_back(rect);
}

} // namespace raym3

#else // RAYM3_USE_YOGA == 0

namespace raym3 {

struct Layout::Impl {};

std::unique_ptr<Layout::Impl> Layout::impl_ = std::make_unique<Layout::Impl>();

void Layout::Begin(Rectangle) {}

void Layout::End() {}

Rectangle Layout::BeginContainer(LayoutStyle) { return {0, 0, 0, 0}; }

void Layout::EndContainer() {}

Rectangle Layout::Alloc(LayoutStyle) { return {0, 0, 0, 0}; }

Rectangle Layout::BeginScrollContainer(LayoutStyle, bool, bool) {
  return {0, 0, 0, 0};
}

Vector2 Layout::GetScrollOffset() { return {0, 0}; }

void Layout::SetScrollOffset(Vector2) {}

bool Layout::IsRectVisibleInScrollContainer(Rectangle) { return true; }

LayoutStyle Layout::Row() { return LayoutStyle{.direction = 0}; }

LayoutStyle Layout::Column() { return LayoutStyle{.direction = 1}; }

LayoutStyle Layout::Flex(float grow) { return LayoutStyle{.flexGrow = grow}; }

LayoutStyle Layout::Fixed(float width, float height) {
  return LayoutStyle{
      .width = width, .height = height, .flexGrow = 0, .flexShrink = 0};
}

} // namespace raym3

#endif // RAYM3_USE_YOGA
