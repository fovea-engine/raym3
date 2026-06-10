#pragma once

#include <raylib.h>
#include "raym3/types.h"

namespace raym3 {

struct RadioButtonOptions {
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
  // Animation progress 0..1 (unselected→selected). -1 snaps. Drives inner-dot
  // scale.
  float animProgress = -1.0f;
};

class RadioButtonComponent {
public:
  static void ResetIds();
  static bool Render(const char *label, Rectangle bounds, bool selected, const RadioButtonOptions* options = nullptr);
};

} // namespace raym3
