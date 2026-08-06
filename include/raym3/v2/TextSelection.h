#pragma once

#include "raym3/v2/Input.h"
#include "raym3/v2/View.h"

#include <functional>
#include <vector>

// Read-only text selection for plain Text nodes (react-native `selectable`).
//
// Reuses the per-node TextEditState that already exists on every Node and the
// Flutter-style handle/toolbar overlay (TextSelectionOverlay.cpp). Unlike
// TextInput's editing engine — which splits lines on '\n' only and uses a
// fixed field font — these functions derive geometry from the node's real
// PreparedText/LayoutText pipeline, so wrapped and styled text hit-tests
// correctly. All coordinates are dp (layout space), matching node.layout.
//
// Usable standalone from raym3: set `node->textSelectable = true` on a Text
// node and selection (long-press / mouse drag / Cmd-C) works without any
// host integration beyond the clipboard hooks TextInput already uses.

namespace raym3::v2 {

// Byte offsets of the current selection change, fired on every selection
// mutation (set/select-all/clear). Single process-global callback — mirrors
// SetTextInputStateCallback; zero per-node cost.
void SetTextSelectionChangeCallback(
    std::function<void(NodeId, int selectionStart, int selectionEnd)> cb);

// Renderer.cpp exposes its prepared-text cache for selection geometry.
const PreparedText &GetPreparedTextForNode(const Node &node);

// True when the node is a Text node with selection enabled.
bool NodeIsSelectableText(const Node &node);

// Geometry (dp). byteOffset is a byte index into node.text.
Rectangle TextNodeBounds(const Node &node);
int TextNodeTextLength(const Node &node);
int TextNodeHitTestCaret(const Node &node, Vector2 posDp);
float TextNodeByteOffsetX(const Node &node, int byteOffset);
float TextNodeByteOffsetY(const Node &node, int byteOffset); // line bottom
float TextNodeLineCenterY(const Node &node, int byteOffset);
float TextNodePreferredLineHeight(const Node &node);
// One rectangle per (partially) selected laid-out line.
std::vector<Rectangle> TextNodeSelectionRects(const Node &node, int start,
                                              int end);

// Word boundaries around a byte position (UTF-8 aware).
void TextNodeWordBoundaries(const Node &node, int byteOffset, int &start,
                            int &end);

// Selection operations. start/end are clamped to UTF-8 boundaries; -1/-1
// clears. All fire the selection-change callback when the range changes.
void TextNodeSetSelection(Node &node, int start, int end);
void TextNodeSelectAll(Node &node);
void TextNodeClearSelection(Node &node);
// Copy the selected slice of node.text via the TextInput clipboard host hooks.
void TextNodeCopy(Node &node);

} // namespace raym3::v2
