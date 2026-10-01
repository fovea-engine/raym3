#pragma once

#include "raym3/v2/View.h"
#include <raylib.h>

namespace raym3::v2 {

struct RenderStats {
  // Nodes VISITED by RenderNode. Counted before the viewport-cull early-return,
  // so it includes offscreen nodes — which still pay for state computation,
  // style resolution, a stack-order entry and onLayout dispatch.
  int nodeCount = 0;
  // Nodes that survived culling and actually painted. The gap against
  // nodeCount is precisely the work viewport culling does NOT save.
  int paintedCount = 0;
  int hitTestCount = 0;
  // Yoga nodes allocated by the last UpdateLayout. The layout tree is rebuilt
  // from scratch every frame (BuildYogaTree -> YGNodeCalculateLayout ->
  // YGNodeFreeRecursive), so this is the per-frame layout cost and it scales
  // with the whole tree, not the visible part. Headline metric for
  // virtualization: it should track window size, not data size.
  int yogaNodesBuilt = 0;
};

void UpdateLayout(const NodePtr &root, Rectangle bounds);
void Render(const NodePtr &root, Rectangle bounds,
            bool layoutAlreadyComputed = false);

// Repaint a subtree on top of the frame WITHOUT disturbing the input state
// (committed hit-test stack + parent map) built by the main Render pass.
// Hosts use this to repaint always-on-top chrome (developer overlay) after
// compositing external producers (worker canvases); a plain second Render
// would clobber the committed input snapshot with the subtree's own, making
// everything outside the subtree unreachable to hit testing.
void RenderOverlayRepaint(const NodePtr &root, Rectangle bounds);

// Parent of `node` in the committed (input) parent map, or null at a root.
// Lets hosts walk ancestry the same way hit-test subtree checks do.
NodePtr CommittedParentOf(const Node *node);
bool NeedsAnotherFrame(const NodePtr &root);
NodePtr HitTest(const NodePtr &root, Vector2 point);
NodePtr InteractiveTargetAt(Vector2 point);
NodePtr InputOwnerAt(Vector2 point);
bool CapturesPoint(Vector2 point);
bool OwnsInput(const NodePtr &node, Vector2 point);
RenderStats GetLastRenderStats();

// Opacity is inherited in CSS: an element's opacity affects every visual in
// its subtree, not just its background. Built-in v2 painters use this rather
// than reading Style::opacity directly so composite controls stay consistent.
float CurrentRenderOpacity();
Color ApplyRenderOpacity(Color color);

void SetIdleSkipEnabled(bool enabled);
bool ShouldSkipRender(const NodePtr &root);
void MarkDirtyRect(Rectangle rect);
void ClearDirtyRects();
const std::vector<Rectangle> &GetDirtyRects();

bool HasModalOverlay();
NodePtr TopmostModalNode();
std::vector<NodePtr> GetFixedOverlayNodes();

// Measured window-space rect of a node (RN measure/measureInWindow).
Rectangle Measure(const NodePtr &node);

// Cancel an in-flight momentum fling on `node`. The fling tick re-derives an
// absolute target from its own start offset every frame, so any external
// offset write (JS scrollTo, programmatic scrollToIndex) must call this first
// or the write is silently reverted on the next tick.
void CancelFling(const NodePtr &node);

// --- Retained layout mirror (opt-in via RAYACT_RETAINED_LAYOUT) -------------
// Keeps a persistent Yoga tree keyed by Node*, reconciled against the retained
// raym3 tree on each call — no per-frame YGNodeNew/YGNodeFreeRecursive. Style
// application reuses the exact per-frame ApplyYogaStyle/MeasureTextNode code,
// so parity with UpdateLayout is by construction, not by copy.
//
// Verify mode: run it right after UpdateLayout each frame and it compares the
// retained tree's computed layout against node->layout, logging up to
// `maxLogPerCall` divergences (relative-to-parent coordinates, scroll offsets
// accounted). Returns the number of diverging nodes. Known blind spots:
// position:fixed subtrees (laid out in a separate pass) are skipped.
struct RetainedLayoutStats {
  int nodesReconciled = 0;
  int yogaNodesCreated = 0;   // new allocations this call (0 in steady state)
  int yogaNodesFreed = 0;     // pruned nodes this call
  int divergences = 0;
};
RetainedLayoutStats RetainedLayoutVerify(const NodePtr &root, Rectangle bounds,
                                         int maxLogPerCall = 0);

// Authoritative retained layout: same result as UpdateLayout (it ends in the
// identical StoreYogaLayout commit walk, so scroll clamp / follow-end / content
// extents behave identically) but without allocating and freeing the whole Yoga
// tree every frame. Nodes whose style did not change stay clean, so Yoga's own
// dirty tracking skips their subtree entirely — a scroll frame over a long list
// costs the commit walk, not a full flex solve.
RetainedLayoutStats RetainedUpdateLayout(const NodePtr &root, Rectangle bounds);

// Drop every retained Yoga node (engine teardown / instance switch).
void RetainedLayoutReset();

// --- Scroll diagnostics (opt-in via RAYACT_SCROLL_TRACE=1) -----------------
// Several code paths write scroll offsets, and they compete: the fling tick
// re-derives an absolute position from its own anchor every frame, so a write
// from any other source is silently reverted on the next tick. Tagging every
// write by origin turns "flinging up feels harder than down" into a histogram
// that names the responsible path instead of a guess.
enum class ScrollWriteSource {
  Drag,       // pointer drag (ScrollNodeBy from ResolveScrollInput)
  Wheel,      // mouse wheel / trackpad
  Fling,      // momentum tick
  Clamp,      // ClampScrollOffset during layout
  FollowEnd,  // autoScrollToEnd pinning to the bottom
  Js,         // JS_setScrollOffset (scrollTo/scrollToEnd)
  Mutation,   // Mutations::SetScrollOffset (command buffer)
};

bool ScrollTraceEnabled();
// Records a tagged offset write. `axis` is 'x' or 'y'. No-op when disabled.
void ScrollTraceOffsetWrite(const Node &node, ScrollWriteSource source,
                            char axis, float from, float to);
// Free-form gesture milestone (press, engage, velocity fit, fling start/stop).
void ScrollTraceEvent(const char *fmt, ...);
// Dumps the per-gesture write histogram and resets it. Called on gesture end
// and on fling completion.
void ScrollTraceFlushGesture(const char *reason);

} // namespace raym3::v2
