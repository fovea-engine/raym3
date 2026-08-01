#include "raym3/v2/Gradient.h"

#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace raym3 {
namespace v2 {
namespace {

// Mesh density. Cells are kept near this edge length (in the same dp space the
// rest of the renderer draws in) and then capped, so a full-screen gradient does
// not explode into tens of thousands of triangles.
constexpr float kTargetCellPx = 3.0f;
constexpr int kMaxDivisions = 96;
// Cap along the axis the gradient actually ramps on. Higher than kMaxDivisions
// because that is the axis banding lives on; the cross axis stays cheap.
constexpr int kMaxRampDivisions = 512;
constexpr int kMinDivisions = 8;
constexpr int kMinConicSlices = 96;
constexpr int kMaxConicSlices = 512;

float Frac(float v) {
  float f = v - std::floor(v);
  return f < 0.0f ? f + 1.0f : f;
}

// Horizontal extent of the rounded rect at height `y`. Outside the corner bands
// this is the full width; inside one it follows the corner circle.
void RoundedRowLimits(const Rectangle &box, float r, float y, float &left,
                      float &right) {
  left = box.x;
  right = box.x + box.width;
  if (r <= 0.0f)
    return;
  float dy = 0.0f;
  if (y < box.y + r)
    dy = (box.y + r) - y;
  else if (y > box.y + box.height - r)
    dy = y - (box.y + box.height - r);
  else
    return;
  dy = std::min(dy, r);
  const float inset = r - std::sqrt(std::max(0.0f, r * r - dy * dy));
  left += inset;
  right -= inset;
}

// Distance from `origin` (inside the shape) to the rounded rect's boundary along
// unit direction `dir`. Used to build the conic fan's outer ring.
float RoundedRectExitDistance(const Rectangle &box, float r, Vector2 origin,
                              Vector2 dir) {
  const float left = box.x, top = box.y;
  const float right = box.x + box.width, bottom = box.y + box.height;
  float t = std::hypot(box.width, box.height); // upper bound
  if (dir.x > 1e-6f)
    t = std::min(t, (right - origin.x) / dir.x);
  else if (dir.x < -1e-6f)
    t = std::min(t, (left - origin.x) / dir.x);
  if (dir.y > 1e-6f)
    t = std::min(t, (bottom - origin.y) / dir.y);
  else if (dir.y < -1e-6f)
    t = std::min(t, (top - origin.y) / dir.y);
  if (r <= 0.0f)
    return std::max(0.0f, t);

  // If the straight-edge hit lands in a corner band, the real boundary there is
  // the corner arc, which is nearer.
  const Vector2 hit{origin.x + dir.x * t, origin.y + dir.y * t};
  Vector2 arc{0.0f, 0.0f};
  bool inCorner = true;
  if (hit.x < left + r && hit.y < top + r)
    arc = {left + r, top + r};
  else if (hit.x > right - r && hit.y < top + r)
    arc = {right - r, top + r};
  else if (hit.x > right - r && hit.y > bottom - r)
    arc = {right - r, bottom - r};
  else if (hit.x < left + r && hit.y > bottom - r)
    arc = {left + r, bottom - r};
  else
    inCorner = false;
  if (!inCorner)
    return std::max(0.0f, t);

  // |origin + t*dir - arc| = r, taking the far root (the exit).
  const float ox = origin.x - arc.x, oy = origin.y - arc.y;
  const float b = 2.0f * (ox * dir.x + oy * dir.y);
  const float c = ox * ox + oy * oy - r * r;
  const float disc = b * b - 4.0f * c;
  if (disc < 0.0f)
    return std::max(0.0f, t);
  const float tArc = (-b + std::sqrt(disc)) * 0.5f;
  return std::max(0.0f, std::min(t, tArc));
}

int DivisionCount(float extent) {
  const int n = (int)std::ceil(extent / kTargetCellPx);
  return std::clamp(n, kMinDivisions, kMaxDivisions);
}

// Divisions for one axis, weighted by how much that axis actually carries the
// gradient.
//
// The mesh used to be square: a full-screen `to bottom` wash got 96 columns it
// did not need (colour is constant along x) and only 96 rows where all the
// colour lives, i.e. 9dp bands on the one axis that matters. Spending the
// budget along the ramp instead gives ~1.7dp steps for *fewer* cells overall.
// weight = |component of the gradient direction on this axis|.
int DivisionCountWeighted(float extent, float weight) {
  if (extent <= 0.0f)
    return kMinDivisions;
  const float w = std::clamp(weight, 0.0f, 1.0f);
  if (w < 1e-3f)
    return kMinDivisions; // colour does not vary along this axis
  const int n = (int)std::ceil(extent * w / kTargetCellPx);
  return std::clamp(n, kMinDivisions, kMaxRampDivisions);
}

// Rounded corners are tessellated by rows (each row clamps to the arc's
// horizontal limits), so a gradient that needs few rows for colour still needs
// enough of them to keep the corner from going polygonal.
int CornerRowFloor(float radius) {
  if (radius <= 0.5f)
    return kMinDivisions;
  return std::clamp((int)std::ceil(2.0f * radius / kTargetCellPx),
                    kMinDivisions, 128);
}

// Division coordinates spanning [from,to], with `pinned` values forced to land
// exactly on a boundary — that is what keeps a hard stop (`red 50%, blue 50%`)
// a straight line instead of a one-cell smear.
std::vector<float> BuildDivisionsN(float from, float to, int n,
                                   const std::vector<float> &pinned) {
  std::vector<float> out;
  const float extent = to - from;
  n = std::max(n, 1);
  out.reserve((size_t)n + pinned.size() + 2);
  for (int i = 0; i <= n; ++i)
    out.push_back(from + extent * ((float)i / (float)n));
  for (float p : pinned) {
    if (p > from && p < to)
      out.push_back(p);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end(),
                        [](float a, float b) { return std::fabs(a - b) < 1e-4f; }),
            out.end());
  return out;
}

std::vector<float> BuildDivisions(float from, float to,
                                  const std::vector<float> &pinned) {
  return BuildDivisionsN(from, to, DivisionCount(to - from), pinned);
}

void EmitVertex(Vector2 p, Color c) {
  rlColor4ub(c.r, c.g, c.b, c.a);
  rlVertex2f(p.x, p.y);
}

Color WithOpacity(Color c, float opacity) {
  const float a = std::clamp((float)c.a * opacity, 0.0f, 255.0f);
  return Color{c.r, c.g, c.b, (unsigned char)std::lround(a)};
}

} // namespace

void NormalizeGradientStops(LinearGradient &gradient) {
  auto &stops = gradient.stops;
  if (stops.empty())
    return;
  // css-images-3 §3.4.3: the first and last stop default to 0% and 100%, a run of
  // unpositioned stops is spread evenly between its pinned neighbours, and a
  // position is never allowed to decrease.
  if (!stops.front().hasPosition) {
    stops.front().position = 0.0f;
    stops.front().hasPosition = true;
  }
  if (!stops.back().hasPosition) {
    stops.back().position = 1.0f;
    stops.back().hasPosition = true;
  }
  float previous = stops.front().position;
  for (auto &stop : stops) {
    if (!stop.hasPosition)
      continue;
    stop.position = std::max(stop.position, previous);
    previous = stop.position;
  }
  size_t i = 0;
  while (i < stops.size()) {
    if (stops[i].hasPosition) {
      ++i;
      continue;
    }
    size_t runEnd = i;
    while (runEnd < stops.size() && !stops[runEnd].hasPosition)
      ++runEnd;
    const float before = stops[i - 1].position;
    const float after = runEnd < stops.size() ? stops[runEnd].position : 1.0f;
    const size_t gaps = runEnd - i + 1;
    for (size_t k = i; k < runEnd; ++k) {
      const float f = (float)(k - i + 1) / (float)gaps;
      stops[k].position = before + (after - before) * f;
      stops[k].hasPosition = true;
    }
    i = runEnd;
  }
}

// Float-precision sample (channels 0..255): the shared core of the exact and
// dithered variants below.
static void SampleGradientF(const LinearGradient &gradient, float t,
                            float out[4]) {
  const auto &stops = gradient.stops;
  if (stops.empty()) {
    out[0] = out[1] = out[2] = out[3] = 0.0f;
    return;
  }
  auto exact = [&](const Color &c) {
    out[0] = c.r; out[1] = c.g; out[2] = c.b; out[3] = c.a;
  };
  if (stops.size() == 1)
    return exact(stops.front().color);
  t = std::clamp(t, 0.0f, 1.0f);
  if (t <= stops.front().position)
    return exact(stops.front().color);
  if (t >= stops.back().position)
    return exact(stops.back().color);
  size_t i = 1;
  while (i < stops.size() && stops[i].position < t)
    ++i;
  const LinearGradientStop &a = stops[i - 1];
  const LinearGradientStop &b = stops[i];
  const float span = b.position - a.position;
  const float f = span <= 1e-6f ? 1.0f : (t - a.position) / span;

  // Premultiplied interpolation, as browsers do: lerping straight RGB would drag
  // a fade-to-`transparent` through that colour's black, greying the ramp.
  const float aa = (float)a.color.a / 255.0f, ba = (float)b.color.a / 255.0f;
  const float outA = aa + (ba - aa) * f;
  auto channel = [&](unsigned char ca, unsigned char cb) {
    const float pa = (float)ca * aa;
    const float pb = (float)cb * ba;
    const float p = pa + (pb - pa) * f;
    const float v = outA <= 1e-6f ? 0.0f : p / outA;
    return std::clamp(v, 0.0f, 255.0f);
  };
  out[0] = channel(a.color.r, b.color.r);
  out[1] = channel(a.color.g, b.color.g);
  out[2] = channel(a.color.b, b.color.b);
  out[3] = std::clamp(outA * 255.0f, 0.0f, 255.0f);
}

Color SampleGradient(const LinearGradient &gradient, float t) {
  float v[4];
  SampleGradientF(gradient, t, v);
  return Color{(unsigned char)std::lround(v[0]), (unsigned char)std::lround(v[1]),
               (unsigned char)std::lround(v[2]), (unsigned char)std::lround(v[3])};
}

namespace {

// CSS gradient line: 0deg points up, angles run clockwise. In screen space (y
// down) that is (sin, -cos). The line is centred on the box and its length is
// the projection of the box onto it, so 0 and 1 land on the box's extremes.
struct LinearAxis {
  Vector2 center;
  Vector2 dir;
  float length;
};

LinearAxis MakeLinearAxis(const Rectangle &box, float angleDegrees) {
  const float rad = angleDegrees * (float)M_PI / 180.0f;
  const float s = std::sin(rad), c = std::cos(rad);
  const float length =
      std::fabs(box.width * s) + std::fabs(box.height * c);
  return {{box.x + box.width * 0.5f, box.y + box.height * 0.5f},
          {s, -c},
          length <= 1e-4f ? 1.0f : length};
}

float LinearT(const LinearAxis &axis, Vector2 p) {
  const float d = (p.x - axis.center.x) * axis.dir.x +
                  (p.y - axis.center.y) * axis.dir.y;
  return std::clamp(d / axis.length + 0.5f, 0.0f, 1.0f);
}

// Conic angle at `p`, measured like CSS: 0 at straight up, increasing clockwise.
float ConicT(Vector2 center, float fromDegrees, Vector2 p) {
  const float deg =
      std::atan2(p.x - center.x, -(p.y - center.y)) * 180.0f / (float)M_PI;
  return Frac((deg - fromDegrees) / 360.0f);
}

// Positions along one axis where a stop boundary falls, for pinning mesh rows or
// columns to hard stops. Only meaningful when the gradient runs along that axis.
std::vector<float> StopPositionsAlong(const LinearGradient &gradient, float from,
                                      float to, bool reverse) {
  std::vector<float> out;
  out.reserve(gradient.stops.size());
  for (const auto &stop : gradient.stops) {
    const float f = reverse ? 1.0f - stop.position : stop.position;
    out.push_back(from + (to - from) * f);
  }
  return out;
}

template <typename Sampler>
void DrawRoundedMesh(const Rectangle &box, float r, const std::vector<float> &xs,
                     const std::vector<float> &ys, const Sampler &sample) {
  if (xs.size() < 2 || ys.size() < 2)
    return;
  std::vector<float> lefts(ys.size()), rights(ys.size());
  for (size_t i = 0; i < ys.size(); ++i)
    RoundedRowLimits(box, r, ys[i], lefts[i], rights[i]);

  rlBegin(RL_TRIANGLES);
  for (size_t row = 0; row + 1 < ys.size(); ++row) {
    const float y0 = ys[row], y1 = ys[row + 1];
    const float l0 = lefts[row], r0 = rights[row];
    const float l1 = lefts[row + 1], r1 = rights[row + 1];
    for (size_t col = 0; col + 1 < xs.size(); ++col) {
      // Clamping (rather than stretching) the cell into the row's extent keeps
      // column positions identical from row to row, so a vertical stop boundary
      // stays straight instead of bowing inward near the corners. Cells wholly
      // outside collapse to zero area and are skipped.
      const float x0 = xs[col], x1 = xs[col + 1];
      const float ax = std::clamp(x0, l0, r0), bx = std::clamp(x1, l0, r0);
      const float dx = std::clamp(x0, l1, r1), cx = std::clamp(x1, l1, r1);
      if (bx - ax <= 1e-4f && cx - dx <= 1e-4f)
        continue;
      const Vector2 a{ax, y0}, b{bx, y0}, c{cx, y1}, d{dx, y1};
      // Sample each corner a hair INSIDE its own cell. A cell boundary pinned to a
      // hard stop (`red 50%, blue 50%`) is shared by two cells; sampling the shared
      // position would give both the same colour and smear the stop across a whole
      // cell. The offset is ~0.1% of a cell, far below a pixel.
      const Vector2 mid{(ax + bx + cx + dx) * 0.25f, (y0 + y1) * 0.5f};
      auto inset = [&](Vector2 p) {
        return Vector2{p.x + (mid.x - p.x) * 1e-3f, p.y + (mid.y - p.y) * 1e-3f};
      };
      const Color ca = sample(inset(a)), cb = sample(inset(b));
      const Color cc = sample(inset(c)), cd = sample(inset(d));
      EmitVertex(a, ca);
      EmitVertex(b, cb);
      EmitVertex(c, cc);
      EmitVertex(a, ca);
      EmitVertex(c, cc);
      EmitVertex(d, cd);
    }
  }
  rlEnd();
}

void DrawConicFan(const Rectangle &box, float r, const LinearGradient &gradient,
                  Vector2 center, float opacity) {
  // Slice boundaries: every stop angle is pinned so a hard stop stays a crisp
  // radial edge, and the spans between them are subdivided for smoothness.
  std::vector<float> angles;
  angles.reserve(gradient.stops.size() + 2);
  angles.push_back(0.0f);
  for (const auto &stop : gradient.stops)
    angles.push_back(std::clamp(stop.position, 0.0f, 1.0f) * 360.0f);
  angles.push_back(360.0f);
  std::sort(angles.begin(), angles.end());
  angles.erase(std::unique(angles.begin(), angles.end(),
                           [](float a, float b) { return std::fabs(a - b) < 1e-3f; }),
               angles.end());

  const float perimeter = 2.0f * (box.width + box.height);
  const int budget = std::clamp((int)(perimeter / kTargetCellPx), kMinConicSlices,
                                kMaxConicSlices);
  const float sliceDeg = 360.0f / (float)budget;

  auto pointAt = [&](float deg) {
    const float rad = (deg + gradient.angleDegrees) * (float)M_PI / 180.0f;
    const Vector2 dir{std::sin(rad), -std::cos(rad)};
    const float t = RoundedRectExitDistance(box, r, center, dir);
    return Vector2{center.x + dir.x * t, center.y + dir.y * t};
  };

  rlBegin(RL_TRIANGLES);
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    const float a0 = angles[i], a1 = angles[i + 1];
    const int steps = std::max(1, (int)std::ceil((a1 - a0) / sliceDeg));
    for (int s = 0; s < steps; ++s) {
      const float s0 = a0 + (a1 - a0) * ((float)s / (float)steps);
      const float s1 = a0 + (a1 - a0) * ((float)(s + 1) / (float)steps);
      // Same reason as the mesh: sample inside the slice so a stop pinned to a
      // slice boundary stays a crisp radial edge.
      const float eps = (s1 - s0) * 1e-3f;
      const Color c0 =
          WithOpacity(SampleGradient(gradient, (s0 + eps) / 360.0f), opacity);
      const Color c1 =
          WithOpacity(SampleGradient(gradient, (s1 - eps) / 360.0f), opacity);
      // The centre is a singularity: give each slice its own centre colour so the
      // discontinuity stays confined to that one point rather than bleeding a
      // single averaged colour outward.
      const Color cm = WithOpacity(
          SampleGradient(gradient, (s0 + s1) * 0.5f / 360.0f), opacity);
      EmitVertex(center, cm);
      EmitVertex(pointAt(s0), c0);
      EmitVertex(pointAt(s1), c1);
    }
  }
  rlEnd();
}

} // namespace

void DrawGradientBorderArea(Rectangle outer, float outerRadius, Rectangle inner,
                            float innerRadius, const LinearGradient &gradient,
                            float opacity, const Rectangle *paintBox) {
  if (outer.width <= 0.0f || outer.height <= 0.0f || gradient.stops.empty())
    return;
  const Rectangle origin = paintBox ? *paintBox : outer;
  const float ro = std::max(
      0.0f, std::min(outerRadius, std::min(outer.width, outer.height) * 0.5f));
  const float ri =
      inner.width <= 0.0f || inner.height <= 0.0f
          ? 0.0f
          : std::max(0.0f, std::min(innerRadius,
                                    std::min(inner.width, inner.height) * 0.5f));
  const bool hasHole = inner.width > 0.0f && inner.height > 0.0f;

  const Vector2 center{outer.x + outer.width * 0.5f,
                       outer.y + outer.height * 0.5f};
  const LinearAxis axis = MakeLinearAxis(origin, gradient.angleDegrees);
  const Vector2 conicCenter{origin.x + origin.width * gradient.centerX,
                            origin.y + origin.height * gradient.centerY};
  auto sample = [&](Vector2 p) {
    const float t = gradient.kind == GradientKind::Conic
                        ? ConicT(conicCenter, gradient.angleDegrees, p)
                        : LinearT(axis, p);
    return WithOpacity(SampleGradient(gradient, t), opacity);
  };

  // Ring boundaries are sampled along rays from the centre. Stop angles are
  // pinned so a hard stop stays a crisp edge across the ring.
  std::vector<float> pinned;
  if (gradient.kind == GradientKind::Conic) {
    for (const auto &stop : gradient.stops)
      pinned.push_back(Frac(gradient.angleDegrees / 360.0f + stop.position) * 360.0f);
    std::sort(pinned.begin(), pinned.end());
  }
  const float perimeter = 2.0f * (outer.width + outer.height);
  const int steps = std::clamp((int)(perimeter / kTargetCellPx), kMinConicSlices,
                               kMaxConicSlices);

  std::vector<float> angles;
  angles.reserve((size_t)steps + pinned.size() + 1);
  for (int i = 0; i <= steps; ++i)
    angles.push_back(360.0f * ((float)i / (float)steps));
  for (float p : pinned) {
    if (p > 0.0f && p < 360.0f)
      angles.push_back(p);
  }
  std::sort(angles.begin(), angles.end());
  angles.erase(std::unique(angles.begin(), angles.end(),
                           [](float a, float b) { return std::fabs(a - b) < 1e-3f; }),
               angles.end());

  auto edgePair = [&](float deg, Vector2 &outPt, Vector2 &inPt) {
    const float rad = deg * (float)M_PI / 180.0f;
    const Vector2 dir{std::sin(rad), -std::cos(rad)};
    const float to = RoundedRectExitDistance(outer, ro, center, dir);
    outPt = {center.x + dir.x * to, center.y + dir.y * to};
    if (!hasHole) {
      inPt = center;
      return;
    }
    const float ti = RoundedRectExitDistance(inner, ri, center, dir);
    inPt = {center.x + dir.x * std::min(ti, to), center.y + dir.y * std::min(ti, to)};
  };

  rlBegin(RL_TRIANGLES);
  for (size_t i = 0; i + 1 < angles.size(); ++i) {
    Vector2 o0, i0, o1, i1;
    edgePair(angles[i], o0, i0);
    edgePair(angles[i + 1], o1, i1);
    // Colours come from just inside the wedge (see the mesh), so a stop pinned to
    // this boundary does not bleed into the neighbouring quad.
    const float eps = (angles[i + 1] - angles[i]) * 1e-3f;
    Vector2 so0, si0, so1, si1;
    edgePair(angles[i] + eps, so0, si0);
    edgePair(angles[i + 1] - eps, so1, si1);
    const Color co0 = sample(so0), co1 = sample(so1);
    const Color ci0 = sample(si0), ci1 = sample(si1);
    EmitVertex(o0, co0);
    EmitVertex(o1, co1);
    EmitVertex(i1, ci1);
    EmitVertex(o0, co0);
    EmitVertex(i1, ci1);
    EmitVertex(i0, ci0);
  }
  rlEnd();
}

void DrawGradientRoundedRect(Rectangle box, float cornerRadius,
                             const LinearGradient &gradient, float opacity,
                             const Rectangle *paintBox) {
  if (box.width <= 0.0f || box.height <= 0.0f || gradient.stops.empty())
    return;
  const float r = std::max(
      0.0f, std::min(cornerRadius, std::min(box.width, box.height) * 0.5f));
  const Rectangle origin = paintBox ? *paintBox : box;

  if (gradient.kind == GradientKind::Conic) {
    const Vector2 center{origin.x + origin.width * gradient.centerX,
                         origin.y + origin.height * gradient.centerY};
    const bool inside = center.x > box.x && center.x < box.x + box.width &&
                        center.y > box.y && center.y < box.y + box.height;
    if (inside) {
      DrawConicFan(box, r, gradient, center, opacity);
      return;
    }
    // Centre outside the box: the fan's rays would not span the shape, so fall
    // back to the cartesian mesh. The singularity is off-shape anyway.
    const std::vector<float> xs = BuildDivisions(box.x, box.x + box.width, {});
    const std::vector<float> ys = BuildDivisions(box.y, box.y + box.height, {});
    DrawRoundedMesh(box, r, xs, ys, [&](Vector2 p) {
      return WithOpacity(
          SampleGradient(gradient, ConicT(center, gradient.angleDegrees, p)),
          opacity);
    });
    return;
  }

  const LinearAxis axis = MakeLinearAxis(origin, gradient.angleDegrees);
  // Pin mesh lines to the stops when the gradient runs along an axis (the common
  // case: `to bottom`, `90deg`, …). At other angles the stops cannot align with a
  // rectangular mesh, so a hard stop softens over roughly one cell.
  const float angle = Frac(gradient.angleDegrees / 360.0f) * 360.0f;
  const bool vertical = std::fabs(angle - 0.0f) < 0.5f || std::fabs(angle - 180.0f) < 0.5f;
  const bool horizontal = std::fabs(angle - 90.0f) < 0.5f || std::fabs(angle - 270.0f) < 0.5f;
  std::vector<float> pinnedX, pinnedY;
  if (vertical)
    pinnedY = StopPositionsAlong(gradient, origin.y, origin.y + origin.height,
                                 std::fabs(angle - 0.0f) < 0.5f);
  else if (horizontal)
    pinnedX = StopPositionsAlong(gradient, origin.x, origin.x + origin.width,
                                 std::fabs(angle - 270.0f) < 0.5f);
  // Direction components decide where the vertex budget goes: a `to bottom`
  // wash spends it all on rows, a diagonal splits it.
  const float rad = angle * (float)M_PI / 180.0f;
  const float wx = std::fabs(std::sin(rad));
  const float wy = std::fabs(std::cos(rad));
  const int nx = DivisionCountWeighted(box.width, wx);
  const int ny = std::max(DivisionCountWeighted(box.height, wy), CornerRowFloor(r));
  const std::vector<float> xs =
      BuildDivisionsN(box.x, box.x + box.width, nx, pinnedX);
  const std::vector<float> ys =
      BuildDivisionsN(box.y, box.y + box.height, ny, pinnedY);
  DrawRoundedMesh(box, r, xs, ys, [&](Vector2 p) {
    return WithOpacity(SampleGradient(gradient, LinearT(axis, p)), opacity);
  });
}

} // namespace v2
} // namespace raym3
