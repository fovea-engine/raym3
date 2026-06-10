#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct BadgeOptions {
  BadgeAlignment alignment = BadgeAlignment::TopEnd;
  Color color = BLANK;
  Color textColor = BLANK;
  int maxCharacterCount = 4;
  float horizontalOffset = -4.0f;
  float verticalOffset = 4.0f;
};

class BadgeComponent {
public:
  static void Render(Rectangle anchorBounds, const char *label = nullptr,
                     const BadgeOptions &options = BadgeOptions{});
};

} // namespace raym3
