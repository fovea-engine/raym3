#pragma once

#include "raym3/v2/Style.h"

#include <raylib.h>

namespace raym3 {
namespace v2 {

// CSS gradient painting.
//
// The mesh itself carries the rounded outline (the same technique Ripple.cpp
// uses), so clipping behaves identically on GL, Metal, Vulkan and WebGPU and no
// scissor/stencil state is touched — which matters because PushRoundedStencil
// degrades to a square scissor on rlwg.

// Fills in the positions css-images-3 leaves implicit: an unpositioned stop is
// spread evenly between its pinned neighbours, and positions never decrease.
// Call once at parse time; SampleGradient assumes the result.
void NormalizeGradientStops(LinearGradient &gradient);

// Colour at `t` (0..1) along the gradient line. Interpolation is premultiplied,
// as in browsers, so a stop fading to `transparent` does not darken on the way.
Color SampleGradient(const LinearGradient &gradient, float t);

// Paints `gradient` over the rounded rect, scaled by `opacity` (0..1). Linear
// gradients use a cartesian mesh, conic gradients an angular fan — a fan keeps
// the centre singularity sharp instead of smearing it across a grid cell.
//
// `box` is where the gradient is painted (background-clip). `paintBox`, when
// given, is the area the gradient's own 0%/100% are measured against
// (background-origin); it defaults to `box`, which is what a single box keyword
// in the `background` shorthand means.
void DrawGradientRoundedRect(Rectangle box, float cornerRadius,
                             const LinearGradient &gradient, float opacity,
                             const Rectangle *paintBox = nullptr);

// `background-clip: border-area`: paints the gradient into the ring between the
// outer and inner rounded rects only. Both boundaries are sampled along rays from
// the centre, so uneven border widths (an inset inner rect) come out right.
// `paintBox` is the background-origin area, as above.
void DrawGradientBorderArea(Rectangle outer, float outerRadius, Rectangle inner,
                            float innerRadius, const LinearGradient &gradient,
                            float opacity, const Rectangle *paintBox = nullptr);

} // namespace v2
} // namespace raym3
