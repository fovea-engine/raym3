#pragma once

#include "raym3/v2/View.h"
#include <cstdint>
#include <raylib.h>

// raym3 v2 unified input system.
//
// Model (microui hover-layer + Dear ImGui ActiveId capture), applied to the
// committed retained tree:
//   1. BeginInputFrame() latches the raw pointer once per frame, in dp.
//   2. Render() builds + commits the paint stack (with per-entry effective z).
//   3. ResolveInput() runs one z-aware pass: topmost interactive node under the
//      pointer wins; activeId holds capture through a drag; the topmost scrim
//      layer filters out everything beneath it (unless a higher-z element sits
//      under the pointer). All control/gesture/modal callbacks fire here.
//
// This replaces the old split model (post-render HitTest for onPress + in-render
// raylib polling for native controls), eliminating the 1-frame lag and the
// Android double dp-conversion.
namespace raym3::v2 {

using NodeId = std::uintptr_t; // reinterpret_cast<uintptr_t>(Node*); 0 = none

struct PointerInput {
  Vector2 pos = {0, 0}; // dp
  bool down = false;
  bool pressed = false;  // down edge this frame
  bool released = false; // up edge this frame
  float wheel = 0.0f;
};

// Latch the raw pointer for this frame. Coordinates must already be in dp.
void BeginInputFrame(Vector2 posDp, bool down, bool pressed, bool released,
                     float wheel);

// Resolve hover/active/focus + fire interaction callbacks. Call AFTER Render()
// so the committed stack (with effective z) is current.
void ResolveInput(const NodePtr &root);

// Scroll gestures run AFTER ResolveInput so controls/buttons keep capture.
// Uses touch-slop + directional claim (vertical drag scrolls; horizontal goes to
// sliders). Call BeginInputFrame before ResolveInput/ResolveScrollInput.
void ResolveScrollInput(const NodePtr &root);
void ResolveTextInput(const NodePtr &root);

// Inertial scroll decay — call once per frame before Render().
void TickScrollMomentum(const NodePtr &root);

// Flutter-aligned touch slop (logical dp).
constexpr float kTouchSlop = 18.0f;

// react-native's default long-press delay (seconds).
constexpr double kLongPressDelay = 0.5;

// --- Queries ---------------------------------------------------------------
const PointerInput &GetPointer();
Vector2 PointerDp();
Vector2 DragDelta(); // pointer - drag origin (valid while a drag is active)

NodeId GetHoveredId();
NodeId GetActiveId();
NodeId GetFocusedId();

bool IsHovered(const NodePtr &node);
bool IsActive(const NodePtr &node);
bool IsFocused(const NodePtr &node);

// Internal state mutation used by ResolveInput (implemented in Renderer.cpp,
// which owns the committed stack). Not intended for app code.
void SetHoveredId(NodeId id);
void SetActiveId(NodeId id);
void SetFocusedId(NodeId id);
void SetDragOrigin(Vector2 originDp);
Vector2 GetDragOrigin();
double FrameTimeMs();
void MarkTextSelectionOverlayPointerConsumed();
bool WasTextSelectionOverlayPointerConsumed();

// Programmatic focus control.
void SetFocusedNode(const NodePtr &node);
void RequestFocus(const NodePtr &node);
void Blur();

// TextField keyboard policy: keep focus unless the user taps outside (not scroll/drag).
bool ShouldKeepTextInputFocused(const NodePtr &tapTarget, bool scrollEngaged,
                                float pointerTravel);
void DismissTextInputIfNeeded(const NodePtr &tapTarget, bool scrollEngaged,
                              float pointerTravel);

inline NodeId IdOf(const Node *n) { return reinterpret_cast<NodeId>(n); }
inline NodeId IdOf(const NodePtr &n) { return reinterpret_cast<NodeId>(n.get()); }

// True while the node is under an active or deferred (pending) press.
bool IsNodePressed(const Node &node);

void SetPendingPressId(NodeId id);
NodeId GetPendingPressId();

// Drop every input reference to `n` (hover/active/focus/pendingPress/scroll
// candidate). Input state holds raw Node* as NodeId, so a freed node would be
// dereferenced on the next input frame (use-after-free). The host MUST call
// this for a node before it is destroyed (e.g. on disposeNode / screen unmount).
void ForgetInputNode(Node *n);

} // namespace raym3::v2
