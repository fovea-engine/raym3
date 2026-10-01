#pragma once

#include <raylib.h>

#include <array>
#include <cstdint>
#include <vector>

namespace raym3::v2 {

enum class ExternalViewHitTestBehavior {
  Opaque,
  Translucent,
  Transparent,
};

enum class ExternalViewMutatorKind {
  Transform,
  ClipRect,
  ClipRoundedRect,
  Opacity,
};

// Renderer-neutral snapshot of the state surrounding a native-view boundary.
// Transforms are row-major 3x3 matrices mapping layout dp to physical pixels.
// Clips stay in exact logical push order so a surface switch can replay them
// without conflating rectangular scissors and rounded stencil clips.
struct ExternalViewMutator {
  ExternalViewMutatorKind kind = ExternalViewMutatorKind::Transform;
  std::array<float, 9> transform = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  Rectangle rect = {0, 0, 0, 0};
  float radius = 0.0f;
  float opacity = 1.0f;
};

// A framework-painted shape above a platform view. Retaining the authored
// corner radius matters on hosts that implement paint ordering with real
// window/DOM holes: reducing a pill button to its bounding rectangle exposes
// the framework background around its rounded corners.
struct ExternalViewOcclusion {
  Rectangle rect = {0, 0, 0, 0};
  float radius = 0.0f;
};

struct ExternalViewComposition {
  int externalViewId = 0;
  Rectangle bounds = {0, 0, 0, 0};
  bool preservesFrameworkUnderlay = false;
  ExternalViewHitTestBehavior hitTestBehavior =
      ExternalViewHitTestBehavior::Opaque;
  std::vector<ExternalViewMutator> mutators;

  // Regions of framework content painted AFTER this view that land on top of
  // it, in the same space as `bounds`. Empty means nothing covers the view.
  //
  // Two uses, and both matter:
  //  - Compositing: an overlay target is only needed when this is non-empty.
  //    A platform view with nothing drawn over it can keep rendering into the
  //    current target, which saves a whole surface per view (see
  //    `requiresOverlay`).
  //  - Hit testing: the host must let pointer events through to the framework
  //    inside these regions, or content drawn above the view is unclickable.
  std::vector<ExternalViewOcclusion> occludingRegions;

  // False when nothing paints over this view, so the embedder should skip
  // acquiring/selecting an overlay surface. It must still return true from
  // CompositeExternalView (the view IS embedded — the framework simply has no
  // content above it this frame).
  bool requiresOverlay = true;
};

// Installed on one RenderContext, never process-global. Implementations switch
// the renderer target at each accepted native-view boundary and retain the
// ordered logical entries for the platform host's hierarchy transaction.
class ExternalViewEmbedder {
public:
  virtual ~ExternalViewEmbedder() = default;
  virtual void BeginFrame(uint64_t surfaceId, Rectangle bounds,
                          float density) = 0;
  virtual bool CompositeExternalView(
      const ExternalViewComposition &composition) = 0;
  // True when the most recent composite changed the physical renderer target
  // and its clip attachments therefore need rebuilding.
  virtual bool RequiresClipReplay() const { return true; }
  virtual void EndFrame(uint64_t surfaceId) = 0;
  virtual void OnGestureDecision(int externalViewId, bool accepted) {
    (void)externalViewId;
    (void)accepted;
  }
};

} // namespace raym3::v2
