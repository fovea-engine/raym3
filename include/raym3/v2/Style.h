#pragma once

#include "raym3/types.h"
#include "raym3/v2/TextEngine.h"
#include <algorithm>
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
  // Unitless CSS `line-height: 1.5` — a multiple of the resolved font size.
  // Kept unresolved so a ratio declared in one rule still applies to a font
  // size declared in another (or set later from JS).
  std::optional<float> lineHeightRatio;
  // CSS `-webkit-line-clamp` / react-native `numberOfLines`: 0 = unlimited.
  std::optional<int> maxLines;
  // What to do with the overflow when maxLines clips: tail ellipsis or a hard cut.
  std::optional<TextOverflow> overflow;
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
  // Percentage dimensions (0..100), resolved by Yoga against the parent.
  // Kept separate from the absolute values above so a style can express
  // `width: 50%` without overloading the px field; the percent form wins when
  // both are set.
  std::optional<float> widthPercent;
  std::optional<float> heightPercent;
  std::optional<float> minWidthPercent;
  std::optional<float> minHeightPercent;
  std::optional<float> maxWidthPercent;
  std::optional<float> maxHeightPercent;
  std::optional<float> flexBasisPercent;
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
  // Per-edge overrides. Unset edges fall back to borderColor/borderWidth, so a
  // uniform border still needs only the two shared fields. Cards that light one
  // edge (`border-bottom: 1px solid …`, `border-left-color: …`) were previously
  // inexpressible: the shared fields painted all four sides.
  std::optional<Color> borderTopColor;
  std::optional<Color> borderRightColor;
  std::optional<Color> borderBottomColor;
  std::optional<Color> borderLeftColor;
  std::optional<float> borderTopWidth;
  std::optional<float> borderRightWidth;
  std::optional<float> borderBottomWidth;
  std::optional<float> borderLeftWidth;
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

// CSS `line-height` resolution: an explicit length wins, then a unitless ratio
// of the font size, then the engine default (~1.43em, CSS `normal`).
inline float ResolveLineHeight(const TextStyle &text, float fontSize) {
  if (text.lineHeight) return *text.lineHeight;
  if (text.lineHeightRatio) return *text.lineHeightRatio * fontSize;
  return std::max(fontSize + 4.0f, fontSize * 1.43f);
}

enum class BoxEdge { Top, Right, Bottom, Left };

// Per-edge border resolution: explicit edge value → shared value → fallback.
inline float ResolveBorderWidth(const Style &style, BoxEdge edge,
                                float fallback = 0.0f) {
  const std::optional<float> *perEdge = nullptr;
  switch (edge) {
  case BoxEdge::Top:    perEdge = &style.borderTopWidth; break;
  case BoxEdge::Right:  perEdge = &style.borderRightWidth; break;
  case BoxEdge::Bottom: perEdge = &style.borderBottomWidth; break;
  case BoxEdge::Left:   perEdge = &style.borderLeftWidth; break;
  }
  if (perEdge && *perEdge) return **perEdge;
  return style.borderWidth.value_or(fallback);
}

inline Color ResolveBorderColor(const Style &style, BoxEdge edge,
                                Color fallback) {
  const std::optional<Color> *perEdge = nullptr;
  switch (edge) {
  case BoxEdge::Top:    perEdge = &style.borderTopColor; break;
  case BoxEdge::Right:  perEdge = &style.borderRightColor; break;
  case BoxEdge::Bottom: perEdge = &style.borderBottomColor; break;
  case BoxEdge::Left:   perEdge = &style.borderLeftColor; break;
  }
  if (perEdge && *perEdge) return **perEdge;
  return style.borderColor.value_or(fallback);
}

// True when the four edges are not identical — the renderer then has to paint
// them one at a time instead of using the single rounded-rect stroke.
inline bool HasPerEdgeBorders(const Style &style) {
  return style.borderTopWidth || style.borderRightWidth ||
         style.borderBottomWidth || style.borderLeftWidth ||
         style.borderTopColor || style.borderRightColor ||
         style.borderBottomColor || style.borderLeftColor;
}

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
