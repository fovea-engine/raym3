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
  double undoTimer = 0.0;
  double redoTimer = 0.0;

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

static int GetPrevWordPos(const char *text, int pos) {
  if (pos <= 0 || !text)
    return 0;
  int i = pos - 1;
  while (i > 0 && isspace((unsigned char)text[i]))
    i--;
  while (i > 0 && !isspace((unsigned char)text[i]) &&
         !ispunct((unsigned char)text[i]))
    i--;
  if (isspace((unsigned char)text[i]) || ispunct((unsigned char)text[i]))
    return i + 1;
  return i;
}

static int GetNextWordPos(const char *text, int pos) {
  if (!text)
    return 0;
  int len = (int)strlen(text);
  if (pos >= len)
    return len;
  int i = pos;
  while (i < len && !isspace((unsigned char)text[i]) &&
         !ispunct((unsigned char)text[i]))
    i++;
  while (i < len && isspace((unsigned char)text[i]))
    i++;
  return i;
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
  float textPadding = basePadding * 2.0f;

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
  float textEndX = inputBounds.x + inputBounds.width - basePadding;
  if (options.leadingIcon) {
    textStartX = inputBounds.x + iconPadding + iconSize + iconPadding;
  }
  if (options.trailingIcon) {
    textEndX = inputBounds.x + inputBounds.width - iconPadding - iconSize -
               iconPadding;
  }
  float availableWidth = textEndX - textStartX;

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
    fieldState.lastBlinkTime = GetTime(); // Always reset blink on click

    if (!wasFocused) {
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

  if (options.variant == TextFieldVariant::Filled && options.drawBackground) {
    Color bgColor = (options.backgroundColor.a > 0)
                        ? options.backgroundColor
                        : scheme.surfaceContainerHighest;
    // Subtler top rounding, flat bottom corners
    float topRadius = 4.0f;
    Renderer::DrawRoundedRectangle(inputBounds, topRadius, bgColor);
    // Overlap with a rectangle on the bottom half to flatten corners
    Rectangle bottomHalf = {inputBounds.x,
                            inputBounds.y + inputBounds.height / 2.0f,
                            inputBounds.width, inputBounds.height / 2.0f};
    DrawRectangleRec(bottomHalf, bgColor);
  }

  Color outlineColor =
      (options.outlineColor.a > 0) ? options.outlineColor : scheme.outline;
  float outlineWidth = 1.0f;
  if (state == ComponentState::Focused && !options.readOnly) {
    outlineColor = scheme.primary;
    outlineWidth = 2.0f;
  } else if (state == ComponentState::Hovered && !options.readOnly) {
    outlineWidth = 1.0f;
  }

  if (options.drawOutline) {
    if (options.variant == TextFieldVariant::Outlined) {
      Renderer::DrawRoundedRectangleEx(inputBounds, cornerRadius, outlineColor,
                                       outlineWidth);
    } else {
      // Filled bottom line
      Rectangle bottomLine = {inputBounds.x,
                              inputBounds.y + inputBounds.height - 2,
                              inputBounds.width, 2};
      DrawRectangleRec(bottomLine, outlineColor);
    }
  } else {
    Renderer::DrawStateLayer(inputBounds, cornerRadius,
                             ColorAlpha(scheme.surface, 0.0f), state);
  }

  float availableWidthForScroll = textEndX - textStartX;

  // Update scroll offset if focused
  if (activeFieldId_ == fieldId) {
    std::string textBeforeCursor(buffer, fieldState.cursorPosition);
    Vector2 cursorSize = Renderer::MeasureText(textBeforeCursor.c_str(), 16.0f,
                                               FontWeight::Regular);
    float cursorX = cursorSize.x;

    // Scroll to keep cursor in view
    if (cursorX - fieldState.scrollOffset > availableWidthForScroll) {
      fieldState.scrollOffset = cursorX - availableWidthForScroll;
    } else if (cursorX - fieldState.scrollOffset < 0) {
      fieldState.scrollOffset = cursorX;
    }

    // Clamp scroll
    Vector2 totalSize =
        Renderer::MeasureText(buffer, 16.0f, FontWeight::Regular);
    float maxScroll = std::max(0.0f, totalSize.x - availableWidthForScroll);
    if (fieldState.scrollOffset > maxScroll)
      fieldState.scrollOffset = maxScroll;
    if (fieldState.scrollOffset < 0)
      fieldState.scrollOffset = 0;
  }

  float currentScroll =
      (activeFieldId_ == fieldId) ? fieldState.scrollOffset : 0.0f;

  // Expand scissor by 1px on left to ensure cursor at position 0 is visible
  BeginScissorMode((int)textStartX - 1, (int)inputBounds.y,
                   (int)availableWidth + 1, (int)inputBounds.height);

  bool isEmpty = !buffer || strlen(buffer) == 0;
  bool showPlaceholder = isEmpty && !isFocused && options.placeholder;

  // Only render raym3 text/cursor/selection if native input is NOT active FOR
  // THIS FIELD When native input is active, the NSTextField handles all visual
  // feedback
  bool skipTextRendering = false;

  if (isFocused && !skipTextRendering) {
    int drawStart = fieldState.selectionStart;
    int drawEnd = fieldState.selectionEnd;
    NormalizeSelection(drawStart, drawEnd);
    if (drawStart != -1 && drawEnd != -1) {
      DrawSelection(inputBounds, buffer, drawStart, drawEnd, currentScroll,
                    textStartX - inputBounds.x);
    }
  }

  Vector2 textPos = {textStartX - currentScroll,
                     inputBounds.y + (inputBounds.height - 16.0f) / 2.0f};

  Color textColorToUse = scheme.onSurface;
  if (options.textColor.a > 0) {
    textColorToUse = options.textColor;
  }

  if (!skipTextRendering) {
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
  }

  if (activeFieldId_ == fieldId && !options.readOnly && !skipTextRendering) {
    bool shiftPressed = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool isAltDown = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
    bool isCtrlDown =
        IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool isSuperDown = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool isCmdDown = isCtrlDown || isSuperDown;

    if (IsKeyDown(KEY_LEFT)) {
      bool shouldMove = false;
      if (IsKeyPressed(KEY_LEFT)) {
        shouldMove = true;
        fieldState.arrowLeftTimer = GetTime() + 0.5;
        fieldState.lastBlinkTime = GetTime(); // Reset blink
      } else if (GetTime() > fieldState.arrowLeftTimer) {
        shouldMove = true;
        fieldState.arrowLeftTimer = GetTime() + 0.05;
      }

      if (shouldMove) {
        int targetPos = fieldState.cursorPosition;
        if (isCmdDown || IsKeyDown(KEY_HOME)) {
          targetPos = 0;
        } else if (isAltDown) {
          targetPos = GetPrevWordPos(buffer, targetPos);
        } else if (targetPos > 0) {
          targetPos--;
        }

        if (shiftPressed) {
          if (fieldState.selectionStart == -1) {
            fieldState.selectionStart = fieldState.cursorPosition;
          }
          fieldState.selectionEnd = targetPos;
          fieldState.cursorPosition = targetPos;
          // Do NOT normalize selection in state here, keep directionality
        } else {
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          fieldState.cursorPosition = targetPos;
        }
      }
    }

    if (IsKeyDown(KEY_RIGHT)) {
      bool shouldMove = false;
      if (IsKeyPressed(KEY_RIGHT)) {
        shouldMove = true;
        fieldState.arrowRightTimer = GetTime() + 0.5;
        fieldState.lastBlinkTime = GetTime(); // Reset blink
      } else if (GetTime() > fieldState.arrowRightTimer) {
        shouldMove = true;
        fieldState.arrowRightTimer = GetTime() + 0.05;
      }

      if (shouldMove) {
        int len = (int)strlen(buffer ? buffer : "");
        int targetPos = fieldState.cursorPosition;

        if (isCmdDown || IsKeyDown(KEY_END)) {
          targetPos = len;
        } else if (isAltDown) {
          targetPos = GetNextWordPos(buffer, targetPos);
        } else if (targetPos < len) {
          targetPos++;
        }

        if (shiftPressed) {
          if (fieldState.selectionStart == -1) {
            fieldState.selectionStart = fieldState.cursorPosition;
          }
          fieldState.selectionEnd = targetPos;
          fieldState.cursorPosition = targetPos;
          // Do NOT normalize selection in state here, keep directionality
        } else {
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          fieldState.cursorPosition = targetPos;
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

        bool selectionDeletedThisLoop = false;
        // If there's a selection, delete it first
        if (fieldState.selectionStart != -1 && fieldState.selectionEnd != -1) {
          int sStart = fieldState.selectionStart;
          int sEnd = fieldState.selectionEnd;
          NormalizeSelection(sStart, sEnd);

          SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                        options.maxUndoHistory);

          int currentLen = (int)strlen(buffer);
          memmove(&buffer[sStart], &buffer[sEnd],
                  (size_t)(currentLen - sEnd + 1));
          fieldState.cursorPosition = sStart;
          fieldState.selectionStart = -1;
          fieldState.selectionEnd = -1;
          len = (int)strlen(buffer);
          selectionDeletedThisLoop = true;
        }

        if (len < bufferSize - 1) {
          if (!selectionDeletedThisLoop) {
            SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                          options.maxUndoHistory);
          }

          if (fieldState.cursorPosition < len) {
            memmove(&buffer[fieldState.cursorPosition + 1],
                    &buffer[fieldState.cursorPosition],
                    (size_t)(len - fieldState.cursorPosition + 1));
          }
          buffer[fieldState.cursorPosition] = (char)key;
          fieldState.cursorPosition++;
          // Ensure null termination at new end of string
          int newLen = (int)strlen(buffer);
          if (newLen >= bufferSize) {
            buffer[bufferSize - 1] = '\0';
          }
          fieldState.lastValue = std::string(buffer);
        }
      }
      key = GetCharPressed();
    }

    if (!skipTextRendering) {
      UpdateCursor(buffer, bufferSize, fieldState.lastBlinkTime);
      DrawCursor(inputBounds, buffer, fieldState.cursorPosition,
                 fieldState.scrollOffset, fieldState.lastBlinkTime,
                 textStartX - inputBounds.x, bgColor);
    }

    int selStart = fieldState.selectionStart;
    int selEnd = fieldState.selectionEnd;
    NormalizeSelection(selStart, selEnd);
    bool hasSelection = selStart != -1 && selEnd != -1;

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
        fieldState.lastBlinkTime = GetTime(); // Reset blink
        if (hasSelection) {
          int sStart = fieldState.selectionStart;
          int sEnd = fieldState.selectionEnd;
          NormalizeSelection(sStart, sEnd);

          SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                        options.maxUndoHistory);

          int len = (int)strlen(buffer);
          memmove(&buffer[sStart], &buffer[sEnd], (size_t)(len - sEnd + 1));
          fieldState.cursorPosition = sStart;
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

    // Select All (Cmd+A)
    if (controlPressed && IsKeyPressed(KEY_A)) {
      fieldState.selectionStart = 0;
      fieldState.selectionEnd = (int)strlen(buffer ? buffer : "");
      fieldState.cursorPosition = fieldState.selectionEnd;
    }

    // Copy (Cmd+C)
    if (controlPressed && IsKeyPressed(KEY_C)) {
      NormalizeSelection(fieldState.selectionStart, fieldState.selectionEnd);
      if (fieldState.selectionStart != -1 && fieldState.selectionEnd != -1) {
        std::string selectedText(buffer + fieldState.selectionStart,
                                 fieldState.selectionEnd -
                                     fieldState.selectionStart);
        SetClipboardText(selectedText.c_str());
      }
    }

    // Cut (Cmd+X)
    if (controlPressed && IsKeyPressed(KEY_X) && !options.readOnly) {
      NormalizeSelection(fieldState.selectionStart, fieldState.selectionEnd);
      if (fieldState.selectionStart != -1 && fieldState.selectionEnd != -1) {
        std::string selectedText(buffer + fieldState.selectionStart,
                                 fieldState.selectionEnd -
                                     fieldState.selectionStart);
        SetClipboardText(selectedText.c_str());

        // Delete selection
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
      }
    }

    if (controlPressed && IsKeyPressed(KEY_V)) {
      const char *clipboard = GetClipboardText();
      if (clipboard != NULL && !options.readOnly) {
        int clipLen = (int)strlen(clipboard);
        int currentLen = (int)strlen(buffer ? buffer : "");

        int sStart = fieldState.selectionStart;
        int sEnd = fieldState.selectionEnd;
        NormalizeSelection(sStart, sEnd);
        bool hasSel = sStart != -1 && sEnd != -1;

        int available =
            bufferSize - 1 - currentLen + (hasSel ? (sEnd - sStart) : 0);

        if (available > 0 && clipLen > 0) {
          int toCopy = std::min(clipLen, available);

          // Test with input mask
          std::string testBuffer = std::string(buffer ? buffer : "");
          if (hasSel) {
            testBuffer.erase(sStart, sEnd - sStart);
            testBuffer.insert(sStart, clipboard, toCopy);
          } else {
            testBuffer.insert(fieldState.cursorPosition, clipboard, toCopy);
          }

          if (!options.inputMask ||
              ValidateInputMask(testBuffer.c_str(), options.inputMask)) {
            SaveToHistory(fieldState, std::string(buffer ? buffer : ""),
                          options.maxUndoHistory);

            if (hasSel) {
              int len = (int)strlen(buffer);
              memmove(&buffer[sStart], &buffer[sEnd], (size_t)(len - sEnd + 1));
              fieldState.cursorPosition = sStart;
              fieldState.selectionStart = -1;
              fieldState.selectionEnd = -1;
            }

            int len = (int)strlen(buffer);
            if (fieldState.cursorPosition < len) {
              memmove(&buffer[fieldState.cursorPosition + toCopy],
                      &buffer[fieldState.cursorPosition],
                      (size_t)(len - fieldState.cursorPosition + 1));
            }

            memcpy(&buffer[fieldState.cursorPosition], clipboard, toCopy);
            fieldState.cursorPosition += toCopy;
            fieldState.lastValue = std::string(buffer);
          }
        }
      }
    }

    if (options.maxUndoHistory > 0) {
      bool shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

      // Undo (Cmd+Z without shift)
      if (IsKeyDown(KEY_Z) && controlPressed && !shiftDown) {
        bool shouldUndo = false;
        if (IsKeyPressed(KEY_Z)) {
          shouldUndo = true;
          fieldState.undoTimer = GetTime() + 0.5;
        } else if (GetTime() > fieldState.undoTimer) {
          shouldUndo = true;
          fieldState.undoTimer = GetTime() + 0.05;
        }

        if (shouldUndo && fieldState.undoIndex > 0) {
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
      }

      // Redo (Cmd+Shift+Z or Cmd+Y)
      bool redoPressed = (IsKeyDown(KEY_Z) && controlPressed && shiftDown) ||
                         (IsKeyDown(KEY_Y) && controlPressed);
      if (redoPressed) {
        bool shouldRedo = false;
        if (IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_Y)) {
          shouldRedo = true;
          fieldState.redoTimer = GetTime() + 0.5;
        } else if (GetTime() > fieldState.redoTimer) {
          shouldRedo = true;
          fieldState.redoTimer = GetTime() + 0.05;
        }

        if (shouldRedo &&
            fieldState.undoIndex < (int)fieldState.undoHistory.size() - 1) {
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
  // No-op: Blinking is handled in DrawCursor based on time elapsed.
  // lastBlinkTime is only reset on user input to restart blink cycle.
}

void TextFieldComponent::DrawCursor(Rectangle bounds, const char *text,
                                    int position, float scrollOffset,
                                    float lastBlinkTime, float textStartX,
                                    Color bgColor) {
  float currentTime = GetTime();
  float blinkCycle = (currentTime - lastBlinkTime) * 2.0f;
  bool showCursor = ((int)blinkCycle % 2 == 0);

  // Show cursor if blinking is active (even cycle), OR if input was recently
  // active (less than 1 sec? no, better to rely on lastBlinkTime reset)
  // Actually, resetting lastBlinkTime makes the cycle start at 0, which is
  // even, so it shows immediately. We also force show if keys are down to
  // prevent flicker during rapid repeat
  if (showCursor || IsKeyDown(KEY_BACKSPACE) || IsKeyDown(KEY_LEFT) ||
      IsKeyDown(KEY_RIGHT)) {
    float cursorX = bounds.x + textStartX - scrollOffset;
    if (text && position > 0) {
      int textLen = (int)strlen(text);
      int pos = (position <= textLen) ? position : textLen;
      if (pos > 0) {
        std::string textBeforeCursor(text, pos);
        Vector2 textSize = Renderer::MeasureText(textBeforeCursor.c_str(),
                                                 16.0f, FontWeight::Regular);
        cursorX += textSize.x;
      }
    }

    Color cursorColor = Theme::GetColorScheme().onSurface;

    // If a custom background color is set, invert the cursor color
    if (bgColor.a > 0) {
      // Calculate luminance of background color
      float luminance =
          (0.299f * bgColor.r + 0.587f * bgColor.g + 0.114f * bgColor.b) /
          255.0f;

      // Invert based on luminance
      if (luminance > 0.5f) {
        // Light background -> dark cursor
        cursorColor = BLACK;
      } else {
        // Dark background -> light cursor
        cursorColor = WHITE;
      }
    }

    // Fixed height for cursor, centered vertically
    float cursorHeight = 16.0f;
    float cursorY = bounds.y + (bounds.height - cursorHeight) / 2.0f;

    DrawLine((int)cursorX, (int)cursorY, (int)cursorX,
             (int)(cursorY + cursorHeight), cursorColor);
  }
}

} // namespace raym3
