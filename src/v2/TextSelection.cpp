#include "raym3/v2/TextSelection.h"

#include "raym3/v2/TextEngine.h"
#include "raym3/v2/TextInput.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string_view>

namespace raym3::v2 {

namespace {

std::function<void(NodeId, int, int)> &ChangeCallback() {
  static std::function<void(NodeId, int, int)> cb;
  return cb;
}

// UTF-8 byte walkers. Small local copies of TextInput.cpp's static helpers —
// stable byte math, duplicated rather than de-static-ing a 1500-line file.
bool IsUtf8Continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

int Utf8Prev(const std::string &text, int pos) {
  if (pos <= 0)
    return 0;
  pos = std::min(pos, static_cast<int>(text.size()));
  do {
    --pos;
  } while (pos > 0 && IsUtf8Continuation(static_cast<unsigned char>(text[pos])));
  return pos;
}

int Utf8Next(const std::string &text, int pos) {
  const int len = static_cast<int>(text.size());
  if (pos < 0)
    return 0;
  if (pos >= len)
    return len;
  unsigned char lead = static_cast<unsigned char>(text[pos]);
  int seq = 1;
  if ((lead & 0xE0) == 0xC0)
    seq = 2;
  else if ((lead & 0xF0) == 0xE0)
    seq = 3;
  else if ((lead & 0xF8) == 0xF0)
    seq = 4;
  return std::min(len, pos + seq);
}

int ClampUtf8Boundary(const std::string &text, int pos) {
  pos = std::clamp(pos, 0, static_cast<int>(text.size()));
  while (pos > 0 && IsUtf8Continuation(static_cast<unsigned char>(text[pos])))
    --pos;
  return pos;
}

bool IsWordChar(char c) {
  unsigned char u = static_cast<unsigned char>(c);
  return u >= 0x80 || (!std::isspace(u) && !std::ispunct(u));
}

// Laid-out geometry snapshot: the same PreparedText + LayoutText the renderer
// runs each frame (LayoutText over the cached prepare is pure arithmetic).
struct TextGeom {
  const PreparedText *prep = nullptr;
  TextLayoutResult layout;
  float lineHeight = 0.0f;
  TextAlignment align = TextAlignment::Left;
};

TextGeom GeomFor(const Node &node) {
  TextGeom g;
  g.prep = &GetPreparedTextForNode(node);
  g.layout = LayoutText(*g.prep, node.layout.width);
  g.lineHeight = std::max(1.0f, g.prep->options.lineHeight);
  g.align = node.style.text.alignment.value_or(TextAlignment::Left);
  if (g.layout.lines.empty()) {
    TextLine empty;
    g.layout.lines.push_back(empty);
  }
  return g;
}

float LineStartX(const Node &node, const TextGeom &g, const TextLine &line) {
  float x = node.layout.x;
  if (g.align == TextAlignment::Center)
    x += (node.layout.width - line.width) * 0.5f;
  else if (g.align == TextAlignment::Right)
    x += node.layout.width - line.width;
  return x;
}

int LineIndexForOffset(const TextGeom &g, int byteOffset) {
  const auto &lines = g.layout.lines;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    if (byteOffset <= static_cast<int>(lines[static_cast<size_t>(i)].byteEnd))
      return i;
  }
  return std::max(0, static_cast<int>(lines.size()) - 1);
}

// Width of source[line.byteStart .. byteOffset), measured with the exact
// measure the layout used. Ellipsized tail lines measure the source slice, not
// the painted '…' — acceptable v1 drift, documented in TextSelection.h.
float PrefixWidth(const TextGeom &g, const TextLine &line, int byteOffset) {
  const std::string &src = g.prep->source;
  const int start = static_cast<int>(line.byteStart);
  byteOffset = std::clamp(byteOffset, start, static_cast<int>(line.byteEnd));
  if (byteOffset <= start)
    return 0.0f;
  return DefaultMeasure(
      std::string_view(src).substr(static_cast<size_t>(start),
                                   static_cast<size_t>(byteOffset - start)),
      g.prep->options);
}

void FireChange(Node &node) {
  if (ChangeCallback())
    ChangeCallback()(IdOf(&node), node.textEdit.selectionStart,
                     node.textEdit.selectionEnd);
}

} // namespace

void SetTextSelectionChangeCallback(
    std::function<void(NodeId, int, int)> cb) {
  ChangeCallback() = std::move(cb);
}

bool NodeIsSelectableText(const Node &node) {
  return node.kind == NodeKind::Text &&
         node.style.text.selectable.value_or(false) && !node.text.empty();
}

Rectangle TextNodeBounds(const Node &node) { return node.layout; }

int TextNodeTextLength(const Node &node) {
  return static_cast<int>(node.text.size());
}

int TextNodeHitTestCaret(const Node &node, Vector2 posDp) {
  TextGeom g = GeomFor(node);
  const auto &lines = g.layout.lines;
  int li = static_cast<int>(std::floor((posDp.y - node.layout.y) / g.lineHeight));
  li = std::clamp(li, 0, static_cast<int>(lines.size()) - 1);
  const TextLine &line = lines[static_cast<size_t>(li)];
  const float rel = posDp.x - LineStartX(node, g, line);
  const int lineStart = static_cast<int>(line.byteStart);
  const int lineEnd = static_cast<int>(line.byteEnd);
  if (rel <= 0.0f)
    return lineStart;

  // Walk grapheme boundaries inside the line; midpoint rule picks the caret.
  int prev = lineStart;
  float prevWidth = 0.0f;
  for (std::size_t boundary : g.prep->graphemeBoundaries) {
    const int b = static_cast<int>(boundary);
    if (b <= lineStart)
      continue;
    if (b > lineEnd)
      break;
    const float width = PrefixWidth(g, line, b);
    if (rel < (prevWidth + width) * 0.5f)
      return prev;
    prev = b;
    prevWidth = width;
  }
  return prev;
}

float TextNodeByteOffsetX(const Node &node, int byteOffset) {
  TextGeom g = GeomFor(node);
  byteOffset = ClampUtf8Boundary(node.text, byteOffset);
  const int li = LineIndexForOffset(g, byteOffset);
  const TextLine &line = g.layout.lines[static_cast<size_t>(li)];
  return LineStartX(node, g, line) + PrefixWidth(g, line, byteOffset);
}

float TextNodeByteOffsetY(const Node &node, int byteOffset) {
  TextGeom g = GeomFor(node);
  byteOffset = ClampUtf8Boundary(node.text, byteOffset);
  const int li = LineIndexForOffset(g, byteOffset);
  return node.layout.y + (static_cast<float>(li) + 1.0f) * g.lineHeight;
}

float TextNodeLineCenterY(const Node &node, int byteOffset) {
  TextGeom g = GeomFor(node);
  byteOffset = ClampUtf8Boundary(node.text, byteOffset);
  const int li = LineIndexForOffset(g, byteOffset);
  return node.layout.y + (static_cast<float>(li) + 0.5f) * g.lineHeight;
}

float TextNodePreferredLineHeight(const Node &node) {
  return std::max(1.0f, GetPreparedTextForNode(node).options.lineHeight);
}

std::vector<Rectangle> TextNodeSelectionRects(const Node &node, int start,
                                              int end) {
  std::vector<Rectangle> rects;
  if (start < 0 || end < 0 || start == end)
    return rects;
  if (start > end)
    std::swap(start, end);
  TextGeom g = GeomFor(node);
  const auto &lines = g.layout.lines;
  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    const TextLine &line = lines[static_cast<size_t>(i)];
    const int lineStart = static_cast<int>(line.byteStart);
    const int lineEnd = static_cast<int>(line.byteEnd);
    const int selStart = std::max(start, lineStart);
    const int selEnd = std::min(end, lineEnd);
    const bool continuesPast = end > lineEnd;
    if (selStart >= selEnd && !(continuesPast && selStart == lineEnd))
      continue;
    if (selStart > lineEnd || selEnd < lineStart)
      continue;
    const float sx = PrefixWidth(g, line, selStart);
    // A selection that runs past this line keeps a small tail so the wrap
    // itself reads as selected (matches TextInput's multiline highlight).
    const float ex = continuesPast ? std::max(line.width + 4.0f, sx + 4.0f)
                                   : PrefixWidth(g, line, selEnd);
    rects.push_back({LineStartX(node, g, line) + sx,
                     node.layout.y + static_cast<float>(i) * g.lineHeight,
                     std::max(1.0f, ex - sx), g.lineHeight});
  }
  return rects;
}

void TextNodeWordBoundaries(const Node &node, int byteOffset, int &start,
                            int &end) {
  const std::string &text = node.text;
  const int len = static_cast<int>(text.size());
  byteOffset = ClampUtf8Boundary(text, std::clamp(byteOffset, 0, len));
  start = byteOffset;
  while (start > 0 && IsWordChar(text[static_cast<size_t>(start - 1)]))
    start = Utf8Prev(text, start);
  end = byteOffset;
  while (end < len && IsWordChar(text[static_cast<size_t>(end)]))
    end = Utf8Next(text, end);
  if (start == end && byteOffset < len)
    end = Utf8Next(text, byteOffset);
}

void TextNodeSetSelection(Node &node, int start, int end) {
  const std::string &text = node.text;
  const int newStart = start < 0 ? -1 : ClampUtf8Boundary(text, start);
  const int newEnd = end < 0 ? -1 : ClampUtf8Boundary(text, end);
  if (node.textEdit.selectionStart == newStart &&
      node.textEdit.selectionEnd == newEnd)
    return;
  node.textEdit.selectionStart = newStart;
  node.textEdit.selectionEnd = newEnd;
  FireChange(node);
}

void TextNodeSelectAll(Node &node) {
  TextNodeSetSelection(node, 0, static_cast<int>(node.text.size()));
}

void TextNodeClearSelection(Node &node) {
  node.textEdit.handlesVisible = false;
  node.textEdit.toolbarVisible = false;
  node.textEdit.isSelecting = false;
  node.textEdit.longPressSelectionActive = false;
  TextNodeSetSelection(node, -1, -1);
}

void TextNodeCopy(Node &node) {
  int start = node.textEdit.selectionStart;
  int end = node.textEdit.selectionEnd;
  if (start < 0 || end < 0 || start == end)
    return;
  if (start > end)
    std::swap(start, end);
  const std::string &text = node.text;
  start = ClampUtf8Boundary(text, start);
  end = ClampUtf8Boundary(text, end);
  if (end <= start)
    return;
  TextInputHostHooks &hooks = GetTextInputHostHooks();
  if (hooks.setClipboardText)
    hooks.setClipboardText(text.substr(static_cast<size_t>(start),
                                       static_cast<size_t>(end - start)));
}

} // namespace raym3::v2
