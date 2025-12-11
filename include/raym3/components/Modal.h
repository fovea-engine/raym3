#pragma once

#include <functional>
#include <raylib.h>

namespace raym3 {

struct ModalResult {
  bool confirmed;
  char textBuffer[256];
};

class ModalComponent {
public:
  static bool Render(const char *title, const char *message,
                     const char *textFieldLabel, char *textBuffer,
                     size_t bufferSize, const char *confirmButton = "OK",
                     const char *cancelButton = "Cancel");

  // Generic API for custom content
  static Rectangle Begin(const char *title, const char *message, float width,
                         float height);
  static bool End(const char *confirmButton, const char *cancelButton,
                  bool *outConfirmed);

  static bool IsOpen() { return isOpen_; }
  static void Close() { isOpen_ = false; }

private:
  static bool isOpen_;
  static bool firstFrame_;

  static void DrawBackdrop();
  static Rectangle GetModalBounds();
};

} // namespace raym3
