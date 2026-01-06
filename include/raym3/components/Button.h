#pragma once

#include "raym3/types.h"
#include <cstring>
#include <raylib.h>

namespace raym3 {

class ButtonComponent {
public:
  static bool Render(const char *text, Rectangle bounds,
                     ButtonVariant variant = ButtonVariant::Filled,
                     const ButtonOptions &options = ButtonOptions{});

private:
  static ComponentState GetState(Rectangle bounds);
  static Color GetBackgroundColor(ButtonVariant variant, ComponentState state);
  static Color GetTextColor(ButtonVariant variant, ComponentState state);
  static float GetCornerRadius();
};

} // namespace raym3
