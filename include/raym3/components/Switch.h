#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct SwitchOptions {
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
  // Animation progress 0..1 (off→on). -1 snaps to the checked value with no
  // interpolation. Drives thumb slide + grow and track color cross-fade.
  float animProgress = -1.0f;
};

class SwitchComponent {
public:
    static void ResetIds();
    static bool Render(const char* label, Rectangle bounds, bool* checked, const SwitchOptions* options = nullptr);
    
private:
    static ComponentState GetState(Rectangle bounds);
    static Rectangle GetSwitchBounds(Rectangle bounds);
};

} // namespace raym3
