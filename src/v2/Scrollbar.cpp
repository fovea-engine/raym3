#include "raym3/v2/Scrollbar.h"

#include <algorithm>
#include <unordered_map>

namespace raym3::v2 {

namespace {

// The tree is rebuilt every frame and reconciled into a retained one, so the node a callback sees is not the one
// that was laid out. What a drag needs, and where the bar was last painted, live here by id.
struct DragState {
  Rectangle rect{};        // where the bar was last painted
  bool dragging = false;
  float startAlong = 0.0f; // press position along the track, from its start
  float grab = 0.0f;       // press position within the thumb
};

std::unordered_map<std::string, DragState> &States() {
  static std::unordered_map<std::string, DragState> states;
  return states;
}

struct Geometry {
  float length = 0.0f, thumb = 0.0f, travel = 0.0f, thumbStart = 0.0f, maxOffset = 0.0f;
};

Geometry Measure(const ScrollbarProps &p, Rectangle r) {
  Geometry g;
  g.length = p.horizontal ? r.width : r.height;
  g.maxOffset = std::max(0.0f, p.contentSize - p.viewportSize);
  const float ratio = p.contentSize > 0.0f ? std::clamp(p.viewportSize / p.contentSize, 0.0f, 1.0f) : 1.0f;
  g.thumb = std::clamp(g.length * ratio, std::min(p.minThumbSize, g.length), g.length);
  g.travel = std::max(0.0f, g.length - g.thumb);
  g.thumbStart = g.maxOffset > 0.0f ? g.travel * std::clamp(p.offset / g.maxOffset, 0.0f, 1.0f) : 0.0f;
  return g;
}

float Along(const ScrollbarProps &p, Rectangle r, Vector2 pt) { return p.horizontal ? pt.x - r.x : pt.y - r.y; }

} // namespace

bool ScrollbarDragging(const std::string &id) {
  const auto it = States().find(id);
  return it != States().end() && it->second.dragging;
}

NodePtr Scrollbar(const ScrollbarProps &props) {
  ViewProps vp;
  vp.id = props.id;
  vp.style = props.style;
  vp.capturesInput = true; // a press here drags the bar, not a scroll view around it
  const ScrollbarProps p = props;
  NodePtr node = Custom(vp, [p](Rectangle r) {
    States()[p.id].rect = r;
    const Geometry g = Measure(p, r);
    const float radius = 0.5f;
    if (p.trackColor.a) DrawRectangleRounded(r, radius, 8, p.trackColor);
    const bool active = ScrollbarDragging(p.id) && p.thumbActiveColor.a;
    const Rectangle thumb = p.horizontal ? Rectangle{r.x + g.thumbStart, r.y, g.thumb, r.height}
                                         : Rectangle{r.x, r.y + g.thumbStart, r.width, g.thumb};
    DrawRectangleRounded(thumb, radius, 8, active ? p.thumbActiveColor : p.thumbColor);
  });
  auto scrollTo = [p](const Geometry &g, float thumbStart) {
    if (!p.onScroll || g.travel <= 0.0f) return;
    p.onScroll(g.maxOffset * std::clamp(thumbStart, 0.0f, g.travel) / g.travel);
  };
  node->onDragStart = [p, scrollTo](Vector2 pt) {
    DragState &s = States()[p.id];
    const Geometry g = Measure(p, s.rect);
    s.dragging = true;
    s.startAlong = Along(p, s.rect, pt);
    const bool onThumb = s.startAlong >= g.thumbStart && s.startAlong <= g.thumbStart + g.thumb;
    s.grab = onThumb ? s.startAlong - g.thumbStart : g.thumb * 0.5f;
    if (!onThumb) scrollTo(g, s.startAlong - s.grab);
  };
  node->onDragMove = [p, scrollTo](Vector2 delta) {
    const auto it = States().find(p.id);
    if (it == States().end() || !it->second.dragging) return;
    const DragState &s = it->second;
    scrollTo(Measure(p, s.rect), s.startAlong + (p.horizontal ? delta.x : delta.y) - s.grab);
  };
  node->onDragEnd = [id = p.id](Vector2) { States()[id].dragging = false; };
  return node;
}

} // namespace raym3::v2
