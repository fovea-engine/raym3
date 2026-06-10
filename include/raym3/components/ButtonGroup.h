#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

struct ButtonGroupItem {
  const char *label;
  const char *icon;     // Optional
  bool disabled = false;
};

// Standard (non-selection) connected button group.
// Each button triggers an independent action.
// Returns index of clicked button, -1 if none clicked.
class ButtonGroupComponent {
public:
  static void ResetIds();
  static int Render(Rectangle bounds, const ButtonGroupItem *items, int count,
                    ButtonVariant variant = ButtonVariant::Outlined);

private:
  static ComponentState GetState(Rectangle bounds);
};

} // namespace raym3
