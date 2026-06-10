#include "raym3/v2/Ripple.h"

#include "raym3/v2/Input.h"
#include "raym3/v2/RenderContext.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace raym3::v2 {

namespace {

constexpr float kFadeInDuration = 0.075f;
constexpr float kExpandDuration = 0.225f;
constexpr float kFadeOutDuration = 0.375f;
constexpr float kInitialRadiusFraction = 0.30f;
constexpr float kEndRadiusExtra = 5.0f;



static float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

static float TargetRadiusForBounds(Rectangle bounds) {
  float d1 = std::sqrt(bounds.width * bounds.width + bounds.height * bounds.height);
  float d2 = std::sqrt(bounds.width * bounds.width + bounds.height * bounds.height);
  return std::max(d1, d2) * 0.5f + kEndRadiusExtra;
}

static Vector2 RippleCenter(const RippleInstance &r) {
  float ease = SmoothStep(r.growT);
  return {r.origin.x + (r.boundsCenter.x - r.origin.x) * ease,
          r.origin.y + (r.boundsCenter.y - r.origin.y) * ease};
}

static float RippleRadius(const RippleInstance &r) {
  float ease = SmoothStep(r.growT);
  return r.targetRadius * (kInitialRadiusFraction + (1.0f - kInitialRadiusFraction) * ease);
}

} // namespace

void SpawnRipple(NodeId owner, Vector2 origin, Rectangle bounds, Color inkColor) {
  if (owner == 0 || inkColor.a == 0)
    return;
  RippleInstance rip;
  rip.ownerId = owner;
  rip.origin = origin;
  rip.boundsCenter = {bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
  rip.targetRadius = TargetRadiusForBounds(bounds);
  rip.inkColor = inkColor;
  Ctx().ripples.push_back(rip);
}

void StartRippleFadeOut(NodeId owner) {
  for (RippleInstance &r : Ctx().ripples) {
    if (r.ownerId == owner && r.phase == RipplePhase::Growing)
      r.phase = RipplePhase::Fading;
  }
}

void CancelRipplesForNode(NodeId owner) {
  Ctx().ripples.erase(
      std::remove_if(Ctx().ripples.begin(), Ctx().ripples.end(),
                     [owner](const RippleInstance &r) { return r.ownerId == owner; }),
      Ctx().ripples.end());
}

bool HasActiveRipples() { return !Ctx().ripples.empty(); }

bool HasRipplesForNode(NodeId owner) {
  if (owner == 0)
    return false;
  for (const RippleInstance &r : Ctx().ripples) {
    if (r.ownerId == owner && r.displayAlpha > 0.001f)
      return true;
  }
  return false;
}

void TickRipples(float dt) {
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;

  for (RippleInstance &r : Ctx().ripples) {
    if (r.phase == RipplePhase::Growing) {
      r.growT = std::min(1.0f, r.growT + dt / kExpandDuration);
      float fadeIn = std::min(1.0f, (r.growT * kExpandDuration) / kFadeInDuration);
      r.displayAlpha = fadeIn;
    } else {
      r.fadeT += dt / kFadeOutDuration;
      r.displayAlpha = std::max(0.0f, 1.0f - r.fadeT);
    }
  }

  Ctx().ripples.erase(
      std::remove_if(Ctx().ripples.begin(), Ctx().ripples.end(),
                     [](const RippleInstance &r) {
                       return r.phase == RipplePhase::Fading && r.fadeT >= 1.0f;
                     }),
      Ctx().ripples.end());
}

void PaintRipplesForNode(const Node &node, Rectangle bounds, float cornerRadius) {
  (void)bounds;
  (void)cornerRadius;
  NodeId id = IdOf(&node);
  for (const RippleInstance &r : Ctx().ripples) {
    if (r.ownerId != id || r.displayAlpha <= 0.001f)
      continue;
    Vector2 center = RippleCenter(r);
    float radius = RippleRadius(r);
    if (radius <= 0.5f)
      continue;
    float inkA = (float)r.inkColor.a / 255.0f * r.displayAlpha;
    Color inner = ColorAlpha(Color{r.inkColor.r, r.inkColor.g, r.inkColor.b, 255}, inkA);
    Color outer = ColorAlpha(inner, 0.0f);
    DrawCircleGradient(center, radius, inner, outer);
  }
}

} // namespace raym3::v2
