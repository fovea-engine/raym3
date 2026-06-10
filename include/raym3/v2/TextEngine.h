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

struct TextLayoutOptions {
  float fontSize = 16.0f;
  float lineHeight = 20.0f;
  float letterSpacing = 0.0f;
  FontWeight weight = FontWeight::Regular;
  WhiteSpace whiteSpace = WhiteSpace::Normal;
  WordBreak wordBreak = WordBreak::Normal;
  std::string fontFamily; // empty = Roboto; non-empty = registered custom font
};

struct TextRun {
  std::string text;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  float width = 0.0f;
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
  std::vector<TextRun> runs;
};

using MeasureTextCallback =
    std::function<float(std::string_view, const TextLayoutOptions &)>;

std::vector<std::size_t> GraphemeBoundaries(std::string_view text);
PreparedText PrepareText(std::string text, const TextLayoutOptions &options,
                         MeasureTextCallback measure = {});
TextLayoutResult LayoutText(const PreparedText &prepared, float maxWidth,
                            MeasureTextCallback measure = {});

// Returns a fingerprint string for (text, fontSize, weight) — used to
// detect when the cached PreparedText on a Node needs invalidation.
std::string TextCacheKey(const std::string &text, float fontSize, FontWeight weight,
                         const std::string &fontFamily = {});

} // namespace raym3::v2
