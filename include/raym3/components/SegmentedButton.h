#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct SegmentedButtonItem {
  const char *label;
  const char *icon;     // Optional icon name
  bool disabled = false;
};

class SegmentedButtonComponent {
public:
  static void ResetIds();
  static bool Render(Rectangle bounds, const SegmentedButtonItem *items,
                     int itemCount, int *selectedIndex,
                     bool multiSelect = false);

private:
  static ComponentState GetState(Rectangle bounds);
};

} // namespace raym3
