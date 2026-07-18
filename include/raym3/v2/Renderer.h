#pragma once

#include "raym3/v2/View.h"
#include <raylib.h>

namespace raym3::v2 {

struct RenderStats {
  int nodeCount = 0;
  int hitTestCount = 0;
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

} // namespace raym3::v2
