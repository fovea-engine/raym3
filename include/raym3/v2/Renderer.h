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
bool NeedsAnotherFrame(const NodePtr &root);
NodePtr HitTest(const NodePtr &root, Vector2 point);
NodePtr InteractiveTargetAt(Vector2 point);
NodePtr InputOwnerAt(Vector2 point);
bool CapturesPoint(Vector2 point);
bool OwnsInput(const NodePtr &node, Vector2 point);
RenderStats GetLastRenderStats();

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
