#pragma once

#include "raym3/types.h"
#include <raylib.h>
#include <vector>

namespace raym3 {

class RangeSliderComponent {
public:
  // Main render function - returns updated values
  // Input/output via vector allows any number of thumbs
  static std::vector<float> Render(Rectangle bounds,
                                   const std::vector<float> &values, float min,
                                   float max, const char *label = nullptr,
                                   const RangeSliderOptions &options = {});

  static void ResetFieldId();

private:
  static Rectangle GetTrackBounds(Rectangle bounds);
  static float GetValueFromPosition(Rectangle trackBounds, float x);
  static int GetClosestThumbIndex(Rectangle trackBounds,
                                  const std::vector<float> &values, float min,
                                  float max, Vector2 mousePos);
};

} // namespace raym3
