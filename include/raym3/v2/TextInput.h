#pragma once

#include "raym3/v2/Input.h"
#include "raym3/v2/View.h"

#include <functional>
#include <string>

namespace raym3::v2 {

struct TextInputHostHooks {
  std::function<std::string()> getClipboardText;
  std::function<void(const std::string &)> setClipboardText;
  std::function<void()> hapticFeedback;
};

struct TextInputEditingState {
  std::string text;
  int selectionStart = -1;
  int selectionEnd = -1;
  int composingStart = -1;
  int composingEnd = -1;
  bool textChanged = false;
};

enum class TextInputDraggedEdge { Start, End };

struct TextInputDragSelectionUpdate {
  int selectionStart = -1;
  int selectionEnd = -1;
  int cursor = 0;
  bool applied = false;
};

void PaintTextInput(Node &node);
void ResyncTextInputBuffer(NodeId nodeId, int cursorPos);
void SetTextInputHostHooks(TextInputHostHooks hooks);
TextInputHostHooks &GetTextInputHostHooks();

// Host hook: fired whenever the focused text field changes editing state.
void SetTextInputStateCallback(
    std::function<void(NodeId, const TextInputEditingState &)> cb);

Rectangle TextInputInputBounds(Node &node);
int TextInputTextLength(Node &node);
int TextInputHitTestCaret(Node &node, float screenX);
int TextInputHitTestCaret(Node &node, Vector2 screenPos);
float TextInputByteOffsetX(Node &node, int byteOffset);
float TextInputByteOffsetY(Node &node, int byteOffset);
float TextInputLineCenterY(Node &node, int byteOffset);
float TextInputPreferredLineHeight(Node &node);
TextInputDragSelectionUpdate
TextInputResolveDraggedSelection(TextInputDraggedEdge draggedEdge,
                                 int fixedAnchor, int currentOffset,
                                 int textLength);
void TextInputSetSelection(Node &node, int start, int end, int cursor);
void TextInputSelectAll(Node &node);
bool TextInputCopy(Node &node);
bool TextInputCut(Node &node);
bool TextInputPaste(Node &node);
bool TextInputReplaceSelection(Node &node, const std::string &text,
                               int composingStart = -1, int composingEnd = -1);
void TextInputSubmitEditing(Node &node);
void TextInputSetEditingState(Node &node, const std::string &text,
                              int selectionStart, int selectionEnd,
                              int composingStart, int composingEnd,
                              bool notifyTextChanged);
void TextInputNotifyEditingState(Node &node, bool textChanged);

} // namespace raym3::v2
