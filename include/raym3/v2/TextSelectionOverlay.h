#pragma once

#include "raym3/v2/View.h"

namespace raym3::v2 {

void PaintTextSelectionOverlay(const NodePtr &root);
bool HandleTextSelectionOverlayInput(const NodePtr &root);

} // namespace raym3::v2
