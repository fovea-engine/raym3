#include "raym3/v2/Animations.h"
#include "raym3/v2/Transitions.h"
#include "raym3/v2/Input.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace raym3::v2 {

namespace {

// Binary-subdivision cubic-bezier solve (x → t → y). Same solver as
// Transitions.cpp / Renderer.cpp; kept local to avoid a cross-TU dependency.
float CubicBezier(float x1, float y1, float x2, float y2, float x) {
  x = std::clamp(x, 0.0f, 1.0f);
  float lo = 0.0f, hi = 1.0f;
  for (int i = 0; i < 12; ++i) {
    float t = (lo + hi) * 0.5f;
    float u = 1.0f - t;
    float bx = 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t;
    if (bx < x) lo = t; else hi = t;
  }
  float t = (lo + hi) * 0.5f;
  float u = 1.0f - t;
  return 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t;
}

// Registered @keyframes tracks, keyed by name. Populated by the CSS bridge.
std::map<std::string, std::vector<Keyframe>>& KeyframeRegistry() {
  static std::map<std::string, std::vector<Keyframe>> reg;
  return reg;
}

// Value of `prop` at a given progress p (0..1) across a sorted keyframe track.
// Interpolates between the surrounding stops that define `prop`; holds at the
// nearest defining edge outside their range. Returns false if no stop sets it.
bool SampleTrack(const std::vector<Keyframe>& frames, TransitionProperty prop,
                 float p, float& out) {
  const Keyframe* lo = nullptr;
  const Keyframe* hi = nullptr;
  float loVal = 0.0f, hiVal = 0.0f;
  for (const Keyframe& kf : frames) {
    float v;
    bool has = false;
    for (const auto& pv : kf.values) {
      if (pv.first == prop) { v = pv.second; has = true; break; }
    }
    if (!has) continue;
    if (kf.offset <= p) { lo = &kf; loVal = v; }
    if (kf.offset >= p && !hi) { hi = &kf; hiVal = v; }
  }
  if (!lo && !hi) return false;
  if (!lo) { out = hiVal; return true; }   // before first defining stop
  if (!hi) { out = loVal; return true; }   // after last defining stop
  if (hi->offset <= lo->offset) { out = hiVal; return true; }
  float f = (p - lo->offset) / (hi->offset - lo->offset);
  out = loVal + (hiVal - loVal) * f;
  return true;
}

} // namespace

void RegisterKeyframes(const std::string& name, std::vector<Keyframe> frames) {
  auto& reg = KeyframeRegistry();
  if (frames.empty()) { reg.erase(name); return; }
  std::sort(frames.begin(), frames.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.offset < b.offset; });
  reg[name] = std::move(frames);
}

const std::vector<Keyframe>* GetKeyframes(const std::string& name) {
  auto& reg = KeyframeRegistry();
  auto it = reg.find(name);
  return it == reg.end() ? nullptr : &it->second;
}

void ClearKeyframes() { KeyframeRegistry().clear(); }

// Start one animation from a spec entry (resolving its keyframes) unless it is
// already running on the node.
static void StartAnimation(const NodePtr& node, const AnimationEntry& e) {
  if (e.name.empty() || e.name == "none") return;
  for (const ActiveAnimation& a : node->activeAnimations)
    if (a.name == e.name) return; // already running — don't restart

  const std::vector<Keyframe>* frames = GetKeyframes(e.name);
  if (!frames || frames->empty()) return;

  ActiveAnimation a;
  a.name = e.name;
  a.durationMs = e.durationMs;
  a.delayMs = e.delayMs;
  a.iterationCount = e.iterationCount;
  a.direction = e.direction;
  a.fill = e.fill;
  a.x1 = e.x1; a.y1 = e.y1; a.x2 = e.x2; a.y2 = e.y2;
  a.keyframes = *frames;
  for (const Keyframe& kf : a.keyframes)
    for (const auto& pv : kf.values)
      if (std::find(a.animatedProps.begin(), a.animatedProps.end(), pv.first) ==
          a.animatedProps.end())
        a.animatedProps.push_back(pv.first);
  if (!a.animatedProps.empty()) node->activeAnimations.push_back(std::move(a));
}

void ApplyStyleWithAnimations(const NodePtr& node, const Style& target,
                              const Style& explicitProps) {
  if (!node) return;

  // Rule 1: this update explicitly (re)declared `animation` — it is the
  // authoritative set. Reconcile running animations to exactly these names.
  // Covers create, className carrying an animation, `animation: none`, and
  // switching animations. Re-declaring the same name doesn't restart it.
  if (explicitProps.animations) {
    const std::vector<AnimationEntry>& spec = *explicitProps.animations;
    if (spec.empty()) { node->activeAnimations.clear(); return; } // animation: none
    node->activeAnimations.erase(
        std::remove_if(node->activeAnimations.begin(), node->activeAnimations.end(),
                       [&](const ActiveAnimation& a) {
                         for (const AnimationEntry& e : spec)
                           if (e.name == a.name) return false;
                         return true;
                       }),
        node->activeAnimations.end());
    for (const AnimationEntry& e : spec) StartAnimation(node, e);
    return;
  }

  // Rule 2: no animation on the target at all (e.g. a className change to a
  // class that declares none). Mirror the transition engine: cancel + snap.
  if (!target.animations) { node->activeAnimations.clear(); return; }

  // Rule 3: animation inherited via merge (an inline style-only update). Leave
  // running animations alone, EXCEPT cancel any whose property this update
  // wrote explicitly — so a JS style override wins and we stop fighting it.
  // Cancellation is sticky: Rule 3 never restarts, so the animation stays
  // cancelled on subsequent inline commits.
  if (!node->activeAnimations.empty()) {
    node->activeAnimations.erase(
        std::remove_if(
            node->activeAnimations.begin(), node->activeAnimations.end(),
            [&](const ActiveAnimation& a) {
              for (TransitionProperty p : a.animatedProps)
                if (HasExplicitValue(explicitProps, p)) return true;
              return false;
            }),
        node->activeAnimations.end());
  }
}

namespace {

// Effective progress (0..1) within one iteration, applying direction. Returns
// the eased sample position; `done` set when a finite animation has ended.
float IterationProgress(const ActiveAnimation& a, float runMs, bool& done) {
  done = false;
  const float dur = a.durationMs > 0.0f ? a.durationMs : 1.0f;
  float iterF = runMs / dur;                 // fractional iteration count
  const bool infinite = a.iterationCount < 0.0f;
  if (!infinite && iterF >= a.iterationCount) {
    done = true;
    iterF = a.iterationCount;                 // clamp to the final position
  }
  long iterIndex = (long)std::floor(iterF);
  float local = iterF - (float)iterIndex;     // 0..1 within this iteration
  if (done) { local = 1.0f; iterIndex = std::max<long>(0, iterIndex - 1); }

  bool reverse = false;
  switch (a.direction) {
    case AnimationDirection::Normal:  reverse = false; break;
    case AnimationDirection::Reverse: reverse = true;  break;
    case AnimationDirection::Alternate:        reverse = (iterIndex & 1) != 0; break;
    case AnimationDirection::AlternateReverse: reverse = (iterIndex & 1) == 0; break;
  }
  if (reverse) local = 1.0f - local;
  return local;
}

void TickNodeAnimations(const NodePtr& node, float dtMs) {
  if (!node) return;
  for (ActiveAnimation& a : node->activeAnimations) {
    a.elapsedMs += dtMs;
    const float runMs = a.elapsedMs - a.delayMs;

    // Delay phase: apply the start keyframe only if fill-mode paints backwards.
    if (runMs < 0.0f) {
      if (a.fill == AnimationFill::Backwards || a.fill == AnimationFill::Both) {
        for (TransitionProperty p : a.animatedProps) {
          float v;
          if (SampleTrack(a.keyframes, p, 0.0f, v)) SetTransitionValue(node->style, p, v);
        }
      }
      continue;
    }

    bool done = false;
    const float local = IterationProgress(a, runMs, done);
    const float eased = CubicBezier(a.x1, a.y1, a.x2, a.y2, local);

    if (done && a.fill != AnimationFill::Forwards && a.fill != AnimationFill::Both) {
      // No forwards fill → leave properties at whatever the base style resolves
      // to (do not write); just mark finished so it's pruned.
      a.finished = true;
      continue;
    }

    for (TransitionProperty p : a.animatedProps) {
      float v;
      if (SampleTrack(a.keyframes, p, eased, v)) SetTransitionValue(node->style, p, v);
    }
    if (done) a.finished = true;
  }

  node->activeAnimations.erase(
      std::remove_if(node->activeAnimations.begin(), node->activeAnimations.end(),
                     [](const ActiveAnimation& a) { return a.finished; }),
      node->activeAnimations.end());

  for (const NodePtr& child : node->children) TickNodeAnimations(child, dtMs);
}

} // namespace

void TickAnimations(const NodePtr& root) {
  if (!root) return;
  const float dtMs = (float)FrameTimeMs();
  if (dtMs <= 0.0f) return;
  TickNodeAnimations(root, dtMs);
}

bool HasActiveAnimations(const NodePtr& root) {
  if (!root) return false;
  if (!root->activeAnimations.empty()) return true;
  for (const NodePtr& child : root->children)
    if (HasActiveAnimations(child)) return true;
  return false;
}

} // namespace raym3::v2
