#include "raym3/v2/Ripple.h"

#include "raym3/v2/Input.h"
#include "raym3/v2/RenderContext.h"
#include <algorithm>
#include <cmath>
#include <rlgl.h>
#include <vector>

namespace raym3::v2 {

namespace {

constexpr float kFadeInDuration = 0.075f;
constexpr float kExpandDuration = 0.225f;
constexpr float kFadeOutDuration = 0.375f;
constexpr float kInitialRadiusFraction = 0.30f;
constexpr float kEndRadiusExtra = 5.0f;
constexpr int kRippleRows = 24;
constexpr int kRippleColumns = 32;

struct RippleVertex {
  Vector2 position;
  Color color;
};

static Color RippleColorAt(Vector2 point, Vector2 center, float radius,
                           Color inner) {
  float dx = point.x - center.x;
  float dy = point.y - center.y;
  float distance = std::sqrt(dx * dx + dy * dy);
  float strength = std::clamp(1.0f - distance / std::max(0.0001f, radius),
                              0.0f, 1.0f);
  Color result = inner;
  result.a = static_cast<unsigned char>(
      std::round(static_cast<float>(inner.a) * strength));
  return result;
}

static void RoundedRowLimits(Rectangle bounds, float cornerRadius, float y,
                             float &left, float &right) {
  float radius = std::clamp(cornerRadius, 0.0f,
                            std::min(bounds.width, bounds.height) * 0.5f);
  left = bounds.x;
  right = bounds.x + bounds.width;
  if (radius <= 0.0f)
    return;

  float localY = std::clamp(y - bounds.y, 0.0f, bounds.height);
  float cornerCenterY = 0.0f;
  if (localY < radius)
    cornerCenterY = radius;
  else if (localY > bounds.height - radius)
    cornerCenterY = bounds.height - radius;
  else
    return;

  float dy = localY - cornerCenterY;
  float arcX = std::sqrt(std::max(0.0f, radius * radius - dy * dy));
  float inset = radius - arcX;
  left += inset;
  right -= inset;
}

static RippleVertex RippleGridVertex(Rectangle bounds, float cornerRadius,
                                     Vector2 center, float rippleRadius,
                                     Color inner, int row, int column) {
  float v = static_cast<float>(row) / static_cast<float>(kRippleRows);
  float y = bounds.y + bounds.height * v;
  float left = bounds.x;
  float right = bounds.x + bounds.width;
  RoundedRowLimits(bounds, cornerRadius, y, left, right);
  float u = static_cast<float>(column) /
            static_cast<float>(kRippleColumns);
  Vector2 point{left + (right - left) * u, y};
  return {point, RippleColorAt(point, center, rippleRadius, inner)};
}

static void EmitRippleVertex(const RippleVertex &vertex) {
  rlColor4ub(vertex.color.r, vertex.color.g, vertex.color.b, vertex.color.a);
  rlVertex2f(vertex.position.x, vertex.position.y);
}

static void DrawClippedRipple(Rectangle bounds, float cornerRadius,
                              Vector2 center, float radius, Color inner) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f || radius <= 0.0f)
    return;

  // The mesh itself has the component's rounded outline, so clipping is
  // identical on GL, Metal, Vulkan, and WebGPU and does not alter global
  // scissor/stencil state used by TextInput and scroll containers.
  rlBegin(RL_TRIANGLES);
  for (int row = 0; row < kRippleRows; ++row) {
    for (int column = 0; column < kRippleColumns; ++column) {
      RippleVertex p00 = RippleGridVertex(bounds, cornerRadius, center, radius,
                                          inner, row, column);
      RippleVertex p10 = RippleGridVertex(bounds, cornerRadius, center, radius,
                                          inner, row, column + 1);
      RippleVertex p01 = RippleGridVertex(bounds, cornerRadius, center, radius,
                                          inner, row + 1, column);
      RippleVertex p11 = RippleGridVertex(bounds, cornerRadius, center, radius,
                                          inner, row + 1, column + 1);
      EmitRippleVertex(p00);
      EmitRippleVertex(p10);
      EmitRippleVertex(p11);
      EmitRippleVertex(p00);
      EmitRippleVertex(p11);
      EmitRippleVertex(p01);
    }
  }
  rlEnd();
}

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
    DrawClippedRipple(bounds, cornerRadius, center, radius, inner);
  }
}

} // namespace raym3::v2
