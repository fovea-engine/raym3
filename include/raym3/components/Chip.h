#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct ChipOptions {
  ChipVariant variant = ChipVariant::Assist;
  const char *leadingIcon = nullptr;
  const char *trailingIcon = nullptr;
  bool selected = false;
  bool disabled = false;
  bool elevated = false;
  bool showSelectedIcon = true;
  bool showCloseIcon = false;
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

class ChipComponent {
public:
  static void ResetIds();
  static bool Render(const char *label, Rectangle bounds,
                     const ChipOptions &options = ChipOptions{});
  static bool Render(const char *label, Rectangle bounds, bool *selected,
                     const ChipOptions &options = ChipOptions{});
};

} // namespace raym3
