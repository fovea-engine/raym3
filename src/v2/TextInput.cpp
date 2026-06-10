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
#include <vector>

namespace raym3::v2 {

namespace {

constexpr float kFieldFontSize = 16.0f;
constexpr float kLabelRestSize = 16.0f;
constexpr float kLabelFloatSize = 12.0f;
constexpr float kBasePadding = 8.0f;
constexpr int kMaxUndoHistory = 32;

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
  return std::string(std::strlen(buffer), '*');
}

static int GetPrevWordPos(const char *text, int pos) {
  if (pos <= 0 || !text)
    return 0;
  int i = pos - 1;
  while (i > 0 && std::isspace(static_cast<unsigned char>(text[i])))
    i--;
  while (i > 0 && !std::isspace(static_cast<unsigned char>(text[i])) &&
         !std::ispunct(static_cast<unsigned char>(text[i])))
    i--;
  if (std::isspace(static_cast<unsigned char>(text[i])) ||
      std::ispunct(static_cast<unsigned char>(text[i])))
    return i + 1;
  return i;
}

static int GetNextWordPos(const char *text, int pos) {
  if (!text)
    return 0;
  int len = static_cast<int>(std::strlen(text));
  if (pos >= len)
    return len;
  int i = pos;
  while (i < len && !std::isspace(static_cast<unsigned char>(text[i])) &&
         !std::ispunct(static_cast<unsigned char>(text[i])))
    i++;
  while (i < len && std::isspace(static_cast<unsigned char>(text[i])))
    i++;
  return i;
}

static bool IsWordChar(char c) {
  unsigned char u = static_cast<unsigned char>(c);
  return !std::isspace(u) && !std::ispunct(u);
}

static void FindWordBoundaries(const char *text, int pos, int &start,
                               int &end) {
  int len = text ? static_cast<int>(std::strlen(text)) : 0;
  pos = std::clamp(pos, 0, len);
  start = pos;
  while (start > 0 && IsWordChar(text[start - 1]))
    start--;
  end = pos;
  while (end < len && IsWordChar(text[end]))
    end++;
  if (start == end && pos < len)
    end = pos + 1;
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

static int HitTestCaret(const char *buffer, float clickRelativeX,
                        bool passwordMode) {
  int len = buffer ? static_cast<int>(std::strlen(buffer)) : 0;
  int clickPosition = len;
  std::string display = DisplayText(buffer, passwordMode);
  if (len > 0) {
    float closestDiff = 10000.0f;
    for (int i = 0; i <= len; ++i) {
      std::string sub = display.substr(0, i);
      Vector2 size = raym3::Renderer::MeasureText(
          sub.c_str(), kFieldFontSize, FontWeight::Regular);
      float diff = std::fabs(size.x - clickRelativeX);
      if (diff < closestDiff) {
        closestDiff = diff;
        clickPosition = i;
      }
    }
  }
  return clickPosition;
}

static void DrawSelection(Rectangle inputBounds, const char *text, int start,
                          int end, float scrollOffset, float textStartX) {
  if (start == -1 || end == -1 || start == end || !text)
    return;
  NormalizeSelection(start, end);
  ColorScheme &scheme = Theme::GetColorScheme();
  std::string beforeStart(text, start);
  std::string beforeEnd(text, end);
  Vector2 startSize = raym3::Renderer::MeasureText(
      beforeStart.c_str(), kFieldFontSize, FontWeight::Regular);
  Vector2 endSize = raym3::Renderer::MeasureText(
      beforeEnd.c_str(), kFieldFontSize, FontWeight::Regular);
  float selectionX = inputBounds.x + textStartX - scrollOffset + startSize.x;
  float selectionWidth = endSize.x - startSize.x;
  float selectionY =
      inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f;
  Color selectionColor = scheme.primary;
  selectionColor.a = 76;
  DrawRectangleRec({selectionX, selectionY, selectionWidth, kFieldFontSize},
                   selectionColor);
}

static void DrawCursor(Rectangle inputBounds, const char *text, int position,
                       float scrollOffset, float lastBlinkTime,
                       float textStartX, Color bgColor, bool passwordMode) {
  float blinkCycle = (GetTime() - lastBlinkTime) * 2.0f;
  bool showCursor = (static_cast<int>(blinkCycle) % 2 == 0) ||
                    IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_DELETE) ||
                    IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_RIGHT);
  if (!showCursor)
    return;

  float cursorX = inputBounds.x + textStartX - scrollOffset;
  if (text && position > 0) {
    int textLen = static_cast<int>(std::strlen(text));
    int pos = std::min(position, textLen);
    if (pos > 0) {
      std::string before = DisplayText(text, passwordMode).substr(0, pos);
      Vector2 textSize = raym3::Renderer::MeasureText(
          before.c_str(), kFieldFontSize, FontWeight::Regular);
      cursorX += textSize.x;
    }
  }

  Color cursorColor = Theme::GetColorScheme().onSurface;
  if (bgColor.a > 0) {
    float luminance =
        (0.299f * bgColor.r + 0.587f * bgColor.g + 0.114f * bgColor.b) / 255.0f;
    cursorColor = luminance > 0.5f ? BLACK : WHITE;
  }
  float cursorY = inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f;
  DrawLine(static_cast<int>(cursorX), static_cast<int>(cursorY),
           static_cast<int>(cursorX),
           static_cast<int>(cursorY + kFieldFontSize), cursorColor);
}

static void SyncScrollForCaret(Node &node, char *buffer, float textStartX,
                               float textEndX, bool passwordMode) {
  TextEditState &edit = node.textEdit;
  float availableWidth = textEndX - textStartX;
  std::string before = DisplayText(buffer, passwordMode).substr(0, edit.cursor);
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
  std::memmove(&buffer[sStart], &buffer[sEnd],
               static_cast<size_t>(currentLen - sEnd + 1));
  edit.cursor = sStart;
  edit.selectionStart = edit.selectionEnd = -1;
}

static void CommitBuffer(Node &node, char *buffer) {
  if (node.textInput.value) {
    *node.textInput.value = std::string(buffer ? buffer : "");
    if (node.textInput.onChange)
      node.textInput.onChange(*node.textInput.value);
  }
}

constexpr double kMultiClickWindow = 0.35;
constexpr float kSelectionAutoScrollSpeed = 600.0f; // px/s past field edges

static std::function<void(NodeId, int)> s_cursorCallback;

static void NotifyCursorMoved(Node &node, int prevCursor) {
  if (s_cursorCallback && node.textEdit.cursor != prevCursor)
    s_cursorCallback(IdOf(&node), node.textEdit.cursor);
}

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

  auto caretAt = [&](float screenX) {
    float clickRelativeX = screenX - (textStartX - edit.scrollOffsetX);
    return HitTestCaret(buffer, clickRelativeX, passwordMode);
  };

  if (p.pressed) {
    if (!CheckCollisionPointRec(p.pos, inputBounds)) {
      edit.isSelecting = false;
      return;
    }
    double now = GetTime();
    int pos = caretAt(p.pos.x);
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
    return;
  }

  if (p.down && edit.isSelecting) {
    float dt = GetFrameTime();
    if (dt <= 0.0f || dt > 0.1f)
      dt = 0.016f;
    // Auto-scroll while dragging past the field edges.
    if (p.pos.x < textStartX) {
      edit.scrollOffsetX = std::max(
          0.0f, edit.scrollOffsetX - kSelectionAutoScrollSpeed * dt);
    } else if (p.pos.x > textEndX) {
      std::string display = DisplayText(buffer, passwordMode);
      Vector2 totalSize = raym3::Renderer::MeasureText(
          display.c_str(), kFieldFontSize, FontWeight::Regular);
      float maxScroll = std::max(0.0f, totalSize.x - (textEndX - textStartX));
      edit.scrollOffsetX = std::min(
          maxScroll, edit.scrollOffsetX + kSelectionAutoScrollSpeed * dt);
    }
    int pos = caretAt(std::clamp(p.pos.x, textStartX, textEndX));
    if (pos != edit.selectionAnchor || edit.selectionStart != -1) {
      edit.selectionStart = edit.selectionAnchor;
      edit.selectionEnd = pos;
    }
    if (pos != edit.cursor) {
      edit.cursor = pos;
      edit.lastBlinkTime = static_cast<float>(GetTime());
    }
    return;
  }

  if (p.released)
    edit.isSelecting = false;
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

  // Enter commits (onChange already fired live) and blurs; Escape blurs.
  if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
      IsKeyPressed(KEY_ESCAPE)) {
    edit.isSelecting = false;
    edit.selectionStart = edit.selectionEnd = -1;
    Blur();
    return;
  }

  if (cmd && IsKeyPressed(KEY_A)) {
    int len = static_cast<int>(std::strlen(buffer));
    edit.selectionStart = 0;
    edit.selectionEnd = len;
    edit.cursor = len;
    edit.lastBlinkTime = static_cast<float>(GetTime());
    return;
  }

  if (cmd && IsKeyPressed(KEY_C)) {
    int s = edit.selectionStart, e = edit.selectionEnd;
    NormalizeSelection(s, e);
    if (s != -1 && e != -1)
      SetClipboardText(std::string(buffer + s, e - s).c_str());
    return;
  }

  if (cmd && IsKeyPressed(KEY_X)) {
    int s = edit.selectionStart, e = edit.selectionEnd;
    NormalizeSelection(s, e);
    if (s != -1 && e != -1) {
      SetClipboardText(std::string(buffer + s, e - s).c_str());
      SaveToHistory(id, buffer);
      DeleteSelection(node, buffer);
      notify();
    }
    return;
  }

  if (cmd && IsKeyPressed(KEY_V)) {
    const char *clip = GetClipboardText();
    if (clip && clip[0]) {
      SaveToHistory(id, buffer);
      int s = edit.selectionStart, e = edit.selectionEnd;
      NormalizeSelection(s, e);
      if (s != -1 && e != -1)
        DeleteSelection(node, buffer);
      int len = static_cast<int>(std::strlen(buffer));
      int clipLen = static_cast<int>(std::strlen(clip));
      int avail = bufferSize - 1 - len;
      int toCopy = std::min(clipLen, avail);
      if (toCopy > 0) {
        edit.cursor = std::clamp(edit.cursor, 0, len);
        if (edit.cursor < len)
          std::memmove(&buffer[edit.cursor + toCopy], &buffer[edit.cursor],
                       static_cast<size_t>(len - edit.cursor + 1));
        std::memcpy(&buffer[edit.cursor], clip, static_cast<size_t>(toCopy));
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
      edit.arrowLeftTimer =
          GetTime() + (IsKeyPressed(KEY_LEFT) ? 0.5 : 0.05);
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
          target--;
        if (shift) {
          if (edit.selectionStart == -1)
            edit.selectionStart = edit.cursor;
          edit.selectionEnd = target;
        } else {
          edit.selectionStart = edit.selectionEnd = -1;
        }
      }
      edit.cursor = target;
      return;
    }
  }

  if (IsKeyDown(KEY_RIGHT)) {
    bool shouldMove = IsKeyPressed(KEY_RIGHT);
    if (!shouldMove && GetTime() > edit.arrowRightTimer)
      shouldMove = true;
    if (shouldMove) {
      edit.arrowRightTimer =
          GetTime() + (IsKeyPressed(KEY_RIGHT) ? 0.5 : 0.05);
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
          target++;
        if (shift) {
          if (edit.selectionStart == -1)
            edit.selectionStart = edit.cursor;
          edit.selectionEnd = target;
        } else {
          edit.selectionStart = edit.selectionEnd = -1;
        }
      }
      edit.cursor = target;
      return;
    }
  }

  if (IsKeyPressed(KEY_HOME)) {
    edit.lastBlinkTime = static_cast<float>(GetTime());
    int target = 0;
    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = target;
    } else {
      edit.selectionStart = edit.selectionEnd = -1;
    }
    edit.cursor = target;
    return;
  }

  if (IsKeyPressed(KEY_END)) {
    edit.lastBlinkTime = static_cast<float>(GetTime());
    int target = static_cast<int>(std::strlen(buffer));
    if (shift) {
      if (edit.selectionStart == -1)
        edit.selectionStart = edit.cursor;
      edit.selectionEnd = target;
    } else {
      edit.selectionStart = edit.selectionEnd = -1;
    }
    edit.cursor = target;
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
        std::memmove(&buffer[0], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
        edit.cursor = 0;
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
        std::memmove(&buffer[edit.cursor - 1], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
        edit.cursor--;
      }
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
        std::memmove(&buffer[edit.cursor], &buffer[edit.cursor + 1],
                     static_cast<size_t>(len - edit.cursor));
      }
      notify();
      return;
    }
  }

  int key = GetCharPressed();
  while (key > 0) {
    int len = static_cast<int>(std::strlen(buffer));
    if (len < bufferSize - 1 && key >= 32 && key <= 126) {
      if (hasSelection) {
        SaveToHistory(id, buffer);
        DeleteSelection(node, buffer);
        len = static_cast<int>(std::strlen(buffer));
      } else {
        SaveToHistory(id, buffer);
      }
      edit.cursor = std::clamp(edit.cursor, 0, len);
      if (edit.cursor < len)
        std::memmove(&buffer[edit.cursor + 1], &buffer[edit.cursor],
                     static_cast<size_t>(len - edit.cursor + 1));
      buffer[edit.cursor] = static_cast<char>(key);
      edit.cursor++;
      edit.lastBlinkTime = static_cast<float>(GetTime());
      notify();
    }
    key = GetCharPressed();
  }
}


} // namespace

void ResyncTextInputBuffer(NodeId nodeId, int cursorPos) {
  Ctx().textInputUndo.erase(nodeId);
}

void SetTextInputCursorCallback(std::function<void(NodeId, int)> cb) {
  s_cursorCallback = std::move(cb);
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
    NodePtr next =
        focused != 0 ? FindNodeById(root, focused) : nullptr;

    const bool switchingField =
        prev && prev->kind == NodeKind::TextInput && next &&
        next->kind == NodeKind::TextInput && IdOf(prev) != IdOf(next);

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
      int prevCursor = focusedNode->textEdit.cursor;
      HandlePointer(*focusedNode, p);
      NotifyCursorMoved(*focusedNode, prevCursor);
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
    edit.labelAnim +=
        (target - edit.labelAnim) * std::min(1.0f, dtl * 16.0f);
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
      DrawRectangleRec(
          {inputBounds.x, inputBounds.y + inputBounds.height - outlineWidth,
           inputBounds.width, outlineWidth},
          outlineColor);
    }
  }

  float textStartX = inputBounds.x + kBasePadding * 2.0f;
  float textEndX = inputBounds.x + inputBounds.width - kBasePadding * 2.0f;
  if (isFocused)
    SyncScrollForCaret(node, buffer, textStartX, textEndX, ti.passwordMode);
  else {
    std::string display = DisplayText(buffer, ti.passwordMode);
    Vector2 totalSize = raym3::Renderer::MeasureText(
        display.c_str(), kFieldFontSize, FontWeight::Regular);
    float availableWidth = textEndX - textStartX;
    edit.scrollOffsetX =
        totalSize.x > availableWidth ? totalSize.x - availableWidth : 0.0f;
  }

  float currentScroll = edit.scrollOffsetX;
  int scissorW = static_cast<int>(std::ceil(textEndX - textStartX));
  int scissorH = static_cast<int>(std::ceil(inputBounds.height));
  bool scissorActive = false;
  if (scissorW > 0 && scissorH > 0) {
    raym3::PushScissor({textStartX, inputBounds.y, static_cast<float>(scissorW),
                        static_cast<float>(scissorH)});
    scissorActive = true;
  }

  if (isFocused) {
    std::string displaySel = DisplayText(buffer, ti.passwordMode);
    DrawSelection(inputBounds, displaySel.c_str(), edit.selectionStart,
                  edit.selectionEnd, currentScroll,
                  textStartX - inputBounds.x);
  }

  bool isEmpty = buffer[0] == '\0';
  bool showPlaceholder = isEmpty && !ti.placeholder.empty() && !label;
  Vector2 textPos = {textStartX - currentScroll,
                     inputBounds.y + (inputBounds.height - kFieldFontSize) / 2.0f};
  Color textColor = style.text.color.value_or(scheme.onSurface);

  if (showPlaceholder) {
    Color ph = scheme.onSurfaceVariant;
    ph.a = 180;
    raym3::Renderer::DrawText(ti.placeholder.c_str(), textPos, kFieldFontSize,
                              ph, FontWeight::Regular);
  } else if (!isEmpty) {
    std::string masked = DisplayText(buffer, ti.passwordMode);
    raym3::Renderer::DrawText(masked.c_str(), textPos, kFieldFontSize,
                              textColor, FontWeight::Regular);
  }

  if (isFocused && !ti.readOnly) {
    DrawCursor(inputBounds, buffer, edit.cursor, currentScroll,
               edit.lastBlinkTime, textStartX - inputBounds.x, bgColor,
               ti.passwordMode);
  }

  if (scissorActive)
    raym3::PopScissor();
}

} // namespace raym3::v2
