#include "raym3/v2/Transitions.h"
#include "raym3/v2/Input.h"

#include <algorithm>
#include <cmath>

namespace raym3::v2 {

namespace {

constexpr float kRetargetEpsilon = 0.01f;

// Binary-subdivision cubic-bezier solve (x → t → y). Local copy of the
// file-static solver in Renderer.cpp.
float CubicBezier(float x1, float y1, float x2, float y2, float x) {
  x = std::clamp(x, 0.0f, 1.0f);
  float lo = 0.0f;
  float hi = 1.0f;
  for (int i = 0; i < 12; ++i) {
    float t = (lo + hi) * 0.5f;
    float u = 1.0f - t;
    float bx = 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t;
    if (bx < x)
      lo = t;
    else
      hi = t;
  }
  float t = (lo + hi) * 0.5f;
  float u = 1.0f - t;
  return 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t;
}

} // namespace

bool GetTransitionValue(const Style &style, TransitionProperty prop,
                        float &out) {
  switch (prop) {
  case TransitionProperty::MarginTop:    out = style.margin.Top();    return true;
  case TransitionProperty::MarginRight:  out = style.margin.Right();  return true;
  case TransitionProperty::MarginBottom: out = style.margin.Bottom(); return true;
  case TransitionProperty::MarginLeft:   out = style.margin.Left();   return true;
  case TransitionProperty::InsetTop:     out = style.inset.Top();     return true;
  case TransitionProperty::InsetRight:   out = style.inset.Right();   return true;
  case TransitionProperty::InsetBottom:  out = style.inset.Bottom();  return true;
  case TransitionProperty::InsetLeft:    out = style.inset.Left();    return true;
  case TransitionProperty::Opacity:    out = style.opacity.value_or(1.0f);    return true;
  case TransitionProperty::TranslateX: out = style.translateX.value_or(0.0f); return true;
  case TransitionProperty::TranslateY: out = style.translateY.value_or(0.0f); return true;
  case TransitionProperty::Scale:      out = style.scale.value_or(1.0f);      return true;
  case TransitionProperty::Rotation:   out = style.rotation.value_or(0.0f);   return true;
  // No animating from "auto": width/height are only animatable when set.
  case TransitionProperty::Width:
    if (!style.width) return false;
    out = *style.width;
    return true;
  case TransitionProperty::Height:
    if (!style.height) return false;
    out = *style.height;
    return true;
  case TransitionProperty::Count:
    break;
  }
  return false;
}

void SetTransitionValue(Style &style, TransitionProperty prop, float value) {
  switch (prop) {
  case TransitionProperty::MarginTop:
    style.margin.top = value;
    style.margin.topAuto = false;
    break;
  case TransitionProperty::MarginRight:
    style.margin.right = value;
    style.margin.rightAuto = false;
    break;
  case TransitionProperty::MarginBottom:
    style.margin.bottom = value;
    style.margin.bottomAuto = false;
    break;
  case TransitionProperty::MarginLeft:
    style.margin.left = value;
    style.margin.leftAuto = false;
    break;
  case TransitionProperty::InsetTop:    style.inset.top = value;    break;
  case TransitionProperty::InsetRight:  style.inset.right = value;  break;
  case TransitionProperty::InsetBottom: style.inset.bottom = value; break;
  case TransitionProperty::InsetLeft:   style.inset.left = value;   break;
  case TransitionProperty::Opacity:    style.opacity = value;    break;
  case TransitionProperty::TranslateX: style.translateX = value; break;
  case TransitionProperty::TranslateY: style.translateY = value; break;
  case TransitionProperty::Scale:      style.scale = value;      break;
  case TransitionProperty::Rotation:   style.rotation = value;   break;
  case TransitionProperty::Width:      style.width = value;      break;
  case TransitionProperty::Height:     style.height = value;     break;
  case TransitionProperty::Count:
    break;
  }
}

bool HasExplicitValue(const Style &style, TransitionProperty prop) {
  switch (prop) {
  case TransitionProperty::MarginTop:
    return style.margin.top || style.margin.vertical || style.margin.all;
  case TransitionProperty::MarginRight:
    return style.margin.right || style.margin.horizontal || style.margin.all;
  case TransitionProperty::MarginBottom:
    return style.margin.bottom || style.margin.vertical || style.margin.all;
  case TransitionProperty::MarginLeft:
    return style.margin.left || style.margin.horizontal || style.margin.all;
  case TransitionProperty::InsetTop:
    return style.inset.top || style.inset.vertical || style.inset.all;
  case TransitionProperty::InsetRight:
    return style.inset.right || style.inset.horizontal || style.inset.all;
  case TransitionProperty::InsetBottom:
    return style.inset.bottom || style.inset.vertical || style.inset.all;
  case TransitionProperty::InsetLeft:
    return style.inset.left || style.inset.horizontal || style.inset.all;
  case TransitionProperty::Opacity:    return style.opacity.has_value();
  case TransitionProperty::TranslateX: return style.translateX.has_value();
  case TransitionProperty::TranslateY: return style.translateY.has_value();
  case TransitionProperty::Scale:      return style.scale.has_value();
  case TransitionProperty::Rotation:   return style.rotation.has_value();
  case TransitionProperty::Width:      return style.width.has_value();
  case TransitionProperty::Height:     return style.height.has_value();
  case TransitionProperty::Count:
    break;
  }
  return false;
}

void ApplyStyleWithTransitions(const NodePtr &node, const Style &target,
                               const Style &explicitProps) {
  if (!node) return;

  // No spec, or explicit `transition: none` → snap and cancel in-flight.
  if (!target.transitions || target.transitions->empty()) {
    node->activeTransitions.clear();
    node->style = target;
    return;
  }

  const std::vector<TransitionEntry> &spec = *target.transitions;

  // Snapshot current effective values (possibly mid-interpolation) before the
  // target overwrites them.
  float current[(size_t)TransitionProperty::Count];
  bool currentValid[(size_t)TransitionProperty::Count] = {false};
  for (const TransitionEntry &e : spec) {
    if (e.property == TransitionProperty::Count) continue;
    currentValid[(size_t)e.property] =
        GetTransitionValue(node->style, e.property, current[(size_t)e.property]);
  }

  node->style = target; // carries the spec along

  // Drop in-flight transitions whose property is no longer in the spec —
  // the assignment above already snapped them to target.
  node->activeTransitions.erase(
      std::remove_if(node->activeTransitions.begin(),
                     node->activeTransitions.end(),
                     [&](const ActiveTransition &a) {
                       for (const TransitionEntry &e : spec)
                         if (e.property == a.property) return false;
                       return true;
                     }),
      node->activeTransitions.end());

  for (const TransitionEntry &e : spec) {
    if (e.property == TransitionProperty::Count) continue;
    const size_t pi = (size_t)e.property;

    // Properties not explicitly set this update keep any in-flight
    // transition untouched; restore the interpolated value the assignment
    // may have overwritten (merge usually preserved it, but be exact).
    ActiveTransition *inflight = nullptr;
    for (ActiveTransition &a : node->activeTransitions)
      if (a.property == e.property) { inflight = &a; break; }

    if (!HasExplicitValue(explicitProps, e.property)) {
      if (inflight && currentValid[pi])
        SetTransitionValue(node->style, e.property, current[pi]);
      continue;
    }

    float targetValue;
    if (!GetTransitionValue(target, e.property, targetValue)) {
      // Target unset (width/height "auto") → snap; cancel in-flight.
      if (inflight)
        node->activeTransitions.erase(
            node->activeTransitions.begin() +
            (inflight - node->activeTransitions.data()));
      continue;
    }

    if (inflight) {
      if (std::fabs(inflight->to - targetValue) <= kRetargetEpsilon) {
        // Same destination — keep running; restore interpolated value.
        if (currentValid[pi])
          SetTransitionValue(node->style, e.property, current[pi]);
        continue;
      }
      if (!currentValid[pi]) {
        // Cannot read a starting value — snap.
        node->activeTransitions.erase(
            node->activeTransitions.begin() +
            (inflight - node->activeTransitions.data()));
        continue;
      }
      // Retarget: restart from the current interpolated value.
      inflight->from = current[pi];
      inflight->to = targetValue;
      inflight->durationMs = e.durationMs;
      inflight->delayMs = e.delayMs;
      inflight->elapsedMs = 0.0f;
      inflight->x1 = e.x1; inflight->y1 = e.y1;
      inflight->x2 = e.x2; inflight->y2 = e.y2;
      SetTransitionValue(node->style, e.property, current[pi]);
      continue;
    }

    // No in-flight transition: start one only when the value actually moves
    // and the timing is animatable.
    if (!currentValid[pi]) continue; // animating from "auto" unsupported
    if (e.durationMs + e.delayMs <= 0.0f) continue; // zero time → instant
    if (std::fabs(targetValue - current[pi]) <= kRetargetEpsilon) continue;

    ActiveTransition a;
    a.property = e.property;
    a.from = current[pi];
    a.to = targetValue;
    a.durationMs = e.durationMs;
    a.delayMs = e.delayMs;
    a.x1 = e.x1; a.y1 = e.y1; a.x2 = e.x2; a.y2 = e.y2;
    node->activeTransitions.push_back(a);
    // Hold the current value this frame so layout doesn't snap to target.
    SetTransitionValue(node->style, e.property, current[pi]);
  }
}

static void TickNodeTransitions(const NodePtr &node, float dtMs) {
  if (!node) return;
  if (!node->activeTransitions.empty()) {
    for (ActiveTransition &a : node->activeTransitions) {
      a.elapsedMs += dtMs;
      const float runMs = a.elapsedMs - a.delayMs;
      if (runMs < 0.0f) {
        SetTransitionValue(node->style, a.property, a.from);
        continue;
      }
      const float t = a.durationMs > 0.0f
                          ? std::clamp(runMs / a.durationMs, 0.0f, 1.0f)
                          : 1.0f;
      const float eased = CubicBezier(a.x1, a.y1, a.x2, a.y2, t);
      SetTransitionValue(node->style, a.property,
                         a.from + (a.to - a.from) * eased);
    }
    node->activeTransitions.erase(
        std::remove_if(node->activeTransitions.begin(),
                       node->activeTransitions.end(),
                       [&](const ActiveTransition &a) {
                         if (a.elapsedMs - a.delayMs < a.durationMs)
                           return false;
                         SetTransitionValue(node->style, a.property, a.to);
                         return true;
                       }),
        node->activeTransitions.end());
  }
  for (const NodePtr &child : node->children)
    TickNodeTransitions(child, dtMs);
}

void TickTransitions(const NodePtr &root) {
  if (!root) return;
  const float dtMs = (float)FrameTimeMs();
  if (dtMs <= 0.0f) return;
  TickNodeTransitions(root, dtMs);
}

} // namespace raym3::v2
