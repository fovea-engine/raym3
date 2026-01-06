#pragma once

#include <cstdint>
#include <raylib.h>

namespace raym3 {

enum class FontWeight { Thin, Light, Regular, Medium, Bold, Black };

enum class FontStyle { Normal, Italic };

enum class LayoutDirection { Row, Column };

enum class JustifyContent {
  FlexStart,
  FlexEnd,
  Center,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly
};

enum class AlignItems { FlexStart, FlexEnd, Center, Stretch, Baseline };

enum class TextAlignment { Left, Center, Right };

enum class ButtonVariant { Text, Filled, Outlined, Tonal, Elevated };

enum class ComponentState { Default, Hovered, Pressed, Focused, Disabled };

enum class CardVariant { Elevated, Filled, Outlined };

enum class TextFieldVariant { Filled, Outlined };

enum class IconVariation { Filled, Outlined, Round, Sharp, TwoTone };

struct TypographyScale {
  float displayLarge;
  float displayMedium;
  float displaySmall;
  float headlineLarge;
  float headlineMedium;
  float headlineSmall;
  float titleLarge;
  float titleMedium;
  float titleSmall;
  float labelLarge;
  float labelMedium;
  float labelSmall;
  float bodyLarge;
  float bodyMedium;
  float bodySmall;
};

struct ShapeTokens {
  float cornerNone;
  float cornerSmall;
  float cornerMedium;
  float cornerLarge;
  float cornerExtraLarge;
};

struct ButtonOptions {
  Color backgroundColor = {0, 0, 0, 0};
  Color outlineColor = {0, 0, 0, 0};
  Color textColor = {0, 0, 0, 0};
  bool drawOutline = true;
  bool drawBackground = true;
};

struct TextFieldOptions {
  TextFieldVariant variant = TextFieldVariant::Outlined;
  const char *placeholder = nullptr;
  bool passwordMode = false;
  bool readOnly = false;
  bool disabled = false;
  const char *inputMask = nullptr;
  int maxUndoHistory = 15;
  const char *leadingIcon = nullptr;
  const char *trailingIcon = nullptr;
  bool (*onLeadingIconClick)() = nullptr;
  bool (*onTrailingIconClick)() = nullptr;
  Color backgroundColor = {0, 0, 0, 0}; // If alpha > 0, use as background color
  Color outlineColor = {0, 0, 0, 0};    // If alpha > 0, use as outline color
  Color textColor = {0, 0, 0, 0};       // If alpha > 0, use as text color
  Color iconColor = {0, 0, 0, 0};       // If alpha > 0, use as icon color
  bool drawOutline = true;
  bool drawBackground = true;
};

struct SliderOptions {
  const char *startIcon = nullptr;
  const char *endIcon = nullptr;
  const char *startText = nullptr;
  const char *endText = nullptr;
  bool showEndDot = true; // Defaults to true, but if endIcon/endText is set,
                          // logic will likely disable unless forced
  bool showValueIndicator = false;  // Show value in a bubble above thumb
  const char *valueFormat = "%.0f"; // Format string for value indicator
  Color activeTrackColor = {0, 0, 0, 0};
  Color inactiveTrackColor = {0, 0, 0, 0};
  Color handleColor = {0, 0, 0, 0};
  // M3 Expressive features
  bool showStopIndicators = false; // Show dots at min/max positions
  float stepValue = 0.0f;          // 0 = continuous, >0 = discrete mode
  bool showTickMarks = false;      // Show tick marks for discrete mode
};

struct RangeSliderOptions {
  bool showValueIndicators = false;
  const char *valueFormat = "%.0f";
  Color activeTrackColor = {0, 0, 0, 0};   // Fill between thumbs
  Color inactiveTrackColor = {0, 0, 0, 0}; // Inactive portions
  Color handleColor = {0, 0, 0, 0};
  bool showStopIndicators = false;
  float stepValue = 0.0f;     // 0 = continuous, >0 = discrete
  bool showTickMarks = false; // Show tick marks for discrete mode
  float minDistance = 0.0f;   // Minimum distance between adjacent thumbs
};

} // namespace raym3
