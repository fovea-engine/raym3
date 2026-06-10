#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct FabOptions {
  FabSize size = FabSize::Regular;
  FabColor color = FabColor::PrimaryContainer;
  bool lowered = false;
  bool disabled = false;
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

class FloatingActionButtonComponent {
public:
  static void ResetIds();
  static bool Render(const char *iconName, Rectangle bounds,
                     const FabOptions &options = FabOptions{});
  static bool RenderExtended(const char *label, const char *iconName,
                             Rectangle bounds,
                             const FabOptions &options = FabOptions{});
};

} // namespace raym3
