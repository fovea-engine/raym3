#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct IconButtonOptions {
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

class IconButtonComponent {
public:
  static void ResetIds();
  static bool Render(const char *iconName, Rectangle bounds,
                     ButtonVariant variant = ButtonVariant::Text,
                     IconVariation iconVariation = IconVariation::Filled,
                     Color iconColorOverride = {0, 0, 0, 0},
                     const IconButtonOptions* options = nullptr);

private:
  static ComponentState GetState(Rectangle bounds);
  static Color GetBackgroundColor(ButtonVariant variant, ComponentState state);
  static Color GetIconColor(ButtonVariant variant, ComponentState state);
};

} // namespace raym3
