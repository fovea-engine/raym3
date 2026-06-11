#pragma once

// CSS transition engine. Styles declare a transition spec
// (Style::transitions, parsed from the CSS `transition` shorthand by the
// embedding bridge); ApplyStyleWithTransitions starts/retargets per-property
// interpolations when a watched property changes, and TickTransitions advances
// them on the render thread each frame. node->style always holds the current
// (possibly interpolated) value, so layout and paint need no awareness of the
// animation — UpdateLayout simply consumes whatever is in the Style.

#include "raym3/v2/Style.h"
#include "raym3/v2/View.h"

namespace raym3::v2 {

// Current effective value of `prop` on `style`, applying per-property
// defaults for unset optionals (margin/inset 0, opacity/scale 1,
// translate/rotation 0). Returns false when the property is unset and not
// animatable from a default (width/height: animating from "auto" is
// unsupported — callers should snap).
bool GetTransitionValue(const Style &style, TransitionProperty prop,
                        float &out);

// Write `value` into the specific Style field for `prop` (per-edge margin or
// inset slot, clearing the matching *Auto flag for margins).
void SetTransitionValue(Style &style, TransitionProperty prop, float value);

// True when `prop` is explicitly present on `style` (margins/insets check the
// per-edge → axis → all chain).
bool HasExplicitValue(const Style &style, TransitionProperty prop);

// Replacement for `node->style = target` at style-update sites. `target` is
// the fully merged style to apply; `explicitProps` is the un-merged style
// from this update, used to gate which properties may start or retarget a
// transition (properties not explicitly set this update leave any in-flight
// transition untouched). An empty transitions vector on `target` (CSS
// `transition: none`) cancels all in-flight transitions and snaps.
void ApplyStyleWithTransitions(const NodePtr &node, const Style &target,
                               const Style &explicitProps);

// Advance all active transitions in the subtree by this frame's delta
// (FrameTimeMs(), pause-safe via its 100ms clamp), writing interpolated
// values into node->style. Completed transitions set their exact final value
// and are pruned. Call once per rendered root per frame, before UpdateLayout.
void TickTransitions(const NodePtr &root);

} // namespace raym3::v2
