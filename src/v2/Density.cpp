#include "raym3/v2/Density.h"
#include "raym3/v2/RenderContext.h"

#include <algorithm>
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

} // namespace raym3::v2
