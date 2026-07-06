#pragma once

#include "raym3/types.h"
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace raym3::v2 {

enum class WhiteSpace { Normal, PreWrap };
enum class WordBreak { Normal, KeepAll, BreakWord };

enum class SegmentBreakKind {
  Text,
  Space,
  PreservedSpace,
  Tab,
  Glue,
  HardBreak,
};

struct TextLayoutOptions {
  float fontSize = 16.0f;
  float lineHeight = 20.0f;
  float letterSpacing = 0.0f;
  FontWeight weight = FontWeight::Regular;
  WhiteSpace whiteSpace = WhiteSpace::Normal;
  WordBreak wordBreak = WordBreak::Normal;
  std::string fontFamily; // empty = Roboto; non-empty = registered custom font
};

struct PreparedSegment {
  std::string text;
  SegmentBreakKind kind;
  float width = 0.0f;
  float lineEndFitAdvance = 0.0f;
  float lineEndPaintAdvance = 0.0f;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  std::vector<float> breakableFitAdvances;
};

struct TextLine {
  std::string text;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  float width = 0.0f;
  float y = 0.0f;
};

struct TextLayoutResult {
  std::vector<TextLine> lines;
  float width = 0.0f;
  float height = 0.0f;
};

struct PreparedText {
  std::string source;
  TextLayoutOptions options;
  std::vector<std::size_t> graphemeBoundaries;
  std::vector<PreparedSegment> segments;
  float spaceWidth = 0.0f;
  float tabStopAdvance = 0.0f;
  bool simpleLineWalkFastPath = true;
};

using MeasureTextCallback =
    std::function<float(std::string_view, const TextLayoutOptions &)>;

std::vector<std::size_t> GraphemeBoundaries(std::string_view text);
PreparedText PrepareText(std::string text, const TextLayoutOptions &options,
                         MeasureTextCallback measure = {});
TextLayoutResult LayoutText(const PreparedText &prepared, float maxWidth,
                            MeasureTextCallback measure = {});

std::string TextCacheKey(const std::string &text, float fontSize, FontWeight weight,
                         const std::string &fontFamily = {},
                         WhiteSpace whiteSpace = WhiteSpace::Normal,
                         WordBreak wordBreak = WordBreak::Normal,
                         float letterSpacing = 0.0f);

// Deterministic measure for golden tests (ports pretext layout.test.ts measureWidth).
float DeterministicTestMeasure(std::string_view text, const TextLayoutOptions &opts);

} // namespace raym3::v2
