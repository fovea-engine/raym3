#pragma once

#include <cstdint>
#include <functional>
#include <raylib.h>

namespace raym3 {

void RequestCursor(int cursor);

enum class TooltipPlacement {
  Auto,
  Above,
  Below,
  Left,
  Right
};

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

enum class TextFieldVariant { Filled, Outlined, Underline, Plain };

enum class IconVariation { Filled, Outlined, Round, Sharp, TwoTone };

enum class ChipVariant { Assist, Filter, Input, Suggestion };

enum class FabSize { Regular, Medium, Large };

enum class FabColor {
  PrimaryContainer,
  Primary,
  SecondaryContainer,
  Secondary,
  TertiaryContainer,
  Tertiary,
  Surface
};

enum class BadgeAlignment { TopEnd, TopStart };

enum class AppBarVariant { Small, CenterAligned, Medium, Large };

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
  float cornerExtraSmall;
  float cornerSmall;
  float cornerMedium;
  float cornerLarge;
  float cornerExtraLarge;
  float cornerFull;
};

struct ButtonOptions {
  Color backgroundColor = {0, 0, 0, 0};
  Color outlineColor = {0, 0, 0, 0};
  Color textColor = {0, 0, 0, 0};
  bool drawOutline = true;
  bool drawBackground = true;
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
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
  // When false, the borderless field paints no self state layer (hover/focus
  // highlight). Used when an interactive parent (e.g. a search bar) owns the
  // single highlight that should span the whole element.
  bool drawStateLayer = true;
  float borderRadius = -1.0f;           // If >= 0, overrides theme cornerRadius
  std::function<void()> onFocus;
  std::function<void()> onBlur;
};

struct SliderOptions {
  const char *startIcon = nullptr;
  const char *endIcon = nullptr;
  const char *startText = nullptr;
  const char *endText = nullptr;
  bool showEndDot = true;
  bool showValueIndicator = false;
  const char *valueFormat = "%.0f";
  Color activeTrackColor = {0, 0, 0, 0};
  Color inactiveTrackColor = {0, 0, 0, 0};
  Color handleColor = {0, 0, 0, 0};
  bool showStopIndicators = false;
  float stepValue = 0.0f;
  bool showTickMarks = false;
  std::function<void()> onRelease;
  
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

struct RangeSliderOptions {
  bool showValueIndicators = false;
  const char *valueFormat = "%.0f";
  Color activeTrackColor = {0, 0, 0, 0};
  Color inactiveTrackColor = {0, 0, 0, 0};
  Color handleColor = {0, 0, 0, 0};
  bool showStopIndicators = false;
  float stepValue = 0.0f;
  bool showTickMarks = false;
  float minDistance = 0.0f;
  
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

} // namespace raym3
