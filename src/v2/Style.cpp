#include "raym3/v2/Style.h"

namespace raym3::v2 {

float EdgeValues::Top(float fallback) const {
  if (top)
    return *top;
  if (vertical)
    return *vertical;
  if (all)
    return *all;
  return fallback;
}

float EdgeValues::Right(float fallback) const {
  if (right)
    return *right;
  if (horizontal)
    return *horizontal;
  if (all)
    return *all;
  return fallback;
}

float EdgeValues::Bottom(float fallback) const {
  if (bottom)
    return *bottom;
  if (vertical)
    return *vertical;
  if (all)
    return *all;
  return fallback;
}

float EdgeValues::Left(float fallback) const {
  if (left)
    return *left;
  if (horizontal)
    return *horizontal;
  if (all)
    return *all;
  return fallback;
}

static EdgeValues MergeEdges(const EdgeValues &base,
                             const EdgeValues &overrideStyle) {
  EdgeValues result = base;
  if (overrideStyle.all)
    result.all = overrideStyle.all;
  if (overrideStyle.horizontal)
    result.horizontal = overrideStyle.horizontal;
  if (overrideStyle.vertical)
    result.vertical = overrideStyle.vertical;
  if (overrideStyle.top)
    result.top = overrideStyle.top;
  if (overrideStyle.right)
    result.right = overrideStyle.right;
  if (overrideStyle.bottom)
    result.bottom = overrideStyle.bottom;
  if (overrideStyle.left)
    result.left = overrideStyle.left;
  return result;
}

TextStyle MergeTextStyles(const TextStyle &base,
                          const TextStyle &overrideStyle) {
  TextStyle result = base;
  if (overrideStyle.fontSize)
    result.fontSize = overrideStyle.fontSize;
  if (overrideStyle.lineHeight)
    result.lineHeight = overrideStyle.lineHeight;
  if (overrideStyle.letterSpacing)
    result.letterSpacing = overrideStyle.letterSpacing;
  if (overrideStyle.weight)
    result.weight = overrideStyle.weight;
  if (overrideStyle.fontStyle)
    result.fontStyle = overrideStyle.fontStyle;
  if (overrideStyle.alignment)
    result.alignment = overrideStyle.alignment;
  if (overrideStyle.color)
    result.color = overrideStyle.color;
  return result;
}

Style MergeStyles(const Style &base, const Style &overrideStyle) {
  Style result = base;

#define RAYM3_MERGE_FIELD(field)                                               \
  if (overrideStyle.field)                                                     \
    result.field = overrideStyle.field

  RAYM3_MERGE_FIELD(display);
  RAYM3_MERGE_FIELD(flexDirection);
  RAYM3_MERGE_FIELD(justifyContent);
  RAYM3_MERGE_FIELD(alignItems);
  RAYM3_MERGE_FIELD(alignSelf);
  RAYM3_MERGE_FIELD(position);
  RAYM3_MERGE_FIELD(overflow);
  RAYM3_MERGE_FIELD(pointerEvents);
  RAYM3_MERGE_FIELD(width);
  RAYM3_MERGE_FIELD(height);
  RAYM3_MERGE_FIELD(minWidth);
  RAYM3_MERGE_FIELD(minHeight);
  RAYM3_MERGE_FIELD(maxWidth);
  RAYM3_MERGE_FIELD(maxHeight);
  RAYM3_MERGE_FIELD(flexGrow);
  RAYM3_MERGE_FIELD(flexShrink);
  RAYM3_MERGE_FIELD(flexBasis);
  RAYM3_MERGE_FIELD(gap);
  RAYM3_MERGE_FIELD(rowGap);
  RAYM3_MERGE_FIELD(columnGap);
  RAYM3_MERGE_FIELD(backgroundColor);
  RAYM3_MERGE_FIELD(backgroundGradient);
  RAYM3_MERGE_FIELD(stateLayerColor);
  RAYM3_MERGE_FIELD(borderColor);
  RAYM3_MERGE_FIELD(borderWidth);
  RAYM3_MERGE_FIELD(borderRadius);
  RAYM3_MERGE_FIELD(backdropBlur);
  RAYM3_MERGE_FIELD(opacity);
  RAYM3_MERGE_FIELD(scrimOpacity);
  RAYM3_MERGE_FIELD(elevation);
  RAYM3_MERGE_FIELD(translateX);
  RAYM3_MERGE_FIELD(translateY);
  RAYM3_MERGE_FIELD(scale);

#undef RAYM3_MERGE_FIELD

  result.margin = MergeEdges(base.margin, overrideStyle.margin);
  result.padding = MergeEdges(base.padding, overrideStyle.padding);
  result.inset = MergeEdges(base.inset, overrideStyle.inset);
  if (!overrideStyle.boxShadows.empty())
    result.boxShadows = overrideStyle.boxShadows;
  result.text = MergeTextStyles(base.text, overrideStyle.text);
  return result;
}

Style ResolveStateStyle(const Style &base, const StateStyles &states,
                        ComponentState state) {
  switch (state) {
  case ComponentState::Hovered:
    return states.hovered ? MergeStyles(base, *states.hovered) : base;
  case ComponentState::Pressed:
    return states.pressed ? MergeStyles(base, *states.pressed) : base;
  case ComponentState::Focused:
    return states.focused ? MergeStyles(base, *states.focused) : base;
  case ComponentState::Disabled:
    return states.disabled ? MergeStyles(base, *states.disabled) : base;
  default:
    return base;
  }
}

} // namespace raym3::v2
