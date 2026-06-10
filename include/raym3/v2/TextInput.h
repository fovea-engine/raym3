#pragma once

#include "raym3/v2/Input.h"
#include "raym3/v2/View.h"

#include <functional>

namespace raym3::v2 {

void PaintTextInput(Node &node);
void ResyncTextInputBuffer(NodeId nodeId, int cursorPos);

// Host hook: fired when a pointer gesture moves the caret on the focused
// field (used on Android to keep the IME InputConnection selection in sync).
void SetTextInputCursorCallback(std::function<void(NodeId, int)> cb);

} // namespace raym3::v2
