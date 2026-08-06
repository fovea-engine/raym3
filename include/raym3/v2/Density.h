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

  // The OS text-size accessibility setting, as a multiplier on every font size
  // (Android Configuration.fontScale, iOS Dynamic Type, the browser's default
  // font size). 1.0 = "Default". Hosts publish it at boot and again whenever the
  // user changes it. Unlike the density above this participates in *layout*, not
  // just rasterization: bigger text has to reflow its container, so it is
  // applied where a font size is resolved (ResolveFontSize in Style.h), not at
  // the render boundary.
  static void SetFontScale(float scale);
  static float GetFontScale();

  static float DpToPx(float dp);
  static float PxToDp(float px);
  static Vector2 DpToPx(Vector2 dp);
  static Vector2 PxToDp(Vector2 px);
  static Rectangle DpToPx(Rectangle dp);
  static Rectangle PxToDp(Rectangle px);

  static int RasterPixels(float dp);

  // CSS / web reference pixels (px, or rem×16) → raym3 layout dp. Matches
  // Flutter logical pixels: physical size = cssPx × platformDensity, expressed
  // in the active layout coordinate system (layoutDensity may differ on Android).
  static float CssReferencePxToLayoutDp(float cssPx);

private:
  static float ClampDensity(float density);
};

} // namespace raym3::v2
