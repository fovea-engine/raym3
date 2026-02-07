#include "raym3/layout/Layout.h"
#include "raym3/components/TabBar.h"
#include <map>
#include <string>
#include <vector>
#include <algorithm>

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
  bool scissorStarted = false;
};

struct Layout::Impl {
  // Current frame state
  YGNodeRef root = nullptr;
  std::vector<YGNodeRef> nodeStack;
  std::vector<bool> nodeIsScrollContainer;
  int currentNodeId = 0;

  // State offset for unrelated layouts (e.g. tabs) sharing the same frame
  int idOffset = 0;

  // Scroll container stack
  std::vector<ScrollContainerState> scrollStack;

  // ID stack for stable hashing
  std::vector<uint32_t> idStack;
  uint32_t currentSeed = 0;
  int childCounter = 0;
  std::vector<int> childCounterStack;

  // Persistent state (mapped by ID order for simplicity in immediate mode)
  // In a real immediate mode system, we might use a hash of the path or ID
  // stack. For this simple implementation, we'll assume deterministic call
  // order.
  std::map<uint32_t, Rectangle> previousFrameBounds;
  std::map<uint32_t, Rectangle> currentFrameBounds;
  std::map<uint32_t, ScrollContainerState> scrollStates;

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
    idOffset = 0; // Default to 0, call SetIdOffset after Begin if needed
    
    // Initialize ID stack
    idStack.clear();
    childCounterStack.clear();
    currentSeed = 0;
    childCounter = 0;

    // Create root
    root = YGNodeNew();
    YGNodeStyleSetWidth(root, rootBounds.width);
    YGNodeStyleSetHeight(root, rootBounds.height);

    // Default root style?
    YGNodeStyleSetFlexDirection(root, YGFlexDirectionColumn);
    
    // Generate a stable ID for root
    // Note: Use 1 instead of 0 to avoid null pointer issues
    uint32_t rootId = 1;
    YGNodeSetContext(root, (void*)(uintptr_t)rootId);

    nodeStack.push_back(root);
    nodeIsScrollContainer.push_back(false);

    // Store root bounds
    currentFrameBounds[rootId] = rootBounds;
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
  
  // FNV-1a hash functions for stable ID generation
  static uint32_t HashStr(const char* str, uint32_t seed) {
    uint32_t hash = seed ^ 2166136261u;
    while (*str) {
      hash ^= (uint8_t)*str++;
      hash *= 16777619u;
    }
    return hash;
  }

  static uint32_t HashInt(int val, uint32_t seed) {
    uint32_t hash = seed ^ 2166136261u;
    hash ^= (uint32_t)val;
    hash *= 16777619u;
    return hash;
  }

  uint32_t GenerateStableId() {
    uint32_t hash = currentSeed;
    hash = HashInt(childCounter++, hash);
    // Apply idOffset for backward compatibility with tab isolation
    if (idOffset != 0) {
      hash = HashInt(idOffset, hash);
    }
    return hash;
  }
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
  
  // Generate stable ID
  uint32_t id = impl_->GenerateStableId();
  
  // Store the ID in context for retrieval during traversal
  YGNodeSetContext(node, (void*)(uintptr_t)id);

  // Add to current parent
  if (!impl_->nodeStack.empty()) {
    YGNodeRef parent = impl_->nodeStack.back();
    YGNodeInsertChild(parent, node, YGNodeGetChildCount(parent));
  }

  impl_->nodeStack.push_back(node);
  impl_->nodeIsScrollContainer.push_back(false);
  
  // Push a new child scope so children hash relative to this container
  impl_->idStack.push_back(impl_->currentSeed);
  impl_->childCounterStack.push_back(impl_->childCounter);
  impl_->currentSeed = id;
  impl_->childCounter = 0;

  // Return bounds from previous frame
  if (impl_->previousFrameBounds.count(id)) {
    return impl_->previousFrameBounds[id];
  }
  return {0, 0, 0, 0}; // Default if new
}

void Layout::EndContainer() {
  if (impl_->nodeStack.size() > 1) {
    // Check if this container was a scroll container
    if (!impl_->nodeIsScrollContainer.empty() &&
        impl_->nodeIsScrollContainer.back()) {
      // End scissor mode if it was started for this scroll container
      if (!impl_->scrollStack.empty()) {
        const ScrollContainerState& scrollState = impl_->scrollStack.back();
        if (scrollState.scissorStarted) {
          EndScissorMode();
          
          // Restore TabContent scissor if it was active (Raylib scissor doesn't stack)
          Rectangle tabScissor = GetTabContentScissorBounds();
          if (tabScissor.width > 0 && tabScissor.height > 0 && 
              (tabScissor.width != GetScreenWidth() || tabScissor.height != GetScreenHeight())) {
            // Apply DPI Scaling (HighDPI support)
            float scaleX = (float)GetRenderWidth() / (float)GetScreenWidth();
            float scaleY = (float)GetRenderHeight() / (float)GetScreenHeight();
            BeginScissorMode((int)(tabScissor.x * scaleX), (int)(tabScissor.y * scaleY), 
                             (int)(tabScissor.width * scaleX), (int)(tabScissor.height * scaleY));
          }
        }
        impl_->scrollStack.pop_back();
      }
    }

    impl_->nodeStack.pop_back();
    if (!impl_->nodeIsScrollContainer.empty()) {
      impl_->nodeIsScrollContainer.pop_back();
    }
    
    // Pop the child scope
    if (!impl_->idStack.empty()) {
      impl_->currentSeed = impl_->idStack.back();
      impl_->idStack.pop_back();
    }
    if (!impl_->childCounterStack.empty()) {
      impl_->childCounter = impl_->childCounterStack.back();
      impl_->childCounterStack.pop_back();
    }
  }
}

Rectangle Layout::Alloc(LayoutStyle style) {
  YGNodeRef node = YGNodeNew();
  ApplyStyle(node, style);
  
  // Generate stable ID
  uint32_t id = impl_->GenerateStableId();
  
  // Store the ID in context for retrieval during traversal
  YGNodeSetContext(node, (void*)(uintptr_t)id);

  if (!impl_->nodeStack.empty()) {
    YGNodeRef parent = impl_->nodeStack.back();
    YGNodeInsertChild(parent, node, YGNodeGetChildCount(parent));
  }

  // Return bounds
  if (impl_->previousFrameBounds.count(id)) {
    return impl_->previousFrameBounds[id];
  }
  return {0, 0, 0, 0};
}

// Helpers
LayoutStyle Layout::Row() { 
  return LayoutStyle{
      .width = -1.0f, .height = -1.0f, 
      .flexGrow = 0.0f, .flexShrink = 1.0f,
      .direction = 0, .justify = 0, .align = 0, .flexWrap = 0
  }; 
}
LayoutStyle Layout::Column() { 
  return LayoutStyle{
      .width = -1.0f, .height = -1.0f, 
      .flexGrow = 0.0f, .flexShrink = 1.0f,
      .direction = 1, .justify = 0, .align = 0, .flexWrap = 0
  }; 
}
LayoutStyle Layout::Flex(float grow) { 
  return LayoutStyle{
      .width = -1.0f, .height = -1.0f, 
      .flexGrow = grow, .flexShrink = 1.0f,
      .direction = 1, .justify = 0, .align = 0, .flexWrap = 0
  }; 
}
LayoutStyle Layout::Fixed(float width, float height) {
  return LayoutStyle{
      .width = width, .height = height, 
      .flexGrow = 0.0f, .flexShrink = 0.0f, 
      .direction = 1, .justify = 0, .align = 0, .flexWrap = 0
  };
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

  // Recursive lambda
  auto traverse = [&](auto &&self, YGNodeRef node, float x, float y) -> void {
    float left = YGNodeLayoutGetLeft(node);
    float top = YGNodeLayoutGetTop(node);
    float width = YGNodeLayoutGetWidth(node);
    float height = YGNodeLayoutGetHeight(node);

    float absX = x + left;
    float absY = y + top;

    uint32_t id = (uint32_t)(uintptr_t)YGNodeGetContext(node);
    impl_->currentFrameBounds[id] = {absX, absY, width, height};

    uint32_t count = YGNodeGetChildCount(node);

    // Check if this node is a scroll container
    Vector2 scrollOffset = {0, 0};
    if (impl_->scrollStates.count(id)) {
      auto &scrollState = impl_->scrollStates[id];
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
           absY + scrollOffset.y);
    }
  };

  // Start traversal.
  traverse(traverse, impl_->root, rootOffsetX, rootOffsetY);

  impl_->previousFrameBounds = impl_->currentFrameBounds;
}

Rectangle Layout::BeginScrollContainer(LayoutStyle style, bool scrollX,
                                       bool scrollY) {
  // Create the container node
  YGNodeRef node = YGNodeNew();
  ApplyStyle(node, style);
  
  // Generate stable ID
  uint32_t id = impl_->GenerateStableId();
  
  // Store the ID in context for retrieval during traversal
  YGNodeSetContext(node, (void*)(uintptr_t)id);

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
  
  // Push a new child scope so children hash relative to this container
  impl_->idStack.push_back(impl_->currentSeed);
  impl_->childCounterStack.push_back(impl_->childCounter);
  impl_->currentSeed = id;
  impl_->childCounter = 0;

  // Get bounds from previous frame (use map lookup, not vector indexing)
  Rectangle bounds = {0, 0, 0, 0};
  auto it = impl_->previousFrameBounds.find(id);
  if (it != impl_->previousFrameBounds.end()) {
    bounds = it->second;
  }

  impl_->nodeIsScrollContainer.push_back(true);

  // Check if bounds are valid
  bool validBounds = (bounds.width > 0 && bounds.height > 0);
  if (!validBounds) {
    // Use minimal placeholder bounds on first frame
    // Bounds will be corrected on next frame when layout is calculated
    bounds = {0, 0, 1, 1};
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

  // Only handle input if we have valid bounds
  if (validBounds) {
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
  }

  // Only begin scissor mode if we have valid bounds
  // Intersect with parent scissor (e.g., TabContent) to avoid conflicts
  if (validBounds && bounds.width > 0 && bounds.height > 0) {
    // Get TabContent scissor bounds if it's active
    Rectangle parentScissor = GetTabContentScissorBounds();
    
    // Calculate intersection with parent scissor
    float left = std::max(bounds.x, parentScissor.x);
    float top = std::max(bounds.y, parentScissor.y);
    float right = std::min(bounds.x + bounds.width, parentScissor.x + parentScissor.width);
    float bottom = std::min(bounds.y + bounds.height, parentScissor.y + parentScissor.height);
    
    if (right > left && bottom > top) {
      Rectangle scissorBounds = {left, top, right - left, bottom - top};

      // Raylib's BeginScissorMode expects physical pixels if the backing store is scaled (HighDPI)
      float scaleX = (float)GetRenderWidth() / (float)GetScreenWidth();
      float scaleY = (float)GetRenderHeight() / (float)GetScreenHeight();

      BeginScissorMode((int)(scissorBounds.x * scaleX), (int)(scissorBounds.y * scaleY), 
                       (int)(scissorBounds.width * scaleX), (int)(scissorBounds.height * scaleY));
      scrollState.scissorStarted = true;
      scrollState.bounds = scissorBounds; // Store intersected bounds (Logical)
    }
  }

  impl_->scrollStack.push_back(scrollState);
  impl_->scrollStates[id] = scrollState;

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
  for (auto& [id, rect] : impl_->currentFrameBounds) {
    // Generate distinct color based on index
    // Using prime number steps to distribute colors across the hue spectrum
    float hue = (float)((id * 67) % 360);
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
  // Use negative IDs for debug rects to avoid collision
  // (We don't really track them frame-to-frame but we need to store them)
  int debugId = -1 - (int)impl_->currentFrameBounds.size(); 
  impl_->currentFrameBounds[debugId] = rect;
}

void Layout::InvalidatePreviousFrame() {
  if (!impl_)
    return;
  impl_->previousFrameBounds.clear();
  impl_->scrollStates.clear();
}

void Layout::SetIdOffset(int offset) {
  if (impl_) {
    impl_->idOffset = offset;
  }
}

void Layout::PushId(const char* str_id) {
  if (!impl_)
    return;
  impl_->idStack.push_back(impl_->currentSeed);
  impl_->childCounterStack.push_back(impl_->childCounter);
  impl_->currentSeed = impl_->HashStr(str_id, impl_->currentSeed);
  impl_->childCounter = 0;
}

void Layout::PushId(int int_id) {
  if (!impl_)
    return;
  impl_->idStack.push_back(impl_->currentSeed);
  impl_->childCounterStack.push_back(impl_->childCounter);
  impl_->currentSeed = impl_->HashInt(int_id, impl_->currentSeed);
  impl_->childCounter = 0;
}

void Layout::PopId() {
  if (!impl_ || impl_->idStack.empty())
    return;
  impl_->currentSeed = impl_->idStack.back();
  impl_->idStack.pop_back();
  if (!impl_->childCounterStack.empty()) {
    impl_->childCounter = impl_->childCounterStack.back();
    impl_->childCounterStack.pop_back();
  }
}

Rectangle Layout::GetActiveScissorBounds() {
  // Start with TabContent scissor bounds (returns screen bounds if not clipping)
  Rectangle result = GetTabContentScissorBounds();
  
  // If we have an active scroll container, intersect with its bounds
  if (impl_ && !impl_->scrollStack.empty()) {
    Rectangle scrollBounds = impl_->scrollStack.back().bounds;
    
    // Calculate intersection
    float left = std::max(result.x, scrollBounds.x);
    float top = std::max(result.y, scrollBounds.y);
    float right = std::min(result.x + result.width, scrollBounds.x + scrollBounds.width);
    float bottom = std::min(result.y + result.height, scrollBounds.y + scrollBounds.height);
    
    if (right > left && bottom > top) {
      result = {left, top, right - left, bottom - top};
    } else {
      // No intersection, return empty rect
      result = {0, 0, 0, 0};
    }
  }
  
  return result;
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

void Layout::InvalidatePreviousFrame() {}

} // namespace raym3

#endif // RAYM3_USE_YOGA
