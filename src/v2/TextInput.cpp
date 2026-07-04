#include "raym3/v2/TextInput.h"

#include "raym3/raym3.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/Input.h"
#include "raym3/v2/RenderContext.h"
#include "raym3/v2/Style.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace raym3::v2 {

namespace {

constexpr float kFieldFontSize = 16.0f;
constexpr float kLabelRestSize = 16.0f;
constexpr float kLabelFloatSize = 12.0f;
constexpr float kBasePadding = 8.0f;
constexpr float kLineHeight = 20.0f;
constexpr int kMaxUndoHistory = 32;

static TextInputHostHooks &HostHooks() {
  static TextInputHostHooks hooks{
      []() -> std::string {
        const char *clip = ::GetClipboardText();
        return clip ? std::string(clip) : std::string{};
      },
      [](const std::string &text) { ::SetClipboardText(text.c_str()); },
      []() {}};
  return hooks;
}

static std::function<void(NodeId, const TextInputEditingState &)> &
StateCallback() {
  static std::function<void(NodeId, const TextInputEditingState &)> cb;
  return cb;
}

static NodePtr FindNodeById(const NodePtr &root, NodeId id) {
  if (!root || id == 0)
    return nullptr;
  if (IdOf(root) == id)
    return root;
  for (const NodePtr &child : root->children) {
    NodePtr found = FindNodeById(child, id);
    if (found)
      return found;
  }
  return nullptr;
}

static void NormalizeSelection(int &start, int &end) {
  if (start == -1 || end == -1) {
    start = end = -1;
    return;
  }
  if (start > end)
    std::swap(start, end);
}

static bool IsUtf8ContinuationByte(unsigned char c) {
  return (c & 0xC0) == 0x80;
}

static int Utf8SequenceLength(unsigned char lead) {
  if ((lead & 0x80) == 0x00)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 1;
}

static int Utf8Prev(const std::string &text, int pos) {
  if (pos <= 0)
    return 0;
  pos = std::min(pos, static_cast<int>(text.size()));
  do {
    --pos;
  } while (pos > 0 &&
           IsUtf8ContinuationByte(static_cast<unsigned char>(text[pos])));
  return pos;
}

static int Utf8Next(const std::string &text, int pos) {
  if (pos < 0)
    return 0;
  if (pos >= static_cast<int>(text.size()))
    return static_cast<int>(text.size());
  unsigned char lead = static_cast<unsigned char>(text[pos]);
  int len = Utf8SequenceLength(lead);
  return std::min(static_cast<int>(text.size()), pos + std::max(1, len));
}

static void Utf8AppendCodepoint(std::string &out, int codepoint) {
  if (codepoint < 0)
    return;
  if (codepoint <= 0x7F) {
    out.push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else if (codepoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
  }
}

static int Utf8CodepointCount(const std::string &text) {
  int count = 0;
  for (int i = 0; i < static_cast<int>(text.size()); i = Utf8Next(text, i))
    ++count;
  return count;
}

static bool IsWordByte(unsigned char c) {
  return c >= 0x80 || (!std::isspace(c) && !std::ispunct(c));
}

static int ClampUtf8Boundary(const std::string &text, int pos) {
  pos = std::clamp(pos, 0, static_cast<int>(text.size()));
  while (pos > 0 &&
         IsUtf8ContinuationByte(static_cast<unsigned char>(text[pos])))
    --pos;
  return pos;
}

static char *TextBuffer(Node &node) {
  if (node.textInput.buffer)
    return node.textInput.buffer;
  return node.inputBuffer.empty() ? nullptr : node.inputBuffer.data();
}

static int TextBufferSize(Node &node) {
  if (node.textInput.bufferSize > 0)
    return node.textInput.bufferSize;
  return static_cast<int>(node.inputBuffer.size());
}

static std::string DisplayText(const char *buffer, bool passwordMode) {
  if (!buffer)
    return {};
  if (!passwordMode)
    return buffer;
  std::string text = buffer;
  std::string masked;
  for (int i = 0; i < static_cast<int>(text.size()); i = Utf8Next(text, i))
    masked.push_back('*');
  return masked;
}

static std::string DisplayPrefix(const char *buffer, int bytePos,
                                 bool passwordMode) {
  if (!buffer)
    return {};
  std::string text = buffer;
  bytePos = ClampUtf8Boundary(text, bytePos);
  if (!passwordMode)
    return text.substr(0, bytePos);
  int count = 0;
  for (int i = 0; i < bytePos; i = Utf8Next(text, i))
    ++count;
  return std::string(static_cast<size_t>(count), '*');
}

static float MeasureDisplayText(std::string_view text) {
  std::string materialized(text);
  return raym3::Renderer::MeasureText(materialized.c_str(), kFieldFontSize,
                                      FontWeight::Regular)
      .x;
}

struct TextLineMetrics {
  int start = 0;
  int end = 0;       // Excludes the newline byte.
  int nextStart = 0; // Includes the newline byte, when present.
  float width = 0.0f;
};

static std::vector<TextLineMetrics> BuildLineMetrics(const char *buffer,
                                                     bool passwordMode) {
  std::string source = buffer ? buffer : "";
  std::vector<TextLineMetrics> lines;
  int len = static_cast<int>(source.size());
  int start = 0;
  for (int i = 0; i <= len; ++i) {
    if (i == len || source[static_cast<size_t>(i)] == '\n') {
      std::string slice = source.substr(static_cast<size_t>(start),
                                        static_cast<size_t>(i - start));
      std::string display =
          passwordMode
              ? std::string(static_cast<size_t>(Utf8CodepointCount(slice)), '*')
              : slice;
      lines.push_back(
          {start, i, i < len ? i + 1 : i, MeasureDisplayText(display)});
      start = i + 1;
    }
  }
  if (lines.empty())
    lines.push_back({0, 0, 0, 0.0f});
  return lines;
}

static int LineIndexForOffset(const std::vector<TextLineMetrics> &lines,
                              int byteOffset) {
  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    if (byteOffset <= lines[static_cast<size_t>(i)].end ||
        byteOffset < lines[static_cast<size_t>(i)].nextStart)
      return i;
  }
  return std::max(0, static_cast<int>(lines.size()) - 1);
}

static float PrefixWidthOnLine(const char *buffer, int lineStart,
                               int byteOffset, bool passwordMode) {
  std::string source = buffer ? buffer : "";
  byteOffset = ClampUtf8Boundary(source, byteOffset);
  lineStart = std::clamp(lineStart, 0, byteOffset);
  std::string slice =
      source.substr(static_cast<size_t>(lineStart),
                    static_cast<size_t>(byteOffset - lineStart));
  if (passwordMode)
    return MeasureDisplayText(
        std::string(static_cast<size_t>(Utf8CodepointCount(slice)), '*'));
  return MeasureDisplayText(slice);
}

static Vector2 TextOrigin(Rectangle inputBounds, bool multiline,
                          int lineCount = 1) {
  if (multiline) {
    return {inputBounds.x + kBasePadding * 2.0f, inputBounds.y + kBasePadding};
  }
  float contentHeight = std::max(kFieldFontSize, lineCount * kLineHeight);
  return {inputBounds.x + kBasePadding * 2.0f,
          inputBounds.y + (inputBounds.height - contentHeight) * 0.5f};
}

static int GetPrevWordPos(const char *text, int pos) {
  if (pos <= 0 || !text)
    return 0;
  std::string s = text;
  pos = ClampUtf8Boundary(s, pos);
  int i = Utf8Prev(s, pos);
  while (i > 0 && !IsWordByte(static_cast<unsigned char>(s[i])))
    i = Utf8Prev(s, i);
  while (i > 0 && IsWordByte(static_cast<unsigned char>(s[Utf8Prev(s, i)])))
    i = Utf8Prev(s, i);
  return i;
}

static int GetNextWordPos(const char *text, int pos) {
  if (!text)
    return 0;
  std::string s = text;
  int len = static_cast<int>(s.size());
  if (pos >= len)
    return len;
  int i = ClampUtf8Boundary(s, pos);
  while (i < len && IsWordByte(static_cast<unsigned char>(s[i])))
    i = Utf8Next(s, i);
  while (i < len && !IsWordByte(static_cast<unsigned char>(s[i])))
    i = Utf8Next(s, i);
  return i;
}

static bool IsWordChar(char c) {
  unsigned char u = static_cast<unsigned char>(c);
  return u >= 0x80 || (!std::isspace(u) && !std::ispunct(u));
}

static void FindWordBoundaries(const char *text, int pos, int &start,
                               int &end) {
  int len = text ? static_cast<int>(std::strlen(text)) : 0;
  pos = std::clamp(pos, 0, len);
  std::string s = text ? text : "";
  pos = ClampUtf8Boundary(s, pos);
  start = pos;
  while (start > 0 && IsWordChar(text[start - 1]))
    start = Utf8Prev(s, start);
  end = pos;
  while (end < len && IsWordChar(text[end]))
    end = Utf8Next(s, end);
  if (start == end && pos < len)
    end = Utf8Next(s, pos);
}

static TextInputEditingState SnapshotEditingState(const Node &node) {
  const TextEditState &edit = node.textEdit;
  const char *buffer =
      node.textInput.buffer ? node.textInput.buffer : node.inputBuffer.data();
  TextInputEditingState state;
  state.text = buffer ? std::string(buffer) : std::string{};
  state.selectionStart = edit.selectionStart;
  state.selectionEnd = edit.selectionEnd;
  state.composingStart = edit.composingStart;
  state.composingEnd = edit.composingEnd;
  return state;
}

static void NotifyTextInputState(Node &node, bool textChanged) {
  if (!StateCallback())
    return;
  NodeId id = IdOf(&node);
  TextInputEditingState next = SnapshotEditingState(node);
  next.textChanged = textChanged;
  auto &last = Ctx().lastNotifiedTextInputState[id];
  if (!textChanged && last.text == next.text &&
      last.selectionStart == next.selectionStart &&
      last.selectionEnd == next.selectionEnd &&
      last.composingStart == next.composingStart &&
      last.composingEnd == next.composingEnd)
    return;
  last.text = next.text;
  last.selectionStart = next.selectionStart;
  last.selectionEnd = next.selectionEnd;
  last.composingStart = next.composingStart;
  last.composingEnd = next.composingEnd;
  StateCallback()(id, next);
}

static void ResetCaretBlink(Node &node) {
  node.textEdit.lastBlinkTime = static_cast<float>(GetTime());
}

static void HideSelectionOverlay(Node &node) {
  node.textEdit.handlesVisible = false;
  node.textEdit.toolbarVisible = false;
  node.textEdit.activeHandle = -1;
  node.textEdit.longPressSelectionActive = false;
}

static void SaveToHistory(NodeId id, const std::string &text) {
  TextInputUndoState &u = Ctx().textInputUndo[id];
  if (u.isUndoRedo)
    return;
  if (u.index >= 0 && u.index < static_cast<int>(u.history.size()) - 1)
    u.history.erase(u.history.begin() + u.index + 1, u.history.end());
  u.history.push_back(text);
  if (static_cast<int>(u.history.size()) > kMaxUndoHistory)
    u.history.erase(u.history.begin());
  else
    u.index++;
  if (u.index >= kMaxUndoHistory)
    u.index = kMaxUndoHistory - 1;
}

static int HitTestCaretOnLine(const char *buffer, float clickRelativeX,
                              bool passwordMode, int lineStart, int lineEnd) {
  std::string source = buffer ? buffer : "";
  lineStart = std::clamp(lineStart, 0, static_cast<int>(source.size()));
  lineEnd = std::clamp(lineEnd, lineStart, static_cast<int>(source.size()));
  int clickPosition = lineEnd;
  float closestDiff = 10000.0f;
  int displayCount = 0;
  for (int i = lineStart;; i = Utf8Next(source, i)) {
    std::string sub = passwordMode
                          ? std::string(static_cast<size_t>(displayCount), '*')
                          : source.substr(static_cast<size_t>(lineStart),
                                          static_cast<size_t>(i - lineStart));
    float diff = std::fabs(MeasureDisplayText(sub) - clickRelativeX);
    if (diff < closestDiff) {
      closestDiff = diff;
      clickPosition = i;
    }
    if (i >= lineEnd)
      break;
    ++displayCount;
  }
  return clickPosition;
}

static int HitTestCaret(const char *buffer, float clickRelativeX,
                        bool passwordMode) {
  int len = buffer ? static_cast<int>(std::strlen(buffer)) : 0;
  return HitTestCaretOnLine(buffer, clickRelativeX, passwordMode, 0, len);
}

static void DrawSelection(Rectangle inputBounds, const char *text, int start,
                          int end, float scrollOffset, float textStartX,
                          bool passwordMode, bool multiline,
                          float scrollOffsetY,
                          const Color *overrideColor = nullptr) {
  if (start == -1 || end == -1 || start == end || !text)
    return;
  NormalizeSelection(start, end);
  ColorScheme &scheme = Theme::GetColorScheme();
  Color selectionColor = scheme.primary;
  selectionColor.a = 76;
  if (overrideColor)
    selectionColor = *overrideColor; // RN selectionColor
  if (!multiline) {
    std::string beforeStart = DisplayPrefix(text, start, passwordMode);
    std::string beforeEnd = DisplayPrefix(text, end, passwordMode);
    float startX = MeasureDisplayText(beforeStart);
    float endX = MeasureDisplayText(beforeEnd);
    float selectionX = inputBounds.x + textStartX - scrollOffset + startX;
    float selectionY =
        inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f;
    DrawRectangleRec({selectionX, selectionY, endX - startX, kFieldFontSize},
                     selectionColor);
    return;
  }

  std::vector<TextLineMetrics> lines = BuildLineMetrics(text, passwordMode);
  Vector2 origin =
      TextOrigin(inputBounds, true, static_cast<int>(lines.size()));
  for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
    const auto &line = lines[static_cast<size_t>(i)];
    int lineSelStart = std::max(start, line.start);
    int lineSelEnd = std::min(end, line.end);
    if (lineSelStart == lineSelEnd && !(start <= line.end && end > line.end))
      continue;
    float sx = PrefixWidthOnLine(text, line.start, lineSelStart, passwordMode);
    float ex =
        (end > line.end && line.nextStart > line.end)
            ? std::max(line.width + 4.0f, sx + 4.0f)
            : PrefixWidthOnLine(text, line.start, lineSelEnd, passwordMode);
    DrawRectangleRec({origin.x - scrollOffset + sx,
                      origin.y - scrollOffsetY + i * kLineHeight,
                      std::max(1.0f, ex - sx), kFieldFontSize},
                     selectionColor);
  }
}

static void DrawComposingUnderline(Rectangle inputBounds, const char *text,
                                   int start, int end, float scrollOffset,
                                   float textStartX, bool passwordMode) {
  if (start == -1 || end == -1 || start == end || !text)
    return;
  NormalizeSelection(start, end);
  std::string beforeStart = DisplayPrefix(text, start, passwordMode);
  std::string beforeEnd = DisplayPrefix(text, end, passwordMode);
  Vector2 startSize = raym3::Renderer::MeasureText(
      beforeStart.c_str(), kFieldFontSize, FontWeight::Regular);
  Vector2 endSize = raym3::Renderer::MeasureText(
      beforeEnd.c_str(), kFieldFontSize, FontWeight::Regular);
  float x = inputBounds.x + textStartX - scrollOffset + startSize.x;
  float width = std::max(1.0f, endSize.x - startSize.x);
  float y = inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f +
            kFieldFontSize - 2.0f;
  ColorScheme &scheme = Theme::GetColorScheme();
  Color underline = scheme.primary;
  underline.a = 255;
  DrawLineEx({x, y}, {x + width, y}, 2.0f, underline);
}

static void DrawCursor(Rectangle inputBounds, const char *text, int position,
                       float scrollOffset, float lastBlinkTime,
                       float textStartX, Color bgColor, bool passwordMode,
                       bool multiline, float scrollOffsetY) {
  float blinkCycle = (GetTime() - lastBlinkTime) * 2.0f;
  bool showCursor = (static_cast<int>(blinkCycle) % 2 == 0) ||
                    IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_DELETE) ||
                    IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
  if (!showCursor)
    return;

  float cursorX = inputBounds.x + textStartX - scrollOffset;
  float cursorY = inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f;
  if (multiline && text) {
    std::vector<TextLineMetrics> lines = BuildLineMetrics(text, passwordMode);
    int lineIndex = LineIndexForOffset(lines, position);
    const auto &line = lines[static_cast<size_t>(lineIndex)];
    Vector2 origin =
        TextOrigin(inputBounds, true, static_cast<int>(lines.size()));
    cursorX = origin.x - scrollOffset +
              PrefixWidthOnLine(text, line.start, position, passwordMode);
    cursorY = origin.y - scrollOffsetY + lineIndex * kLineHeight;
  } else if (text && position > 0) {
    int textLen = static_cast<int>(std::strlen(text));
    int pos = std::min(position, textLen);
    if (pos > 0) {
      std::string before = DisplayPrefix(text, pos, passwordMode);
      cursorX += MeasureDisplayText(before);
    }
  }

  Color cursorColor = Theme::GetColorScheme().onSurface;
  if (bgColor.a > 0) {
    float luminance =
        (0.299f * bgColor.r + 0.587f * bgColor.g + 0.114f * bgColor.b) / 255.0f;
    cursorColor = luminance > 0.5f ? BLACK : WHITE;
  }
  DrawLine(static_cast<int>(cursorX), static_cast<int>(cursorY),
           static_cast<int>(cursorX),
           static_cast<int>(cursorY + kFieldFontSize), cursorColor);
}

static void SyncScrollForCaret(Node &node, char *buffer, float textStartX,
                               float textEndX, bool passwordMode) {
  TextEditState &edit = node.textEdit;
  float availableWidth = textEndX - textStartX;
  std::string before = DisplayPrefix(buffer, edit.cursor, passwordMode);
  Vector2 cursorSize = raym3::Renderer::MeasureText(
      before.c_str(), kFieldFontSize, FontWeight::Regular);
  float cursorX = cursorSize.x;
  if (cursorX - edit.scrollOffsetX > availableWidth)
    edit.scrollOffsetX = cursorX - availableWidth;
  else if (cursorX - edit.scrollOffsetX < 0.0f)
    edit.scrollOffsetX = cursorX;
  std::string display = DisplayText(buffer, passwordMode);
  Vector2 totalSize = raym3::Renderer::MeasureText(
      display.c_str(), kFieldFontSize, FontWeight::Regular);
  float maxScroll = std::max(0.0f, totalSize.x - availableWidth);
  edit.scrollOffsetX = std::clamp(edit.scrollOffsetX, 0.0f, maxScroll);
}

static void SyncScrollForCaret(Node &node, char *buffer, Rectangle inputBounds,
                               bool passwordMode) {
  TextEditState &edit = node.textEdit;
  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  float textEndX = inputBounds.x + inputBounds.width - kBasePadding * 2.0f;
  if (!node.textInput.multiline) {
    edit.scrollOffsetY = 0.0f;
    SyncScrollForCaret(node, buffer, textStartX, textEndX, passwordMode);
    return;
  }

  std::vector<TextLineMetrics> lines = BuildLineMetrics(buffer, passwordMode);
  int lineIndex = LineIndexForOffset(lines, edit.cursor);
  const auto &line = lines[static_cast<size_t>(lineIndex)];
  float availableWidth = std::max(1.0f, textEndX - textStartX);
  float cursorX =
      PrefixWidthOnLine(buffer, line.start, edit.cursor, passwordMode);
  if (cursorX - edit.scrollOffsetX > availableWidth)
    edit.scrollOffsetX = cursorX - availableWidth;
  else if (cursorX - edit.scrollOffsetX < 0.0f)
    edit.scrollOffsetX = cursorX;
  float maxLineWidth = 0.0f;
  for (const auto &candidate : lines)
    maxLineWidth = std::max(maxLineWidth, candidate.width);
  edit.scrollOffsetX = std::clamp(
      edit.scrollOffsetX, 0.0f, std::max(0.0f, maxLineWidth - availableWidth));

  float contentHeight = lines.size() * kLineHeight;
  float caretTop = lineIndex * kLineHeight;
  float caretBottom = caretTop + kLineHeight;
  float visibleHeight =
      std::max(1.0f, inputBounds.height - kBasePadding * 2.0f);
  if (caretBottom - edit.scrollOffsetY > visibleHeight)
    edit.scrollOffsetY = caretBottom - visibleHeight;
  else if (caretTop - edit.scrollOffsetY < 0.0f)
    edit.scrollOffsetY = caretTop;
  edit.scrollOffsetY = std::clamp(
      edit.scrollOffsetY, 0.0f, std::max(0.0f, contentHeight - visibleHeight));
}

static Rectangle InputBoundsFor(Node &node) {
  Rectangle bounds = node.layout;
  const char *label =
      node.textInput.label.empty() ? nullptr : node.textInput.label.c_str();
  Rectangle inputBounds = bounds;
  if (label) {
    inputBounds.y += 20.0f;
    inputBounds.height -= 20.0f;
  }
  return inputBounds;
}

static void DeleteSelection(Node &node, char *buffer) {
  TextEditState &edit = node.textEdit;
  int sStart = edit.selectionStart, sEnd = edit.selectionEnd;
  NormalizeSelection(sStart, sEnd);
  if (sStart == -1 || sEnd == -1)
    return;
  int currentLen = static_cast<int>(std::strlen(buffer));
  sStart = ClampUtf8Boundary(buffer, sStart);
  sEnd = ClampUtf8Boundary(buffer, sEnd);
  std::memmove(&buffer[sStart], &buffer[sEnd],
               static_cast<size_t>(currentLen - sEnd + 1));
  edit.cursor = sStart;
  edit.selectionStart = edit.selectionEnd = -1;
  edit.composingStart = edit.composingEnd = -1;
}

static void CommitBuffer(Node &node, char *buffer) {
  if (node.textInput.value) {
    *node.textInput.value = std::string(buffer ? buffer : "");
    if (node.textInput.onChange)
      node.textInput.onChange(*node.textInput.value);
  }
  NotifyTextInputState(node, true);
}

constexpr double kMultiClickWindow = 0.35;
constexpr float kSelectionAutoScrollSpeed = 600.0f; // px/s past field edges

static void HandlePointer(Node &node, const PointerInput &p) {
  if (node.disabled || node.textInput.disabled || node.textInput.readOnly)
    return;
  char *buffer = TextBuffer(node);
  if (!buffer)
    return;

  TextEditState &edit = node.textEdit;
  Rectangle inputBounds = InputBoundsFor(node);
  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  float textEndX = inputBounds.x + inputBounds.width - kBasePadding * 2.0f;
  bool passwordMode = node.textInput.passwordMode;

  auto caretAt = [&](float screenX, float screenY) {
    float clickRelativeX = screenX - (textStartX - edit.scrollOffsetX);
    if (!node.textInput.multiline)
      return HitTestCaret(buffer, clickRelativeX, passwordMode);
    std::vector<TextLineMetrics> lines = BuildLineMetrics(buffer, passwordMode);
    Vector2 origin =
        TextOrigin(inputBounds, true, static_cast<int>(lines.size()));
    int lineIndex = static_cast<int>(
        std::floor((screenY - origin.y + edit.scrollOffsetY) / kLineHeight));
    lineIndex = std::clamp(lineIndex, 0, static_cast<int>(lines.size()) - 1);
    const auto &line = lines[static_cast<size_t>(lineIndex)];
    return HitTestCaretOnLine(buffer, clickRelativeX, passwordMode, line.start,
                              line.end);
  };

  if (p.pressed) {
    if (!CheckCollisionPointRec(p.pos, inputBounds)) {
      edit.isSelecting = false;
      edit.longPressActive = false;
      edit.longPressSelectionActive = false;
      edit.longPressHapticSent = false;
      return;
    }
    double now = GetTime();
    int pos = caretAt(p.pos.x, p.pos.y);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = pos;
      edit.cursor = pos;
      edit.selectionAnchor = edit.selectionStart;
      edit.isSelecting = true;
      edit.clickCount = 1;
    } else {
      if (now - edit.lastClickTime < kMultiClickWindow &&
          std::abs(pos - edit.lastClickPos) <= 1)
        edit.clickCount++;
      else
        edit.clickCount = 1;
      edit.lastClickPos = pos;

      if (edit.clickCount >= 3) {
        int len = static_cast<int>(std::strlen(buffer));
        edit.selectionStart = 0;
        edit.selectionEnd = len;
        edit.selectionAnchor = 0;
        edit.cursor = len;
        edit.isSelecting = false;
      } else if (edit.clickCount == 2) {
        int ws = pos, we = pos;
        FindWordBoundaries(buffer, pos, ws, we);
        edit.selectionStart = ws;
        edit.selectionEnd = we;
        edit.selectionAnchor = ws;
        edit.cursor = we;
        edit.isSelecting = false;
      } else {
        edit.cursor = pos;
        edit.selectionStart = edit.selectionEnd = -1;
        edit.selectionAnchor = pos;
        edit.isSelecting = true;
      }
    }
    edit.lastClickTime = now;
    edit.lastBlinkTime = static_cast<float>(GetTime());
    edit.longPressActive = true;
    edit.longPressSelectionActive = false;
    edit.longPressHapticSent = false;
    edit.longPressStartTime = now;
    edit.longPressOrigin = p.pos;
    edit.longPressAnchor = pos;
    edit.handlesVisible = false;
    edit.toolbarVisible = false;
    edit.composingStart = edit.composingEnd = -1;
    NotifyTextInputState(node, false);
    return;
  }

  if (p.down && edit.isSelecting) {
    float dt = GetFrameTime();
    if (dt <= 0.0f || dt > 0.1f)
      dt = 0.016f;
    // Auto-scroll while dragging past the field edges.
    if (p.pos.x < textStartX) {
      edit.scrollOffsetX =
          std::max(0.0f, edit.scrollOffsetX - kSelectionAutoScrollSpeed * dt);
    } else if (p.pos.x > textEndX) {
      std::string display = DisplayText(buffer, passwordMode);
      Vector2 totalSize = raym3::Renderer::MeasureText(
          display.c_str(), kFieldFontSize, FontWeight::Regular);
      float maxScroll = std::max(0.0f, totalSize.x - (textEndX - textStartX));
      edit.scrollOffsetX = std::min(
          maxScroll, edit.scrollOffsetX + kSelectionAutoScrollSpeed * dt);
    }
    int pos = caretAt(std::clamp(p.pos.x, textStartX, textEndX), p.pos.y);
    if (edit.longPressActive && GetTime() - edit.longPressStartTime > 0.45) {
      int start = 0;
      int end = 0;
      FindWordBoundaries(buffer, edit.longPressAnchor, start, end);
      int pressStart = start;
      FindWordBoundaries(buffer, pos, start, end);
      edit.selectionStart = pressStart;
      edit.selectionEnd = end;
      edit.handlesVisible = true;
      edit.toolbarVisible = true;
      edit.longPressSelectionActive = true;
      if (!edit.longPressHapticSent && HostHooks().hapticFeedback) {
        HostHooks().hapticFeedback();
        edit.longPressHapticSent = true;
      }
    } else if (pos != edit.selectionAnchor || edit.selectionStart != -1) {
      edit.selectionStart = edit.selectionAnchor;
      edit.selectionEnd = pos;
    }
    if (pos != edit.cursor) {
      edit.cursor = pos;
      edit.lastBlinkTime = static_cast<float>(GetTime());
    }
    NotifyTextInputState(node, false);
    return;
  }

  if (p.released) {
    const bool keepToolbar = edit.longPressSelectionActive;
    edit.isSelecting = false;
    edit.longPressActive = false;
    edit.longPressSelectionActive = false;
    edit.longPressHapticSent = false;
    if (edit.selectionStart != -1 && edit.selectionEnd != -1)
      edit.handlesVisible = true;
    if (keepToolbar)
      edit.toolbarVisible = true;
    NotifyTextInputState(node, false);
  }
}

static void ProcessKeyboard(Node &node) {
  char *buffer = TextBuffer(node);
  int bufferSize = TextBufferSize(node);
  if (!buffer || bufferSize <= 1)
    return;
  if (node.disabled || node.textInput.disabled || node.textInput.readOnly)
    return;

  TextEditState &edit = node.textEdit;
  NodeId id = IdOf(&node);
  TextInputUndoState &undo = Ctx().textInputUndo[id];

  bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
  bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
  bool control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                 IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
  bool cmd = control;

  auto notify = [&]() { CommitBuffer(node, buffer); };

  // Escape always blurs without submitting.
  if (IsKeyPressed(KEY_ESCAPE)) {
    edit.isSelecting = false;
    edit.selectionStart = edit.selectionEnd = -1;
    Blur();
    return;
  }

  // Enter: a multiline field that should not blur inserts a newline; otherwise
  // it fires onSubmit (react-native onSubmitEditing) and, when blurOnSubmit is
  // set (single-line default), blurs.
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
    if (node.textInput.multiline && !node.textInput.blurOnSubmit) {
      int len = static_cast<int>(std::strlen(buffer));
      if (!node.textInput.maxLength ||
          Utf8CodepointCount(std::string(buffer)) < node.textInput.maxLength) {
        if (edit.selectionStart != -1 && edit.selectionEnd != -1) {
          SaveToHistory(id, buffer);
          DeleteSelection(node, buffer);
          len = static_cast<int>(std::strlen(buffer));
        } else {
          SaveToHistory(id, buffer);
        }
        edit.cursor = std::clamp(edit.cursor, 0, len);
        if (len + 1 < bufferSize) {
          if (edit.cursor < len)
            std::memmove(&buffer[edit.cursor + 1], &buffer[edit.cursor],
                         static_cast<size_t>(len - edit.cursor + 1));
          buffer[edit.cursor] = '\n';
          edit.cursor += 1;
        }
        edit.selectionStart = edit.selectionEnd = -1;
        edit.composingStart = edit.composingEnd = -1;
        edit.lastBlinkTime = static_cast<float>(GetTime());
        notify();
      }
      return;
    }
    if (node.textInput.onSubmit)
      node.textInput.onSubmit(std::string(buffer));
    if (node.textInput.blurOnSubmit) {
      edit.isSelecting = false;
      edit.selectionStart = edit.selectionEnd = -1;
      Blur();
    }
    return;
  }

  if (cmd && IsKeyPressed(KEY_A)) {
    int len = static_cast<int>(std::strlen(buffer));
    edit.selectionStart = 0;
    edit.selectionEnd = len;
    edit.cursor = len;
    edit.lastBlinkTime = static_cast<float>(GetTime());
    NotifyTextInputState(node, false);
    return;
  }

  if (cmd && IsKeyPressed(KEY_C)) {
    int s = edit.selectionStart, e = edit.selectionEnd;
    NormalizeSelection(s, e);
    if (s != -1 && e != -1) {
      if (HostHooks().setClipboardText)
        HostHooks().setClipboardText(std::string(buffer + s, e - s));
      else
        SetClipboardText(std::string(buffer + s, e - s).c_str());
    }
    return;
  }

  if (cmd && IsKeyPressed(KEY_X)) {
    int s = edit.selectionStart, e = edit.selectionEnd;
    NormalizeSelection(s, e);
    if (s != -1 && e != -1) {
      if (HostHooks().setClipboardText)
        HostHooks().setClipboardText(std::string(buffer + s, e - s));
      else
        SetClipboardText(std::string(buffer + s, e - s).c_str());
      SaveToHistory(id, buffer);
      DeleteSelection(node, buffer);
      notify();
    }
    return;
  }

  if (cmd && IsKeyPressed(KEY_V)) {
    std::string clip =
        HostHooks().getClipboardText
            ? HostHooks().getClipboardText()
            : std::string(GetClipboardText() ? GetClipboardText() : "");
    const char *clipText = clip.c_str();
    if (clipText && clipText[0]) {
      SaveToHistory(id, buffer);
      int s = edit.selectionStart, e = edit.selectionEnd;
      NormalizeSelection(s, e);
      if (s != -1 && e != -1)
        DeleteSelection(node, buffer);
      int len = static_cast<int>(std::strlen(buffer));
      // react-native maxLength: truncate the pasted text so the total codepoint
      // count never exceeds the limit.
      if (node.textInput.maxLength > 0) {
        int have = Utf8CodepointCount(std::string(buffer));
        int room = node.textInput.maxLength - have;
        if (room <= 0) {
          clip.clear();
          clipText = clip.c_str();
        } else if (Utf8CodepointCount(clip) > room) {
          int bytePos = 0, cps = 0;
          for (; bytePos < static_cast<int>(clip.size()) && cps < room;
               bytePos = Utf8Next(clip, bytePos))
            cps++;
          clip.resize(static_cast<size_t>(bytePos));
          clipText = clip.c_str();
        }
      }
      int clipLen = static_cast<int>(std::strlen(clipText));
      int avail = bufferSize - 1 - len;
      int toCopy = std::min(clipLen, avail);
      if (toCopy > 0) {
        edit.cursor = std::clamp(edit.cursor, 0, len);
        if (edit.cursor < len)
          std::memmove(&buffer[edit.cursor + toCopy], &buffer[edit.cursor],
                       static_cast<size_t>(len - edit.cursor + 1));
        std::memcpy(&buffer[edit.cursor], clipText,
                    static_cast<size_t>(toCopy));
        edit.cursor += toCopy;
        edit.selectionStart = edit.selectionEnd = -1;
        edit.lastBlinkTime = static_cast<float>(GetTime());
        notify();
      }
    }
    return;
  }

  if (cmd && !shift) {
    bool shouldUndo = false;
    if (IsKeyPressed(KEY_Z)) {
      shouldUndo = true;
      edit.undoTimer = GetTime() + 0.5;
    } else if (IsKeyDown(KEY_Z) && GetTime() > edit.undoTimer) {
      shouldUndo = true;
      edit.undoTimer = GetTime() + 0.05;
    }
    if (shouldUndo && undo.index > 0) {
      undo.isUndoRedo = true;
      undo.index--;
      std::strncpy(buffer, undo.history[undo.index].c_str(), bufferSize - 1);
      buffer[bufferSize - 1] = '\0';
      edit.cursor = static_cast<int>(std::strlen(buffer));
      edit.selectionStart = edit.selectionEnd = -1;
      edit.lastBlinkTime = static_cast<float>(GetTime());
      undo.isUndoRedo = false;
      notify();
      return;
    }
  }

  if (cmd && (shift || IsKeyDown(KEY_Y))) {
    bool shouldRedo = false;
    if (IsKeyPressed(KEY_Y) || (shift && IsKeyPressed(KEY_Z))) {
      shouldRedo = true;
      edit.redoTimer = GetTime() + 0.5;
    } else if ((IsKeyDown(KEY_Y) || (shift && IsKeyDown(KEY_Z))) &&
               GetTime() > edit.redoTimer) {
      shouldRedo = true;
      edit.redoTimer = GetTime() + 0.05;
    }
    if (shouldRedo && undo.index < static_cast<int>(undo.history.size()) - 1) {
      undo.isUndoRedo = true;
      undo.index++;
      std::strncpy(buffer, undo.history[undo.index].c_str(), bufferSize - 1);
      buffer[bufferSize - 1] = '\0';
      edit.cursor = static_cast<int>(std::strlen(buffer));
      edit.selectionStart = edit.selectionEnd = -1;
      edit.lastBlinkTime = static_cast<float>(GetTime());
      undo.isUndoRedo = false;
      notify();
      return;
    }
  }

  if (IsKeyDown(KEY_LEFT)) {
    bool shouldMove = IsKeyPressed(KEY_LEFT);
    if (!shouldMove && GetTime() > edit.arrowLeftTimer)
      shouldMove = true;
    if (shouldMove) {
      edit.arrowLeftTimer = GetTime() + (IsKeyPressed(KEY_LEFT) ? 0.5 : 0.05);
      edit.lastBlinkTime = static_cast<float>(GetTime());
      int target = edit.cursor;
      if (!shift && edit.selectionStart != -1) {
        int s = edit.selectionStart, e = edit.selectionEnd;
        NormalizeSelection(s, e);
        target = s;
        edit.selectionStart = edit.selectionEnd = -1;
      } else {
        if (cmd)
          target = 0;
        else if (alt)
          target = GetPrevWordPos(buffer, target);
        else if (target > 0)
          target = Utf8Prev(buffer, target);
        if (shift) {
          if (edit.selectionStart == -1)
            edit.selectionStart = edit.cursor;
          edit.selectionEnd = target;
        } else {
          edit.selectionStart = edit.selectionEnd = -1;
        }
      }
      edit.cursor = target;
      edit.composingStart = edit.composingEnd = -1;
      HideSelectionOverlay(node);
      NotifyTextInputState(node, false);
      return;
    }
  }

  if (IsKeyDown(KEY_RIGHT)) {
    bool shouldMove = IsKeyPressed(KEY_RIGHT);
    if (!shouldMove && GetTime() > edit.arrowRightTimer)
      shouldMove = true;
    if (shouldMove) {
      edit.arrowRightTimer = GetTime() + (IsKeyPressed(KEY_RIGHT) ? 0.5 : 0.05);
      edit.lastBlinkTime = static_cast<float>(GetTime());
      int len = static_cast<int>(std::strlen(buffer));
      int target = edit.cursor;
      if (!shift && edit.selectionStart != -1) {
        int s = edit.selectionStart, e = edit.selectionEnd;
        NormalizeSelection(s, e);
        target = e;
        edit.selectionStart = edit.selectionEnd = -1;
      } else {
        if (cmd)
          target = len;
        else if (alt)
          target = GetNextWordPos(buffer, target);
        else if (target < len)
          target = Utf8Next(buffer, target);
        if (shift) {
          if (edit.selectionStart == -1)
            edit.selectionStart = edit.cursor;
          edit.selectionEnd = target;
        } else {
          edit.selectionStart = edit.selectionEnd = -1;
        }
      }
      edit.cursor = target;
      edit.composingStart = edit.composingEnd = -1;
      HideSelectionOverlay(node);
      NotifyTextInputState(node, false);
      return;
    }
  }

  if (node.textInput.multiline &&
      (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN))) {
    std::vector<TextLineMetrics> lines =
        BuildLineMetrics(buffer, node.textInput.passwordMode);
    int lineIndex = LineIndexForOffset(lines, edit.cursor);
    const auto &line = lines[static_cast<size_t>(lineIndex)];
    float currentX = PrefixWidthOnLine(buffer, line.start, edit.cursor,
                                       node.textInput.passwordMode);
    int nextLine =
        IsKeyPressed(KEY_UP)
            ? std::max(0, lineIndex - 1)
            : std::min(static_cast<int>(lines.size()) - 1, lineIndex + 1);
    const auto &targetLine = lines[static_cast<size_t>(nextLine)];
    int target =
        HitTestCaretOnLine(buffer, currentX, node.textInput.passwordMode,
                           targetLine.start, targetLine.end);
    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = target;
    } else {
      edit.selectionStart = edit.selectionEnd = -1;
    }
    edit.cursor = target;
    edit.composingStart = edit.composingEnd = -1;
    edit.lastBlinkTime = static_cast<float>(GetTime());
    HideSelectionOverlay(node);
    NotifyTextInputState(node, false);
    return;
  }

  if (IsKeyPressed(KEY_HOME)) {
    edit.lastBlinkTime = static_cast<float>(GetTime());
    int target = 0;
    if (node.textInput.multiline && !cmd) {
      auto lines = BuildLineMetrics(buffer, node.textInput.passwordMode);
      target =
          lines[static_cast<size_t>(LineIndexForOffset(lines, edit.cursor))]
              .start;
    }
    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = target;
    } else {
      edit.selectionStart = edit.selectionEnd = -1;
    }
    edit.cursor = target;
    edit.composingStart = edit.composingEnd = -1;
    HideSelectionOverlay(node);
    NotifyTextInputState(node, false);
    return;
  }

  if (IsKeyPressed(KEY_END)) {
    edit.lastBlinkTime = static_cast<float>(GetTime());
    int target = static_cast<int>(std::strlen(buffer));
    if (node.textInput.multiline && !cmd) {
      auto lines = BuildLineMetrics(buffer, node.textInput.passwordMode);
      target =
          lines[static_cast<size_t>(LineIndexForOffset(lines, edit.cursor))]
              .end;
    }
    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = target;
    } else {
      edit.selectionStart = edit.selectionEnd = -1;
    }
    edit.cursor = target;
    edit.composingStart = edit.composingEnd = -1;
    HideSelectionOverlay(node);
    NotifyTextInputState(node, false);
    return;
  }

  int sStart = edit.selectionStart, sEnd = edit.selectionEnd;
  NormalizeSelection(sStart, sEnd);
  bool hasSelection = sStart != -1 && sEnd != -1;

  if (IsKeyDown(KEY_BACKSPACE)) {
    bool shouldDelete = IsKeyPressed(KEY_BACKSPACE);
    if (!shouldDelete && GetTime() > edit.backspaceTimer)
      shouldDelete = true;
    if (shouldDelete) {
      edit.backspaceTimer =
          GetTime() + (IsKeyPressed(KEY_BACKSPACE) ? 0.5 : 0.05);
      edit.lastBlinkTime = static_cast<float>(GetTime());
      if (hasSelection) {
        SaveToHistory(id, buffer);
        DeleteSelection(node, buffer);
      } else if (cmd && edit.cursor > 0) {
        SaveToHistory(id, buffer);
        int len = static_cast<int>(std::strlen(buffer));
        int start = GetPrevWordPos(buffer, edit.cursor);
        std::memmove(&buffer[start], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
        edit.cursor = start;
      } else if (alt && edit.cursor > 0) {
        int prev = GetPrevWordPos(buffer, edit.cursor);
        SaveToHistory(id, buffer);
        int len = static_cast<int>(std::strlen(buffer));
        std::memmove(&buffer[prev], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
        edit.cursor = prev;
      } else if (edit.cursor > 0) {
        SaveToHistory(id, buffer);
        int len = static_cast<int>(std::strlen(buffer));
        int prev = Utf8Prev(std::string(buffer), edit.cursor);
        std::memmove(&buffer[prev], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
        edit.cursor = prev;
      }
      edit.composingStart = edit.composingEnd = -1;
      HideSelectionOverlay(node);
      notify();
      return;
    }
  }

  if (IsKeyDown(KEY_DELETE)) {
    bool shouldDelete = IsKeyPressed(KEY_DELETE);
    if (!shouldDelete && GetTime() > edit.deleteTimer)
      shouldDelete = true;
    if (shouldDelete) {
      edit.deleteTimer = GetTime() + (IsKeyPressed(KEY_DELETE) ? 0.5 : 0.05);
      edit.lastBlinkTime = static_cast<float>(GetTime());
      int len = static_cast<int>(std::strlen(buffer));
      if (hasSelection) {
        SaveToHistory(id, buffer);
        DeleteSelection(node, buffer);
      } else if (alt && edit.cursor < len) {
        int next = GetNextWordPos(buffer, edit.cursor);
        SaveToHistory(id, buffer);
        std::memmove(&buffer[edit.cursor], &buffer[next],
                     static_cast<size_t>(len - next + 1));
      } else if (edit.cursor < len) {
        SaveToHistory(id, buffer);
        int next = Utf8Next(std::string(buffer), edit.cursor);
        std::memmove(&buffer[edit.cursor], &buffer[next],
                     static_cast<size_t>(len - next + 1));
      }
      edit.composingStart = edit.composingEnd = -1;
      HideSelectionOverlay(node);
      notify();
      return;
    }
  }

  int key = GetCharPressed();
  while (key > 0) {
    int len = static_cast<int>(std::strlen(buffer));
    // react-native maxLength: cap by codepoint count. A selection replace still
    // counts as a net change, so only block when there is no selection to free
    // up room.
    bool atMaxLength =
        node.textInput.maxLength > 0 && !hasSelection &&
        Utf8CodepointCount(std::string(buffer)) >= node.textInput.maxLength;
    if (len < bufferSize - 1 && key >= 32 && !atMaxLength) {
      if (hasSelection) {
        SaveToHistory(id, buffer);
        DeleteSelection(node, buffer);
        len = static_cast<int>(std::strlen(buffer));
      } else {
        SaveToHistory(id, buffer);
      }
      edit.cursor = std::clamp(edit.cursor, 0, len);
      std::string insert;
      Utf8AppendCodepoint(insert, key);
      int insertLen = static_cast<int>(insert.size());
      if (len + insertLen < bufferSize) {
        if (edit.cursor < len)
          std::memmove(&buffer[edit.cursor + insertLen], &buffer[edit.cursor],
                       static_cast<size_t>(len - edit.cursor + 1));
        std::memcpy(&buffer[edit.cursor], insert.data(),
                    static_cast<size_t>(insertLen));
        edit.cursor += insertLen;
      }
      edit.lastBlinkTime = static_cast<float>(GetTime());
      edit.selectionStart = edit.selectionEnd = -1;
      edit.composingStart = edit.composingEnd = -1;
      HideSelectionOverlay(node);
      notify();
    }
    key = GetCharPressed();
  }
}

} // namespace

void ResyncTextInputBuffer(NodeId nodeId, int cursorPos) {
  (void)cursorPos;
  Ctx().textInputUndo.erase(nodeId);
  Ctx().lastNotifiedTextInputState.erase(nodeId);
}

void SetTextInputHostHooks(TextInputHostHooks hooks) {
  HostHooks() = std::move(hooks);
}

void SetTextInputStateCallback(
    std::function<void(NodeId, const TextInputEditingState &)> cb) {
  StateCallback() = std::move(cb);
}

TextInputHostHooks &GetTextInputHostHooks() { return HostHooks(); }

Rectangle TextInputInputBounds(Node &node) { return InputBoundsFor(node); }

int TextInputHitTestCaret(Node &node, float screenX) {
  char *buffer = TextBuffer(node);
  if (!buffer)
    return 0;
  Rectangle inputBounds = InputBoundsFor(node);
  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  float clickRelativeX = screenX - (textStartX - node.textEdit.scrollOffsetX);
  return HitTestCaret(buffer, clickRelativeX, node.textInput.passwordMode);
}

float TextInputByteOffsetX(Node &node, int byteOffset) {
  char *buffer = TextBuffer(node);
  if (!buffer)
    return node.layout.x;
  Rectangle inputBounds = InputBoundsFor(node);
  if (node.textInput.multiline) {
    auto lines = BuildLineMetrics(buffer, node.textInput.passwordMode);
    int lineIndex = LineIndexForOffset(lines, byteOffset);
    const auto &line = lines[static_cast<size_t>(lineIndex)];
    Vector2 origin =
        TextOrigin(inputBounds, true, static_cast<int>(lines.size()));
    return origin.x - node.textEdit.scrollOffsetX +
           PrefixWidthOnLine(buffer, line.start, byteOffset,
                             node.textInput.passwordMode);
  }
  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  std::string prefix =
      DisplayPrefix(buffer, byteOffset, node.textInput.passwordMode);
  return textStartX - node.textEdit.scrollOffsetX + MeasureDisplayText(prefix);
}

float TextInputByteOffsetY(Node &node, int byteOffset) {
  Rectangle inputBounds = InputBoundsFor(node);
  char *buffer = TextBuffer(node);
  if (!buffer || !node.textInput.multiline)
    return inputBounds.y + inputBounds.height * 0.5f;
  auto lines = BuildLineMetrics(buffer, node.textInput.passwordMode);
  int lineIndex = LineIndexForOffset(lines, byteOffset);
  Vector2 origin =
      TextOrigin(inputBounds, true, static_cast<int>(lines.size()));
  return origin.y - node.textEdit.scrollOffsetY + lineIndex * kLineHeight +
         kFieldFontSize * 0.5f;
}

void TextInputNotifyEditingState(Node &node, bool textChanged) {
  NotifyTextInputState(node, textChanged);
}

void TextInputSetSelection(Node &node, int start, int end, int cursor) {
  char *buffer = TextBuffer(node);
  std::string s = buffer ? std::string(buffer) : std::string{};
  node.textEdit.selectionStart = start < 0 ? -1 : ClampUtf8Boundary(s, start);
  node.textEdit.selectionEnd = end < 0 ? -1 : ClampUtf8Boundary(s, end);
  node.textEdit.cursor = ClampUtf8Boundary(s, cursor);
  node.textEdit.composingStart = node.textEdit.composingEnd = -1;
  ResetCaretBlink(node);
  NotifyTextInputState(node, false);
}

void TextInputSelectAll(Node &node) {
  char *buffer = TextBuffer(node);
  int len = buffer ? static_cast<int>(std::strlen(buffer)) : 0;
  node.textEdit.selectionStart = 0;
  node.textEdit.selectionEnd = len;
  node.textEdit.cursor = len;
  node.textEdit.composingStart = node.textEdit.composingEnd = -1;
  node.textEdit.handlesVisible = len > 0;
  node.textEdit.toolbarVisible = len > 0;
  ResetCaretBlink(node);
  NotifyTextInputState(node, false);
}

bool TextInputCopy(Node &node) {
  char *buffer = TextBuffer(node);
  if (!buffer)
    return false;
  int s = node.textEdit.selectionStart, e = node.textEdit.selectionEnd;
  NormalizeSelection(s, e);
  if (s < 0 || e < 0 || s == e)
    return false;
  if (HostHooks().setClipboardText)
    HostHooks().setClipboardText(std::string(buffer + s, e - s));
  return true;
}

bool TextInputCut(Node &node) {
  if (node.textInput.readOnly || node.textInput.disabled || node.disabled)
    return false;
  char *buffer = TextBuffer(node);
  if (!buffer || !TextInputCopy(node))
    return false;
  SaveToHistory(IdOf(&node), buffer);
  DeleteSelection(node, buffer);
  node.textEdit.handlesVisible = false;
  node.textEdit.toolbarVisible = false;
  ResetCaretBlink(node);
  CommitBuffer(node, buffer);
  return true;
}

bool TextInputPaste(Node &node) {
  if (node.textInput.readOnly || node.textInput.disabled || node.disabled)
    return false;
  std::string clip = HostHooks().getClipboardText
                         ? HostHooks().getClipboardText()
                         : std::string{};
  if (clip.empty())
    return false;
  return TextInputReplaceSelection(node, clip);
}

bool TextInputReplaceSelection(Node &node, const std::string &text,
                               int composingStart, int composingEnd) {
  if (node.textInput.readOnly || node.textInput.disabled || node.disabled)
    return false;
  char *buffer = TextBuffer(node);
  int bufferSize = TextBufferSize(node);
  if (!buffer || bufferSize <= 1)
    return false;
  SaveToHistory(IdOf(&node), buffer);
  int s = node.textEdit.selectionStart, e = node.textEdit.selectionEnd;
  NormalizeSelection(s, e);
  if (s < 0 || e < 0) {
    s = e = std::clamp(node.textEdit.cursor, 0,
                       static_cast<int>(std::strlen(buffer)));
  }
  std::string current(buffer);
  s = ClampUtf8Boundary(current, s);
  e = ClampUtf8Boundary(current, e);
  current.replace(static_cast<size_t>(s), static_cast<size_t>(e - s), text);
  if (static_cast<int>(current.size()) >= bufferSize)
    current.resize(static_cast<size_t>(bufferSize - 1));
  std::fill(buffer, buffer + bufferSize, '\0');
  std::memcpy(buffer, current.data(), current.size());
  int insertedEnd =
      ClampUtf8Boundary(current, s + static_cast<int>(text.size()));
  node.textEdit.cursor = insertedEnd;
  node.textEdit.selectionStart = node.textEdit.selectionEnd = -1;
  if (composingStart >= 0 && composingEnd >= composingStart) {
    node.textEdit.composingStart =
        ClampUtf8Boundary(current, s + composingStart);
    node.textEdit.composingEnd = ClampUtf8Boundary(current, s + composingEnd);
  } else {
    node.textEdit.composingStart = node.textEdit.composingEnd = -1;
  }
  node.textEdit.handlesVisible = false;
  node.textEdit.toolbarVisible = false;
  ResetCaretBlink(node);
  CommitBuffer(node, buffer);
  return true;
}

void TextInputSetEditingState(Node &node, const std::string &text,
                              int selectionStart, int selectionEnd,
                              int composingStart, int composingEnd,
                              bool notifyTextChanged) {
  char *buffer = TextBuffer(node);
  int bufferSize = TextBufferSize(node);
  if (!buffer || bufferSize <= 1)
    return;
  std::string clipped = text;
  if (static_cast<int>(clipped.size()) >= bufferSize)
    clipped.resize(static_cast<size_t>(bufferSize - 1));
  std::fill(buffer, buffer + bufferSize, '\0');
  std::memcpy(buffer, clipped.data(), clipped.size());
  if (node.textInput.value)
    *node.textInput.value = clipped;
  int len = static_cast<int>(clipped.size());
  node.textEdit.selectionStart =
      selectionStart < 0 ? -1 : ClampUtf8Boundary(clipped, selectionStart);
  node.textEdit.selectionEnd =
      selectionEnd < 0 ? -1 : ClampUtf8Boundary(clipped, selectionEnd);
  node.textEdit.cursor =
      selectionEnd < 0 ? len : ClampUtf8Boundary(clipped, selectionEnd);
  node.textEdit.composingStart =
      composingStart < 0 ? -1 : ClampUtf8Boundary(clipped, composingStart);
  node.textEdit.composingEnd =
      composingEnd < 0 ? -1 : ClampUtf8Boundary(clipped, composingEnd);
  ResetCaretBlink(node);
  if (notifyTextChanged && node.textInput.onChange)
    node.textInput.onChange(clipped);
  NotifyTextInputState(node, notifyTextChanged);
}

void ResolveTextInput(const NodePtr &root) {
  if (!root)
    return;

  const PointerInput &p = GetPointer();
  NodeId focused = GetFocusedId();

  if (focused != Ctx().lastFocusedTextInput) {
    NodePtr prev = Ctx().lastFocusedTextInput != 0
                       ? FindNodeById(root, Ctx().lastFocusedTextInput)
                       : nullptr;
    NodePtr next = focused != 0 ? FindNodeById(root, focused) : nullptr;

    const bool switchingField = prev && prev->kind == NodeKind::TextInput &&
                                next && next->kind == NodeKind::TextInput &&
                                IdOf(prev) != IdOf(next);

    // Gain focus (and IME switch) before blur so onBlur can see the new field.
    if (next && next->kind == NodeKind::TextInput) {
      if ((!next->textEdit.wasFocused || switchingField) &&
          next->textInput.onFocus)
        next->textInput.onFocus();
      next->textEdit.wasFocused = true;
      char *buffer = TextBuffer(*next);
      if (buffer) {
        int len = static_cast<int>(std::strlen(buffer));
        if (next->textEdit.cursor > len)
          next->textEdit.cursor = len;
        TextInputUndoState &u = Ctx().textInputUndo[IdOf(next)];
        if (u.history.empty()) {
          u.history.push_back(buffer);
          u.index = 0;
        }
      }
      NotifyTextInputState(*next, true);
    }

    if (prev && prev->kind == NodeKind::TextInput) {
      if (prev->textInput.onBlur)
        prev->textInput.onBlur();
      prev->textEdit.wasFocused = false;
    }

    Ctx().lastFocusedTextInput = focused;
  }

  if (focused != 0) {
    NodePtr focusedNode = FindNodeById(root, focused);
    if (focusedNode && focusedNode->kind == NodeKind::TextInput) {
      HandlePointer(*focusedNode, p);
      ProcessKeyboard(*focusedNode);
    }
  }

  // I-beam cursor over hovered (enabled) text inputs on desktop.
  bool wantIBeam = false;
  if (NodeId hovered = GetHoveredId()) {
    NodePtr hoveredNode = FindNodeById(root, hovered);
    if (hoveredNode && hoveredNode->kind == NodeKind::TextInput &&
        !hoveredNode->disabled && !hoveredNode->textInput.disabled &&
        CheckCollisionPointRec(p.pos, InputBoundsFor(*hoveredNode)))
      wantIBeam = true;
  }
  if (wantIBeam != Ctx().ibeamCursorActive) {
    SetMouseCursor(wantIBeam ? MOUSE_CURSOR_IBEAM : MOUSE_CURSOR_DEFAULT);
    Ctx().ibeamCursorActive = wantIBeam;
  }
}

void PaintTextInput(Node &node) {
  char *buffer = TextBuffer(node);
  int bufferSize = TextBufferSize(node);
  if (!buffer || bufferSize <= 0)
    return;

  const TextInputProps &ti = node.textInput;
  TextEditState &edit = node.textEdit;
  bool isFocused = GetFocusedId() == IdOf(&node);

  bool disabled = ti.disabled || node.disabled;
  Rectangle bounds = node.layout;
  const char *label = ti.label.empty() ? nullptr : ti.label.c_str();
  Rectangle inputBounds = InputBoundsFor(node);

  ColorScheme &scheme = Theme::GetColorScheme();
  Style style = node.style;
  float cornerRadius =
      style.borderRadius.value_or(Theme::GetShapeTokens().cornerMedium);

  if (disabled) {
    if (label) {
      raym3::Renderer::DrawText(label, {bounds.x, bounds.y}, kLabelFloatSize,
                                scheme.onSurfaceVariant, FontWeight::Regular);
    }
    raym3::Renderer::DrawRoundedRectangleEx(inputBounds, cornerRadius,
                                            scheme.outline, 1.0f);
    if (buffer[0]) {
      Vector2 textPos = {inputBounds.x + kBasePadding * 2.0f,
                         inputBounds.y +
                             (inputBounds.height - kFieldFontSize) / 2.0f};
      Color disabledText = scheme.onSurface;
      disabledText.a = 128;
      raym3::Renderer::DrawText(buffer, textPos, kFieldFontSize, disabledText,
                                FontWeight::Regular);
    } else if (!ti.placeholder.empty()) {
      Vector2 textPos = {inputBounds.x + kBasePadding * 2.0f,
                         inputBounds.y +
                             (inputBounds.height - kFieldFontSize) / 2.0f};
      Color ph = scheme.onSurfaceVariant;
      ph.a = 128;
      raym3::Renderer::DrawText(ti.placeholder.c_str(), textPos, kFieldFontSize,
                                ph, FontWeight::Regular);
    }
    return;
  }

  if (label) {
    bool hasContent = buffer[0] != '\0';
    float target = (isFocused || hasContent) ? 1.0f : 0.0f;
    float dtl = GetFrameTime();
    if (dtl <= 0.0f || dtl > 0.1f)
      dtl = 0.016f;
    edit.labelAnim += (target - edit.labelAnim) * std::min(1.0f, dtl * 16.0f);
    float a = edit.labelAnim;
    float restY = inputBounds.y + (inputBounds.height - kLabelRestSize) / 2.0f;
    float ly = restY + (bounds.y - restY) * a;
    float restX = inputBounds.x + kBasePadding * 2.0f;
    float lx = restX + (bounds.x + kBasePadding - restX) * a;
    float fontSize = kLabelRestSize + (kLabelFloatSize - kLabelRestSize) * a;
    Color labelColor = isFocused ? scheme.primary : scheme.onSurfaceVariant;
    raym3::Renderer::DrawText(label, {lx, ly}, fontSize, labelColor,
                              FontWeight::Regular);
  }

  Color bgColor = scheme.surface;
  if (style.backgroundColor)
    bgColor = *style.backgroundColor;

  if (ti.variant == TextFieldVariant::Filled && ti.drawBackground) {
    bgColor = style.backgroundColor.value_or(scheme.surfaceContainerHighest);
    raym3::Renderer::DrawRoundedRectangle(inputBounds, cornerRadius, bgColor);
    if (cornerRadius > 0.0f) {
      DrawRectangleRec({inputBounds.x,
                        inputBounds.y + inputBounds.height - cornerRadius,
                        inputBounds.width, cornerRadius},
                       bgColor);
    }
  }

  Color outlineColor = scheme.outline;
  float outlineWidth = 1.0f;
  if (isFocused && !ti.readOnly) {
    outlineColor = scheme.primary;
    outlineWidth = 2.0f;
  }

  if (ti.drawOutline) {
    if (ti.variant == TextFieldVariant::Outlined) {
      raym3::Renderer::DrawRoundedRectangleEx(inputBounds, cornerRadius,
                                              outlineColor, outlineWidth);
    } else {
      DrawRectangleRec({inputBounds.x,
                        inputBounds.y + inputBounds.height - outlineWidth,
                        inputBounds.width, outlineWidth},
                       outlineColor);
    }
  }

  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  float textEndX = inputBounds.x + inputBounds.width - kBasePadding * 2.0f;
  if (isFocused)
    SyncScrollForCaret(node, buffer, inputBounds, ti.passwordMode);
  else {
    float availableWidth = textEndX - textStartX;
    if (ti.multiline) {
      auto lines = BuildLineMetrics(buffer, ti.passwordMode);
      float maxLineWidth = 0.0f;
      for (const auto &line : lines)
        maxLineWidth = std::max(maxLineWidth, line.width);
      edit.scrollOffsetX =
          maxLineWidth > availableWidth ? maxLineWidth - availableWidth : 0.0f;
      edit.scrollOffsetY = 0.0f;
    } else {
      std::string display = DisplayText(buffer, ti.passwordMode);
      float totalWidth = MeasureDisplayText(display);
      edit.scrollOffsetX =
          totalWidth > availableWidth ? totalWidth - availableWidth : 0.0f;
      edit.scrollOffsetY = 0.0f;
    }
  }

  float currentScroll = edit.scrollOffsetX;
  float currentScrollY = edit.scrollOffsetY;
  int scissorW = static_cast<int>(std::ceil(textEndX - textStartX));
  int scissorH = static_cast<int>(std::ceil(inputBounds.height));
  bool scissorActive = false;
  if (scissorW > 0 && scissorH > 0) {
    raym3::PushScissor({textStartX, inputBounds.y, static_cast<float>(scissorW),
                        static_cast<float>(scissorH)});
    scissorActive = true;
  }

  if (isFocused) {
    DrawSelection(inputBounds, buffer, edit.selectionStart, edit.selectionEnd,
                  currentScroll, textStartX - inputBounds.x, ti.passwordMode,
                  ti.multiline, currentScrollY,
                  ti.hasSelectionColor ? &ti.selectionColor : nullptr);
    if (!ti.multiline) {
      DrawComposingUnderline(inputBounds, buffer, edit.composingStart,
                             edit.composingEnd, currentScroll,
                             textStartX - inputBounds.x, ti.passwordMode);
    }
  }

  bool isEmpty = buffer[0] == '\0';
  bool showPlaceholder = isEmpty && !ti.placeholder.empty() && !label;
  auto lines = BuildLineMetrics(buffer, ti.passwordMode);
  Vector2 origin =
      TextOrigin(inputBounds, ti.multiline, static_cast<int>(lines.size()));
  Vector2 textPos = {textStartX - currentScroll,
                     inputBounds.y +
                         (inputBounds.height - kFieldFontSize) / 2.0f};
  if (ti.multiline)
    textPos = {origin.x - currentScroll, origin.y - currentScrollY};
  Color textColor = style.text.color.value_or(scheme.onSurface);

  if (showPlaceholder) {
    Color ph = scheme.onSurfaceVariant;
    ph.a = 180;
    if (ti.hasPlaceholderColor)
      ph = ti.placeholderColor; // RN placeholderTextColor
    raym3::Renderer::DrawText(ti.placeholder.c_str(), textPos, kFieldFontSize,
                              ph, FontWeight::Regular);
  } else if (!isEmpty) {
    if (ti.multiline) {
      for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const auto &line = lines[static_cast<size_t>(i)];
        std::string source(buffer);
        std::string slice =
            source.substr(static_cast<size_t>(line.start),
                          static_cast<size_t>(line.end - line.start));
        std::string display =
            ti.passwordMode
                ? std::string(static_cast<size_t>(Utf8CodepointCount(slice)),
                              '*')
                : slice;
        raym3::Renderer::DrawText(
            display.c_str(), {textPos.x, textPos.y + i * kLineHeight},
            kFieldFontSize, textColor, FontWeight::Regular);
      }
    } else {
      std::string masked = DisplayText(buffer, ti.passwordMode);
      raym3::Renderer::DrawText(masked.c_str(), textPos, kFieldFontSize,
                                textColor, FontWeight::Regular);
    }
  }

  // RN caretHidden suppresses the blinking cursor.
  if (isFocused && !ti.readOnly && !ti.caretHidden) {
    DrawCursor(inputBounds, buffer, edit.cursor, currentScroll,
               edit.lastBlinkTime, textStartX - inputBounds.x, bgColor,
               ti.passwordMode, ti.multiline, currentScrollY);
  }

  if (scissorActive)
    raym3::PopScissor();
}

} // namespace raym3::v2
