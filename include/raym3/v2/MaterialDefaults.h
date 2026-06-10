#pragma once

#include "raym3/v2/Components.h"
#include <raylib.h>

namespace raym3::v2 {

struct MaterialComponentMetrics {
  float layoutWidth = 0.0f;
  float layoutHeight = 0.0f;
  float minWidth = 0.0f;
  float minHeight = 0.0f;
  float visualWidth = 0.0f;
  float visualHeight = 0.0f;
  float touchWidth = 0.0f;
  float touchHeight = 0.0f;
};

struct MaterialReference {
  const char *flutterSource;
  const char *status;
};

MaterialComponentMetrics GetMaterialMetrics(M3Component component);
MaterialReference GetMaterialReference(M3Component component);
Rectangle CenteredVisualRect(Rectangle layout, float visualWidth,
                             float visualHeight);
Rectangle MinimumTouchRect(Rectangle visual, float minWidth, float minHeight);

} // namespace raym3::v2
