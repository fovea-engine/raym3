#pragma once

#include "raym3/types.h"
#include <raylib.h>

namespace raym3 {

class TextFieldComponent {
public:
  static bool Render(char *buffer, int bufferSize, Rectangle bounds,
                     const char *label = nullptr,
                     const TextFieldOptions &options = TextFieldOptions{});
  static void ResetFieldId();
  static bool IsAnyFieldFocused();

private:
  static void UpdateCursor(char *buffer, int bufferSize, float &lastBlinkTime);
  static void DrawCursor(Rectangle bounds, const char *text, int position,
                         float scrollOffset, float lastBlinkTime,
                         float textStartX, Color bgColor);
};

} // namespace raym3
