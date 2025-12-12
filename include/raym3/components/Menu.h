#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct MenuItem; // Forward declaration

class MenuComponent {
public:
  static void Render(Rectangle bounds, const struct MenuItem *items,
                     int itemCount, int *selected, bool iconOnly = false);

private:
  static ComponentState GetItemState(Rectangle itemBounds, int index,
                                     int *selected);
};

} // namespace raym3
