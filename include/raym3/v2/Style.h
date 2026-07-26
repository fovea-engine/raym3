#pragma once

#include "raym3/types.h"
#include "raym3/v2/TextEngine.h"
#include <optional>
#include <vector>
#include <raylib.h>

namespace raym3::v2 {

enum class Display { Flex, None, Contents };
enum class FlexDirection { Row, Column, RowReverse, ColumnReverse };
enum class FlexWrap { NoWrap, Wrap, WrapReverse };
enum class Justify { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class Align { Auto, FlexStart, FlexEnd, Center, Stretch, Baseline };
enum class PositionType { Relative, Absolute, Static, Fixed };
enum class Overflow { Visible, Hidden, Scroll };
enum class PointerEvents { Auto, None };
enum class Easing { Linear, Standard, Emphasized, EmphasizedDecelerate, EmphasizedAccelerate };
enum class MotionRole {
  Standard,
  ExpressiveFastSpatial,
  ExpressiveFastEffects,
  ShapeMorph,
  ActiveIndicator,
  ContainerTransform,
  Enter,
  Exit,
  Loading,
  Drag
};

struct EdgeValues {
  std::optional<float> all;
  std::optional<float> horizontal;
  std::optional<float> vertical;
  std::optional<float> top;
  std::optional<float> right;
  std::optional<float> bottom;
  std::optional<float> left;

  std::optional<bool> allAuto;
  std::optional<bool> horizontalAuto;
  std::optional<bool> verticalAuto;
  std::optional<bool> topAuto;
  std::optional<bool> rightAuto;
  std::optional<bool> bottomAuto;
  std::optional<bool> leftAuto;

  float Top(float fallback = 0.0f) const;
  float Right(float fallback = 0.0f) const;
  float Bottom(float fallback = 0.0f) const;
  float Left(float fallback = 0.0f) const;
  bool TopIsAuto() const;
  bool RightIsAuto() const;
  bool BottomIsAuto() const;
  bool LeftIsAuto() const;
};

struct TextStyle {
  std::optional<float> fontSize;
  std::optional<float> lineHeight;
  std::optional<float> letterSpacing;
  std::optional<FontWeight> weight;
  std::optional<FontStyle> fontStyle;
  std::optional<TextAlignment> alignment;
  std::optional<Color> color;
  std::optional<std::string> fontFamily; // named font registered via registerFont()
  std::optional<WhiteSpace> whiteSpace;
  std::optional<WordBreak> wordBreak;
};

struct LinearGradientStop {
  Color color;
  float position = 0.0f;
};

struct LinearGradient {
  float angleDegrees = 180.0f;
  std::vector<LinearGradientStop> stops;
};

struct BoxShadow {
  float offsetX = 0.0f;
  float offsetY = 0.0f;
  float blurRadius = 0.0f;
  float spreadRadius = 0.0f;
  Color color = {0, 0, 0, 0};
  bool inset = false;
};

// ─── CSS transitions ─────────────────────────────────────────────────────────
// Float-valued style properties a CSS `transition` declaration can animate.
enum class TransitionProperty : uint8_t {
  MarginTop, MarginRight, MarginBottom, MarginLeft,
  InsetTop, InsetRight, InsetBottom, InsetLeft,
  Opacity, TranslateX, TranslateY, Scale, Rotation,
  Width, Height,
  Count
};

// One parsed `transition` segment: which property, timing, and easing.
struct TransitionEntry {
  TransitionProperty property = TransitionProperty::Count;
  float durationMs = 0.0f;
  float delayMs = 0.0f;
  // cubic-bezier control points; default = CSS `ease`.
  float x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1.0f;
};

// In-flight interpolation state for one property on one node.
struct ActiveTransition {
  TransitionProperty property = TransitionProperty::Count;
  float from = 0.0f;
  float to = 0.0f;
  float durationMs = 0.0f;
  float delayMs = 0.0f;
  float elapsedMs = 0.0f;
  float x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1.0f;
};

// ─── CSS animations (@keyframes) ─────────────────────────────────────────────
enum class AnimationDirection : uint8_t { Normal, Reverse, Alternate, AlternateReverse };
enum class AnimationFill : uint8_t { None, Forwards, Backwards, Both };

// One parsed `animation` segment: which @keyframes, timing, and playback.
struct AnimationEntry {
  std::string name;
  float durationMs = 0.0f;
  float delayMs = 0.0f;
  float iterationCount = 1.0f;   // <0 = infinite
  AnimationDirection direction = AnimationDirection::Normal;
  AnimationFill fill = AnimationFill::None;
  // cubic-bezier control points; default = CSS `ease`.
  float x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1.0f;
};

// One keyframe stop: offset in [0,1] and the property values set at it.
struct Keyframe {
  float offset = 0.0f;
  std::vector<std::pair<TransitionProperty, float>> values;
};

// In-flight playback state for one running animation on one node. The resolved
// keyframe track is snapshotted at start so a later stylesheet reload can't
// mutate a playing animation mid-flight.
struct ActiveAnimation {
  std::string name;
  float durationMs = 0.0f;
  float delayMs = 0.0f;
  float iterationCount = 1.0f;
  AnimationDirection direction = AnimationDirection::Normal;
  AnimationFill fill = AnimationFill::None;
  float x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1.0f;
  float elapsedMs = 0.0f;
  bool finished = false;
  std::vector<Keyframe> keyframes;                 // resolved, sorted by offset
  std::vector<TransitionProperty> animatedProps;    // union of props across stops
};

struct Style {
  std::optional<Display> display;
  std::optional<FlexDirection> flexDirection;
  std::optional<FlexWrap> flexWrap;
  std::optional<Justify> justifyContent;
  std::optional<Align> alignItems;
  std::optional<Align> alignSelf;
  std::optional<PositionType> position;
  std::optional<Overflow> overflow;
  std::optional<PointerEvents> pointerEvents;

  std::optional<float> width;
  std::optional<float> height;
  std::optional<float> minWidth;
  std::optional<float> minHeight;
  std::optional<float> maxWidth;
  std::optional<float> maxHeight;
  std::optional<float> flexGrow;
  std::optional<float> flexShrink;
  std::optional<float> flexBasis;
  std::optional<float> gap;
  std::optional<float> rowGap;
  std::optional<float> columnGap;

  EdgeValues margin;
  EdgeValues padding;
  EdgeValues inset;

  std::optional<Color> backgroundColor;
  std::optional<LinearGradient> backgroundGradient;
  // Hover/press overlay tint (RGB), alpha = press intensity. On plain
  // interactive Views this drives the hover/press dim; unset = a sensible
  // default derived from the content color.
  std::optional<Color> stateLayerColor;
  // Ink-ripple color for interactive Views (CSS `ripple-color`). Presence also
  // opts a plain View+onPress into ripples.
  std::optional<Color> rippleColor;
  std::optional<Color> borderColor;
  std::optional<float> borderWidth;
  std::optional<float> borderRadius;
  std::vector<BoxShadow> boxShadows;
  std::optional<float> backdropBlur;
  std::optional<float> opacity;
  // Backdrop scrim alpha for modal overlays (Dialog/BottomSheet/pickers). The
  // scrim paints on its own layer one z-step below the modal container, so this
  // only affects the dimmed backdrop, never the panel. 0 = fully transparent.
  std::optional<float> scrimOpacity;
  std::optional<float> elevation;
  std::optional<float> translateX;
  std::optional<float> translateY;
  std::optional<float> scale;
  std::optional<float> rotation; // degrees, clockwise, about the node's center

  // CSS `transition` spec. nullopt = not specified (merge inherits the base
  // spec); empty vector = explicit `transition: none` (cancels in-flight).
  std::optional<std::vector<TransitionEntry>> transitions;

  // CSS `animation` spec. nullopt = not specified (merge inherits); empty
  // vector = explicit `animation: none` (cancels running animations).
  std::optional<std::vector<AnimationEntry>> animations;

  TextStyle text;
};

struct StateStyles {
  std::optional<Style> hovered;
  std::optional<Style> pressed;
  std::optional<Style> focused;
  std::optional<Style> disabled;
};

struct MotionSpec {
  bool enabled = true;
  float durationSeconds = 0.2f;
  Easing easing = Easing::Standard;
  MotionRole role = MotionRole::Standard;
  bool spring = false;
  float dampingRatio = 1.0f;
};

Style MergeStyles(const Style &base, const Style &overrideStyle);
TextStyle MergeTextStyles(const TextStyle &base, const TextStyle &overrideStyle);
Style ResolveStateStyle(const Style &base, const StateStyles &states,
                        ComponentState state);

} // namespace raym3::v2
