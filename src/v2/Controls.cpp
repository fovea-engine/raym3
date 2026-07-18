#include "raym3/v2/Controls.h"

#include "raym3/styles/Theme.h"
#include "raym3/v2/IconRenderer.h"
#include "raym3/v2/Renderer.h"
#include "raym3/v2/MaterialTokens.h"
#include <algorithm>
#include <cmath>

namespace raym3::v2 {

bool IsControlKind(NodeKind kind) {
  switch (kind) {
  case NodeKind::Slider:
  case NodeKind::RangeSlider:
  case NodeKind::Switch:
  case NodeKind::Checkbox:
  case NodeKind::RadioButton:
    return true;
  default:
    return false;
  }
}

namespace {

Color LerpColor(Color a, Color b, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  auto ch = [t](unsigned char from, unsigned char to) {
    return (unsigned char)std::round((float)from + ((float)to - (float)from) * t);
  };
  return {ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a)};
}

Rectangle CenteredRect(Rectangle outer, float w, float h) {
  return {outer.x + (outer.width - w) * 0.5f, outer.y + (outer.height - h) * 0.5f,
          w, h};
}

float RoundedRectRoundness(float w, float h, float radius) {
  float minDim = std::min(w, h);
  return minDim > 0.0f ? std::clamp((2.0f * radius) / minDim, 0.0f, 1.0f) : 0.0f;
}

void DrawSliderTrackSegment(Rectangle r, float leftRadius, float rightRadius,
                            Color color) {
  if (r.width <= 0.5f || r.height <= 0.5f)
    return;
  leftRadius = std::clamp(leftRadius, 0.0f, r.height * 0.5f);
  rightRadius = std::clamp(rightRadius, 0.0f, r.height * 0.5f);
  float left = r.x, right = r.x + r.width, top = r.y, bottom = r.y + r.height;
  DrawRectangleRec({left + leftRadius, top,
                    std::max(0.0f, r.width - leftRadius - rightRadius), r.height},
                   color);
  DrawRectangleRec({left, top + leftRadius, leftRadius,
                    std::max(0.0f, r.height - leftRadius * 2.0f)},
                   color);
  DrawRectangleRec({right - rightRadius, top + rightRadius, rightRadius,
                    std::max(0.0f, r.height - rightRadius * 2.0f)},
                   color);
  if (leftRadius > 0.0f) {
    DrawCircleSector({left + leftRadius, top + leftRadius}, leftRadius, 180.0f,
                     270.0f, 12, color);
    DrawCircleSector({left + leftRadius, bottom - leftRadius}, leftRadius, 90.0f,
                     180.0f, 12, color);
  }
  if (rightRadius > 0.0f) {
    DrawCircleSector({right - rightRadius, top + rightRadius}, rightRadius, 270.0f,
                     360.0f, 12, color);
    DrawCircleSector({right - rightRadius, bottom - rightRadius}, rightRadius, 0.0f,
                     90.0f, 12, color);
  }
}

void DrawCheckmark(Rectangle r, Color color) {
  Vector2 c = {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
  float s = std::min(r.width, r.height) * 0.5f;
  Vector2 p1 = {c.x - s * 0.5f, c.y};
  Vector2 p2 = {c.x - s * 0.1f, c.y + s * 0.45f};
  Vector2 p3 = {c.x + s * 0.55f, c.y - s * 0.45f};
  DrawLineEx(p1, p2, 2.0f, color);
  DrawLineEx(p2, p3, 2.0f, color);
}

void PaintCheckbox(const Node &node, float progress, bool pressed) {
  const auto &scheme = Theme::GetColorScheme();
  Rectangle layout = node.layout;
  Rectangle visual = CenteredRect(layout, tokens::kCheckboxVisualSize,
                                  tokens::kCheckboxVisualSize);
  float t = std::clamp(progress, 0.0f, 1.0f);
  bool selected = t >= 0.5f;
  bool disabled = node.disabled;
  float contentOpacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  float opacity = CurrentRenderOpacity() * contentOpacity;

  Color border = selected ? Color{0, 0, 0, 0} : scheme.onSurfaceVariant;
  Color fill = scheme.primary;
  Color mark = scheme.onPrimary;
  if (disabled) {
    border = selected ? Color{0, 0, 0, 0} : scheme.onSurface;
    fill = scheme.onSurface;
    mark = scheme.surface;
  } else if (pressed && !selected) {
    border = scheme.onSurface;
  }

  if (pressed && !disabled) {
    Vector2 center = {layout.x + layout.width * 0.5f,
                      layout.y + layout.height * 0.5f};
    DrawCircleV(center, tokens::kStateLayerSize * 0.5f,
                ColorAlpha(selected ? scheme.onSurface : scheme.primary,
                           tokens::kPressedStateOpacity));
  }

  float fillAlpha = selected ? opacity : opacity * t;
  if (fillAlpha > 0.0f) {
    float roundness = RoundedRectRoundness(visual.width, visual.height, 2.0f);
    DrawRectangleRounded(visual, roundness, 8, ColorAlpha(fill, fillAlpha));
  }
  if (!selected) {
    float roundness = RoundedRectRoundness(visual.width, visual.height, 2.0f);
    DrawRectangleRoundedLinesEx(visual, roundness, 8, 2.0f,
                                ColorAlpha(border, opacity));
  }
  if (t > 0.0f)
    DrawCheckmark(visual, ColorAlpha(mark, opacity * t));
}

void PaintRadio(const Node &node, float progress, bool pressed) {
  const auto &scheme = Theme::GetColorScheme();
  Rectangle layout = node.layout;
  Rectangle visual = CenteredRect(layout, tokens::kRadioVisualSize,
                                  tokens::kRadioVisualSize);
  float t = std::clamp(progress, 0.0f, 1.0f);
  bool selected = t >= 0.5f;
  bool disabled = node.disabled;
  float contentOpacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  float opacity = CurrentRenderOpacity() * contentOpacity;
  Vector2 center = {visual.x + visual.width * 0.5f, visual.y + visual.height * 0.5f};
  Color color = selected ? scheme.primary : scheme.onSurfaceVariant;
  if (disabled || (pressed && !selected))
    color = scheme.onSurface;

  if (pressed && !disabled) {
    Vector2 stateCenter = {layout.x + layout.width * 0.5f,
                           layout.y + layout.height * 0.5f};
    DrawCircleV(stateCenter, tokens::kStateLayerSize * 0.5f,
                ColorAlpha(selected ? scheme.onSurface : scheme.primary,
                           tokens::kPressedStateOpacity));
  }

  float outer = tokens::kRadioVisualSize * 0.5f;
  DrawRing(center, outer - 2.0f, outer, 0.0f, 360.0f, 32,
           ColorAlpha(color, opacity));
  if (t > 0.0f)
    DrawCircleV(center, 4.5f * t, ColorAlpha(scheme.primary, opacity));
}

void PaintSwitch(const Node &node, float progress, bool pressed) {
  const auto &scheme = Theme::GetColorScheme();
  Rectangle layout = node.layout;
  Rectangle track = CenteredRect(layout, tokens::kSwitchTrackWidth,
                                 tokens::kSwitchTrackHeight);
  float t = std::clamp(progress, 0.0f, 1.0f);
  bool selected = t >= 0.5f;
  bool disabled = node.disabled;
  float contentOpacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  float opacity = CurrentRenderOpacity() * contentOpacity;

  Color offTrack = scheme.surfaceContainerHighest;
  Color onTrack = scheme.primary;
  Color offThumb = scheme.outline;
  Color onThumb = scheme.onPrimary;
  Color trackColor = LerpColor(offTrack, onTrack, t);
  Color thumbColor = LerpColor(offThumb, onThumb, t);
  Color iconColor =
      selected ? scheme.onPrimaryContainer : scheme.surfaceContainerHighest;
  if (disabled) {
    trackColor = ColorAlpha(scheme.onSurface, tokens::kDisabledContainerOpacity);
    thumbColor = selected ? scheme.surface : scheme.onSurface;
    iconColor = scheme.onSurface;
  }

  DrawRectangleRounded(track, 1.0f, 16, ColorAlpha(trackColor, opacity));
  if (t < 0.5f && !disabled) {
    DrawRectangleRoundedLinesEx(
        {track.x + 1.0f, track.y + 1.0f, track.width - 2.0f, track.height - 2.0f},
        1.0f, 16, 2.0f, ColorAlpha(scheme.outline, opacity));
  }

  float thumbSize = tokens::kSwitchInactiveThumbSize +
                    (tokens::kSwitchActiveThumbSize -
                     tokens::kSwitchInactiveThumbSize) *
                        t;
  if (pressed)
    thumbSize = tokens::kSwitchPressedThumbSize;
  float cy = track.y + track.height * 0.5f;
  float offX = track.x + track.height * 0.5f;
  float onX = track.x + track.width - track.height * 0.5f;
  float cx = offX + (onX - offX) * t;
  Rectangle thumb = {cx - thumbSize * 0.5f, cy - thumbSize * 0.5f, thumbSize,
                     thumbSize};

  if (pressed && !disabled) {
    DrawCircleV({cx, cy}, tokens::kStateLayerSize * 0.5f,
                ColorAlpha(selected ? scheme.primary : scheme.onSurface,
                           opacity * tokens::kPressedStateOpacity));
  }

  DrawRectangleRounded(thumb, 1.0f, 16, ColorAlpha(thumbColor, opacity));
  if (thumbSize >= tokens::kSwitchActiveThumbSize - 0.1f) {
    DrawMaterialIcon(selected ? 0xe5ca : 0xe5cd, thumb,
                     ColorAlpha(iconColor, contentOpacity), (int)tokens::kSwitchIconSize,
                     true);
  }
}

void PaintSlider(const Node &node, bool hovered) {
  const auto &scheme = Theme::GetColorScheme();
  const ControlState &st = node.control;
  Rectangle layout = node.layout;
  float opacity = CurrentRenderOpacity() *
      (node.disabled ? tokens::kDisabledContentOpacity : 1.0f);
  float trackX = layout.x;
  float trackW = layout.width;
  float span = st.maxValue - st.minValue;
  float p = span > 0.0f
                ? std::clamp((st.value - st.minValue) / span, 0.0f, 1.0f)
                : 0.0f;
  float trackH = st.sliderTrackH;
  float handleH = st.sliderHandleH;
  float handleW = tokens::kSliderHandleWidth;
  float cy = layout.y + layout.height * 0.5f;
  float thumbX = std::clamp(trackX + p * trackW, trackX, trackX + trackW);
  float handleGap = tokens::kSliderTrackGap;
  float activeEnd = std::max(trackX, thumbX - handleW * 0.5f - handleGap);
  float inactiveStart =
      std::min(trackX + trackW, thumbX + handleW * 0.5f + handleGap);
  float innerRadius = std::min(2.0f, trackH * 0.05f);
  if (activeEnd > trackX)
    DrawSliderTrackSegment({trackX, cy - trackH * 0.5f, activeEnd - trackX, trackH},
                           trackH * 0.5f, innerRadius,
                           ColorAlpha(scheme.primary, opacity));
  if (inactiveStart < trackX + trackW)
    DrawSliderTrackSegment({inactiveStart, cy - trackH * 0.5f,
                            trackX + trackW - inactiveStart, trackH},
                           innerRadius, trackH * 0.5f,
                           ColorAlpha(scheme.secondaryContainer, opacity));
  if (!node.disabled && (hovered || st.dragging))
    DrawCircle((int)thumbX, (int)cy, 20.0f, ColorAlpha(scheme.primary, 0.12f));
  DrawRectangleRounded(
      {thumbX - handleW * 0.5f, cy - handleH * 0.5f, handleW, handleH},
      handleW * 0.5f, 8, ColorAlpha(scheme.primary, opacity));
}

void PaintRangeSlider(const Node &node, bool hovered) {
  const auto &scheme = Theme::GetColorScheme();
  const ControlState &st = node.control;
  Rectangle layout = node.layout;
  float opacity = CurrentRenderOpacity() *
      (node.disabled ? tokens::kDisabledContentOpacity : 1.0f);
  float trackX = layout.x;
  float trackW = layout.width;
  float start = std::clamp(st.startValue, 0.0f, 1.0f);
  float end = std::clamp(st.endValue, 0.0f, 1.0f);
  if (start > end)
    std::swap(start, end);
  float trackH = st.sliderTrackH;
  float handleH = st.sliderHandleH;
  float handleW = tokens::kSliderHandleWidth;
  float handleGap = tokens::kSliderTrackGap;
  float cy = layout.y + layout.height * 0.5f;
  float leftX = trackX + start * trackW;
  float rightX = trackX + end * trackW;
  float leftInactiveEnd = std::max(trackX, leftX - handleW * 0.5f - handleGap);
  float activeStart = std::min(trackX + trackW, leftX + handleW * 0.5f + handleGap);
  float activeEnd = std::max(trackX, rightX - handleW * 0.5f - handleGap);
  float rightInactiveStart =
      std::min(trackX + trackW, rightX + handleW * 0.5f + handleGap);
  float innerRadius = std::min(2.0f, trackH * 0.05f);

  if (leftInactiveEnd > trackX)
    DrawSliderTrackSegment({trackX, cy - trackH * 0.5f, leftInactiveEnd - trackX,
                            trackH},
                           trackH * 0.5f, innerRadius,
                           ColorAlpha(scheme.secondaryContainer, opacity));
  if (activeEnd > activeStart)
    DrawSliderTrackSegment({activeStart, cy - trackH * 0.5f,
                            activeEnd - activeStart, trackH},
                           innerRadius, innerRadius,
                           ColorAlpha(scheme.primary, opacity));
  if (rightInactiveStart < trackX + trackW)
    DrawSliderTrackSegment({rightInactiveStart, cy - trackH * 0.5f,
                            trackX + trackW - rightInactiveStart, trackH},
                           innerRadius, trackH * 0.5f,
                           ColorAlpha(scheme.secondaryContainer, opacity));

  Color handleColor = ColorAlpha(scheme.primary, opacity);
  DrawRectangleRounded(
      {leftX - handleW * 0.5f, cy - handleH * 0.5f, handleW, handleH},
      handleW * 0.5f, 8, handleColor);
  DrawRectangleRounded(
      {rightX - handleW * 0.5f, cy - handleH * 0.5f, handleW, handleH},
      handleW * 0.5f, 8, handleColor);
  (void)hovered;
}

} // namespace

void TickControlAnimation(Node &node, float dtMs) {
  if (node.kind != NodeKind::Switch && node.kind != NodeKind::Checkbox &&
      node.kind != NodeKind::RadioButton)
    return;
  ControlState &st = node.control;
  float target = st.checked ? 1.0f : 0.0f;
  if (st.anim < 0.0f) {
    st.anim = target; // initialize without animating on first frame
    return;
  }
  // ~80ms exponential ease toward target.
  st.anim += (target - st.anim) * std::clamp(dtMs / 80.0f, 0.0f, 1.0f);
}

void PaintControl(const Node &node, bool active, bool hovered) {
  switch (node.kind) {
  case NodeKind::Slider:
    PaintSlider(node, hovered);
    break;
  case NodeKind::RangeSlider:
    PaintRangeSlider(node, hovered);
    break;
  case NodeKind::Switch:
    PaintSwitch(node, node.control.anim < 0.0f ? (node.control.checked ? 1.0f : 0.0f)
                                               : node.control.anim,
                active);
    break;
  case NodeKind::Checkbox:
    PaintCheckbox(node,
                  node.control.anim < 0.0f ? (node.control.checked ? 1.0f : 0.0f)
                                           : node.control.anim,
                  active);
    break;
  case NodeKind::RadioButton:
    PaintRadio(node,
               node.control.anim < 0.0f ? (node.control.checked ? 1.0f : 0.0f)
                                        : node.control.anim,
               active);
    break;
  default:
    break;
  }
}

} // namespace raym3::v2
