#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct CheckboxOptions {
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
  // Animation progress 0..1 (unchecked→checked). -1 snaps. Drives box fill +
  // checkmark draw-in.
  float animProgress = -1.0f;
};

class CheckboxComponent {
public:
    static void ResetIds();
    static bool Render(const char* label, Rectangle bounds, bool* checked, const CheckboxOptions* options = nullptr);
    
private:
    static ComponentState GetState(Rectangle bounds);
    static Rectangle GetCheckboxBounds(Rectangle bounds);
};

} // namespace raym3
