#include "raym3/components/Dialog.h"
#include "raym3/components/Button.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/ClipScope.h"
#include <cstring>
#include <raylib.h>
#include <sstream>
#include <vector>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

int DialogComponent::buttonCount_ = 0;
int DialogComponent::selectedButton_ = -1;
bool DialogComponent::isOpen_ = false;
bool DialogComponent::isRendering_ = false;

bool DialogComponent::Render(const char *title, const char *message,
                             const char *buttons) {
  if (!isOpen_) {
    isOpen_ = true;
    selectedButton_ = -1;

    if (buttons) {
      buttonCount_ = 1;
      for (const char *p = buttons; *p; p++) {
        if (*p == ';')
          buttonCount_++;
      }
    } else {
      buttonCount_ = 0;
    }
  }

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::PushLayer(9999);
  Rectangle screenBounds = {0, 0, (float)GetScreenWidth(),
                            (float)GetScreenHeight()};
  InputLayerManager::RegisterBlockingRegion(screenBounds, true);
  DrawBackdrop();
#else
  DrawBackdrop();
#endif

  // Calculate dynamic dimensions
  float width = 320.0f; // Minimum width
  float padding = 24.0f;

  // Measure text to potentially increase width or determine height
  float titleHeight = 0;
  if (title) {
    Vector2 titleSize =
        Renderer::MeasureText(title, 24.0f, FontWeight::Regular);
    if (titleSize.x + padding * 2 > width)
      width = titleSize.x + padding * 2;
    titleHeight = titleSize.y + 16.0f; // + gap
  }

  float messageHeight = 0;
  if (message) {
    // Simple approximation for wrapping text height or just single line for now
    // For a proper implementation we'd need a multi-line text measure function
    // extending the width if too wide, or wrapping.
    // Keeping it simple: Extend width up to a max, then wrap height
    Vector2 msgSize =
        Renderer::MeasureText(message, 14.0f, FontWeight::Regular);
    float maxWidth = 560.0f;

    if (width < msgSize.x + padding * 2) {
      width = msgSize.x + padding * 2;
      if (width > maxWidth)
        width = maxWidth;
    }

    // Rough height estimation assuming wrapping
    float textWidth = width - padding * 2;
    float lines = std::ceil(msgSize.x / textWidth);
    if (lines < 1)
      lines = 1;
    messageHeight = lines * 20.0f; // Line height 20
  }

  float buttonAreaHeight = (buttons && buttonCount_ > 0) ? 52.0f : 0.0f;

  float height =
      padding + titleHeight + messageHeight + buttonAreaHeight + padding;
  if (buttons && buttonCount_ > 0)
    height -= padding; // Button area includes bottom padding usually

  // Center on screen
  Rectangle screen = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
  Rectangle dialogBounds = {screen.x + (screen.width - width) / 2.0f,
                            screen.y + (screen.height - height) / 2.0f, width,
                            height};

  Layout::RegisterDebugRect(dialogBounds);

  ColorScheme &scheme = Theme::GetColorScheme();
  float cornerRadius = Theme::GetShapeTokens().cornerExtraLarge; // 28dp

  // Dialog Container - draw directly without registering as blocking region
  // Only the backdrop blocks lower layers; buttons inside dialog need to work
  Renderer::DrawElevatedRectangle(dialogBounds, cornerRadius, 3,
                                  scheme.surface);

  float y = dialogBounds.y + padding;

  // Title
  if (title) {
    Vector2 titlePos = {dialogBounds.x + padding, y};
    Renderer::DrawText(title, titlePos, 24.0f, scheme.onSurface,
                       FontWeight::Regular); // Headline Small

    // Debug rect for title
    Vector2 titleSize =
        Renderer::MeasureText(title, 24.0f, FontWeight::Regular);
    Rectangle titleBounds = {titlePos.x, titlePos.y, titleSize.x, titleSize.y};
    Layout::RegisterDebugRect(titleBounds);

    y += 40.0f; // Line height + spacing
  }

  // Message (Supporting Text)
  if (message) {
    Rectangle textBounds = {dialogBounds.x + padding, y,
                            dialogBounds.width - padding * 2, messageHeight};
    Layout::RegisterDebugRect(textBounds);
    Renderer::DrawText(message, {textBounds.x, textBounds.y}, 14.0f,
                       scheme.onSurfaceVariant,
                       FontWeight::Regular); // Body Medium
  }

  // Buttons (Actions)
  isRendering_ = true; // Allow interaction for children
  if (buttons && buttonCount_ > 0) {
    std::vector<std::string> buttonLabels;
    std::istringstream iss(buttons);
    std::string token;
    while (std::getline(iss, token, ';')) {
      buttonLabels.push_back(token);
    }

    float buttonHeight = 40.0f;
    float buttonY = dialogBounds.y + dialogBounds.height - padding -
                    buttonHeight; // Bottom alignment

    // Align Right (End)
    float currentX = dialogBounds.x + dialogBounds.width - padding;

    // Iterate backwards to place from right
    for (int i = (int)buttonLabels.size() - 1; i >= 0; i--) {
      // Measure button width
      Vector2 textSize = Renderer::MeasureText(buttonLabels[i].c_str(), 14.0f,
                                               FontWeight::Medium);
      float btnWidth = textSize.x + 24.0f; // Padding
      if (btnWidth < 60.0f)
        btnWidth = 60.0f;

      currentX -= btnWidth;
      Rectangle buttonBounds = {currentX, buttonY, btnWidth, buttonHeight};
      Layout::RegisterDebugRect(buttonBounds);

      // MD3 Dialog actions are typically Text Buttons
      ButtonVariant variant = ButtonVariant::Text;

      if (ButtonComponent::Render(buttonLabels[i].c_str(), buttonBounds,
                                  variant)) {
        selectedButton_ = i;
        isOpen_ = false;
        isRendering_ = false;
#if RAYM3_USE_INPUT_LAYERS
        InputLayerManager::PopLayer();
#endif
        return true;
      }

      currentX -= 8.0f; // Spacing between buttons
    }
  }
  isRendering_ = false;

#if RAYM3_USE_INPUT_LAYERS
  // Pop the dialog layer we pushed earlier
  InputLayerManager::PopLayer();
#endif

  if (selectedButton_ >= 0) {
    isOpen_ = false;
    int result = selectedButton_;
    selectedButton_ = -1;
    return result >= 0;
  }

  return false;
}

void DialogComponent::DrawBackdrop() {
  int screenWidth = GetScreenWidth();
  int screenHeight = GetScreenHeight();
  Rectangle backdrop = {0, 0, (float)screenWidth, (float)screenHeight};
  ColorScheme &scheme = Theme::GetColorScheme();
  Color scrimColor = ColorAlpha(scheme.scrim, 0.32f); // MD3 opacity 0.32
  DrawRectangleRec(backdrop, scrimColor);
}

int DialogComponent::GetSelectedButtonIndex() { return selectedButton_; }

} // namespace raym3
