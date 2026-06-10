#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct SearchBarOptions {
  const char *leadingIcon = "search";
  const char *trailingIcon = nullptr;
  bool disabled = false;
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

class SearchBarComponent {
public:
  static bool Render(char *buffer, int bufferSize, Rectangle bounds,
                     const char *placeholder = "Search",
                     const SearchBarOptions &options = SearchBarOptions{});
};

} // namespace raym3
