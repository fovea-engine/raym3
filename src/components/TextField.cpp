#include "raym3/components/TextField.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/layout/Layout.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <raylib.h>
#include <regex>
#include <set>
#include <string>
#include <vector>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

struct TextFieldState {
  std::string lastValue;
  int cursorPosition = 0;
  float scrollOffset = 0.0f;
  float lastBlinkTime = 0.0f;
  double backspaceTimer = 0.0;
  double arrowLeftTimer = 0.0;
  double arrowRightTimer = 0.0;

  int selectionStart = -1;
  int selectionEnd = -1;
  bool isSelecting = false;
  float lastClickTime = 0.0f;
  int lastClickPosition = -1;

  std::vector<std::string> undoHistory;
  int undoIndex = -1;
  bool isUndoRedoOperation = false;

  int lastActiveFrame = -1;
};

static int activeFieldId_ = -1;
static int currentFieldId_ = 0;
static std::map<int, TextFieldState> fieldStates_;
static std::set<int> activeFieldIdsThisFrame_;
static int currentFrame_ = 0;
static std::vector<Rectangle> allFieldBounds_;

void TextFieldComponent::ResetFieldId() {
  // Check if clicking outside all fields should unfocus the active field
  if (activeFieldId_ != -1 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    Vector2 mousePos = GetMousePosition();
    bool clickedInsideAnyField = false;

    for (const Rectangle &bounds : allFieldBounds_) {
      if (CheckCollisionPointRec(mousePos, bounds)) {
        clickedInsideAnyField = true;
        break;
      }
    }

    if (!clickedInsideAnyField) {
      // Clicked outside all fields, unfocus the active field
      if (fieldStates_.find(activeFieldId_) != fieldStates_.end()) {
        fieldStates_[activeFieldId_].lastValue = "";
        fieldStates_[activeFieldId_].isSelecting = false;
      }
      activeFieldId_ = -1;
    }
  }

  currentFieldId_ = 0;
  currentFrame_++;
  allFieldBounds_.clear();

  for (auto it = fieldStates_.begin(); it != fieldStates_.end();) {
    if (it->second.lastActiveFrame < currentFrame_ - 1) {
      it->second.undoHistory.clear();
      it = fieldStates_.erase(it);
    } else {
      ++it;
    }
  }

  activeFieldIdsThisFrame_.clear();
}

bool TextFieldComponent::IsAnyFieldFocused() { return activeFieldId_ != -1; }

static void FindWordBoundaries(const char *text, int position, int &wordStart,
                               int &wordEnd) {
  if (!text || position < 0) {
    wordStart = 0;
    wordEnd = 0;
    return;
  }

  int len = (int)strlen(text);
  if (position > len)
    position = len;

  wordStart = position;
  wordEnd = position;

  while (wordStart > 0 && !isspace((unsigned char)text[wordStart - 1]) &&
         !ispunct((unsigned char)text[wordStart - 1])) {
    wordStart--;
  }

  while (wordEnd < len && !isspace((unsigned char)text[wordEnd]) &&
         !ispunct((unsigned char)text[wordEnd])) {
    wordEnd++;
  }
}

static void NormalizeSelection(int &start, int &end) {
  if (start == -1 || end == -1) {
    start = -1;
    end = -1;
    return;
  }
  if (start > end) {
    std::swap(start, end);
  }
}

static void DrawSelection(Rectangle bounds, const char *text, int start,
                          int end, float scrollOffset, float padding) {
  if (start == -1 || end == -1 || start == end)
    return;
  if (!text)
    return;

  NormalizeSelection(start, end);

  ColorScheme &scheme = Theme::GetColorScheme();

  std::string textBeforeStart(text, start);
  std::string textBeforeEnd(text, end);

  Vector2 startSize = Renderer::MeasureText(textBeforeStart.c_str(), 16.0f,
                                            FontWeight::Regular);
  Vector2 endSize =
      Renderer::MeasureText(textBeforeEnd.c_str(), 16.0f, FontWeight::Regular);

  float selectionX = bounds.x + padding - scrollOffset + startSize.x;
  float selectionWidth = endSize.x - startSize.x;
  float selectionY = bounds.y + (bounds.height - 16.0f) / 2.0f;
  float selectionHeight = 16.0f;

  Color selectionColor = scheme.primary;
  selectionColor.a = 76;

  DrawRectangleRec({selectionX, selectionY, selectionWidth, selectionHeight},
                   selectionColor);
}

static bool ValidateInputMask(const char *text, const char *pattern) {
  if (!pattern || !text)
    return true;

  try {
    std::regex regexPattern(pattern);
    return std::regex_match(text, regexPattern);
  } catch (...) {
    return true;
  }
}

static void SaveToHistory(TextFieldState &state, const std::string &text,
                          int maxHistory) {
  if (state.isUndoRedoOperation || maxHistory <= 0)
    return;

  if (state.undoIndex >= 0 &&
      state.undoIndex < (int)state.undoHistory.size() - 1) {
    state.undoHistory.erase(state.undoHistory.begin() + state.undoIndex + 1,
                            state.undoHistory.end());
  }

  state.undoHistory.push_back(text);

  if ((int)state.undoHistory.size() > maxHistory) {
    state.undoHistory.erase(state.undoHistory.begin());
  } else {
    state.undoIndex++;
  }

  if (state.undoIndex >= maxHistory) {
    state.undoIndex = maxHistory - 1;
  }
}

static ComponentState GetTextFieldState(Rectangle bounds, int fieldId,
                                        bool disabled) {
  if (disabled) {
    return ComponentState::Disabled;
  }

  if (activeFieldId_ != -1 && activeFieldId_ != fieldId) {
    return ComponentState::Default;
  }

  if (activeFieldId_ == fieldId) {
    return ComponentState::Focused;
  }

  Vector2 mousePos = GetMousePosition();
  bool isVisible = Layout::IsRectVisibleInScrollContainer(bounds);
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput =
      isVisible && InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
#else
  bool isHovered = isVisible && CheckCollisionPointRec(mousePos, bounds);
#endif

  if (isHovered)
    return ComponentState::Hovered;
  return ComponentState::Default;
}

bool TextFieldComponent::Render(char *buffer, int bufferSize, Rectangle bounds,
                                const char *label,
                                const TextFieldOptions &options) {
  int fieldId = currentFieldId_++;

  TextFieldState &fieldState = fieldStates_[fieldId];
  fieldState.lastActiveFrame = currentFrame_;
  activeFieldIdsThisFrame_.insert(fieldId);

  bool isFocused = (activeFieldId_ == fieldId);

  if (options.disabled) {
    activeFieldId_ = -1;
  }

  Rectangle inputBounds = bounds;
  if (label) {
    float labelHeight = 16.0f;
    inputBounds.y += labelHeight + 4.0f;
    inputBounds.height -= labelHeight + 4.0f;
  }

  // Track this field's bounds for click-outside detection
  allFieldBounds_.push_back(inputBounds);

  std::string incomingValue(buffer ? buffer : "");
  if (isFocused) {
  } else {
    if (fieldState.lastValue.empty() || incomingValue != fieldState.lastValue) {
      fieldState.lastValue = incomingValue;
      fieldState.cursorPosition = (int)strlen(buffer);
      fieldState.scrollOffset = 0.0f;
      fieldState.selectionStart = -1;
      fieldState.selectionEnd = -1;
    }
  }

  if (options.disabled) {
    ComponentState state = ComponentState::Disabled;
    ColorScheme &scheme = Theme::GetColorScheme();
    float cornerRadius = Theme::GetShapeTokens().cornerMedium;
    if (label) {
      Vector2 labelPos = {bounds.x, bounds.y};
      Renderer::DrawText(label, labelPos, 12.0f, scheme.onSurfaceVariant,
                         FontWeight::Regular);
    }
    Renderer::DrawRoundedRectangleEx(inputBounds, cornerRadius, scheme.outline,
                                     1.0f);
    Renderer::DrawStateLayer(inputBounds, cornerRadius, scheme.surface, state);
    if (buffer && strlen(buffer) > 0) {
      Vector2 textPos = {inputBounds.x + 16.0f,
                         inputBounds.y + (inputBounds.height - 16.0f) / 2.0f};
      Color disabledText = scheme.onSurface;
      disabledText.a = 128;
      Renderer::DrawText(buffer, textPos, 16.0f, disabledText,
                         FontWeight::Regular);
    } else if (options.placeholder) {
      Vector2 textPos = {inputBounds.x + 16.0f,
                         inputBounds.y + (inputBounds.height - 16.0f) / 2.0f};
      Color placeholderColor = scheme.onSurfaceVariant;
      placeholderColor.a = 128;
      Renderer::DrawText(options.placeholder, textPos, 16.0f, placeholderColor,
                         FontWeight::Regular);
    }
    return false;
  }

  Vector2 mousePos = GetMousePosition();
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput =
      InputLayerManager::ShouldProcessMouseInput(inputBounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, bounds);
  bool isPressed = canProcessInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  bool isDown = canProcessInput && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool isReleased = canProcessInput && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool isHovered = CheckCollisionPointRec(mousePos, bounds);
  bool isPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  bool isDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool isReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif

  if (DialogComponent::IsActive() && !DialogComponent::IsRendering()) {
    isHovered = false;
    isPressed = false;
    isDown = false;
  }

  float iconSize = 24.0f;
  float iconPadding = 12.0f;
  float basePadding = 16.0f;
  float textPadding = basePadding;

  if (options.leadingIcon) {
    textPadding += iconSize + iconPadding;
  }
  if (options.trailingIcon) {
    textPadding += iconSize + iconPadding;
  }

  float padding = basePadding;
  bool clickedInField =
      isPressed && CheckCollisionPointRec(mousePos, inputBounds);
  bool clickedOutside =
      isPressed && !CheckCollisionPointRec(mousePos, inputBounds);

  Rectangle leadingIconBounds = {0, 0, 0, 0};
  Rectangle trailingIconBounds = {0, 0, 0, 0};

  if (options.leadingIcon) {
    leadingIconBounds = {inputBounds.x + iconPadding,
                         inputBounds.y + (inputBounds.height - iconSize) / 2.0f,
                         iconSize, iconSize};
  }

  if (options.trailingIcon) {
    trailingIconBounds = {
        inputBounds.x + inputBounds.width - iconSize - iconPadding,
        inputBounds.y + (inputBounds.height - iconSize) / 2.0f, iconSize,
        iconSize};
  }

  bool clickedLeadingIcon = isReleased && options.leadingIcon &&
                            CheckCollisionPointRec(mousePos, leadingIconBounds);
  bool clickedTrailingIcon =
      isReleased && options.trailingIcon &&
      CheckCollisionPointRec(mousePos, trailingIconBounds);

  if (clickedLeadingIcon && options.onLeadingIconClick) {
    options.onLeadingIconClick();
  }

  if (clickedTrailingIcon && options.onTrailingIconClick) {
    options.onTrailingIconClick();
  }

  if (clickedLeadingIcon || clickedTrailingIcon) {
    clickedInField = false;
  }

  float textStartX = inputBounds.x + basePadding;
  if (options.leadingIcon) {
    textStartX += iconSize + iconPadding;
  }

  if (clickedInField && !options.readOnly) {
    bool wasFocused = (activeFieldId_ == fieldId);
    activeFieldId_ = fieldId;

    float fieldScroll = isFocused ? fieldState.scrollOffset : 0.0f;
    float clickRelativeX = mousePos.x - (textStartX - fieldScroll);
    int len = (int)strlen(buffer ? buffer : "");
    int clickPosition = len;

    if (len > 0) {
      float closestDiff = 10000.0f;
      for (int i = 0; i <= len; i++) {
        std::string sub(buffer, i);
        Vector2 size =
            Renderer::MeasureText(sub.c_str(), 16.0f, FontWeight::Regular);
        float diff = std::abs(size.x - clickRelativeX);
        if (diff < closestDiff) {
          closestDiff = diff;
          clickPosition = i;
        }
      }
    }

    float currentTime = GetTime();
    bool isDoubleClick =
        (currentTime - fieldState.lastClickTime < 0.3f) &&
        (abs(clickPosition - fieldState.lastClickPosition) < 3);

    if (isDoubleClick) {
      int wordStart, wordEnd;
      FindWordBoundaries(buffer, clickPosition, wordStart, wordEnd);
      fieldState.selectionStart = wordStart;
      fieldState.selectionEnd = wordEnd;
      fieldState.cursorPosition = wordEnd;
    } else {
      fieldState.selectionStart = -1;
      fieldState.selectionEnd = -1;
      fieldState.cursorPosition = clickPosition;
    }

    fieldState.lastClickTime = currentTime;
    fieldState.lastClickPosition = clickPosition;
    fieldState.isSelecting = true;

    if (!wasFocused) {
      fieldState.lastBlinkTime = GetTime();
      fieldState.lastValue = std::string(buffer ? buffer : "");
    }
  } else if (clickedOutside) {
    if (activeFieldId_ == fieldId) {
      fieldState.lastValue = std::string(buffer ? buffer : "");
      activeFieldId_ = -1;
      fieldState.isSelecting = false;
    }
  }

  if (isDown && isFocused && fieldState.isSelecting) {
    float fieldScroll = fieldState.scrollOffset;
    float dragRelativeX = mousePos.x - (textStartX - fieldScroll);
    int len = (int)strlen(buffer ? buffer : "");
    int dragPosition = len;

    if (len > 0) {
      float closestDiff = 10000.0f;
      for (int i = 0; i <= len; i++) {
        std::string sub(buffer, i);
        Vector2 size =
            Renderer::MeasureText(sub.c_str(), 16.0f, FontWeight::Regular);
        float diff = std::abs(size.x - dragRelativeX);
        if (diff < closestDiff) {
          closestDiff = diff;
          dragPosition = i;
        }
      }
    }

    if (fieldState.selectionStart == -1) {
      fieldState.selectionStart = fieldState.cursorPosition;
    }
    fieldState.selectionEnd = dragPosition;
    fieldState.cursorPosition = dragPosition;
  }

  if (isReleased) {
    fieldState.isSelecting = false;
  }

  ComponentState state = GetTextFieldState(bounds, fieldId, options.disabled);

  ColorScheme &scheme = Theme::GetColorScheme();
  float cornerRadius = Theme::GetShapeTokens().cornerMedium;

  if (label) {
    Vector2 labelPos = {bounds.x, bounds.y};
    Renderer::DrawText(label, labelPos, 12.0f, scheme.onSurfaceVariant,
                       FontWeight::Regular);
  }

  Color bgColor = scheme.surface;
  if (options.backgroundColor.a > 0) {
    bgColor = options.backgroundColor;
  } else if (options.readOnly) {
    bgColor = ColorAlpha(scheme.surface, 0.5f);
  }

  if (options.variant == TextFieldVariant::Filled) {
    // Use DrawRoundedRectangle for filled background, not
    // DrawRoundedRectangleEx
    Renderer::DrawRoundedRectangle(inputBounds, cornerRadius, bgColor);
  }

  Color outlineColor = scheme.outline;
  float outlineWidth = 1.0f;
  if (state == ComponentState::Focused && !options.readOnly) {
    outlineColor = scheme.primary;
    outlineWidth = 2.0f;
  } else if (state == ComponentState::Hovered && !options.readOnly) {
    outlineWidth = 1.0f;
  }

  Renderer::DrawRoundedRectangleEx(inputBounds, cornerRadius, outlineColor,
                                   outlineWidth);

  if (options.variant == TextFieldVariant::Filled) {
    Renderer::DrawStateLayer(inputBounds, cornerRadius, bgColor, state);
  } else {
    Renderer::DrawStateLayer(inputBounds, cornerRadius,
                             ColorAlpha(scheme.surface, 0.0f), state);
  }

  float availableWidth = inputBounds.width - textPadding;

  // Update scroll offset if focused
  if (activeFieldId_ == fieldId) {
    std::string textBeforeCursor(buffer, fieldState.cursorPosition);
    Vector2 cursorSize = Renderer::MeasureText(textBeforeCursor.c_str(), 16.0f,
                                               FontWeight::Regular);
    float cursorX = cursorSize.x;

    // Scroll to keep cursor in view
    if (cursorX - fieldState.scrollOffset > availableWidth) {
      fieldState.scrollOffset = cursorX - availableWidth;
    } else if (cursorX - fieldState.scrollOffset < 0) {
      fieldState.scrollOffset = cursorX;
    }

    // Clamp scroll
    Vector2 totalSize =
        Renderer::MeasureText(buffer, 16.0f, FontWeight::Regular);
    float maxScroll = std::max(0.0f, totalSize.x - availableWidth);
    if (fieldState.scrollOffset > maxScroll)
      fieldState.scrollOffset = maxScroll;
    if (fieldState.scrollOffset < 0)
      fieldState.scrollOffset = 0;
  }

  float currentScroll =
      (activeFieldId_ == fieldId) ? fieldState.scrollOffset : 0.0f;

  BeginScissorMode((int)inputBounds.x, (int)inputBounds.y,
                   (int)inputBounds.width, (int)inputBounds.height);

  bool isEmpty = !buffer || strlen(buffer) == 0;
  bool showPlaceholder = isEmpty && !isFocused && options.placeholder;

  if (isFocused) {
    NormalizeSelection(fieldState.selectionStart, fieldState.selectionEnd);
    if (fieldState.selectionStart != -1 && fieldState.selectionEnd != -1) {
      DrawSelection(inputBounds, buffer, fieldState.selectionStart,
                    fieldState.selectionEnd, currentScroll,
                    textStartX - inputBounds.x);
    }
  }

  Vector2 textPos = {textStartX - currentScroll,
                     inputBounds.y + (inputBounds.height - 16.0f) / 2.0f};

  Color textColorToUse = scheme.onSurface;
  if (options.textColor.a > 0) {
    textColorToUse = options.textColor;
  }

  if (showPlaceholder) {
    Color placeholderColor = scheme.onSurfaceVariant;
    placeholderColor.a = 180;
    if (options.textColor.a > 0) {
      placeholderColor = options.textColor;
      placeholderColor.a = 180;
    }
    Renderer::DrawText(options.placeholder, textPos, 16.0f, placeholderColor,
                       FontWeight::Regular);
  } else if (!isEmpty) {
    const char *displayText = buffer;
    std::string maskedText;
    if (options.passwordMode) {
      maskedText = std::string(strlen(buffer), '*');
      displayText = maskedText.c_str();
    }
    Renderer::DrawText(displayText, textPos, 16.0f, textColorToUse,
                       FontWeight::Regular);
  }

  if (activeFieldId_ == fieldId && !options.readOnly) {
    bool shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

    if (IsKeyDown(KEY_LEFT)) {
      bool shouldMove = false;
      if (IsKeyPressed(KEY_LEFT)) {
        shouldMove = true;
        fieldState.arrowLeftTimer = GetTime() + 0.5;
      } else if (GetTime() > fieldState.arrowLeftTimer) {
        shouldMove = true;
        fieldState.arrowLeftTimer = GetTime() + 0.05;
      }

      if (shouldMove) {
        if (shiftPressed) {
          if (fieldState.selectionStart == -1) {
            fieldState.selectionStart = fieldState.cursorPosition;
            fieldState.selectionEnd = fieldState.cursorPosition;
          }
          if (fieldState.cursorPosition > 0) {
            fieldState.cursorPosition--;
            fieldState.selectionEnd = fieldState.cursorPosition;
          }
          NormalizeSelection(fieldState.selectionStart,
                             fieldState.selectionEnd);
        } else {
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          if (fieldState.cursorPosition > 0) {
            fieldState.cursorPosition--;
          }
        }
      }
    }

    if (IsKeyDown(KEY_RIGHT)) {
      bool shouldMove = false;
      if (IsKeyPressed(KEY_RIGHT)) {
        shouldMove = true;
        fieldState.arrowRightTimer = GetTime() + 0.5;
      } else if (GetTime() > fieldState.arrowRightTimer) {
        shouldMove = true;
        fieldState.arrowRightTimer = GetTime() + 0.05;
      }

      if (shouldMove) {
        int len = (int)strlen(buffer ? buffer : "");
        if (shiftPressed) {
          if (fieldState.selectionStart == -1) {
            fieldState.selectionStart = fieldState.cursorPosition;
            fieldState.selectionEnd = fieldState.cursorPosition;
          }
          if (fieldState.cursorPosition < len) {
            fieldState.cursorPosition++;
            fieldState.selectionEnd = fieldState.cursorPosition;
          }
          NormalizeSelection(fieldState.selectionStart,
                             fieldState.selectionEnd);
        } else {
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          if (fieldState.cursorPosition < len) {
            fieldState.cursorPosition++;
          }
        }
      }
    }

    int key = GetCharPressed();
    while (key > 0) {
      int len = (int)strlen(buffer ? buffer : "");
      if (len < bufferSize - 1 && key >= 32 && key <= 126) {
        if (fieldState.cursorPosition > len)
          fieldState.cursorPosition = len;
        if (fieldState.cursorPosition < 0)
          fieldState.cursorPosition = 0;

        std::string testBuffer = std::string(buffer ? buffer : "");
        if (fieldState.selectionStart != -1 && fieldState.selectionEnd != -1) {
          NormalizeSelection(fieldState.selectionStart,
                             fieldState.selectionEnd);
          testBuffer.erase(fieldState.selectionStart,
                           fieldState.selectionEnd - fieldState.selectionStart);
          fieldState.cursorPosition = fieldState.selectionStart;
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
        }

        testBuffer.insert(fieldState.cursorPosition, 1, (char)key);

        if (!options.inputMask ||
            ValidateInputMask(testBuffer.c_str(), options.inputMask)) {
          SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                        options.maxUndoHistory);

          if (fieldState.selectionStart != -1 &&
              fieldState.selectionEnd != -1) {
            NormalizeSelection(fieldState.selectionStart,
                               fieldState.selectionEnd);
            int len = (int)strlen(buffer);
            memmove(&buffer[fieldState.selectionStart],
                    &buffer[fieldState.selectionEnd],
                    (size_t)(len - fieldState.selectionEnd + 1));
            fieldState.cursorPosition = fieldState.selectionStart;
            fieldState.selectionStart = -1;
            fieldState.selectionEnd = -1;
          }

          int len = (int)strlen(buffer);
          if (fieldState.cursorPosition < len) {
            memmove(&buffer[fieldState.cursorPosition + 1],
                    &buffer[fieldState.cursorPosition],
                    (size_t)(len - fieldState.cursorPosition + 1));
          }
          buffer[fieldState.cursorPosition] = (char)key;
          fieldState.cursorPosition++;
          if (fieldState.cursorPosition < bufferSize) {
            buffer[fieldState.cursorPosition] = '\0';
          } else {
            buffer[bufferSize - 1] = '\0';
          }
          fieldState.lastValue = std::string(buffer);
        }
      }
      key = GetCharPressed();
    }

    bool showPlaceholder = isEmpty && options.placeholder;
    if (!showPlaceholder) {
      UpdateCursor(buffer, bufferSize, fieldState.lastBlinkTime);
      DrawCursor(inputBounds, buffer, fieldState.cursorPosition,
                 fieldState.scrollOffset, fieldState.lastBlinkTime,
                 textStartX - inputBounds.x);
    }

    NormalizeSelection(fieldState.selectionStart, fieldState.selectionEnd);
    bool hasSelection =
        fieldState.selectionStart != -1 && fieldState.selectionEnd != -1;

    if (IsKeyDown(KEY_BACKSPACE)) {
      bool shouldDelete = false;
      if (IsKeyPressed(KEY_BACKSPACE)) {
        shouldDelete = true;
        fieldState.backspaceTimer = GetTime() + 0.5;
      } else if (GetTime() > fieldState.backspaceTimer) {
        shouldDelete = true;
        fieldState.backspaceTimer = GetTime() + 0.05;
      }

      if (shouldDelete) {
        if (hasSelection) {
          SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                        options.maxUndoHistory);
          int len = (int)strlen(buffer);
          memmove(&buffer[fieldState.selectionStart],
                  &buffer[fieldState.selectionEnd],
                  (size_t)(len - fieldState.selectionEnd + 1));
          fieldState.cursorPosition = fieldState.selectionStart;
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          fieldState.lastValue = std::string(buffer);
        } else if (fieldState.cursorPosition > 0) {
          SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                        options.maxUndoHistory);
          int len = (int)strlen(buffer);
          memmove(&buffer[fieldState.cursorPosition - 1],
                  &buffer[fieldState.cursorPosition],
                  (size_t)(len - fieldState.cursorPosition + 1));
          fieldState.cursorPosition--;
          fieldState.lastValue = std::string(buffer);
        }
      }
    }

    if (IsKeyPressed(KEY_DELETE)) {
      if (hasSelection) {
        SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                      options.maxUndoHistory);
        int len = (int)strlen(buffer);
        memmove(&buffer[fieldState.selectionStart],
                &buffer[fieldState.selectionEnd],
                (size_t)(len - fieldState.selectionEnd + 1));
        fieldState.cursorPosition = fieldState.selectionStart;
        fieldState.selectionStart = -1;
        fieldState.selectionEnd = -1;
        fieldState.lastValue = std::string(buffer);
      } else if (fieldState.cursorPosition < (int)strlen(buffer)) {
        SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                      options.maxUndoHistory);
        int len = (int)strlen(buffer);
        memmove(&buffer[fieldState.cursorPosition],
                &buffer[fieldState.cursorPosition + 1],
                (size_t)(len - fieldState.cursorPosition));
        fieldState.lastValue = std::string(buffer);
      }
    }

    bool controlPressed =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
        IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (controlPressed && IsKeyPressed(KEY_V)) {
      const char *clipboard = GetClipboardText();
      if (clipboard != NULL) {
        int clipLen = (int)strlen(clipboard);
        int currentLen = (int)strlen(buffer ? buffer : "");
        int available = bufferSize - 1 - currentLen;

        if (available > 0 && clipLen > 0) {
          std::string testBuffer = std::string(buffer ? buffer : "");
          if (hasSelection) {
            testBuffer.erase(fieldState.selectionStart,
                             fieldState.selectionEnd -
                                 fieldState.selectionStart);
            testBuffer.insert(fieldState.selectionStart, clipboard,
                              std::min(clipLen, available));
          } else {
            testBuffer.insert(fieldState.cursorPosition, clipboard,
                              std::min(clipLen, available));
          }

          if (!options.inputMask ||
              ValidateInputMask(testBuffer.c_str(), options.inputMask)) {
            SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                          options.maxUndoHistory);

            if (hasSelection) {
              int len = (int)strlen(buffer);
              memmove(&buffer[fieldState.selectionStart],
                      &buffer[fieldState.selectionEnd],
                      (size_t)(len - fieldState.selectionEnd + 1));
              fieldState.cursorPosition = fieldState.selectionStart;
              fieldState.selectionStart = -1;
              fieldState.selectionEnd = -1;
            }

            int toCopy = std::min(clipLen, available);
            int currentLen = (int)strlen(buffer);

            if (fieldState.cursorPosition < currentLen) {
              memmove(&buffer[fieldState.cursorPosition + toCopy],
                      &buffer[fieldState.cursorPosition],
                      (size_t)(currentLen - fieldState.cursorPosition + 1));
            }

            strncpy(&buffer[fieldState.cursorPosition], clipboard, toCopy);
            fieldState.cursorPosition += toCopy;
            buffer[currentLen + toCopy] = '\0';
            fieldState.lastValue = std::string(buffer);
          }
        }
      }
    }

    if (options.maxUndoHistory > 0) {
      if (IsKeyPressed(KEY_Z) && controlPressed && !IsKeyDown(KEY_LEFT_SHIFT) &&
          !IsKeyDown(KEY_RIGHT_SHIFT)) {
        if (fieldState.undoIndex > 0) {
          fieldState.isUndoRedoOperation = true;
          fieldState.undoIndex--;
          strncpy(buffer, fieldState.undoHistory[fieldState.undoIndex].c_str(),
                  bufferSize - 1);
          buffer[bufferSize - 1] = '\0';
          fieldState.cursorPosition = (int)strlen(buffer);
          fieldState.lastValue = std::string(buffer);
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          fieldState.isUndoRedoOperation = false;
        }
      } else if (IsKeyPressed(KEY_Z) && controlPressed &&
                 (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))) {
        if (fieldState.undoIndex < (int)fieldState.undoHistory.size() - 1) {
          fieldState.isUndoRedoOperation = true;
          fieldState.undoIndex++;
          strncpy(buffer, fieldState.undoHistory[fieldState.undoIndex].c_str(),
                  bufferSize - 1);
          buffer[bufferSize - 1] = '\0';
          fieldState.cursorPosition = (int)strlen(buffer);
          fieldState.lastValue = std::string(buffer);
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          fieldState.isUndoRedoOperation = false;
        }
      }
    }

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
      fieldState.lastValue = std::string(buffer ? buffer : "");
      activeFieldId_ = -1;
      fieldState.selectionStart = -1;
      fieldState.selectionEnd = -1;
      return true;
    }
  }

  EndScissorMode();

  if (options.leadingIcon) {
    Color iconColor = scheme.onSurfaceVariant;
    if (options.iconColor.a > 0) {
      iconColor = options.iconColor;
    } else if (CheckCollisionPointRec(mousePos, leadingIconBounds)) {
      iconColor = scheme.onSurface;
    }
    IconComponent::Render(options.leadingIcon, leadingIconBounds,
                          IconVariation::Outlined, iconColor);
  }

  if (options.trailingIcon) {
    Color iconColor = scheme.onSurfaceVariant;
    if (options.iconColor.a > 0) {
      iconColor = options.iconColor;
    } else if (CheckCollisionPointRec(mousePos, trailingIconBounds)) {
      iconColor = scheme.onSurface;
    }
    IconComponent::Render(options.trailingIcon, trailingIconBounds,
                          IconVariation::Outlined, iconColor);
  }

#if RAYM3_USE_INPUT_LAYERS
  if (isFocused || isHovered || clickedInField) {
    InputLayerManager::ConsumeInput();
  }
#endif

  return false;
}

void TextFieldComponent::UpdateCursor(char *buffer, int bufferSize,
                                      float &lastBlinkTime) {
  float currentTime = GetTime();
  if (currentTime - lastBlinkTime > 0.5f) {
    lastBlinkTime = currentTime;
  }
}

void TextFieldComponent::DrawCursor(Rectangle bounds, const char *text,
                                    int position, float scrollOffset,
                                    float lastBlinkTime, float textStartX) {
  float currentTime = GetTime();
  float blinkCycle = (currentTime - lastBlinkTime) * 2.0f;
  bool showCursor = ((int)blinkCycle % 2 == 0);

  if (showCursor || IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_LEFT) ||
      IsKeyDown(KEY_RIGHT)) {
    float cursorX = bounds.x + textStartX - scrollOffset;
    if (text && position > 0 && position <= (int)strlen(text)) {
      std::string textBeforeCursor(text, position);
      Vector2 textSize = Renderer::MeasureText(textBeforeCursor.c_str(), 16.0f,
                                               FontWeight::Regular);
      cursorX += textSize.x;
    }

    Color cursorColor = Theme::GetColorScheme().onSurface;
    float cursorHeight = bounds.height - 8.0f;
    DrawLine((int)cursorX, (int)(bounds.y + 4), (int)cursorX,
             (int)(bounds.y + 4 + cursorHeight), cursorColor);
  }
}

} // namespace raym3
