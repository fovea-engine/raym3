#pragma once

#include "raym3/v2/View.h"
#include <functional>
#include <raylib.h>
#include <string>

namespace raym3::v2 {

// A scroll bar the pointer can drag, for content the app scrolls itself (a virtualized grid, a
// custom list). The thumb's length shows how much of the content is visible and its place shows
// where. Dragging the thumb keeps the grab point under the pointer; pressing the track elsewhere
// jumps the thumb's centre there and keeps dragging from that point.
struct ScrollbarProps {
  std::string id;              // required: the drag in progress is kept per id across frames
  Style style;                 // the track's size and placement; the thumb fills its cross axis
  float viewportSize = 0.0f;   // visible length of the content
  float contentSize = 0.0f;    // full length of the content
  float offset = 0.0f;         // current scroll offset, 0 .. contentSize - viewportSize
  float minThumbSize = 24.0f;
  bool horizontal = false;
  Color trackColor{0, 0, 0, 0};
  Color thumbColor{128, 128, 128, 255};
  Color thumbActiveColor{0, 0, 0, 0};  // while dragged; alpha 0 means use thumbColor
  std::function<void(float)> onScroll; // the new offset, already clamped
};

NodePtr Scrollbar(const ScrollbarProps &props);

// True while the scroll bar with this id is being dragged.
bool ScrollbarDragging(const std::string &id);

} // namespace raym3::v2
