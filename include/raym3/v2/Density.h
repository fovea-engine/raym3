#pragma once

#include <raylib.h>

namespace raym3::v2 {

// raym3 v2 layout units are device-independent pixels (dp), matching Flutter's
// logical pixel model. Density is applied only at platform/render boundaries.
class Density {
public:
  static void SetPlatformDensity(float density);
  static float GetPlatformDensity();

  static void SetLayoutDensity(float density);
  static float GetLayoutDensity();

  static float DpToPx(float dp);
  static float PxToDp(float px);
  static Vector2 DpToPx(Vector2 dp);
  static Vector2 PxToDp(Vector2 px);
  static Rectangle DpToPx(Rectangle dp);
  static Rectangle PxToDp(Rectangle px);

  static int RasterPixels(float dp);

private:
  static float ClampDensity(float density);
};

} // namespace raym3::v2
