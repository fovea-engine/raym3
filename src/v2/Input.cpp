#include "raym3/v2/Input.h"
#include "raym3/v2/RenderContext.h"

namespace raym3::v2 {

void SetPendingPressId(NodeId id) { Ctx().input.pendingPress = id; }
NodeId GetPendingPressId() { return Ctx().input.pendingPress; }

void BeginInputFrame(Vector2 posDp, bool down, bool pressed, bool released,
                     float wheel) {
  PointerInput &pointer = Ctx().input.pointer;
  pointer.pos = posDp;
  pointer.down = down;
  pointer.pressed = pressed;
  pointer.released = released;
  pointer.wheel = wheel;
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

} // namespace raym3::v2
