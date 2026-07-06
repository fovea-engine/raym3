#pragma once

// Line breaking aligned with pretext/src/line-break.ts (simple + fit/paint path).

#include "raym3/v2/TextEngine.h"

namespace raym3::v2 {

constexpr float kDefaultLineFitEpsilon = 0.5f;

struct LineBreakCursor {
  std::size_t segmentIndex = 0;
  std::size_t graphemeIndex = 0;
};

struct LineRange {
  float paintWidth = 0.0f;
  LineBreakCursor start;
  LineBreakCursor end;
};

TextLayoutResult LayoutPreparedText(const PreparedText &prepared, float maxWidth,
                                    MeasureTextCallback measure = {},
                                    float lineFitEpsilon = kDefaultLineFitEpsilon);

} // namespace raym3::v2
