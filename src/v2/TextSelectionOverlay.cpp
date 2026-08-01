#include "raym3/v2/TextSelectionOverlay.h"

#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/Input.h"
#include "raym3/v2/TextInput.h"

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

Node *FocusedTextInput() {
  NodeId id = GetFocusedId();
  if (!id)
    return nullptr;
  auto *node = reinterpret_cast<Node *>(id);
  if (!node || node->kind != NodeKind::TextInput)
    return nullptr;
  // A field backed by a real platform editor owns its own selection UI: the
  // UITextField/EditText draws the caret, the drag handles, and the
  // cut/copy/paste menu. Painting raym3's overlay on top would double every
  // one of them, so the renderer stays out of the way entirely.
  if (node->textInput.nativeEditor)
    return nullptr;
  return node;
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
  Rectangle input = TextInputInputBounds(node);
  float startX = hasSelection
                     ? TextInputByteOffsetX(node, start)
                     : TextInputByteOffsetX(node, node.textEdit.cursor);
  float endX = hasSelection ? TextInputByteOffsetX(node, end) : startX;
  float centerX = (startX + endX) * 0.5f;
  const float widths[] = {42, 50, 54, 74};
  float totalW = kToolbarPad * 2.0f + kToolbarGap * 3.0f;
  for (float w : widths)
    totalW += w;
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
  const float widths[] = {42, 50, 54, 74};
  float x = r.x + kToolbarPad;
  for (int i = 0; i < 4; ++i) {
    Rectangle b{x, r.y + 4.0f, widths[i], r.height - 8.0f};
    if (CheckCollisionPointRec(p, b)) {
      buttonIndex = i;
      return true;
    }
    x += widths[i] + kToolbarGap;
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
      TextInputLineCenterY(node, edit.activeHandleOffset) - pointer.y;
  edit.toolbarVisible = false;
}

float SnappedHandleDragY(Node &node, float dragY, float handleY) {
  const float lineHeight = std::max(1.0f, TextInputPreferredLineHeight(node));
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
  return TextInputHitTestCaret(node, target);
}

void UpdateDraggedSelection(Node &node, int currentOffset) {
  TextEditState &edit = node.textEdit;
  const int draggedHandle = edit.activeHandle;
  const int anchor = edit.activeHandleAnchor >= 0 ? edit.activeHandleAnchor
                                                  : node.textEdit.cursor;
  TextInputDragSelectionUpdate update = TextInputResolveDraggedSelection(
      draggedHandle == 0 ? TextInputDraggedEdge::Start
                         : TextInputDraggedEdge::End,
      anchor, currentOffset, TextInputTextLength(node));
  if (!update.applied)
    return;

  TextInputSetSelection(node, update.selectionStart, update.selectionEnd,
                        update.cursor);
  edit.activeHandle = draggedHandle;
  edit.activeHandleAnchor = anchor;
  edit.activeHandleOffset = update.cursor;
  edit.activeHandleDragY = TextInputLineCenterY(node, update.cursor) -
                           edit.activeHandleDragTargetY;
}

TextHandleType VisualHandleType(int offset, int anchor) {
  if (offset < anchor)
    return TextHandleType::Left;
  if (offset > anchor)
    return TextHandleType::Right;
  return TextHandleType::Collapsed;
}

void DrawHandle(float x, float y, TextHandleType type) {
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
  Rectangle r = ToolbarRect(node);
  ColorScheme &scheme = Theme::GetColorScheme();
  DrawRectangleRounded(r, 0.22f, 8, scheme.inverseSurface);
  const char *labels[] = {"Cut", "Copy", "Paste", "All"};
  const float widths[] = {42, 50, 54, 74};
  float x = r.x + kToolbarPad;
  for (int i = 0; i < 4; ++i) {
    Rectangle b{x, r.y + 4.0f, widths[i], r.height - 8.0f};
    DrawRectangleRounded(b, 0.2f, 6, ColorAlpha(scheme.inverseSurface, 0.0f));
    raym3::Renderer::DrawText(labels[i], {b.x + 8.0f, b.y + 7.0f}, 13.0f,
                              scheme.inverseOnSurface, FontWeight::Medium);
    x += widths[i] + kToolbarGap;
  }
}

} // namespace

bool HandleTextSelectionOverlayInput(const NodePtr &root) {
  (void)root;
  Node *node = FocusedTextInput();
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

  float startY = hasSelection ? TextInputByteOffsetY(*node, start)
                              : TextInputByteOffsetY(*node, edit.cursor);
  float endY = hasSelection ? TextInputByteOffsetY(*node, end) : startY;
  float startX = hasSelection ? TextInputByteOffsetX(*node, start)
                              : TextInputByteOffsetX(*node, edit.cursor);
  float endX = hasSelection ? TextInputByteOffsetX(*node, end) : startX;

  if (p.pressed) {
    int button = -1;
    if (edit.toolbarVisible && PointInToolbarButton(*node, p.pos, button)) {
      if (button == 0)
        TextInputCut(*node);
      else if (button == 1)
        TextInputCopy(*node);
      else if (button == 2)
        TextInputPaste(*node);
      else if (button == 3)
        TextInputSelectAll(*node);
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
    if (!CheckCollisionPointRec(p.pos, TextInputInputBounds(*node))) {
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
  Node *node = FocusedTextInput();
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
      DrawHandle(TextInputByteOffsetX(*node, active),
                 TextInputByteOffsetY(*node, active),
                 VisualHandleType(active, anchor));
      if (active != anchor) {
        DrawHandle(TextInputByteOffsetX(*node, anchor),
                   TextInputByteOffsetY(*node, anchor),
                   VisualHandleType(anchor, active));
      }
    } else {
      DrawHandle(TextInputByteOffsetX(*node, start),
                 TextInputByteOffsetY(*node, start), TextHandleType::Left);
      DrawHandle(TextInputByteOffsetX(*node, end),
                 TextInputByteOffsetY(*node, end), TextHandleType::Right);
    }
  }
  PaintToolbar(*node);
}

} // namespace raym3::v2
