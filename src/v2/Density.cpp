#include "raym3/v2/Density.h"
#include "raym3/v2/RenderContext.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace raym3::v2 {

float Density::ClampDensity(float density) {
  if (!std::isfinite(density))
    return 1.0f;
  return std::clamp(density, 0.1f, 8.0f);
}

void Density::SetPlatformDensity(float density) {
  Ctx().platformDensity = ClampDensity(density);
}

float Density::GetPlatformDensity() {
  return Ctx().platformDensity;
}

void Density::SetLayoutDensity(float density) {
  Ctx().layoutDensity = ClampDensity(density);
}

float Density::GetLayoutDensity() {
  return Ctx().layoutDensity;
}

// Deliberately NOT on RenderContext, unlike the densities above: rendering runs
// under a per-surface context that the host swaps in on the render thread, so a
// value written from the JS thread would never be seen. The OS text-size setting
// is one process-wide user preference either way, and it is read from both
// threads (Yoga measure and paint), hence the atomic.
static std::atomic<float> g_fontScale{1.0f};

void Density::SetFontScale(float scale) {
  if (!std::isfinite(scale) || scale <= 0.0f) {
    g_fontScale.store(1.0f, std::memory_order_relaxed);
    return;
  }
  // Android tops out at 2.0 in Settings (and non-linear beyond 1.3 since 14);
  // iOS accessibility sizes reach ~3.1x. Clamp only against nonsense values.
  g_fontScale.store(std::clamp(scale, 0.5f, 4.0f), std::memory_order_relaxed);
}

float Density::GetFontScale() {
  return g_fontScale.load(std::memory_order_relaxed);
}

float Density::DpToPx(float dp) {
  return dp * Ctx().layoutDensity;
}

float Density::PxToDp(float px) {
  return px / Ctx().layoutDensity;
}

Vector2 Density::DpToPx(Vector2 dp) {
  return {DpToPx(dp.x), DpToPx(dp.y)};
}

Vector2 Density::PxToDp(Vector2 px) {
  return {PxToDp(px.x), PxToDp(px.y)};
}

Rectangle Density::DpToPx(Rectangle dp) {
  return {DpToPx(dp.x), DpToPx(dp.y), DpToPx(dp.width), DpToPx(dp.height)};
}

Rectangle Density::PxToDp(Rectangle px) {
  return {PxToDp(px.x), PxToDp(px.y), PxToDp(px.width), PxToDp(px.height)};
}

int Density::RasterPixels(float dp) {
  return std::max(1, static_cast<int>(std::round(DpToPx(dp))));
}

float Density::CssReferencePxToLayoutDp(float cssPx) {
  const float layout = Ctx().layoutDensity;
  const float platform = Ctx().platformDensity;
  if (layout <= 0.0f || !std::isfinite(layout))
    return cssPx;
  if (platform <= 0.0f || !std::isfinite(platform))
    return cssPx;
  return cssPx * platform / layout;
}

} // namespace raym3::v2
