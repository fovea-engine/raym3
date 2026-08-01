#include "raym3/v2/Input.h"
#include "raym3/v2/RenderContext.h"

namespace raym3::v2 {

void SetPendingPressId(NodeId id) { Ctx().input.pendingPress = id; }
NodeId GetPendingPressId() { return Ctx().input.pendingPress; }

void ForgetInputNode(Node *n) {
  if (!n)
    return;
  NodeId id = IdOf(n);
  RenderContext &c = Ctx();
  InputState &in = c.input;
  if (in.hovered == id)
    in.hovered = 0;
  if (in.active == id)
    in.active = 0;
  if (in.focused == id)
    in.focused = 0;
  if (in.pendingPress == id)
    in.pendingPress = 0;
  if (n->externalViewId != 0 &&
      in.pendingExternalViewId == n->externalViewId)
    in.pendingExternalViewId = 0;
  if (c.lastFocusedTextInput == id)
    c.lastFocusedTextInput = 0;
  if (c.scroll.candidate.get() == n)
    c.scroll.candidate = nullptr;
  if (c.scroll.pendingPressTarget.get() == n)
    c.scroll.pendingPressTarget = nullptr;
}

void BeginInputFrame(Vector2 posDp, bool down, bool pressed, bool released,
                     float wheel, bool cancelled) {
  PointerInput &pointer = Ctx().input.pointer;
  pointer.pos = posDp;
  pointer.down = down;
  pointer.pressed = pressed;
  pointer.released = released;
  pointer.cancelled = cancelled;
  pointer.wheel = wheel;
  Ctx().input.textSelectionOverlayConsumedPointer = false;
}

const PointerInput &GetPointer() { return Ctx().input.pointer; }
Vector2 PointerDp() { return Ctx().input.pointer.pos; }
Vector2 DragDelta() {
  InputState &in = Ctx().input;
  return {in.pointer.pos.x - in.dragOrigin.x, in.pointer.pos.y - in.dragOrigin.y};
}

NodeId GetHoveredId() { return Ctx().input.hovered; }
NodeId GetActiveId() { return Ctx().input.active; }
NodeId GetFocusedId() { return Ctx().input.focused; }

bool IsHovered(const NodePtr &node) { return node && IdOf(node) == Ctx().input.hovered; }
bool IsActive(const NodePtr &node) { return node && IdOf(node) == Ctx().input.active; }
bool IsFocused(const NodePtr &node) { return node && IdOf(node) == Ctx().input.focused; }

void SetHoveredId(NodeId id) { Ctx().input.hovered = id; }
void SetActiveId(NodeId id) { Ctx().input.active = id; }
void SetFocusedId(NodeId id) { Ctx().input.focused = id; }
void SetDragOrigin(Vector2 originDp) { Ctx().input.dragOrigin = originDp; }
Vector2 GetDragOrigin() { return Ctx().input.dragOrigin; }
void MarkTextSelectionOverlayPointerConsumed() {
  Ctx().input.textSelectionOverlayConsumedPointer = true;
}
bool WasTextSelectionOverlayPointerConsumed() {
  return Ctx().input.textSelectionOverlayConsumedPointer;
}

double FrameTimeMs() {
  float dt = GetFrameTime();
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;
  return dt * 1000.0;
}

void SetFocusedNode(const NodePtr &node) {
  NodeId next = node ? IdOf(node) : 0;
  if (next == Ctx().input.focused)
    return;
  Ctx().input.focused = next;
  if (node && node->onFocus)
    node->onFocus();
}

void RequestFocus(const NodePtr &node) { SetFocusedNode(node); }
void Blur() { Ctx().input.focused = 0; }

// A tap on a TextInput's own subtree counts as a tap on the field. The mobile
// native editor is an external-view CHILD of the raym3 text-input node, so a
// tap that focuses the real editor must not read as "tap outside" and blur
// the chrome the JS focus event is about to (or just did) focus.
bool NodeOrAncestorIsTextInput(const NodePtr &node) {
  for (NodePtr current = node; current;) {
    if (current->kind == NodeKind::TextInput)
      return true;
    auto it = Ctx().committedParentMap.find(current.get());
    current = it != Ctx().committedParentMap.end() ? it->second : nullptr;
  }
  return false;
}

bool ShouldKeepTextInputFocused(const NodePtr &tapTarget, bool scrollEngaged,
                                float pointerTravel) {
  if (GetFocusedId() == 0)
    return true;
  auto *fn = reinterpret_cast<Node *>(GetFocusedId());
  if (!fn || fn->kind != NodeKind::TextInput)
    return true;
  if (scrollEngaged)
    return true;
  if (tapTarget && NodeOrAncestorIsTextInput(tapTarget))
    return true;
  // ScrollView keyboardShouldPersistTaps applies to every descendant, not
  // merely to the container itself.
  for (NodePtr current = tapTarget; current;) {
    if (current->keepTextInputFocusOnTap)
      return true;
    auto it = Ctx().committedParentMap.find(current.get());
    current = it != Ctx().committedParentMap.end() ? it->second : nullptr;
  }
  if (pointerTravel > kTouchSlop)
    return true;
  return false;
}

void DismissTextInputIfNeeded(const NodePtr &tapTarget, bool scrollEngaged,
                              float pointerTravel) {
  if (ShouldKeepTextInputFocused(tapTarget, scrollEngaged, pointerTravel))
    return;
  Blur();
}

} // namespace raym3::v2
