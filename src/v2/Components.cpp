#include "raym3/v2/Components.h"

#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/IconRenderer.h"
#include "raym3/v2/MaterialDefaults.h"
#include "raym3/v2/MaterialTokens.h"
#include <algorithm>
#include <cmath>
#include <raylib.h>
#include <utility>

namespace raym3::v2 {

// --- Structural custom-render painters (M3 anatomy) -----------------------
// Components whose visuals are more than a styled box (track + handle, animated
// arcs, moving indeterminate segments) paint themselves here so they match the
// M3 spec rather than collapsing to a flat rectangle.

static void DrawSliderTrackSegment(Rectangle r, float leftRadius,
                                   float rightRadius, Color color) {
  if (r.width <= 0.5f || r.height <= 0.5f)
    return;
  leftRadius = std::clamp(leftRadius, 0.0f, r.height * 0.5f);
  rightRadius = std::clamp(rightRadius, 0.0f, r.height * 0.5f);
  float left = r.x;
  float right = r.x + r.width;
  float top = r.y;
  float bottom = r.y + r.height;
  DrawRectangleRec({left + leftRadius, top,
                    std::max(0.0f, r.width - leftRadius - rightRadius),
                    r.height},
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
    DrawCircleSector({left + leftRadius, bottom - leftRadius}, leftRadius,
                     90.0f, 180.0f, 12, color);
  }
  if (rightRadius > 0.0f) {
    DrawCircleSector({right - rightRadius, top + rightRadius}, rightRadius,
                     270.0f, 360.0f, 12, color);
    DrawCircleSector({right - rightRadius, bottom - rightRadius}, rightRadius,
                     0.0f, 90.0f, 12, color);
  }
}

static void PaintSlider(Rectangle r, float progress, bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  MaterialComponentMetrics metrics = GetMaterialMetrics(M3Component::Slider);
  float p = std::clamp(progress < 0.0f ? 0.5f : progress, 0.0f, 1.0f);
  float trackH = tokens::kSliderTrackHeight;
  float layoutH = std::max(r.height, metrics.touchHeight);
  float cy = r.y + r.height * 0.5f;
  float opacity = disabled ? 0.38f : 1.0f;
  float thumbX = std::clamp(r.x + p * r.width, r.x, r.x + r.width);
  float handleW = tokens::kSliderHandleWidth;
  float handleH = tokens::kSliderHandleHeight;
  float handleGap = tokens::kSliderTrackGap;
  float activeEnd = std::max(r.x, thumbX - handleW * 0.5f - handleGap);
  float inactiveStart =
      std::min(r.x + r.width, thumbX + handleW * 0.5f + handleGap);
  float innerRadius = 2.0f;
  // XS slider: tracks are split by transparent space around the handle.
  if (activeEnd > r.x)
    DrawSliderTrackSegment({r.x, cy - trackH * 0.5f, activeEnd - r.x, trackH},
                           trackH * 0.5f, innerRadius,
                           ColorAlpha(c.primary, opacity));
  if (inactiveStart < r.x + r.width)
    DrawSliderTrackSegment(
        {inactiveStart, cy - trackH * 0.5f, r.x + r.width - inactiveStart,
         trackH},
        innerRadius, trackH * 0.5f, ColorAlpha(c.secondaryContainer, opacity));
  raym3::Renderer::DrawRoundedRectangle(
      {thumbX - handleW * 0.5f, cy - std::min(handleH, layoutH) * 0.5f,
       handleW, std::min(handleH, layoutH)},
      handleW * 0.5f, ColorAlpha(c.primary, opacity));
}

static void PaintRangeSlider(Rectangle r, float startProgress, float endProgress,
                             bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  MaterialComponentMetrics metrics = GetMaterialMetrics(M3Component::RangeSlider);
  float start = startProgress < 0.0f ? 0.25f : startProgress;
  float end = endProgress < 0.0f ? 0.75f : endProgress;
  start = std::clamp(start, 0.0f, 1.0f);
  end = std::clamp(end, 0.0f, 1.0f);
  if (start > end)
    std::swap(start, end);

  float trackH = tokens::kSliderTrackHeight;
  float layoutH = std::max(r.height, metrics.touchHeight);
  float cy = r.y + r.height * 0.5f;
  float opacity = disabled ? 0.38f : 1.0f;
  float handleW = tokens::kSliderHandleWidth;
  float handleH = std::min(tokens::kSliderHandleHeight, layoutH);
  float handleGap = tokens::kSliderTrackGap;
  float leftThumbX = std::clamp(r.x + start * r.width, r.x, r.x + r.width);
  float rightThumbX = std::clamp(r.x + end * r.width, r.x, r.x + r.width);
  // Gaps (masks) on BOTH sides of each thumb, like the standard slider.
  float leftInactiveEnd =
      std::max(r.x, leftThumbX - handleW * 0.5f - handleGap);
  float activeStart =
      std::min(r.x + r.width, leftThumbX + handleW * 0.5f + handleGap);
  float activeEnd = std::max(r.x, rightThumbX - handleW * 0.5f - handleGap);
  float rightInactiveStart =
      std::min(r.x + r.width, rightThumbX + handleW * 0.5f + handleGap);
  float innerRadius = 2.0f;

  // Left inactive segment (gap on left side of left thumb).
  if (leftInactiveEnd > r.x) {
    DrawSliderTrackSegment(
        {r.x, cy - trackH * 0.5f, leftInactiveEnd - r.x, trackH},
        trackH * 0.5f, innerRadius, ColorAlpha(c.secondaryContainer, opacity));
  }
  // Active segment between thumbs.
  if (activeEnd > activeStart) {
    DrawSliderTrackSegment(
        {activeStart, cy - trackH * 0.5f, activeEnd - activeStart, trackH},
        innerRadius, innerRadius, ColorAlpha(c.primary, opacity));
  }
  // Right inactive segment (gap on right side of right thumb).
  if (rightInactiveStart < r.x + r.width) {
    DrawSliderTrackSegment({rightInactiveStart, cy - trackH * 0.5f,
                            r.x + r.width - rightInactiveStart, trackH},
                           innerRadius, trackH * 0.5f,
                           ColorAlpha(c.secondaryContainer, opacity));
  }
  Color handleColor = ColorAlpha(c.primary, opacity);
  raym3::Renderer::DrawRoundedRectangle(
      {leftThumbX - handleW * 0.5f, cy - handleH * 0.5f, handleW, handleH},
      handleW * 0.5f, handleColor);
  raym3::Renderer::DrawRoundedRectangle(
      {rightThumbX - handleW * 0.5f, cy - handleH * 0.5f, handleW, handleH},
      handleW * 0.5f, handleColor);
}

static void DrawWavyLine(Vector2 start, Vector2 end, float thickness,
                         float amplitude, float wavelength, float phase,
                         Color color) {
  float dx = end.x - start.x;
  float dy = end.y - start.y;
  float len = sqrtf(dx * dx + dy * dy);
  if (len <= 0.5f)
    return;
  float nx = -dy / len;
  float ny = dx / len;
  int steps = std::max(8, (int)ceilf(len / 4.0f));
  Vector2 prev = start;
  float capR = thickness * 0.5f;
  for (int i = 0; i <= steps; ++i) {
    float d = len * (float)i / (float)steps;
    float offset = amplitude * sinf((d / wavelength) * 2.0f * PI + phase);
    Vector2 p = {start.x + dx * ((float)i / (float)steps) + nx * offset,
                 start.y + dy * ((float)i / (float)steps) + ny * offset};
    if (i > 0)
      DrawLineEx(prev, p, thickness, color);
    // Round join at each vertex fills DrawLineEx's outer-edge gaps on bends.
    DrawCircleV(p, capR, color);
    prev = p;
  }
  DrawCircleV(start, capR, color);
}

static void PaintLinearProgress(Rectangle r, float progress, bool indeterminate,
                                bool wavy, float wavelength) {
  const ColorScheme &c = Theme::GetColorScheme();
  float trackH = std::min(r.height, tokens::kLinearProgressTrackHeight);
  float cy = r.y + r.height * 0.5f;
  float gap = 4.0f;        // M3 gap between active and inactive segments
  float dotR = trackH * 0.5f;
  float right = r.x + r.width;
  wavelength = wavelength > 0.0f ? wavelength : 40.0f;

  auto inactive = [&](float x0, float x1) {
    if (x1 - x0 > 0.5f)
      raym3::Renderer::DrawRoundedRectangle(
          {x0, cy - trackH * 0.5f, x1 - x0, trackH}, trackH * 0.5f,
          c.secondaryContainer);
  };

  // Active is a SOLID rounded segment so it fully masks the track in its
  // region — the inactive track is only ever drawn outside the active segment
  // (with a gap), never beneath it.
  auto active = [&](float x0, float x1) {
    if (x1 - x0 > 0.5f) {
      if (wavy) {
        DrawWavyLine({x0, cy}, {x1, cy}, trackH, 3.0f, wavelength,
                     (float)GetTime() * 4.0f, c.primary);
        return;
      }
      raym3::Renderer::DrawRoundedRectangle(
          {x0, cy - trackH * 0.5f, x1 - x0, trackH}, trackH * 0.5f, c.primary);
    }
  };

  if (indeterminate) {
    float t = (float)GetTime();
    float phase = fmodf(t * 0.9f, 1.0f);
    float segW = r.width * 0.35f;
    float x = r.x - segW + phase * (r.width + segW);
    float left = std::max(r.x, x);
    float rt = std::min(right, x + segW);
    inactive(r.x, std::max(r.x, left - gap));
    inactive(std::min(right, rt + gap), right);
    active(left, rt);
  } else {
    float p = std::clamp(progress < 0.0f ? 0.0f : progress, 0.0f, 1.0f);
    float activeEnd = r.x + r.width * p;
    active(r.x, activeEnd);
    // Inactive runs from after the active segment to just before the stop dot.
    inactive(activeEnd + gap, right - dotR - gap);
    // M3 stop indicator dot at the trailing end.
    DrawCircle((int)(right - dotR), (int)cy, dotR, c.primary);
  }
}

static void PaintCheckmark(Rectangle r, Color color) {
  Vector2 c = {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
  float s = std::min(r.width, r.height) * 0.5f;
  Vector2 p1 = {c.x - s * 0.5f, c.y};
  Vector2 p2 = {c.x - s * 0.1f, c.y + s * 0.45f};
  Vector2 p3 = {c.x + s * 0.55f, c.y - s * 0.45f};
  DrawLineEx(p1, p2, 2.0f, color);
  DrawLineEx(p2, p3, 2.0f, color);
}

static void PaintCheckboxControl(Rectangle layout, bool selected,
                                 bool indeterminate, bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  Rectangle visual = CenteredVisualRect(layout, tokens::kCheckboxVisualSize,
                                        tokens::kCheckboxVisualSize);
  float opacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  Color border = selected ? c.primary : c.onSurfaceVariant;
  Color fill = selected ? c.primary : Color{0, 0, 0, 0};
  if (selected)
    raym3::Renderer::DrawRoundedRectangle(
        visual, 2.0f, ColorAlpha(fill, opacity));
  raym3::Renderer::DrawRoundedRectangleEx(
      visual, 2.0f, ColorAlpha(border, opacity), 2.0f);
  if (selected) {
    if (indeterminate) {
      DrawLineEx({visual.x + 4.0f, visual.y + visual.height * 0.5f},
                 {visual.x + visual.width - 4.0f, visual.y + visual.height * 0.5f},
                 2.0f, ColorAlpha(c.onPrimary, opacity));
    } else {
      PaintCheckmark(visual, ColorAlpha(c.onPrimary, opacity));
    }
  }
}

static void PaintRadioControl(Rectangle layout, bool selected, bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  Rectangle visual =
      CenteredVisualRect(layout, tokens::kRadioVisualSize, tokens::kRadioVisualSize);
  float opacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  Vector2 center = {visual.x + visual.width * 0.5f,
                    visual.y + visual.height * 0.5f};
  Color color = selected ? c.primary : c.onSurfaceVariant;
  // M3 spec: 2dp border ring. DrawRing gives an exact filled annulus vs the
  // two-DrawCircleLines hack that produced ~1px anti-aliased arcs.
  float outerR = visual.width * 0.5f;
  float innerR = outerR - 2.0f;
  DrawRing(center, innerR, outerR, 0.0f, 360.0f, 36, ColorAlpha(color, opacity));
  if (selected)
    DrawCircleV(center, 4.5f, ColorAlpha(c.primary, opacity));
}

static void PaintSwitchControl(Rectangle layout, bool selected, bool disabled,
                               float progress) {
  const ColorScheme &c = Theme::GetColorScheme();
  Rectangle track =
      CenteredVisualRect(layout, tokens::kSwitchTrackWidth, tokens::kSwitchTrackHeight);
  float t = std::clamp(progress >= 0.0f ? progress : (selected ? 1.0f : 0.0f),
                       0.0f, 1.0f);
  float opacity = disabled ? tokens::kDisabledContentOpacity : 1.0f;
  Color trackColor =
      selected ? c.primary : c.surfaceContainerHighest;
  Color outlineColor = selected ? Color{0, 0, 0, 0} : c.outline;
  Color thumbColor = selected ? c.onPrimary : c.outline;
  Color iconColor = selected ? c.onPrimaryContainer : c.surfaceContainerHighest;
  if (disabled) {
    trackColor = ColorAlpha(c.onSurface, selected ? tokens::kDisabledContainerOpacity
                                                  : tokens::kDisabledContainerOpacity);
    thumbColor = selected ? c.surface : c.onSurface;
    iconColor = c.onSurface;
  }
  raym3::Renderer::DrawRoundedRectangle(track, track.height * 0.5f,
                                        ColorAlpha(trackColor, opacity));
  if (!selected) {
    raym3::Renderer::DrawRoundedRectangleEx(
        {track.x + 1.0f, track.y + 1.0f, track.width - 2.0f, track.height - 2.0f},
        (track.height - 2.0f) * 0.5f, ColorAlpha(outlineColor, opacity), 2.0f);
  }
  float thumbSize = tokens::kSwitchInactiveThumbSize +
                    (tokens::kSwitchActiveThumbSize -
                     tokens::kSwitchInactiveThumbSize) *
                        t;
  float cy = track.y + track.height * 0.5f;
  float offX = track.x + track.height * 0.5f;
  float onX = track.x + track.width - track.height * 0.5f;
  float cx = offX + (onX - offX) * t;
  Rectangle thumb = {cx - thumbSize * 0.5f, cy - thumbSize * 0.5f,
                     thumbSize, thumbSize};
  raym3::Renderer::DrawRoundedRectangle(thumb, thumbSize * 0.5f,
                                        ColorAlpha(thumbColor, opacity));
  // Icon appears only once the thumb reaches active size (24dp). The 0.1f
  // tolerance avoids a 1-frame flash on the very first frame when animProgress
  // is still settling. M3 spec: icon is visible in the active/on state only.
  if (thumbSize >= tokens::kSwitchActiveThumbSize - 0.1f) {
    raym3::v2::DrawMaterialIcon(selected ? 0xe5ca : 0xe5cd, thumb,
                                ColorAlpha(iconColor, opacity),
                                (int)tokens::kSwitchIconSize, true);
  }
}

static void PaintTextFieldContainer(Rectangle layout, bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  float opacity = disabled ? tokens::kDisabledContainerOpacity : 1.0f;
  // M3 filled text field: surfaceContainerHighest bg with top-only 4dp corners.
  // Approximate with rounded rect + square-off strip at bottom-right/left.
  float r = tokens::kCornerExtraSmall;
  raym3::Renderer::DrawRoundedRectangle(layout, r, ColorAlpha(c.surfaceContainerHighest, opacity));
  // Square the bottom corners: overdraw bottom band with a plain rect.
  DrawRectangleRec({layout.x, layout.y + layout.height - r,
                    layout.width, r},
                   ColorAlpha(c.surfaceContainerHighest, opacity));
  // Bottom indicator line: 1dp, onSurfaceVariant (becomes primary when focused,
  // handled via state layer at a higher level for now).
  float indH = 1.0f;
  DrawRectangleRec({layout.x, layout.y + layout.height - indH,
                    layout.width, indH},
                   ColorAlpha(disabled ? c.onSurface : c.onSurfaceVariant,
                              disabled ? tokens::kDisabledContentOpacity : 1.0f));
}

static void PaintBottomSheetHandle(Rectangle layout, bool disabled) {
  const ColorScheme &c = Theme::GetColorScheme();
  Rectangle handle = {
      layout.x + (layout.width - tokens::kBottomSheetDragHandleWidth) * 0.5f,
      layout.y + 8.0f,
      tokens::kBottomSheetDragHandleWidth,
      tokens::kBottomSheetDragHandleHeight};
  raym3::Renderer::DrawRoundedRectangle(
      handle, tokens::kBottomSheetDragHandleHeight * 0.5f,
      ColorAlpha(c.onSurfaceVariant,
                 disabled ? tokens::kDisabledContentOpacity : 0.4f));
}

static void PaintCircularLoading(Rectangle r, float progress, bool indeterminate,
                                 bool wavy, float wavelength) {
  const ColorScheme &c = Theme::GetColorScheme();
  Vector2 center = {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
  float outer = std::min(r.width, r.height) * 0.5f;
  float stroke = tokens::kProgressIndicatorStroke;
  float inner = outer - stroke;
  float radius = (inner + outer) * 0.5f;
  float capR = stroke * 0.5f;
  float gapDeg = radius > 0.0f ? (4.0f / radius) * RAD2DEG : 0.0f;
  wavelength = wavelength > 0.0f ? wavelength : 15.0f;
  auto cap = [&](float deg, Color color) {
    float a = deg * DEG2RAD;
    DrawCircleV({center.x + cosf(a) * radius, center.y + sinf(a) * radius},
                capR, color);
  };
  auto activeArc = [&](float startDeg, float endDeg, Color color) {
    if (wavy) {
      float arcLen = fabsf((endDeg - startDeg) * DEG2RAD * radius);
      int steps = std::max(12, (int)ceilf(arcLen / 2.5f));
      Vector2 prev = {0.0f, 0.0f};
      Vector2 first = {0.0f, 0.0f};
      float phase = (float)GetTime() * 5.0f;
      for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float deg = startDeg + (endDeg - startDeg) * t;
        float distance = arcLen * t;
        float rr = radius + 1.6f * sinf((distance / wavelength) * 2.0f * PI + phase);
        float a = deg * DEG2RAD;
        Vector2 p = {center.x + cosf(a) * rr, center.y + sinf(a) * rr};
        if (i == 0) first = p;
        else DrawLineEx(prev, p, stroke, color);
        // Round join at each vertex fills the triangular gaps ("cracks") that
        // DrawLineEx leaves on the outer edge of every bend.
        DrawCircleV(p, capR, color);
        prev = p;
      }
      DrawCircleV(first, capR, color);
      return;
    }
    DrawRing(center, inner, outer, startDeg, endDeg, 96, color);
    cap(startDeg, color);
    cap(endDeg, color);
  };
  auto trackArc = [&](float startDeg, float endDeg) {
    if (endDeg - startDeg > 0.5f)
      DrawRing(center, inner, outer, startDeg, endDeg, 96,
               c.secondaryContainer);
  };
  if (indeterminate || progress < 0.0f) {
    float t = (float)GetTime();
    float sweep = 36.0f + (sinf(t * 3.2f) * 0.5f + 0.5f) * 234.0f;
    float start = fmodf(t * 270.0f, 360.0f) - 90.0f;
    trackArc(start + sweep + gapDeg, start + 360.0f - gapDeg);
    activeArc(start, start + sweep, c.primary);
  } else {
    float p = std::clamp(progress, 0.0f, 1.0f);
    float activeEnd = -90.0f + 360.0f * p;
    if (p > 0.0f) activeArc(-90.0f, activeEnd, c.primary);
    // Inactive arc fills only the remainder, with gaps on both sides — never
    // beneath the active arc.
    if (p < 1.0f)
      trackArc(activeEnd + gapDeg, 270.0f - gapDeg);
  }
}

static MotionSpec Motion(float seconds, Easing easing, MotionRole role,
                         bool spring = false, float dampingRatio = 1.0f) {
  MotionSpec spec;
  spec.durationSeconds = seconds;
  spec.easing = easing;
  spec.role = role;
  spec.spring = spring;
  spec.dampingRatio = dampingRatio;
  return spec;
}

static bool IsInteractive(M3Component component) {
  switch (component) {
  case M3Component::ButtonGroup:
  case M3Component::Button:
  case M3Component::Checkbox:
  case M3Component::Chip:
  case M3Component::DatePicker:
  case M3Component::ExtendedFab:
  case M3Component::Fab:
  case M3Component::FabMenu:
  case M3Component::IconButton:
  // Menu container is a passive surface — only its items are interactive.
  case M3Component::MenuItem:
  case M3Component::NavigationBar:
  case M3Component::NavigationDrawer:
  case M3Component::NavigationRail:
  case M3Component::RadioButton:
  case M3Component::RangeSlider:
  case M3Component::Search:
  case M3Component::SegmentedButton:
  case M3Component::Slider:
  case M3Component::SplitButton:
  case M3Component::Switch:
  case M3Component::Tabs:
  case M3Component::TextField:
  case M3Component::TimePicker:
  case M3Component::Toolbar:
  case M3Component::Tooltip:
  case M3Component::Popover:
    return true;
  default:
    return false;
  }
}

static bool IsSelectable(M3Component component) {
  switch (component) {
  case M3Component::ButtonGroup:
  case M3Component::Checkbox:
  case M3Component::Chip:
  case M3Component::IconButton:
  case M3Component::MenuItem:
  case M3Component::NavigationBar:
  case M3Component::NavigationDrawer:
  case M3Component::NavigationRail:
  case M3Component::RadioButton:
  case M3Component::SegmentedButton:
  case M3Component::Switch:
  case M3Component::Tabs:
    return true;
  default:
    return false;
  }
}

static bool IsOverlay(M3Component component) {
  switch (component) {
  case M3Component::BottomSheet:
  case M3Component::DatePicker:
  case M3Component::Dialog:
  case M3Component::FabMenu:
  case M3Component::Menu:
  case M3Component::SideSheet:
  case M3Component::Snackbar:
  case M3Component::TimePicker:
  case M3Component::Tooltip:
  case M3Component::Popover:
    return true;
  default:
    return false;
  }
}

static MotionSpec DefaultMotionFor(M3Component component) {
  switch (component) {
  case M3Component::LoadingIndicator:
  case M3Component::ProgressIndicator:
    return Motion(1.4f, Easing::Linear, MotionRole::Loading);
  case M3Component::BottomSheet:
  case M3Component::SideSheet:
  case M3Component::Dialog:
  case M3Component::DatePicker:
  case M3Component::TimePicker:
    return Motion(0.4f, Easing::EmphasizedDecelerate, MotionRole::Enter, true,
                  0.82f);
  case M3Component::FabMenu:
  case M3Component::Menu:
  case M3Component::Search:
  case M3Component::Snackbar:
  case M3Component::Tooltip:
  case M3Component::Popover:
    return Motion(0.25f, Easing::EmphasizedDecelerate,
                  MotionRole::ExpressiveFastEffects, true, 0.9f);
  case M3Component::NavigationBar:
  case M3Component::NavigationDrawer:
  case M3Component::NavigationRail:
  case M3Component::Tabs:
    return Motion(0.3f, Easing::Emphasized, MotionRole::ActiveIndicator, true,
                  0.88f);
  case M3Component::Button:
  case M3Component::ButtonGroup:
  case M3Component::ExtendedFab:
  case M3Component::Fab:
  case M3Component::IconButton:
  case M3Component::SegmentedButton:
  case M3Component::SplitButton:
    return Motion(0.2f, Easing::Emphasized, MotionRole::ShapeMorph, true,
                  0.86f);
  case M3Component::Carousel:
  case M3Component::RangeSlider:
  case M3Component::Slider:
    return Motion(0.3f, Easing::Emphasized, MotionRole::Drag, true, 0.9f);
  case M3Component::Checkbox:
  case M3Component::Chip:
  case M3Component::RadioButton:
  case M3Component::Switch:
    return Motion(0.2f, Easing::Emphasized,
                  MotionRole::ExpressiveFastEffects, true, 0.9f);
  default:
    return Motion(0.3f, Easing::Standard, MotionRole::Standard);
  }
}

static StateStyles MergeStateStyles(const StateStyles &base,
                                    const StateStyles &overrideStyles) {
  StateStyles result = base;
  if (overrideStyles.hovered)
    result.hovered = result.hovered ? MergeStyles(*result.hovered,
                                                  *overrideStyles.hovered)
                                    : overrideStyles.hovered;
  if (overrideStyles.pressed)
    result.pressed = result.pressed ? MergeStyles(*result.pressed,
                                                  *overrideStyles.pressed)
                                    : overrideStyles.pressed;
  if (overrideStyles.focused)
    result.focused = result.focused ? MergeStyles(*result.focused,
                                                  *overrideStyles.focused)
                                    : overrideStyles.focused;
  if (overrideStyles.disabled)
    result.disabled = result.disabled ? MergeStyles(*result.disabled,
                                                    *overrideStyles.disabled)
                                      : overrideStyles.disabled;
  return result;
}

static Style DefaultsFor(M3Component component) {
  Style style;
  MaterialComponentMetrics metrics = GetMaterialMetrics(component);
  style.flexDirection = FlexDirection::Column;
  style.alignItems = Align::Stretch;
  style.text.fontSize = Theme::GetTypographyScale().bodyMedium;
  style.text.lineHeight = 20.0f;
  style.text.color = Theme::GetColorScheme().onSurface;

  switch (component) {
  case M3Component::AppBar:
  case M3Component::Toolbar:
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.padding.horizontal = 16.0f;
    style.gap = 8.0f;
    style.backgroundColor = Theme::GetColorScheme().surface;
    style.text.fontSize = Theme::GetTypographyScale().titleLarge;
    style.text.lineHeight = 28.0f;
    break;
  case M3Component::Button:
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.padding.horizontal = tokens::kButtonHorizontalPadding;
    style.gap = tokens::kButtonIconGap;
    style.borderRadius = tokens::kButtonRadius;
    style.backgroundColor = Theme::GetColorScheme().primary;
    style.text.color = Theme::GetColorScheme().onPrimary;
    style.text.fontSize = Theme::GetTypographyScale().labelLarge;
    style.text.lineHeight = 20.0f;
    style.text.weight = FontWeight::Medium;
    break;
  case M3Component::NavigationBar:
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::SpaceAround;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainer;
    style.elevation = 3.0f;
    break;
  case M3Component::NavigationBarItem:
    // Vertical stack: [active-indicator pill + icon] over a label. The pill is
    // painted by the renderer (NodeRole::NavItem) behind the icon child.
    style.height = metrics.layoutHeight;
    style.minHeight = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Column;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    // The 32dp indicator pill is centered on the 24dp icon, so it overhangs the
    // icon by 4dp top/bottom. Use an 8dp icon?label gap (4dp overhang + 4dp M3
    // clear space) so the pill never touches the label.
    style.gap = 8.0f;
    style.minWidth = metrics.minWidth;
    style.padding.top = 6.0f;
    style.padding.bottom = 6.0f;
    style.text.fontSize = Theme::GetTypographyScale().labelMedium;
    style.text.lineHeight = 16.0f;
    style.text.weight = FontWeight::Medium;
    style.text.color = Theme::GetColorScheme().onSurfaceVariant;
    break;
  case M3Component::NavigationDrawer:
    style.width = metrics.layoutWidth;
    style.padding.all = 12.0f;
    style.gap = 4.0f;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerLow;
    style.borderRadius = Theme::GetShapeTokens().cornerLarge;
    break;
  case M3Component::NavigationRail:
    style.width = metrics.layoutWidth;
    style.padding.top = 44.0f;
    style.padding.bottom = 4.0f;
    style.gap = 4.0f;
    style.overflow = Overflow::Visible;
    style.backgroundColor = Theme::GetColorScheme().surface;
    style.elevation = 3.0f;
    break;
  case M3Component::BottomSheet:
    style.position = PositionType::Fixed; // rendered in the viewport overlay pass
    // Dock full-width to the bottom edge of the viewport (CSS: left/right/bottom: 0)
    style.inset.left   = 0.0f;
    style.inset.right  = 0.0f;
    style.inset.bottom = 0.0f;
    style.padding.all = 24.0f;
    style.gap = 16.0f;
    style.borderRadius = tokens::kCornerExtraLarge;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerLow;
    style.elevation = 1.0f;
    break;
  case M3Component::SideSheet:
    style.position = PositionType::Fixed; // rendered in the viewport overlay pass
    // Dock full-height to the right edge of the viewport
    style.inset.top    = 0.0f;
    style.inset.right  = 0.0f;
    style.inset.bottom = 0.0f;
    style.width = metrics.layoutWidth;
    style.padding.all = 24.0f;
    style.gap = 16.0f;
    style.borderRadius = tokens::kCornerLarge;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerHigh;
    style.elevation = 2.0f;
    break;
  case M3Component::Dialog:
    style.position = PositionType::Fixed; // rendered in the viewport overlay pass
    style.width = 320.0f;                 // M3 minimum dialog width
    style.minWidth = 280.0f;              // Flutter Dialog floor
    style.minHeight = 140.0f;
    style.padding.all = 24.0f;
    style.gap = 16.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerExtraLarge;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerHigh;
    style.elevation = 6.0f;
    break;
  case M3Component::Card:
    style.padding.all = 16.0f;
    style.gap = 8.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerMedium;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerLow;
    style.elevation = 1.0f;
    break;
  case M3Component::Carousel:
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Stretch;
    style.overflow = Overflow::Hidden;
    style.padding.all = 8.0f;
    style.gap = 8.0f;
    break;
  case M3Component::Checkbox:
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    style.minHeight = metrics.minHeight;
    style.borderRadius = 2.0f;
    style.borderWidth = 2.0f;
    style.borderColor = Theme::GetColorScheme().onSurfaceVariant;
    break;
  case M3Component::RadioButton:
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    style.minHeight = metrics.minHeight;
    style.borderRadius = tokens::kRadioVisualSize * 0.5f;
    style.borderWidth = 2.0f;
    style.borderColor = Theme::GetColorScheme().onSurfaceVariant;
    break;
  case M3Component::Switch:
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    style.minHeight = metrics.minHeight;
    style.borderRadius = tokens::kSwitchTrackHeight * 0.5f;
    style.borderWidth = 2.0f;
    style.borderColor = Theme::GetColorScheme().outline;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerHighest;
    break;
  case M3Component::Badge:
    // M3 large badge: 16dp circle, number centered. minWidth==height keeps a
    // single digit circular; multi-digit grows into a pill via padding.
    style.minWidth = metrics.minWidth;
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.padding.horizontal = tokens::kBadgeLargeHorizontalPadding;
    style.borderRadius = tokens::kBadgeLargeHeight * 0.5f;
    style.backgroundColor = Theme::GetColorScheme().error;
    style.text.color = Theme::GetColorScheme().onError;
    style.text.fontSize = 11.0f;
    style.text.lineHeight = 11.0f;
    style.text.alignment = TextAlignment::Center;
    break;
  case M3Component::Chip:
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.padding.horizontal = 8.0f;
    style.gap = 8.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerSmall;
    style.borderWidth = 1.0f;
    style.borderColor = Theme::GetColorScheme().outline;
    break;
  case M3Component::SplitButton:
    // Two connected segments (main action + dropdown) painted by the renderer
    // (NodeRole::SplitButton). Stretch so both segments fill the 40dp height.
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Stretch;
    style.gap = 0.0f;
    style.text.fontSize = Theme::GetTypographyScale().labelLarge;
    style.text.weight = FontWeight::Medium;
    break;
  case M3Component::ButtonGroup:
    // M3 connected button group: a row of individually fully-rounded buttons
    // with small (2dp) gaps. Each item paints its own fill (NodeRole
    // ButtonGroupConnected); no shared outline or dividers.
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Stretch;
    style.gap = 2.0f;
    style.text.fontSize = Theme::GetTypographyScale().labelLarge;
    style.text.weight = FontWeight::Medium;
    break;
  case M3Component::SegmentedButton:
    // Container: pill border + segment fills drawn manually by the renderer
    // (ButtonGroupContainer role) so no overflow:Hidden/stencil is needed —
    // fills are drawn with DrawSegmentShape which rounds only the outer edges.
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Stretch;
    style.gap = 0.0f;
    style.text.fontSize = Theme::GetTypographyScale().labelLarge;
    style.text.weight = FontWeight::Medium;
    break;
  case M3Component::Fab:
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.borderRadius = Theme::GetShapeTokens().cornerLarge;
    style.backgroundColor = Theme::GetColorScheme().primaryContainer;
    style.text.color = Theme::GetColorScheme().onPrimaryContainer;
    style.elevation = 3.0f;
    break;
  case M3Component::ExtendedFab:
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.gap = 8.0f;
    style.padding.horizontal = 16.0f;
    style.minWidth = metrics.minWidth;
    style.borderRadius = Theme::GetShapeTokens().cornerLarge;
    style.backgroundColor = Theme::GetColorScheme().primaryContainer;
    style.text.color = Theme::GetColorScheme().onPrimaryContainer;
    style.elevation = 3.0f;
    break;
  case M3Component::IconButton:
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    style.borderRadius = tokens::kIconButtonRadius;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    break;
  case M3Component::List:
    style.padding.vertical = 8.0f;
    style.gap = 2.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerLarge;
    break;
  case M3Component::Menu:
  case M3Component::FabMenu:
    // M3 vertical menu: surfaceContainer-low container, 12dp rounded corners,
    // 8dp vertical / 4dp horizontal padding so item highlights inset from the
    // container edge, 2dp gap between items.
    style.flexDirection = FlexDirection::Column;
    style.padding.vertical = 8.0f;
    style.padding.horizontal = 4.0f;
    style.gap = 2.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerMedium;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerLow;
    style.elevation = 3.0f;
    style.minWidth = 160.0f;
    break;
  case M3Component::MenuItem:
    // M3 menu item: 48dp min height, 12dp horizontal padding, leading/trailing
    // 12dp gap, 8dp rounded state-layer highlight, onSurface label.
    style.minHeight = 48.0f;
    style.padding.horizontal = 12.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.gap = 12.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerSmall;
    style.text.color = Theme::GetColorScheme().onSurface;
    break;
  case M3Component::Search:
    style.height = metrics.layoutHeight;
    style.padding.horizontal = 16.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.gap = 12.0f;
    style.borderRadius = 28.0f;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerHigh;
    style.text.color = Theme::GetColorScheme().onSurfaceVariant;
    break;
  case M3Component::DatePicker:
  case M3Component::TimePicker:
    style.position = PositionType::Fixed; // Rendered in viewport overlay pass
    style.width = 328.0f;                 // Standard M3 dialog width
    style.minWidth = 238.0f;              // Flutter time-picker scroll floor
    style.minHeight = 326.0f;
    style.padding.all = 24.0f;
    style.gap = 8.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerExtraLarge;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainerHigh;
    style.elevation = 6.0f;
    break;
  case M3Component::Divider:
    style.height = 1.0f;
    style.backgroundColor = Theme::GetColorScheme().outlineVariant;
    break;
  case M3Component::Banner:
    // Full-width informational surface: leading content + supporting text +
    // trailing actions.
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.minHeight = 54.0f;
    style.padding.horizontal = 16.0f;
    style.padding.vertical = 8.0f;
    style.gap = 16.0f;
    style.backgroundColor = Theme::GetColorScheme().surface;
    break;
  case M3Component::BottomAppBar:
    // M3: 80dp container, surfaceContainer, elevation 3.
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::SpaceBetween;
    style.padding.horizontal = 16.0f;
    style.gap = 8.0f;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainer;
    style.elevation = 3.0f;
    break;
  case M3Component::DockedToolbar:
    // Full-width bar docked to an edge; square corners, surfaceContainer.
    style.height = 64.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::SpaceAround;
    style.padding.horizontal = 8.0f;
    style.gap = 8.0f;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainer;
    style.elevation = 2.0f;
    break;
  case M3Component::FloatingToolbar:
    // Pill-shaped floating cluster of actions; full corner, elevated.
    style.height = 64.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.padding.horizontal = 12.0f;
    style.gap = 4.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerFull;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainer;
    style.elevation = 3.0f;
    break;
  case M3Component::DataTable:
    style.flexDirection = FlexDirection::Column;
    style.alignItems = Align::Stretch;
    style.borderRadius = Theme::GetShapeTokens().cornerMedium;
    style.borderWidth = 1.0f;
    style.borderColor = Theme::GetColorScheme().outlineVariant;
    style.backgroundColor = Theme::GetColorScheme().surface;
    style.overflow = Overflow::Hidden;
    break;
  case M3Component::LoadingIndicator:
    // Painted via PaintCircularLoading — container only reserves the size.
    style.width = metrics.layoutWidth;
    style.height = metrics.layoutHeight;
    break;
  case M3Component::ProgressIndicator:
    // Painted via PaintLinearProgress — no box fill behind the track.
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    break;
  case M3Component::Slider:
  case M3Component::RangeSlider:
    // Painted via slider-family painters (track + active + handle). 44dp touch target.
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    break;
  case M3Component::Snackbar:
    style.minHeight = metrics.minHeight;
    style.padding.horizontal = 16.0f;
    style.padding.vertical = 8.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.gap = 8.0f;
    // M3 snackbar container shape = corner extra small (4dp).
    style.borderRadius = Theme::GetShapeTokens().cornerExtraSmall;
    style.backgroundColor = Theme::GetColorScheme().inverseSurface;
    style.text.color = Theme::GetColorScheme().inverseOnSurface;
    style.elevation = 6.0f;
    break;
  case M3Component::Tabs:
    style.height = metrics.layoutHeight;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    // Active indicator is painted post-layout by the renderer (NodeRole::Tabs);
    // M3 tab rows carry only a bottom divider, not a full-box border.
    break;
  case M3Component::TextField:
    style.height = metrics.layoutHeight;
    style.minWidth = metrics.minWidth;
    // M3 filled text field: no box border — PaintTextFieldContainer draws bg
    // with top-only 4dp corners and the 1dp bottom indicator line.
    style.backgroundColor = {};
    break;
  case M3Component::Tooltip:
    style.padding.horizontal = 8.0f;
    style.padding.vertical = 4.0f;
    style.borderRadius = Theme::GetShapeTokens().cornerSmall;
    style.backgroundColor = Theme::GetColorScheme().inverseSurface;
    style.text.color = Theme::GetColorScheme().inverseOnSurface;
    break;
  case M3Component::Popover:
    style.position = PositionType::Fixed;
    style.backgroundColor = Theme::GetColorScheme().surfaceContainer;
    style.borderRadius = Theme::GetShapeTokens().cornerMedium;
    style.elevation = 6.0f;
    break;
  default:
    style.height = 40.0f;
    break;
  }

  return style;
}

static void ApplyOpenState(M3Component component, const ComponentProps &props,
                           Style &style) {
  // Modal surfaces are hidden until opened (toggled via the `open` prop).
  if (component == M3Component::Dialog ||
      component == M3Component::BottomSheet ||
      component == M3Component::SideSheet ||
      component == M3Component::DatePicker ||
      component == M3Component::TimePicker ||
      component == M3Component::Popover) {
    style.display = props.open ? Display::Flex : Display::None;
  }
  const bool rowNavItem =
      component == M3Component::NavigationBarItem &&
      (props.open ||
       props.navigationItemLayout == NavigationItemLayout::Row);
  const bool explicitColumnNavItem =
      component == M3Component::NavigationBarItem &&
      props.navigationItemLayout == NavigationItemLayout::Column && !props.open;

  switch (component) {
  case M3Component::NavigationRail:
    if (!props.open)
      break;
    // Expanded rail is an overlay: reserve the collapsed 96dp layout slot and
    // let the renderer paint the wider surface without changing rail spacing.
    style.alignItems = Align::FlexStart;
    break;
  case M3Component::NavigationBarItem:
    if (rowNavItem) {
      if (props.open) {
        style.width = 240.0f;
        style.maxWidth = 240.0f;
        style.minWidth = 240.0f;
        style.padding.left = 36.0f;
        style.padding.right = 20.0f;
        // Expanded rail items keep the collapsed rail's vertical footprint so
        // opening the rail reads as horizontal expansion instead of reflow.
        style.height = 48.0f;
        style.minHeight = 48.0f;
        style.padding.vertical = 6.0f;
      } else {
        style.padding.horizontal = 16.0f;
        style.height = 64.0f;
        style.minHeight = 64.0f;
        style.padding.vertical = 12.0f;
      }
      style.flexDirection = FlexDirection::Row;
      style.justifyContent = Justify::FlexStart;
      style.alignItems = props.open ? Align::FlexStart : Align::Center;
      style.gap = 4.0f;
    } else if (explicitColumnNavItem) {
      style.flexDirection = FlexDirection::Column;
    }
    break;
  default:
    break;
  }
}

static StateStyles DefaultStateStylesFor(M3Component component,
                                         const Style &base) {
  StateStyles states;
  if (!IsInteractive(component))
    return states;

  Color content = base.text.color.value_or(Theme::GetColorScheme().onSurface);

  Style hovered;
  hovered.stateLayerColor =
      Theme::GetStateLayerColor(content, ComponentState::Hovered);
  states.hovered = hovered;

  Style pressed;
  pressed.stateLayerColor =
      Theme::GetStateLayerColor(content, ComponentState::Pressed);
  states.pressed = pressed;

  Style focused;
  focused.stateLayerColor =
      Theme::GetStateLayerColor(content, ComponentState::Focused);
  focused.borderColor = Theme::GetColorScheme().primary;
  states.focused = focused;

  Style disabled;
  disabled.opacity = 0.38f;
  disabled.stateLayerColor =
      Theme::GetStateLayerColor(content, ComponentState::Disabled);
  states.disabled = disabled;

  return states;
}

static void ApplySelectionState(M3Component component, const ComponentProps &props,
                                Style &style) {
  const bool selected = props.selected || props.checked || props.indeterminate;
  if (!selected)
    return;

  switch (component) {
  case M3Component::Checkbox:
  case M3Component::RadioButton:
    style.backgroundColor = Theme::GetColorScheme().primary;
    style.borderColor = Theme::GetColorScheme().primary;
    style.text.color = Theme::GetColorScheme().onPrimary;
    break;
  case M3Component::Switch:
  case M3Component::Chip:
  case M3Component::IconButton:
    style.backgroundColor = Theme::GetColorScheme().secondaryContainer;
    style.borderColor = Theme::GetColorScheme().secondaryContainer;
    style.text.color = Theme::GetColorScheme().onSecondaryContainer;
    break;
  case M3Component::SegmentedButton:
  case M3Component::ButtonGroup:
    // Item-level selection: fill with secondaryContainer, no item border
    // (the container draws the shared outline).
    style.backgroundColor = Theme::GetColorScheme().secondaryContainer;
    style.text.color = Theme::GetColorScheme().onSecondaryContainer;
    break;
  case M3Component::MenuItem:
    // M3 selected menu item: tertiary-container highlight + on-tertiary-container
    // label/check (spec color roles 7, 8, 11).
    style.backgroundColor = Theme::GetColorScheme().tertiaryContainer;
    style.text.color = Theme::GetColorScheme().onTertiaryContainer;
    break;
  case M3Component::NavigationBarItem:
    // Selected label brightens; the indicator pill is painted separately so no
    // full-item background fill here.
    style.text.color = Theme::GetColorScheme().onSurface;
    break;
  case M3Component::NavigationRail:
    // The rail conveys open/closed via width + the per-item active-indicator
    // pill, not a container fill — and `open` is mapped onto `selected`, so a
    // selection fill here would paint the whole surface pill-colored once the
    // rail has been opened. Keep the surface as `surface`.
    break;
  case M3Component::NavigationBar:
  case M3Component::NavigationDrawer:
    style.backgroundColor = Theme::GetColorScheme().secondaryContainer;
    style.text.color = Theme::GetColorScheme().onSecondaryContainer;
    break;
  case M3Component::Tabs:
    // M3 tabs: selected state = primary-colored label + the bottom indicator
    // (painted by the renderer). NO container fill — a fill would draw a tall
    // box behind the selected tab instead of the 3dp underline.
    style.text.color = Theme::GetColorScheme().primary;
    break;
  default:
    break;
  }
}

ComponentSpec GetComponentSpec(M3Component component) {
  switch (component) {
  case M3Component::AppBar:
    return {component, "App bars", "app-bars", DefaultMotionFor(component),
            false, false, false};
  case M3Component::Badge:
    return {component, "Badges", "badges", DefaultMotionFor(component), false,
            false, false};
  case M3Component::Banner:
    return {component, "Banners", "banners", DefaultMotionFor(component), false,
            false, false};
  case M3Component::BottomAppBar:
    return {component, "Bottom app bar", "bottom-app-bar",
            DefaultMotionFor(component), true, false, false};
  case M3Component::DataTable:
    return {component, "Data tables", "data-tables",
            DefaultMotionFor(component), false, false, false};
  case M3Component::DockedToolbar:
    return {component, "Docked toolbar", "docked-toolbar",
            DefaultMotionFor(component), true, false, false};
  case M3Component::FloatingToolbar:
    return {component, "Floating toolbar", "floating-toolbar",
            DefaultMotionFor(component), true, false, false};
  case M3Component::BottomSheet:
    return {component, "Bottom sheets", "bottom-sheets",
            DefaultMotionFor(component), false, false, true};
  case M3Component::ButtonGroup:
    return {component, "Button groups", "button-groups",
            DefaultMotionFor(component), true, true, false};
  case M3Component::Button:
    return {component, "Buttons", "buttons", DefaultMotionFor(component), true,
            false, false};
  case M3Component::Card:
    return {component, "Cards", "cards", DefaultMotionFor(component), false,
            false, false};
  case M3Component::Carousel:
    return {component, "Carousel", "carousel", DefaultMotionFor(component),
            true, false, false};
  case M3Component::Checkbox:
    return {component, "Checkbox", "checkbox", DefaultMotionFor(component),
            true, true, false};
  case M3Component::Chip:
    return {component, "Chips", "chips", DefaultMotionFor(component), true,
            true, false};
  case M3Component::DatePicker:
    return {component, "Date pickers", "date-pickers",
            DefaultMotionFor(component), true, false, true};
  case M3Component::Dialog:
    return {component, "Dialogs", "dialogs", DefaultMotionFor(component),
            false, false, true};
  case M3Component::Divider:
    return {component, "Divider", "divider", DefaultMotionFor(component),
            false, false, false};
  case M3Component::ExtendedFab:
    return {component, "Extended FABs", "extended-fab",
            DefaultMotionFor(component), true, false, false};
  case M3Component::Fab:
    return {component, "FABs", "floating-action-button",
            DefaultMotionFor(component), true, false, false};
  case M3Component::FabMenu:
    return {component, "FAB menu", "fab-menu", DefaultMotionFor(component),
            true, false, true};
  case M3Component::IconButton:
    return {component, "Icon buttons", "icon-buttons",
            DefaultMotionFor(component), true, true, false};
  case M3Component::List:
    return {component, "Lists", "lists", DefaultMotionFor(component), false,
            false, false};
  case M3Component::LoadingIndicator:
    return {component, "Loading indicator", "loading-indicator",
            DefaultMotionFor(component), false, false, false};
  case M3Component::Menu:
    return {component, "Menus", "menus", DefaultMotionFor(component), true,
            false, true};
  case M3Component::MenuItem:
    return {component, "Menu item", "menus", DefaultMotionFor(component), true,
            true, false};
  case M3Component::NavigationBar:
    return {component, "Navigation bar", "navigation-bar",
            DefaultMotionFor(component), true, true, false};
  case M3Component::NavigationBarItem:
    return {component, "Navigation bar item", "navigation-bar",
            DefaultMotionFor(component), true, true, false};
  case M3Component::NavigationDrawer:
    return {component, "Navigation drawer", "navigation-drawer",
            DefaultMotionFor(component), true, true, false};
  case M3Component::NavigationRail:
    return {component, "Navigation rail", "navigation-rail",
            DefaultMotionFor(component), true, true, false};
  case M3Component::ProgressIndicator:
    return {component, "Progress indicators", "progress-indicators",
            DefaultMotionFor(component), false, false, false};
  case M3Component::RadioButton:
    return {component, "Radio button", "radio-button",
            DefaultMotionFor(component), true, true, false};
  case M3Component::Search:
    return {component, "Search", "search", DefaultMotionFor(component), true,
            false, false};
  case M3Component::SegmentedButton:
    return {component, "Segmented buttons", "segmented-buttons",
            DefaultMotionFor(component), true, true, false};
  case M3Component::SideSheet:
    return {component, "Side sheets", "side-sheets",
            DefaultMotionFor(component), false, false, true};
  case M3Component::Slider:
    return {component, "Sliders", "sliders", DefaultMotionFor(component), true,
            false, false};
  case M3Component::RangeSlider:
    return {component, "Range sliders", "range-sliders",
            DefaultMotionFor(component), true, false, false};
  case M3Component::Snackbar:
    return {component, "Snackbar", "snackbar", DefaultMotionFor(component),
            false, false, true};
  case M3Component::SplitButton:
    return {component, "Split buttons", "split-button",
            DefaultMotionFor(component), true, false, false};
  case M3Component::Switch:
    return {component, "Switch", "switch", DefaultMotionFor(component), true,
            true, false};
  case M3Component::Tabs:
    return {component, "Tabs", "tabs", DefaultMotionFor(component), true, true,
            false};
  case M3Component::TextField:
    return {component, "Text fields", "text-fields",
            DefaultMotionFor(component), true, false, false};
  case M3Component::TimePicker:
    return {component, "Time pickers", "time-pickers",
            DefaultMotionFor(component), true, false, true};
  case M3Component::Toolbar:
    return {component, "Toolbars", "toolbars", DefaultMotionFor(component),
            true, false, false};
  case M3Component::Tooltip:
    return {component, "Tooltips", "tooltips", DefaultMotionFor(component),
            true, false, true};
  case M3Component::Popover:
    return {component, "Popover", "popovers", DefaultMotionFor(component),
            true, false, true};
  }

  return {component, "Unknown", "", DefaultMotionFor(component), false, false,
          false};
}

static void ApplyModalOverlayFlags(NodePtr &node, M3Component component) {
  if (component == M3Component::Dialog ||
      component == M3Component::BottomSheet ||
      component == M3Component::SideSheet ||
      component == M3Component::DatePicker ||
      component == M3Component::TimePicker) {
    node->hasScrim = true;
    node->capturesInput = true;
  }
}

static NodePtr FinishM3Node(NodePtr node, M3Component component) {
  if (node)
    node->inkRipple = IsInteractive(component);
  return node;
}

NodePtr MaterialComponent(M3Component component, const ComponentProps &props,
                          std::vector<NodePtr> children) {
  Style defaults = DefaultsFor(component);
  ApplyOpenState(component, props, defaults);
  ApplySelectionState(component, props, defaults);
  Style style = MergeStyles(defaults, props.style);
  StateStyles stateStyles =
      MergeStateStyles(DefaultStateStylesFor(component, style), props.stateStyles);
  MotionSpec motion = props.motion.enabled ? props.motion : DefaultMotionFor(component);
  if (props.motion.durationSeconds == 0.2f &&
      props.motion.easing == Easing::Standard &&
      props.motion.role == MotionRole::Standard && !props.motion.spring) {
    motion = DefaultMotionFor(component);
  }

  const bool isSelected = props.selected || props.checked || props.indeterminate;

  // Badge dot variant (no label): M3 small badge = 6dp circle, no padding.
  if (component == M3Component::Badge && props.label.empty()) {
    constexpr float kDot = 6.0f;
    style.width = kDot;
    style.height = kDot;
    style.minWidth = kDot;
    style.borderRadius = kDot * 0.5f;
    style.padding = {};
  }

  if (component == M3Component::Button || component == M3Component::Fab ||
      component == M3Component::ExtendedFab ||
      component == M3Component::IconButton) {
    // When the caller composes children (typically a leading Icon), the label
    // must become a flex Text child so Yoga lays out [icon][label] as a
    // centered row. Otherwise the Button node draws node->text centered, which
    // would overlap the icon. Label-only buttons keep the centered-text path.
    std::string buttonLabel = props.label;
    if (!children.empty() && !props.label.empty()) {
      TextProps textProps;
      textProps.style.text = style.text;
      children.push_back(Text(props.label, textProps));
      buttonLabel.clear();
    }
    NodePtr node = Button({.id = props.id,
                           .label = buttonLabel,
                           .variant = ButtonVariant::Filled,
                           .style = style,
                           .stateStyles = stateStyles,
                           .motion = motion,
                           .disabled = props.disabled,
                           .onPress = props.onPress},
                          std::move(children));
    node->selected = isSelected;
    return FinishM3Node(node, component);
  }

  ViewProps viewProps;
  viewProps.id = props.id;
  viewProps.style = style;
  viewProps.stateStyles = stateStyles;
  viewProps.motion = motion;
  viewProps.disabled = props.disabled;
  viewProps.capturesInput = props.capturesInput;
  viewProps.zIndex =
      (component == M3Component::NavigationRail && props.zIndex == 0) ? 30 :
      (component == M3Component::Dialog       && props.zIndex == 0) ? 9999 :
      (component == M3Component::BottomSheet  && props.zIndex == 0) ? 9998 :
      (component == M3Component::SideSheet    && props.zIndex == 0) ? 9997 :
      (component == M3Component::DatePicker   && props.zIndex == 0) ? 9996 :
      (component == M3Component::TimePicker   && props.zIndex == 0) ? 9995 :
      (component == M3Component::Popover      && props.zIndex == 0) ? 9000 :
      props.zIndex;
  viewProps.onPress = props.onPress;

  // Components with real M3 anatomy paint themselves (track + handle, animated
  // arcs, indeterminate segments) instead of rendering as a flat box.
  if (component == M3Component::Slider) {
    float progress = props.progress;
    bool disabled = props.disabled;
    return FinishM3Node(Custom(viewProps,
                  [progress, disabled](Rectangle r) {
                    PaintSlider(r, progress, disabled);
                  },
                  std::move(children)), component);
  }
  if (component == M3Component::RangeSlider) {
    float start = props.startProgress;
    float end = props.endProgress;
    bool disabled = props.disabled;
    return FinishM3Node(Custom(viewProps,
                  [start, end, disabled](Rectangle r) {
                    PaintRangeSlider(r, start, end, disabled);
                  },
                  std::move(children)), component);
  }
  if (component == M3Component::ProgressIndicator) {
    float progress = props.progress;
    bool indeterminate = props.indeterminate || props.progress < 0.0f;
    bool wavy = props.wavy;
    float wavelength = props.wavelength;
    NodePtr node = FinishM3Node(Custom(viewProps,
                  [progress, indeterminate, wavy, wavelength](Rectangle r) {
                    PaintLinearProgress(r, progress, indeterminate, wavy,
                                        wavelength);
                  },
                  std::move(children)), component);
    // Indeterminate/wavy paint is GetTime()-driven — keep the frame scheduler
    // rendering (Android renders on demand; see NodeNeedsAnotherFrame).
    if (node) node->alwaysAnimates = indeterminate || wavy;
    return node;
  }
  if (component == M3Component::LoadingIndicator) {
    float progress = props.progress;
    bool indeterminate = props.indeterminate || props.progress < 0.0f;
    bool wavy = props.wavy;
    float wavelength = props.wavelength;
    NodePtr node = FinishM3Node(Custom(viewProps,
                  [progress, indeterminate, wavy, wavelength](Rectangle r) {
                    PaintCircularLoading(r, progress, indeterminate, wavy,
                                         wavelength);
                  },
                  std::move(children)), component);
    if (node) node->alwaysAnimates = indeterminate || wavy;
    return node;
  }
  if (component == M3Component::Checkbox) {
    bool selected = isSelected;
    bool indeterminate = props.indeterminate;
    bool disabled = props.disabled;
    viewProps.style.backgroundColor = {};
    viewProps.style.borderColor = {};
    viewProps.style.borderWidth = {};
    return FinishM3Node(Custom(viewProps,
                  [selected, indeterminate, disabled](Rectangle r) {
                    PaintCheckboxControl(r, selected, indeterminate, disabled);
                  },
                  std::move(children)), component);
  }
  if (component == M3Component::RadioButton) {
    bool selected = isSelected;
    bool disabled = props.disabled;
    viewProps.style.backgroundColor = {};
    viewProps.style.borderColor = {};
    viewProps.style.borderWidth = {};
    return FinishM3Node(Custom(viewProps,
                  [selected, disabled](Rectangle r) {
                    PaintRadioControl(r, selected, disabled);
                  },
                  std::move(children)), component);
  }
  if (component == M3Component::Switch) {
    bool selected = isSelected;
    bool disabled = props.disabled;
    float progress = props.progress;
    viewProps.style.backgroundColor = {};
    viewProps.style.borderColor = {};
    viewProps.style.borderWidth = {};
    return FinishM3Node(Custom(viewProps,
                  [selected, disabled, progress](Rectangle r) {
                    PaintSwitchControl(r, selected, disabled, progress);
                  },
                  std::move(children)), component);
  }
  if (component == M3Component::BottomSheet) {
    bool disabled = props.disabled;
    NodePtr node = Custom(viewProps,
                          [disabled](Rectangle r) {
                            PaintBottomSheetHandle(r, disabled);
                          },
                          std::move(children));
    ApplyModalOverlayFlags(node, component);
    node->role = NodeRole::BottomSheet;
    return FinishM3Node(node, component);
  }
  if (component == M3Component::TextField) {
    bool disabled = props.disabled;
    return FinishM3Node(Custom(viewProps,
                  [disabled](Rectangle r) {
                    PaintTextFieldContainer(r, disabled);
                  },
                  std::move(children)), component);
  }

  // ?? ButtonGroup / SegmentedButton ??????????????????????????????????????????
  // Bridge calls MaterialComponent WITHOUT children (added later via
  // JS_appendChild). Use label presence to distinguish: container has no label,
  // items always carry a label prop.
  // ?? NavigationBar ??????????????????????????????????????????????????????????
  // Container (no label) owns a single sliding active-indicator pill that
  // moves between item icon centers (NodeRole::NavigationBar). Items paint
  // labels/icons only — no per-item selection fill.
  if (component == M3Component::NavigationBar) {
    if (props.label.empty()) {
      ViewProps containerProps;
      containerProps.id = props.id;
      containerProps.style = style;
      containerProps.motion = motion;
      containerProps.disabled = props.disabled;
      containerProps.zIndex = props.zIndex;
      NodePtr node = View(containerProps, std::move(children));
      node->role = NodeRole::NavigationBar;
      return FinishM3Node(node, component);
    }
  }

  // ?? Tabs ???????????????????????????????????????????????????????????????????
  // Like SegmentedButton: container (no label) lays items in a row and paints
  // the sliding bottom indicator (NodeRole::Tabs); items (label) are equal-width
  // centered-text cells with no fill. Only the container carries the Tabs role.
  if (component == M3Component::Tabs) {
    if (props.label.empty()) {
      style.flexDirection = FlexDirection::Row;
      style.alignItems = Align::Stretch;
      style.gap = 0.0f;
      ViewProps containerProps;
      containerProps.id = props.id;
      containerProps.style = style;
      containerProps.motion = motion;
      containerProps.disabled = props.disabled;
      containerProps.zIndex = props.zIndex;
      NodePtr node = View(containerProps, std::move(children));
      node->role = NodeRole::Tabs;
      return FinishM3Node(node, component);
    }
    // ITEM: equal-width centered cell, no fill (indicator is drawn by container).
    style.flexGrow = 1.0f;
    style.flexShrink = 1.0f;
    style.flexBasis = 0.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.gap = 8.0f;
    style.padding.horizontal = 16.0f;
    style.backgroundColor = {};
    viewProps.style = style;
    if (children.empty()) {
      TextProps textProps;
      textProps.style.text = style.text;
      children.push_back(Text(props.label, textProps));
    }
    NodePtr node = View(viewProps, std::move(children));
    node->selected = isSelected;
    return FinishM3Node(node, component);
  }
  if (component == M3Component::SegmentedButton) {
    if (props.label.empty()) {
      // CONTAINER: structural pill wrapper — no state layers, no fill.
      // Renderer (ButtonGroupContainer role) draws segment fills and border.
      style.flexDirection   = FlexDirection::Row;
      style.alignItems      = Align::Stretch;
      style.gap             = 0.0f;
      style.backgroundColor = {};
      style.stateLayerColor = {};
      ViewProps containerProps;
      containerProps.id       = props.id;
      containerProps.style    = style;
      containerProps.motion   = motion;
      containerProps.disabled = props.disabled;
      containerProps.zIndex   = props.zIndex;
      // No stateStyles, no onPress — items own interaction.
      NodePtr node = View(containerProps, std::move(children));
      node->selected = false;
      node->role = NodeRole::ButtonGroupContainer;
      return FinishM3Node(node, component);
    }
    // ITEM: renderer draws fill + state-layer via DrawSegmentShape.
    // No backgroundColor so DrawNodeBackground doesn't draw a rectangle fill.
    style.backgroundColor    = {};
    style.alignItems         = Align::Center;
    style.justifyContent     = Justify::Center;
    style.flexDirection      = FlexDirection::Row;
    style.gap                = 8.0f;
    style.flexGrow           = 1.0f;
    style.flexShrink         = 1.0f;
    style.padding.horizontal = 12.0f;
    viewProps.style = style;
  }

  // ?? ButtonGroup (connected) ????????????????????????????????????????????????
  // Row of individually fully-rounded buttons with 2dp gaps. Each item paints
  // its own fill via normal DrawNodeBackground (no shared outline/dividers).
  if (component == M3Component::ButtonGroup) {
    if (props.label.empty()) {
      style.flexDirection = FlexDirection::Row;
      style.alignItems = Align::Stretch;
      style.gap = 2.0f;
      style.backgroundColor = {};
      ViewProps containerProps;
      containerProps.id = props.id;
      containerProps.style = style;
      containerProps.motion = motion;
      containerProps.disabled = props.disabled;
      containerProps.zIndex = props.zIndex;
      NodePtr node = View(containerProps, std::move(children));
      node->role = NodeRole::ButtonGroupConnected;
      return FinishM3Node(node, component);
    }
    // ITEM: fully-rounded tonal button. Selected = secondaryContainer. Falls
    // through to the generic text-push + node creation (which handles ButtonGroup
    // and rebuilds cleanly on selection updates).
    const ColorScheme &cs = Theme::GetColorScheme();
    style.flexGrow = 1.0f;
    style.flexShrink = 1.0f;
    style.flexBasis = 0.0f;
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Center;
    style.justifyContent = Justify::Center;
    style.gap = 8.0f;
    style.padding.horizontal = 12.0f;
    style.borderRadius = tokens::kCornerFull;
    style.backgroundColor = isSelected ? cs.secondaryContainer : cs.surfaceContainerHigh;
    style.text.color = isSelected ? cs.onSecondaryContainer : cs.onSurfaceVariant;
    // Resting state must not set stateLayerColor — its alpha seeds animStateAlpha
    // and an opaque value floods the pill over the label. Hover/press layers come
    // from DefaultStateStylesFor via stateStyles.
    style.stateLayerColor = {};
    viewProps.style = style;
  }

  // ?? SplitButton ?????????????????????????????????????????????????????????????
  // Two connected segments: leading main-action button + trailing dropdown icon
  // button, sharing one pill (renderer NodeRole::SplitButton paints the fills).
  if (component == M3Component::SplitButton) {
    if (props.label.empty() && children.empty()) {
      // CONTAINER created without a label: build the two segments internally.
      style.flexDirection = FlexDirection::Row;
      style.alignItems = Align::Stretch;
      style.gap = 0.0f;
      style.backgroundColor = {};
      ViewProps containerProps;
      containerProps.id = props.id;
      containerProps.style = style;
      containerProps.motion = motion;
      containerProps.disabled = props.disabled;
      containerProps.zIndex = props.zIndex;
      NodePtr node = View(containerProps, std::move(children));
      node->role = NodeRole::SplitButton;
      return FinishM3Node(node, component);
    }
    // With a label: leading [text] + trailing [chevron] as two stretch children.
    style.flexDirection = FlexDirection::Row;
    style.alignItems = Align::Stretch;
    style.gap = 0.0f;
    style.backgroundColor = {};
    ViewProps containerProps;
    containerProps.id = props.id;
    containerProps.style = style;
    containerProps.motion = motion;
    containerProps.disabled = props.disabled;
    containerProps.zIndex = props.zIndex;
    containerProps.onPress = props.onPress;
    const ColorScheme &cs = Theme::GetColorScheme();
    // Leading main-action segment (grows).
    Style leadStyle;
    leadStyle.flexGrow = 1.0f;
    leadStyle.flexDirection = FlexDirection::Row;
    leadStyle.alignItems = Align::Center;
    leadStyle.justifyContent = Justify::Center;
    leadStyle.gap = 8.0f;
    leadStyle.padding.horizontal = tokens::kButtonHorizontalPadding;
    leadStyle.text = style.text;
    leadStyle.text.color = cs.onPrimary;
    TextProps leadText;
    leadText.style.text = leadStyle.text;
    ViewProps leadProps;
    leadProps.style = leadStyle;
    std::vector<NodePtr> leadChildren = std::move(children);
    leadChildren.push_back(Text(props.label, leadText));
    NodePtr lead = View(leadProps, std::move(leadChildren));
    // Trailing dropdown segment (fixed width, chevron icon).
    Style trailStyle;
    trailStyle.width = tokens::kSplitButtonTrailingWidth;
    trailStyle.alignItems = Align::Center;
    trailStyle.justifyContent = Justify::Center;
    Color chevron = cs.onPrimary;
    ViewProps trailProps;
    trailProps.style = trailStyle;
    NodePtr trail = Custom(trailProps, [chevron](Rectangle r) {
      raym3::v2::DrawMaterialIcon(0xe5cf, r, chevron, 24, true); // arrow_drop_down
    });
    std::vector<NodePtr> segs = {lead, trail};
    NodePtr node = View(containerProps, std::move(segs));
    node->role = NodeRole::SplitButton;
    return FinishM3Node(node, component);
  }

  if (component == M3Component::NavigationBarItem &&
      !props.label.empty()) {
    // Nav item composes [icon child] + [label]. React appends the icon after
    // native creation, so JS_appendChild preserves this label at the end.
    TextProps textProps;
    textProps.style.text = style.text;
    children.push_back(Text(props.label, textProps));
  } else if (component == M3Component::Badge && !props.label.empty() && children.empty()) {
    TextProps textProps;
    textProps.style.text = style.text;
    children.push_back(Text(props.label, textProps));
  } else if ((component == M3Component::SegmentedButton ||
              component == M3Component::ButtonGroup) &&
             !props.label.empty() && children.empty()) {
    // Item text: no explicit height so the parent's justifyContent:Center
    // vertically centers the auto-sized text node within the 40dp item cell.
    TextProps textProps;
    textProps.style.text = style.text;
    children.push_back(Text(props.label, textProps));
  } else if (!props.label.empty() && children.empty()) {
    TextProps textProps;
    textProps.style.width = style.width;
    textProps.style.height = style.height;
    textProps.style.text = style.text;
    children.push_back(Text(props.label, textProps));
  }

  // Selected segmented button items get a leading checkmark — injected AFTER
  // label so children is [Text], becomes [Checkmark, Text]. Same pattern as Chip.
  if (component == M3Component::SegmentedButton && isSelected) {
    ViewProps checkProps;
    checkProps.style.width  = 18.0f;
    checkProps.style.height = 18.0f;
    Color checkColor =
        style.text.color.value_or(Theme::GetColorScheme().onSecondaryContainer);
    auto check = Custom(checkProps, [checkColor](Rectangle r) {
      PaintCheckmark(r, checkColor);
    });
    children.insert(children.begin(), check);
  }

  // Selected chips show an 18dp leading checkmark (M3 filter/input chips).
  if (component == M3Component::Chip && isSelected) {
    ViewProps checkProps;
    checkProps.style.width = 18.0f;
    checkProps.style.height = 18.0f;
    Color checkColor =
        style.text.color.value_or(Theme::GetColorScheme().onSecondaryContainer);
    auto check = Custom(checkProps, [checkColor](Rectangle r) {
      PaintCheckmark(r, checkColor);
    });
    children.insert(children.begin(), check);
  }


  if (component == M3Component::Popover) {
    NodePtr node = View(viewProps, std::move(children));
    node->selected = isSelected;
    node->anchorId = props.anchorId;
    node->placement = props.placement;
    if (props.scrim) {
      node->hasScrim = true;
      node->capturesInput = true;
    }
    return FinishM3Node(node, component);
  }

  NodePtr node = View(viewProps, std::move(children));
  node->selected = isSelected;
  ApplyModalOverlayFlags(node, component);
  // Tabs (container + items) are handled and returned earlier; reaching here
  // means a non-Tabs component.
  if (component == M3Component::NavigationBarItem)
    node->role = NodeRole::NavItem;
  else if (component == M3Component::NavigationRail) {
    node->role = NodeRole::NavigationRail;
    node->selected = props.open;
  }
  return FinishM3Node(node, component);
}

} // namespace raym3::v2
