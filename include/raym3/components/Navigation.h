#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct NavigationItem {
  const char *label = nullptr;
  const char *icon = nullptr;
  const char *selectedIcon = nullptr;
  const char *badgeLabel = nullptr;
  bool showBadge = false;
  bool disabled = false;
};

struct NavigationOptions {
  bool showLabels = true;
  bool expressiveRail = true;
  const char *headerIcon = nullptr;
};

class NavigationComponent {
public:
  static bool Bar(Rectangle bounds, const NavigationItem *items, int itemCount,
                  int *selectedIndex,
                  const NavigationOptions &options = NavigationOptions{});
  static bool Rail(Rectangle bounds, const NavigationItem *items, int itemCount,
                   int *selectedIndex,
                   const NavigationOptions &options = NavigationOptions{});
};

} // namespace raym3
