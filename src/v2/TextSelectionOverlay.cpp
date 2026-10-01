#include "raym3/v2/TextSelectionOverlay.h"

#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/Input.h"
#include "raym3/v2/TextInput.h"
#include "raym3/v2/TextSelection.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <raylib.h>
#include <vector>

namespace raym3::v2 {
namespace {

constexpr float kHandleSize = 22.0f;
constexpr float kHandleRadius = kHandleSize * 0.5f;
constexpr float kHandleHit = 44.0f;
constexpr float kToolbarH = 38.0f;
constexpr float kToolbarPad = 8.0f;
constexpr float kToolbarGap = 4.0f;
constexpr float kToolbarYGap = 8.0f;

enum class TextHandleType { Left, Right, Collapsed };

// The overlay serves two selection sources: editable TextInput nodes and
// read-only `selectable` Text nodes. Sel* wrappers dispatch on the kind.
bool IsSelectableTextTarget(const Node &node) {
  return node.kind == NodeKind::Text;
}

Node *FocusedSelectionTarget() {
  NodeId id = GetFocusedId();
  if (!id)
    return nullptr;
  auto *node = reinterpret_cast<Node *>(id);
  if (!node)
    return nullptr;
  if (node->kind == NodeKind::TextInput) {
    // A field backed by a real platform editor owns its own selection UI: the
    // UITextField/EditText draws the caret, the drag handles, and the
    // cut/copy/paste menu. Painting raym3's overlay on top would double every
    // one of them, so the renderer stays out of the way entirely.
    if (node->textInput.nativeEditor)
      return nullptr;
    return node;
  }
  if (NodeIsSelectableText(*node))
    return node;
  return nullptr;
}

Rectangle SelBounds(Node &node) {
  return IsSelectableTextTarget(node) ? TextNodeBounds(node)
                                      : TextInputInputBounds(node);
}

float SelByteOffsetX(Node &node, int off) {
  return IsSelectableTextTarget(node) ? TextNodeByteOffsetX(node, off)
                                      : TextInputByteOffsetX(node, off);
}

float SelByteOffsetY(Node &node, int off) {
  return IsSelectableTextTarget(node) ? TextNodeByteOffsetY(node, off)
                                      : TextInputByteOffsetY(node, off);
}

float SelLineCenterY(Node &node, int off) {
  return IsSelectableTextTarget(node) ? TextNodeLineCenterY(node, off)
                                      : TextInputLineCenterY(node, off);
}

float SelPreferredLineHeight(Node &node) {
  return IsSelectableTextTarget(node) ? TextNodePreferredLineHeight(node)
                                      : TextInputPreferredLineHeight(node);
}

int SelHitTestCaret(Node &node, Vector2 pos) {
  return IsSelectableTextTarget(node) ? TextNodeHitTestCaret(node, pos)
                                      : TextInputHitTestCaret(node, pos);
}

int SelTextLength(Node &node) {
  return IsSelectableTextTarget(node) ? TextNodeTextLength(node)
                                      : TextInputTextLength(node);
}

void SelSetSelection(Node &node, int start, int end, int cursor) {
  if (IsSelectableTextTarget(node))
    TextNodeSetSelection(node, start, end);
  else
    TextInputSetSelection(node, start, end, cursor);
}

// Toolbar layout differs per target: editable fields offer Cut/Copy/Paste/All,
// read-only text only Copy/All.
struct ToolbarButton {
  const char *label;
  float width;
};

int SelToolbarButtons(Node &node, const ToolbarButton *&buttons) {
  static const ToolbarButton kInputButtons[] = {
      {"Cut", 42}, {"Copy", 50}, {"Paste", 54}, {"All", 74}};
  static const ToolbarButton kTextButtons[] = {{"Copy", 50}, {"All", 74}};
  if (IsSelectableTextTarget(node)) {
    buttons = kTextButtons;
    return 2;
  }
  buttons = kInputButtons;
  return 4;
}

bool HasSelection(Node &node, int &start, int &end) {
  start = node.textEdit.selectionStart;
  end = node.textEdit.selectionEnd;
  if (start < 0 || end < 0 || start == end)
    return false;
  if (start > end)
    std::swap(start, end);
  return true;
}

// Native selection-menu host state (see SetSelectionMenuHost below). Defined
// early: PaintToolbar and DrawHandle consult them.
std::function<void(const SelectionMenuRequest &)> &MenuShowFn() {
  static std::function<void(const SelectionMenuRequest &)> fn;
  return fn;
}
std::function<void()> &MenuHideFn() {
  static std::function<void()> fn;
  return fn;
}
SelectionHandleStyle &HandleStyle() {
  static SelectionHandleStyle style = SelectionHandleStyle::Material;
  return style;
}

Vector2 HandleAnchor(TextHandleType type) {
  switch (type) {
  case TextHandleType::Left:
    return {kHandleSize, 0.0f};
  case TextHandleType::Right:
    return {0.0f, 0.0f};
  case TextHandleType::Collapsed:
    return {kHandleSize * 0.5f, -4.0f};
  }
  return {0.0f, 0.0f};
}

float HandleRotation(TextHandleType type) {
  switch (type) {
  case TextHandleType::Left:
    return 90.0f;
  case TextHandleType::Right:
    return 0.0f;
  case TextHandleType::Collapsed:
    return 45.0f;
  }
  return 0.0f;
}

Vector2 HandleTopLeft(float x, float y, TextHandleType type) {
  Vector2 anchor = HandleAnchor(type);
  return {x - anchor.x, y - anchor.y};
}

Rectangle HandleHitRect(float x, float y, TextHandleType type) {
  Vector2 topLeft = HandleTopLeft(x, y, type);
  Vector2 center = {topLeft.x + kHandleRadius, topLeft.y + kHandleRadius};
  return {center.x - kHandleHit * 0.5f, center.y - kHandleHit * 0.5f,
          kHandleHit, kHandleHit};
}

Rectangle ToolbarRect(Node &node) {
  int start = 0;
  int end = 0;
  bool hasSelection = HasSelection(node, start, end);
  Rectangle input = SelBounds(node);
  float startX = hasSelection
                     ? SelByteOffsetX(node, start)
                     : SelByteOffsetX(node, node.textEdit.cursor);
  float endX = hasSelection ? SelByteOffsetX(node, end) : startX;
  float centerX = (startX + endX) * 0.5f;
  const ToolbarButton *buttons = nullptr;
  const int count = SelToolbarButtons(node, buttons);
  float totalW = kToolbarPad * 2.0f + kToolbarGap * (float)(count - 1);
  for (int i = 0; i < count; ++i)
    totalW += buttons[i].width;
  float x = std::clamp(centerX - totalW * 0.5f, input.x,
                       std::max(input.x, input.x + input.width - totalW));
  float y = input.y - kToolbarH - kToolbarYGap;
  if (y < 0.0f)
    y = input.y + input.height + kToolbarYGap;
  return {x, y, totalW, kToolbarH};
}

bool PointInToolbarButton(Node &node, Vector2 p, int &buttonIndex) {
  Rectangle r = ToolbarRect(node);
  if (!CheckCollisionPointRec(p, r))
    return false;
  const ToolbarButton *buttons = nullptr;
  const int count = SelToolbarButtons(node, buttons);
  float x = r.x + kToolbarPad;
  for (int i = 0; i < count; ++i) {
    Rectangle b{x, r.y + 4.0f, buttons[i].width, r.height - 8.0f};
    if (CheckCollisionPointRec(p, b)) {
      buttonIndex = i;
      return true;
    }
    x += buttons[i].width + kToolbarGap;
  }
  return true;
}

void ClearActiveHandle(TextEditState &edit) {
  edit.activeHandle = -1;
  edit.activeHandleAnchor = -1;
  edit.activeHandleOffset = -1;
  edit.activeHandleDragY = 0.0f;
  edit.activeHandleDragTargetY = 0.0f;
}

void BeginHandleDrag(Node &node, bool startHandle, int start, int end,
                     Vector2 pointer) {
  TextEditState &edit = node.textEdit;
  edit.activeHandle = startHandle ? 0 : 1;
  edit.activeHandleAnchor = startHandle ? end : start;
  edit.activeHandleOffset = startHandle ? start : end;
  edit.activeHandleDragY = pointer.y;
  edit.activeHandleDragTargetY =
      SelLineCenterY(node, edit.activeHandleOffset) - pointer.y;
  edit.toolbarVisible = false;
}

float SnappedHandleDragY(Node &node, float dragY, float handleY) {
  const float lineHeight = std::max(1.0f, SelPreferredLineHeight(node));
  const float distanceDragged = dragY - handleY;
  const float dragDirection = distanceDragged < 0.0f ? -1.0f : 1.0f;
  const float linesDragged =
      dragDirection * std::floor(std::fabs(distanceDragged) / lineHeight);
  return handleY + linesDragged * lineHeight;
}

int HitTestHandleDragTarget(Node &node, Vector2 pointer) {
  TextEditState &edit = node.textEdit;
  const float snappedY =
      SnappedHandleDragY(node, pointer.y, edit.activeHandleDragY);
  edit.activeHandleDragY = snappedY;
  Vector2 target = {pointer.x, snappedY + edit.activeHandleDragTargetY};
  return SelHitTestCaret(node, target);
}

void UpdateDraggedSelection(Node &node, int currentOffset) {
  TextEditState &edit = node.textEdit;
  const int draggedHandle = edit.activeHandle;
  const int anchor = edit.activeHandleAnchor >= 0 ? edit.activeHandleAnchor
                                                  : node.textEdit.cursor;
  TextInputDragSelectionUpdate update = TextInputResolveDraggedSelection(
      draggedHandle == 0 ? TextInputDraggedEdge::Start
                         : TextInputDraggedEdge::End,
      anchor, currentOffset, SelTextLength(node));
  if (!update.applied)
    return;

  SelSetSelection(node, update.selectionStart, update.selectionEnd,
                  update.cursor);
  edit.activeHandle = draggedHandle;
  edit.activeHandleAnchor = anchor;
  edit.activeHandleOffset = update.cursor;
  edit.activeHandleDragY = SelLineCenterY(node, update.cursor) -
                           edit.activeHandleDragTargetY;
}

TextHandleType VisualHandleType(int offset, int anchor) {
  if (offset < anchor)
    return TextHandleType::Left;
  if (offset > anchor)
    return TextHandleType::Right;
  return TextHandleType::Collapsed;
}

void DrawCupertinoHandle(float x, float y, TextHandleType type) {
  // iOS lollipop: 2dp bar the height of a line with a 5dp ball — ball on top
  // for the start handle, on the bottom for the end handle.
  Color color = Theme::GetColorScheme().primary;
  const float barW = 2.0f;
  const float barH = 18.0f;
  const float ballR = 5.0f;
  if (type == TextHandleType::Collapsed) {
    DrawCircleV({x, y + ballR}, ballR, color);
    return;
  }
  const float top = y - barH;
  DrawRectangleRec({x - barW * 0.5f, top, barW, barH}, color);
  if (type == TextHandleType::Left)
    DrawCircleV({x, top - ballR + 1.0f}, ballR, color);
  else
    DrawCircleV({x, y + ballR - 1.0f}, ballR, color);
}

void DrawHandle(float x, float y, TextHandleType type) {
  if (HandleStyle() == SelectionHandleStyle::Cupertino) {
    DrawCupertinoHandle(x, y, type);
    return;
  }
  Color color = Theme::GetColorScheme().primary;
  Vector2 topLeft = HandleTopLeft(x, y, type);
  Vector2 center = {topLeft.x + kHandleRadius, topLeft.y + kHandleRadius};
  const float rotation = HandleRotation(type) * DEG2RAD;
  const float cs = std::cos(rotation);
  const float sn = std::sin(rotation);
  auto transform = [&](Vector2 local) -> Vector2 {
    const float dx = local.x - kHandleRadius;
    const float dy = local.y - kHandleRadius;
    return {center.x + dx * cs - dy * sn, center.y + dx * sn + dy * cs};
  };

  constexpr int kArcSegments = 24;
  std::vector<Vector2> points;
  points.reserve(kArcSegments + 4);
  points.push_back(transform({kHandleRadius, kHandleRadius}));

  // Flutter's Material handle is a 22dp square painter containing the union of
  // a circle and its top-left quadrant rectangle. Draw the union as one fan so
  // the square quadrant never appears as a separate overdrawn primitive.
  points.push_back(transform({0.0f, 0.0f}));
  points.push_back(transform({kHandleRadius, 0.0f}));
  for (int i = 1; i <= kArcSegments; ++i) {
    const float angle = (-90.0f + 270.0f * (float)i / (float)kArcSegments) *
                        DEG2RAD;
    points.push_back(transform({kHandleRadius + std::cos(angle) * kHandleRadius,
                                kHandleRadius + std::sin(angle) * kHandleRadius}));
  }
  points.push_back(transform({0.0f, 0.0f}));
  DrawTriangleFan(points.data(), static_cast<int>(points.size()), color);
}

void PaintToolbar(Node &node) {
  if (!node.textEdit.toolbarVisible)
    return;
  if (MenuShowFn()) // native menu host owns the toolbar UI
    return;
  Rectangle r = ToolbarRect(node);
  ColorScheme &scheme = Theme::GetColorScheme();
  DrawRectangleRounded(r, 0.22f, 8, scheme.inverseSurface);
  const ToolbarButton *buttons = nullptr;
  const int count = SelToolbarButtons(node, buttons);
  float x = r.x + kToolbarPad;
  for (int i = 0; i < count; ++i) {
    Rectangle b{x, r.y + 4.0f, buttons[i].width, r.height - 8.0f};
    DrawRectangleRounded(b, 0.2f, 6, ColorAlpha(scheme.inverseSurface, 0.0f));
    raym3::Renderer::DrawText(buttons[i].label, {b.x + 8.0f, b.y + 7.0f}, 13.0f,
                              scheme.inverseOnSurface, FontWeight::Medium);
    x += buttons[i].width + kToolbarGap;
  }
}

} // namespace

// ─── Native selection UI host ───────────────────────────────────────────────

namespace {
bool g_menuShown = false;
Rectangle g_menuAnchor{};

// Frame-synced: called from PaintTextSelectionOverlay with the current target
// (or null). Shows/moves/hides the platform menu to match toolbarVisible.
void SyncSelectionMenuHost(Node *node) {
  if (!MenuShowFn())
    return;
  int start = 0, end = 0;
  const bool wants = node && node->textEdit.toolbarVisible &&
                     HasSelection(*node, start, end);
  if (!wants) {
    if (g_menuShown) {
      g_menuShown = false;
      if (MenuHideFn())
        MenuHideFn()();
    }
    return;
  }
  const float lineH = std::max(1.0f, SelPreferredLineHeight(*node));
  const float sx = SelByteOffsetX(*node, start);
  const float ex = SelByteOffsetX(*node, end);
  const float sy = SelByteOffsetY(*node, start);
  const float ey = SelByteOffsetY(*node, end);
  Rectangle anchor;
  anchor.x = std::min(sx, ex);
  anchor.width = std::max(1.0f, std::fabs(ex - sx));
  anchor.y = std::min(sy, ey) - lineH;
  anchor.height = std::max(lineH, std::fabs(ey - sy) + lineH);
  // Pad by exactly how far the handles hang past the selection so the platform
  // positions its menu clear of them — no more. The OS adds its own gap on top
  // of this rect, so any extra here reads as a floating, disconnected menu.
  if (HandleStyle() == SelectionHandleStyle::Cupertino) {
    // Lollipop: ball above the start handle, ball below the end handle.
    anchor.y -= 6.0f;
    anchor.height += 6.0f + 10.0f;
  } else {
    // Material teardrop hangs below the line; nothing above.
    anchor.height += kHandleSize;
  }
  const bool moved = std::fabs(anchor.x - g_menuAnchor.x) > 2.0f ||
                     std::fabs(anchor.y - g_menuAnchor.y) > 2.0f ||
                     std::fabs(anchor.width - g_menuAnchor.width) > 2.0f ||
                     std::fabs(anchor.height - g_menuAnchor.height) > 2.0f;
  if (g_menuShown && !moved)
    return;
  g_menuShown = true;
  g_menuAnchor = anchor;
  SelectionMenuRequest request;
  request.anchor = anchor;
  const bool editable = !IsSelectableTextTarget(*node);
  request.canCut = editable;
  request.canPaste = editable;
  request.canSelectAll = true;
  MenuShowFn()(request);
}
} // namespace

void SetSelectionMenuHost(std::function<void(const SelectionMenuRequest &)> show,
                          std::function<void()> hide) {
  MenuShowFn() = std::move(show);
  MenuHideFn() = std::move(hide);
}

bool SelectionMenuHostActive() { return static_cast<bool>(MenuShowFn()); }

void SetSelectionHandleStyle(SelectionHandleStyle style) {
  HandleStyle() = style;
}

void PerformSelectionMenuAction(const std::string &action) {
  Node *node = FocusedSelectionTarget();
  if (!node)
    return;
  TextEditState &edit = node->textEdit;
  if (IsSelectableTextTarget(*node)) {
    if (action == "copy") { // dismisses the selection, like RN/Android
      TextNodeCopy(*node);
      TextNodeClearSelection(*node);
    } else if (action == "selectAll") {
      TextNodeSelectAll(*node);
      // Handles + toolbar are a touch affordance. On a mouse host (desktop,
      // web) a Cmd+A that popped a floating Copy bar would be off-convention.
      edit.handlesVisible = !PointerIsMouse();
      edit.toolbarVisible = !PointerIsMouse();
      return;
    }
  } else {
    if (action == "cut")
      TextInputCut(*node);
    else if (action == "copy")
      TextInputCopy(*node);
    else if (action == "paste")
      TextInputPaste(*node);
    else if (action == "selectAll") {
      TextInputSelectAll(*node);
      edit.handlesVisible = !PointerIsMouse();
      edit.toolbarVisible = !PointerIsMouse();
      return;
    }
  }
  edit.toolbarVisible = false;
}

bool HandleTextSelectionOverlayInput(const NodePtr &root) {
  (void)root;
  Node *node = FocusedSelectionTarget();
  if (!node)
    return false;
  TextEditState &edit = node->textEdit;
  const PointerInput &p = GetPointer();
  int start = 0;
  int end = 0;
  bool hasSelection = HasSelection(*node, start, end);
  bool visible =
      edit.handlesVisible || edit.toolbarVisible || edit.activeHandle >= 0;
  if (!visible)
    return false;

  float startY = hasSelection ? SelByteOffsetY(*node, start)
                              : SelByteOffsetY(*node, edit.cursor);
  float endY = hasSelection ? SelByteOffsetY(*node, end) : startY;
  float startX = hasSelection ? SelByteOffsetX(*node, start)
                              : SelByteOffsetX(*node, edit.cursor);
  float endX = hasSelection ? SelByteOffsetX(*node, end) : startX;

  if (p.pressed) {
    int button = -1;
    if (edit.toolbarVisible && !SelectionMenuHostActive() &&
        PointInToolbarButton(*node, p.pos, button)) {
      if (IsSelectableTextTarget(*node)) {
        if (button == 0) { // Copy dismisses the selection, like RN/Android.
          TextNodeCopy(*node);
          TextNodeClearSelection(*node);
        } else if (button == 1) {
          TextNodeSelectAll(*node);
          edit.handlesVisible = true;
        }
      } else {
        if (button == 0)
          TextInputCut(*node);
        else if (button == 1)
          TextInputCopy(*node);
        else if (button == 2)
          TextInputPaste(*node);
        else if (button == 3)
          TextInputSelectAll(*node);
      }
      edit.toolbarVisible = false;
      MarkTextSelectionOverlayPointerConsumed();
      return true;
    }
    if (hasSelection && CheckCollisionPointRec(
                            p.pos,
                            HandleHitRect(startX, startY,
                                          TextHandleType::Left))) {
      BeginHandleDrag(*node, true, start, end, p.pos);
      MarkTextSelectionOverlayPointerConsumed();
      return true;
    }
    if (hasSelection && CheckCollisionPointRec(
                            p.pos,
                            HandleHitRect(endX, endY,
                                          TextHandleType::Right))) {
      BeginHandleDrag(*node, false, start, end, p.pos);
      MarkTextSelectionOverlayPointerConsumed();
      return true;
    }
    if (!CheckCollisionPointRec(p.pos, SelBounds(*node))) {
      edit.handlesVisible = false;
      edit.toolbarVisible = false;
      ClearActiveHandle(edit);
      edit.longPressSelectionActive = false;
    }
    return false;
  }

  if (edit.activeHandle >= 0) {
    if (p.down) {
      int pos = HitTestHandleDragTarget(*node, p.pos);
      UpdateDraggedSelection(*node, pos);
      edit.handlesVisible = true;
      edit.toolbarVisible = false;
      MarkTextSelectionOverlayPointerConsumed();
      return true;
    }
    if (p.released) {
      ClearActiveHandle(edit);
      int s = 0;
      int e = 0;
      edit.toolbarVisible = HasSelection(*node, s, e);
      edit.handlesVisible = edit.toolbarVisible;
      MarkTextSelectionOverlayPointerConsumed();
      return true;
    }
  }

  return false;
}

void PaintTextSelectionOverlay(const NodePtr &root) {
  (void)root;
  Node *node = FocusedSelectionTarget();
  SyncSelectionMenuHost(node);
  if (!node)
    return;
  int start = 0;
  int end = 0;
  bool hasSelection = HasSelection(*node, start, end);
  const TextEditState &edit = node->textEdit;
  if (edit.handlesVisible &&
      (hasSelection ||
       (edit.activeHandle >= 0 && edit.activeHandleAnchor >= 0 &&
        edit.activeHandleOffset >= 0))) {
    if (edit.activeHandle >= 0 && edit.activeHandleAnchor >= 0 &&
        edit.activeHandleOffset >= 0) {
      const int active = edit.activeHandleOffset;
      const int anchor = edit.activeHandleAnchor;
      DrawHandle(SelByteOffsetX(*node, active),
                 SelByteOffsetY(*node, active),
                 VisualHandleType(active, anchor));
      if (active != anchor) {
        DrawHandle(SelByteOffsetX(*node, anchor),
                   SelByteOffsetY(*node, anchor),
                   VisualHandleType(anchor, active));
      }
    } else {
      DrawHandle(SelByteOffsetX(*node, start),
                 SelByteOffsetY(*node, start), TextHandleType::Left);
      DrawHandle(SelByteOffsetX(*node, end),
                 SelByteOffsetY(*node, end), TextHandleType::Right);
    }
  }
  PaintToolbar(*node);
}

} // namespace raym3::v2
