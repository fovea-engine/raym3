#pragma once

#include "raym3/v2/View.h"

// First-class interactive control painting (Slider/RangeSlider/Switch/
// Checkbox/RadioButton). Migrated out of rayact's bridge so the visuals live
// in raym3 core and pure C++ apps render real controls. Interaction is driven
// by Input.cpp (ResolveInput); these functions only paint + tick animation.
namespace raym3::v2 {

bool IsControlKind(NodeKind kind);

// Advance a control's toggle animation toward its target (checked) state.
void TickControlAnimation(Node &node, float dtMs);

// Paint the control at node->layout. `active` = activeId capture (pressed),
// `hovered` = pointer over.
void PaintControl(const Node &node, bool active, bool hovered);

} // namespace raym3::v2
