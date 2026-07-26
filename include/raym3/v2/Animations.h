#pragma once

// CSS @keyframes animation engine — the looping/keyframed counterpart to the
// transition engine (Transitions.h). `@keyframes` blocks are parsed by the
// embedding bridge and registered here by name; a Style's `animation` spec
// (Style::animations) names one or more of them. ApplyStyleWithAnimations
// starts/cancels animations when the style updates, and TickAnimations advances
// running ones on the render thread each frame — writing interpolated values
// straight into node->style, so layout and paint stay animation-unaware (same
// contract as transitions). No per-frame JavaScript is involved.
//
// A JS/explicit write to an animated property cancels that animation, so app
// code always wins and does not fight the native tick.

#include "raym3/v2/Style.h"
#include "raym3/v2/View.h"

#include <string>
#include <vector>

namespace raym3::v2 {

// ─── keyframe registry (populated from parsed @keyframes) ────────────────────
// Register (or replace) the keyframe track for `name`. `frames` need not be
// sorted; the registry sorts by offset. Empty `frames` removes the entry.
void RegisterKeyframes(const std::string& name, std::vector<Keyframe> frames);

// Resolved keyframe track for `name`, or nullptr if undeclared.
const std::vector<Keyframe>* GetKeyframes(const std::string& name);

// Drop all registered @keyframes (called when the stylesheet is cleared).
void ClearKeyframes();

// ─── engine ──────────────────────────────────────────────────────────────────
// Style-apply hook, analogous to ApplyStyleWithTransitions. Starts animations
// named in `target.animations` (looking up keyframes by name), cancels all when
// `animation: none`, and — for any property written explicitly this update
// (present in `explicitProps`) — cancels the running animation on that property
// so the JS/explicit value wins. `node->style` is assigned `target`.
void ApplyStyleWithAnimations(const NodePtr& node, const Style& target,
                              const Style& explicitProps);

// Advance all running animations in the subtree by this frame's delta
// (FrameTimeMs()), writing interpolated values into node->style. Finished
// non-infinite animations settle per fill-mode and are pruned. Call once per
// rendered root per frame, before UpdateLayout (after TickTransitions).
void TickAnimations(const NodePtr& root);

// True if any node in the subtree has a running animation — used by the frame
// scheduler to keep rendering. (Node-level check lives in NodeNeedsAnotherFrame.)
bool HasActiveAnimations(const NodePtr& root);

} // namespace raym3::v2
