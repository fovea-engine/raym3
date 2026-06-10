#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct AppBarAction {
  const char *icon = nullptr;
  const char *tooltip = nullptr;
};

struct AppBarOptions {
  AppBarVariant variant = AppBarVariant::Small;
  const char *navigationIcon = nullptr;
  const AppBarAction *actions = nullptr;
  int actionCount = 0;
  const char *subtitle = nullptr;
  bool elevated = false;
};

class AppBarComponent {
public:
  static int Render(Rectangle bounds, const char *title,
                    const AppBarOptions &options = AppBarOptions{});
};

} // namespace raym3
