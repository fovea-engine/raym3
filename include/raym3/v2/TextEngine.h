#pragma once

#include "raym3/types.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace raym3::v2 {

enum class WhiteSpace { Normal, PreWrap };
enum class WordBreak { Normal, KeepAll, BreakWord };
// What happens to text that does not fit inside `maxLines`.
enum class TextOverflow { Clip, Ellipsis, Head, Middle };

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
  FontStyle fontStyle = FontStyle::Normal;
  WhiteSpace whiteSpace = WhiteSpace::Normal;
  WordBreak wordBreak = WordBreak::Normal;
  // 0 = unlimited. Clamps the laid-out line count (react-native
  // `numberOfLines`, CSS `-webkit-line-clamp`).
  int maxLines = 0;
  // Applied to the last kept line when maxLines truncates the text.
  TextOverflow overflow = TextOverflow::Clip;
  // empty = platform UI font (or embedded Roboto on web); else registered family
  std::string fontFamily;

  bool operator==(const TextLayoutOptions &o) const {
    return fontSize == o.fontSize && lineHeight == o.lineHeight &&
           letterSpacing == o.letterSpacing && weight == o.weight &&
           fontStyle == o.fontStyle && whiteSpace == o.whiteSpace &&
           wordBreak == o.wordBreak && maxLines == o.maxLines &&
           overflow == o.overflow && fontFamily == o.fontFamily;
  }
  bool operator!=(const TextLayoutOptions &o) const { return !(*this == o); }
};

// One styled run inside a Text node's content.
//
// Markdown and other rich text need bold/italic/code *inside* one wrapping
// paragraph. Without this a host has to emit a separate Text node per styled
// run — and since separate nodes cannot share a line box, in practice per
// *word*, which is how a long reply became thousands of nodes. A span carries
// only the properties that can vary mid-paragraph; everything else comes from
// the node's own TextLayoutOptions.
struct TextSpan {
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  std::optional<FontWeight> weight;
  std::optional<FontStyle> fontStyle;
  std::optional<Color> color;
  std::optional<Color> backgroundColor;
  std::optional<std::string> fontFamily;
  bool underline = false;
  bool lineThrough = false;

  bool operator==(const TextSpan &o) const {
    return byteStart == o.byteStart && byteEnd == o.byteEnd &&
           weight == o.weight && fontStyle == o.fontStyle &&
           colorEq(color, o.color) && colorEq(backgroundColor, o.backgroundColor) &&
           fontFamily == o.fontFamily && underline == o.underline &&
           lineThrough == o.lineThrough;
  }
  bool operator!=(const TextSpan &o) const { return !(*this == o); }

private:
  static bool colorEq(const std::optional<Color> &a, const std::optional<Color> &b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    return a->r == b->r && a->g == b->g && a->b == b->b && a->a == b->a;
  }
};

struct PreparedSegment {
  std::string text;
  SegmentBreakKind kind;
  // Index into PreparedText::spans, or -1 for the node's base style. Set when
  // the segment falls inside a span; segments are split at span boundaries so
  // one segment never straddles two styles.
  int spanIndex = -1;
  float width = 0.0f;
  float lineEndFitAdvance = 0.0f;
  float lineEndPaintAdvance = 0.0f;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  std::vector<float> breakableFitAdvances;
};

// A piece of one line that shares a single style — what the painter draws in
// one call. Plain text yields exactly one piece per line.
struct TextLinePiece {
  std::string text;
  int spanIndex = -1;
  float x = 0.0f;      // offset from the line's left edge, dp
  float width = 0.0f;
};

struct TextLine {
  std::string text;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  float width = 0.0f;
  float y = 0.0f;
  // Only populated when the prepared text has spans; empty means "draw
  // `text` with the node's base style", the fast path.
  std::vector<TextLinePiece> pieces;
};

struct TextLayoutResult {
  std::vector<TextLine> lines;
  float width = 0.0f;
  float height = 0.0f;
};

struct PreparedText {
  std::string source;
  TextLayoutOptions options;
  // Sorted by byteStart, non-overlapping. Empty for plain text, which keeps
  // the whole span path out of the common case.
  std::vector<TextSpan> spans;
  std::vector<std::size_t> graphemeBoundaries;
  std::vector<PreparedSegment> segments;
  float spaceWidth = 0.0f;
  float tabStopAdvance = 0.0f;
  bool simpleLineWalkFastPath = true;
};

using MeasureTextCallback =
    std::function<float(std::string_view, const TextLayoutOptions &)>;

std::vector<std::size_t> GraphemeBoundaries(std::string_view text);
// The measure used when PrepareText/LayoutText get no callback (raylib font
// metrics + letter spacing). Public so selection hit-testing measures with
// exactly the same widths the layout was built from.
float DefaultMeasure(std::string_view text, const TextLayoutOptions &options);
PreparedText PrepareTextWithSpans(std::string text,
                                  const TextLayoutOptions &options,
                                  std::vector<TextSpan> spans,
                                  MeasureTextCallback measure = {});
PreparedText PrepareText(std::string text, const TextLayoutOptions &options,
                         MeasureTextCallback measure = {});
TextLayoutResult LayoutText(const PreparedText &prepared, float maxWidth,
                            MeasureTextCallback measure = {});

std::string TextCacheKey(const std::string &text, float fontSize, FontWeight weight,
                         const std::string &fontFamily = {},
                         WhiteSpace whiteSpace = WhiteSpace::Normal,
                         WordBreak wordBreak = WordBreak::Normal,
                         float letterSpacing = 0.0f,
                         FontStyle fontStyle = FontStyle::Normal,
                         std::uint64_t fontGeneration = 0);

// Deterministic measure for golden tests (ports pretext layout.test.ts measureWidth).
float DeterministicTestMeasure(std::string_view text, const TextLayoutOptions &opts);

} // namespace raym3::v2
