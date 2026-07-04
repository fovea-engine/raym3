#include "raym3/v2/TextSelectionOverlay.h"

#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/Input.h"
#include "raym3/v2/TextInput.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <raylib.h>

namespace raym3::v2 {
namespace {

constexpr float kHandleRadius = 7.0f;
constexpr float kHandleStem = 18.0f;
constexpr float kHandleHit = 28.0f;
constexpr float kToolbarH = 38.0f;
constexpr float kToolbarPad = 8.0f;
constexpr float kToolbarGap = 4.0f;
constexpr float kToolbarYGap = 8.0f;

Node *FocusedTextInput() {
  NodeId id = GetFocusedId();
  if (!id)
    return nullptr;
  auto *node = reinterpret_cast<Node *>(id);
  if (!node || node->kind != NodeKind::TextInput)
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

Rectangle HandleHitRect(float x, float y) {
  return {x - kHandleHit * 0.5f, y - kHandleHit * 0.5f, kHandleHit, kHandleHit};
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

void DrawHandle(float x, float y, bool start) {
  Color color = Theme::GetColorScheme().primary;
  DrawLineEx({x, y}, {x, y + kHandleStem}, 2.0f, color);
  DrawCircleV({x, start ? y : y + kHandleStem}, kHandleRadius, color);
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
      return true;
    }
    if (hasSelection &&
        CheckCollisionPointRec(p.pos, HandleHitRect(startX, startY))) {
      edit.activeHandle = 0;
      return true;
    }
    if (hasSelection && CheckCollisionPointRec(
                            p.pos, HandleHitRect(endX, endY + kHandleStem))) {
      edit.activeHandle = 1;
      return true;
    }
    if (!CheckCollisionPointRec(p.pos, TextInputInputBounds(*node))) {
      edit.handlesVisible = false;
      edit.toolbarVisible = false;
      edit.activeHandle = -1;
      edit.longPressSelectionActive = false;
    }
    return false;
  }

  if (edit.activeHandle >= 0) {
    if (p.down) {
      int pos = TextInputHitTestCaret(*node, p.pos);
      if (edit.activeHandle == 0) {
        TextInputSetSelection(*node, pos, end, end);
      } else {
        TextInputSetSelection(*node, start, pos, pos);
      }
      edit.handlesVisible = true;
      edit.toolbarVisible = false;
      return true;
    }
    if (p.released) {
      edit.activeHandle = -1;
      int s = 0;
      int e = 0;
      edit.toolbarVisible = HasSelection(*node, s, e);
      edit.handlesVisible = edit.toolbarVisible;
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
  if (node->textEdit.handlesVisible && hasSelection) {
    DrawHandle(TextInputByteOffsetX(*node, start),
               TextInputByteOffsetY(*node, start), true);
    DrawHandle(TextInputByteOffsetX(*node, end),
               TextInputByteOffsetY(*node, end), false);
  }
  PaintToolbar(*node);
}

} // namespace raym3::v2
