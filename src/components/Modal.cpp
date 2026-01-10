#include "raym3/components/Modal.h"
#include "raym3/components/Button.h"
#include "raym3/components/Card.h"
#include "raym3/components/Text.h"
#include "raym3/components/TextField.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <cstring>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

bool ModalComponent::isOpen_ = false;

// Store bounds for the current frame to share between Begin and End
static Rectangle s_currentModalBounds = {0, 0, 0, 0};

Rectangle ModalComponent::Begin(const char *title, const char *message,
                                float width, float height) {
  if (!isOpen_) {
    isOpen_ = true;
    TooltipManager::DismissAll(); // Dismiss any active tooltips when modal opens
  }

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::PushLayer(9999);
  Rectangle screenBounds = {0, 0, (float)GetScreenWidth(),
                            (float)GetScreenHeight()};
  InputLayerManager::RegisterBlockingRegion(screenBounds, true);
#endif

  DrawBackdrop();

  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();

  // Calculate bounds centered
  s_currentModalBounds = {(screenWidth - width) / 2.0f,
                          (screenHeight - height) / 2.0f, width, height};

  // Register the main modal container for debug
  Layout::RegisterDebugRect(s_currentModalBounds);

  ColorScheme &scheme = Theme::GetColorScheme();
  float cornerRadius = Theme::GetShapeTokens().cornerExtraLarge;

  // Draw modal card
  Renderer::DrawElevatedRectangle(s_currentModalBounds, cornerRadius, 3,
                                  scheme.surface);

  float padding = 24.0f;
  float y = s_currentModalBounds.y + padding;
  float contentWidth = s_currentModalBounds.width - padding * 2;

  // Title
  if (title) {
    Vector2 titlePos = {s_currentModalBounds.x + padding, y};
    Renderer::DrawText(title, titlePos, 24.0f, scheme.onSurface,
                       FontWeight::Regular);

    // Debug rect for title
    Rectangle titleBounds = {titlePos.x, titlePos.y, contentWidth, 30.0f};
    Layout::RegisterDebugRect(titleBounds);

    y += 40.0f;
  }

  // Message
  if (message) {
    Vector2 messagePos = {s_currentModalBounds.x + padding, y};
    Renderer::DrawText(message, messagePos, 14.0f, scheme.onSurfaceVariant,
                       FontWeight::Regular);

    // Debug rect for message
    Rectangle messageBounds = {messagePos.x, messagePos.y, contentWidth, 20.0f};
    Layout::RegisterDebugRect(messageBounds);

    y += 30.0f;
  }

  // Return the safe text content area
  // The caller can use this x,y to start drawing their custom controls
  return {
      s_currentModalBounds.x + padding, y, contentWidth,
      (s_currentModalBounds.y + s_currentModalBounds.height) - y - padding -
          40.0f // Reserve space for buttons
  };
}

bool ModalComponent::End(const char *confirmButton, const char *cancelButton,
                         bool *outConfirmed) {
  if (outConfirmed)
    *outConfirmed = false;

  float padding = 24.0f;
  float buttonHeight = 40.0f;
  float buttonY = s_currentModalBounds.y + s_currentModalBounds.height -
                  padding - buttonHeight;
  float currentX =
      s_currentModalBounds.x + s_currentModalBounds.width - padding;

  bool shouldClose = false;
  bool confirmed = false;

  // Confirm button (right)
  if (confirmButton) {
    Vector2 textSize =
        Renderer::MeasureText(confirmButton, 14.0f, FontWeight::Medium);
    float btnWidth = textSize.x + 24.0f;
    if (btnWidth < 60.0f)
      btnWidth = 60.0f;

    currentX -= btnWidth;
    Rectangle confirmBounds = {currentX, buttonY, btnWidth, buttonHeight};
    Layout::RegisterDebugRect(confirmBounds);

    if (ButtonComponent::Render(confirmButton, confirmBounds,
                                ButtonVariant::Filled)) {
      confirmed = true;
      shouldClose = true;
    }

    currentX -= 8.0f;
  }

  // Cancel button (left of confirm)
  if (cancelButton) {
    Vector2 textSize =
        Renderer::MeasureText(cancelButton, 14.0f, FontWeight::Medium);
    float btnWidth = textSize.x + 24.0f;
    if (btnWidth < 60.0f)
      btnWidth = 60.0f;

    currentX -= btnWidth;
    Rectangle cancelBounds = {currentX, buttonY, btnWidth, buttonHeight};
    Layout::RegisterDebugRect(cancelBounds);

    if (ButtonComponent::Render(cancelButton, cancelBounds,
                                ButtonVariant::Text)) {
      confirmed = false;
      shouldClose = true;
    }
  }

  // Handle Escape key to close
  if (IsKeyPressed(KEY_ESCAPE)) {
    confirmed = false;
    shouldClose = true;
  }

  // We don't handle Enter key generically here because we don't know the state
  // of custom inputs. The legacy Render() wrapper will handle Enter for its
  // specific text field.

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::PopLayer();
#endif

  if (shouldClose) {
    isOpen_ = false;
    if (outConfirmed)
      *outConfirmed = confirmed;
    return true;
  }

  return false;
}

bool ModalComponent::Render(const char *title, const char *message,
                            const char *textFieldLabel, char *textBuffer,
                            size_t bufferSize, const char *confirmButton,
                            const char *cancelButton) {

  // Legacy fixed size
  Rectangle docRect = Begin(title, message, 400.0f, 300.0f);

  // Text Field logic embedded in legacy component
  if (textFieldLabel && textBuffer) {
    Rectangle fieldBounds = {docRect.x, docRect.y, docRect.width, 56.0f};
    Layout::RegisterDebugRect(fieldBounds);

    TextFieldOptions options;
    options.placeholder = textFieldLabel;
    options.variant = TextFieldVariant::Filled;

    TextFieldComponent::Render(textBuffer, bufferSize, fieldBounds, nullptr,
                               options);
  }

  bool confirmed = false;
  bool closed = End(confirmButton, cancelButton, &confirmed);

  // Legacy Enter key support
  bool enterPressed =
      (IsKeyPressed(KEY_ENTER) && textBuffer && strlen(textBuffer) > 0);

  if (closed) {
    return confirmed;
  }

  if (enterPressed) {
    isOpen_ = false;
    return true;
  }

  return false;
}

void ModalComponent::DrawBackdrop() {
  Rectangle backdrop = {0, 0, (float)GetScreenWidth(),
                        (float)GetScreenHeight()};
  ColorScheme &scheme = Theme::GetColorScheme();
  Color scrimColor = ColorAlpha(scheme.scrim, 0.32f);
  DrawRectangleRec(backdrop, scrimColor);
}

Rectangle ModalComponent::GetModalBounds() {
  // Return last calculated bounds
  return s_currentModalBounds;
}

} // namespace raym3
