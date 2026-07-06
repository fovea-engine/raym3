#include "raym3/v2/TextEngine.h"

#include "raym3/fonts/FontManager.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/EmojiFont.h"
#include "raym3/v2/TextAnalysis.h"
#include "raym3/v2/TextLineBreak.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace raym3::v2 {

namespace {

bool IsLeftStickyPunctuationChar(uint32_t cp) {
  return cp == '.' || cp == ',' || cp == '!' || cp == '?' || cp == ':' ||
         cp == ';' || cp == ')' || cp == ']' || cp == '}' || cp == '%' ||
         cp == '"' || cp == 0x2026;
}

bool EndsWithClosingQuote(std::string_view text) {
  if (text.empty())
    return false;
  std::size_t i = text.size();
  while (i > 0) {
    std::size_t start = i;
    while (start > 0 && (static_cast<unsigned char>(text[start - 1]) & 0xC0) == 0x80)
      --start;
    --start;
    std::size_t cpIdx = start;
    uint32_t cp = DecodeUtf8Codepoint(text, cpIdx);
    if (cp == 0x201D || cp == 0x2019 || cp == 0x300D || cp == 0x300F ||
        cp == 0x3011 || cp == 0xFF09)
      return true;
    if (!IsLeftStickyPunctuationChar(cp))
      return false;
    i = start;
  }
  return false;
}

bool IsKinsokuEndChar(uint32_t cp) {
  return cp == '"' || cp == '(' || cp == '[' || cp == '{' || cp == 0xFF08 ||
         cp == 0x3014 || cp == 0x3008 || cp == 0x300A || cp == 0x300C ||
         cp == 0x300E || cp == 0x3010 || cp == 0x3016 || cp == 0x3018 ||
         cp == 0x301A || cp == 0x201C || cp == 0x2018;
}

bool IsKinsokuStartChar(uint32_t cp) {
  return cp == 0xFF0C || cp == 0xFF0E || cp == 0xFF01 || cp == 0xFF1A ||
         cp == 0xFF1B || cp == 0xFF1F || cp == 0x3001 || cp == 0x3002 ||
         cp == 0x30FB || cp == 0xFF09 || cp == 0x3009 || cp == 0x300B ||
         cp == 0x300D || cp == 0x300F || cp == 0x3011 || cp == '.' ||
         cp == ',' || cp == '!' || cp == '?' || cp == ':' || cp == ';';
}

struct CjkUnit {
  std::string text;
  std::size_t start = 0;
};

std::vector<CjkUnit> BuildBaseCjkUnits(std::string_view segText,
                                       const AnalysisProfile &profile) {
  std::vector<CjkUnit> units;
  std::vector<std::string> unitParts;
  std::size_t unitStart = 0;
  bool unitContainsCJK = false;
  bool unitEndsWithClosingQuote = false;
  bool unitIsSingleKinsokuEnd = false;

  auto pushUnit = [&]() {
    if (unitParts.empty())
      return;
    std::string text = unitParts.size() == 1 ? unitParts[0]
                                             : [&]() {
                                                 std::string joined;
                                                 for (const auto &p : unitParts)
                                                   joined += p;
                                                 return joined;
                                               }();
    units.push_back({text, unitStart});
    unitParts.clear();
    unitContainsCJK = false;
    unitEndsWithClosingQuote = false;
    unitIsSingleKinsokuEnd = false;
  };

  const auto boundaries = EmojiAwareGraphemeBoundaries(segText);
  for (std::size_t bi = 0; bi + 1 < boundaries.size(); ++bi) {
    std::size_t gStart = boundaries[bi];
    std::string_view grapheme(segText.data() + gStart,
                              boundaries[bi + 1] - gStart);
    std::string gStr(grapheme);
    bool graphemeCJK = IsCJKText(grapheme);

    std::size_t cpIdx = 0;
    uint32_t cp = DecodeUtf8Codepoint(grapheme, cpIdx);

    if (unitParts.empty()) {
      unitParts.push_back(gStr);
      unitStart = gStart;
      unitContainsCJK = graphemeCJK;
      unitEndsWithClosingQuote = EndsWithClosingQuote(gStr);
      unitIsSingleKinsokuEnd = IsKinsokuEndChar(cp);
      continue;
    }

    if (unitIsSingleKinsokuEnd || IsKinsokuStartChar(cp) ||
        IsLeftStickyPunctuationChar(cp) ||
        (profile.carryCJKAfterClosingQuote && graphemeCJK &&
         unitEndsWithClosingQuote)) {
      unitParts.push_back(gStr);
      unitContainsCJK = unitContainsCJK || graphemeCJK;
      unitEndsWithClosingQuote = EndsWithClosingQuote(gStr);
      unitIsSingleKinsokuEnd = false;
      continue;
    }

    if (!unitContainsCJK && !graphemeCJK) {
      unitParts.push_back(gStr);
      continue;
    }

    pushUnit();
    unitParts.push_back(gStr);
    unitStart = gStart;
    unitContainsCJK = graphemeCJK;
    unitIsSingleKinsokuEnd = IsKinsokuEndChar(cp);
  }
  pushUnit();
  return units;
}

std::vector<CjkUnit>
MergeKeepAllUnits(std::string_view segText, const std::vector<CjkUnit> &units,
                  bool breakAfterPunctuation) {
  if (units.size() <= 1)
    return units;
  std::vector<CjkUnit> merged;
  std::size_t groupStart = static_cast<std::size_t>(-1);
  bool groupCJK = false;

  auto flush = [&](std::size_t end) {
    if (groupStart == static_cast<std::size_t>(-1))
      return;
    if (groupCJK && groupStart + 1 < end) {
      std::size_t sourceStart = units[groupStart].start;
      std::size_t sourceEnd =
          end < units.size() ? units[end].start : segText.size();
      merged.push_back(
          {std::string(segText.substr(sourceStart, sourceEnd - sourceStart)),
           sourceStart});
    } else {
      for (std::size_t j = groupStart; j < end; ++j)
        merged.push_back(units[j]);
    }
    groupStart = static_cast<std::size_t>(-1);
    groupCJK = false;
  };

  for (std::size_t i = 0; i < units.size(); ++i) {
    if (groupStart != static_cast<std::size_t>(-1) &&
        !CanContinueKeepAllTextRun(units[i - 1].text, breakAfterPunctuation))
      flush(i);
    if (groupStart == static_cast<std::size_t>(-1))
      groupStart = i;
    groupCJK = groupCJK || IsCJKText(units[i].text);
  }
  flush(units.size());
  return merged;
}

float DefaultMeasure(std::string_view text, const TextLayoutOptions &options) {
  std::string materialized(text);
  Vector2 size;
  if (!options.fontFamily.empty()) {
    Font font =
        FontManager::LoadFontByFamily(options.fontFamily, (int)options.fontSize);
    size = MeasureTextWithEmoji(font, materialized, options.fontSize,
                                options.letterSpacing);
  } else {
    Font font = Theme::GetFont(options.fontSize, options.weight);
    size = MeasureTextWithEmoji(font, materialized, options.fontSize,
                                options.letterSpacing);
  }
  return size.x;
}

float AddLetterSpacing(float width, std::string_view text, float letterSpacing,
                       SegmentBreakKind kind) {
  if (letterSpacing == 0.0f)
    return width;
  if (kind == SegmentBreakKind::HardBreak || kind == SegmentBreakKind::Tab)
    return width;
  std::size_t graphemeCount = GraphemeBoundaries(text).size() - 1;
  if (graphemeCount > 1)
    return width + (static_cast<float>(graphemeCount) - 1.0f) * letterSpacing;
  return width;
}

std::vector<float> BuildBreakableFitAdvances(
    std::string_view text, const MeasureTextCallback &measure,
    const TextLayoutOptions &options) {
  const auto boundaries = GraphemeBoundaries(text);
  if (boundaries.size() <= 2)
    return {};

  std::vector<float> advances;
  advances.reserve(boundaries.size() - 1);
  float cumulative = 0.0f;
  for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
    std::string_view grapheme(text.data() + boundaries[i],
                              boundaries[i + 1] - boundaries[i]);
    cumulative += measure(grapheme, options);
    advances.push_back(cumulative);
  }
  return advances;
}

void PushMeasuredSegment(PreparedText &prepared, std::string text, float width,
                         float lineEndFitAdvance, float lineEndPaintAdvance,
                         SegmentBreakKind kind, std::size_t byteStart,
                         std::vector<float> breakableFitAdvances) {
  if (kind != SegmentBreakKind::Text && kind != SegmentBreakKind::Space)
    prepared.simpleLineWalkFastPath = false;
  if (!breakableFitAdvances.empty())
    prepared.simpleLineWalkFastPath = false;

  PreparedSegment seg;
  seg.text = std::move(text);
  seg.kind = kind;
  seg.width = width;
  seg.lineEndFitAdvance = lineEndFitAdvance;
  seg.lineEndPaintAdvance = lineEndPaintAdvance;
  seg.byteStart = byteStart;
  seg.byteEnd = byteStart + seg.text.size();
  seg.breakableFitAdvances = std::move(breakableFitAdvances);
  prepared.segments.push_back(std::move(seg));
}

void PushMeasuredTextSegment(PreparedText &prepared, std::string text,
                             SegmentBreakKind kind, std::size_t byteStart,
                             bool wordLike, bool allowOverflowBreaks,
                             const MeasureTextCallback &measure,
                             const TextLayoutOptions &options) {
  float width =
      AddLetterSpacing(measure(text, options), text, options.letterSpacing, kind);

  float baseFit =
      (kind == SegmentBreakKind::Space ||
       kind == SegmentBreakKind::PreservedSpace)
          ? 0.0f
          : width;
  float lineEndFit = baseFit;
  float lineEndPaint =
      (kind == SegmentBreakKind::Space) ? 0.0f : width;

  std::vector<float> breakable;
  if (allowOverflowBreaks && wordLike && text.size() > 1 &&
      options.wordBreak != WordBreak::KeepAll)
    breakable = BuildBreakableFitAdvances(text, measure, options);

  PushMeasuredSegment(prepared, std::move(text), width, lineEndFit, lineEndPaint,
                      kind, byteStart, std::move(breakable));
}

} // namespace

std::vector<std::size_t> GraphemeBoundaries(std::string_view text) {
  return EmojiAwareGraphemeBoundaries(text);
}

PreparedText PrepareText(std::string text, const TextLayoutOptions &options,
                         MeasureTextCallback measure) {
  PreparedText prepared;
  prepared.options = options;
  prepared.simpleLineWalkFastPath = options.letterSpacing == 0.0f;

  MeasureTextCallback measureFn =
      measure ? std::move(measure) : MeasureTextCallback(DefaultMeasure);

  AnalysisProfile profile = DefaultAnalysisProfile();
  TextAnalysis analysis =
      AnalyzeText(text, profile, options.whiteSpace, options.wordBreak);
  prepared.source = analysis.normalized;
  prepared.graphemeBoundaries = GraphemeBoundaries(prepared.source);

  float spaceWidth = measureFn(" ", options);
  prepared.spaceWidth = spaceWidth;
  prepared.tabStopAdvance = spaceWidth * 8.0f;

  for (const AnalyzedSegment &seg : analysis.segments) {
    if (seg.kind == SegmentBreakKind::HardBreak) {
      PushMeasuredSegment(prepared, seg.text, 0.0f, 0.0f, 0.0f,
                          SegmentBreakKind::HardBreak, seg.byteStart, {});
      continue;
    }
    if (seg.kind == SegmentBreakKind::Tab) {
      prepared.simpleLineWalkFastPath = false;
      PushMeasuredSegment(prepared, seg.text, 0.0f, 0.0f, 0.0f,
                          SegmentBreakKind::Tab, seg.byteStart, {});
      continue;
    }
    if (seg.kind == SegmentBreakKind::Glue) {
      PushMeasuredTextSegment(prepared, seg.text, SegmentBreakKind::Glue,
                              seg.byteStart, seg.wordLike, false, measureFn,
                              options);
      continue;
    }

    if (seg.kind == SegmentBreakKind::Text && IsCJKText(seg.text)) {
      auto units = BuildBaseCjkUnits(seg.text, profile);
      if (options.wordBreak == WordBreak::KeepAll)
        units = MergeKeepAllUnits(seg.text, units,
                                  profile.breakKeepAllAfterPunctuation);

      for (const CjkUnit &unit : units) {
        bool allowBreak =
            options.wordBreak == WordBreak::KeepAll || !IsCJKText(unit.text);
        PushMeasuredTextSegment(
            prepared, unit.text, SegmentBreakKind::Text,
            seg.byteStart + unit.start, seg.wordLike, allowBreak, measureFn,
            options);
      }
      continue;
    }

    PushMeasuredTextSegment(prepared, seg.text, seg.kind, seg.byteStart,
                            seg.wordLike, true, measureFn, options);
  }

  return prepared;
}

TextLayoutResult LayoutText(const PreparedText &prepared, float maxWidth,
                            MeasureTextCallback measure) {
  MeasureTextCallback measureFn =
      measure ? std::move(measure) : MeasureTextCallback(DefaultMeasure);
  return LayoutPreparedText(prepared, maxWidth, measureFn, kDefaultLineFitEpsilon);
}

std::string TextCacheKey(const std::string &text, float fontSize,
                         FontWeight weight, const std::string &fontFamily,
                         WhiteSpace whiteSpace, WordBreak wordBreak,
                         float letterSpacing) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%.1f:%d:%d:%d:%.2f:", fontSize,
                static_cast<int>(weight), static_cast<int>(whiteSpace),
                static_cast<int>(wordBreak), letterSpacing);
  std::string key(buf);
  if (!fontFamily.empty()) {
    key += fontFamily;
    key += ':';
  }
  return key + text;
}

float DeterministicTestMeasure(std::string_view text,
                               const TextLayoutOptions &opts) {
  float fontSize = opts.fontSize;
  float width = 0.0f;
  bool previousWasDecimalDigit = false;

  const auto boundaries = GraphemeBoundaries(text);
  for (std::size_t i = 0; i + 1 < boundaries.size(); ++i) {
    std::string ch(text.substr(boundaries[i], boundaries[i + 1] - boundaries[i]));

    if (ch == " ") {
      width += fontSize * 0.33f;
      previousWasDecimalDigit = false;
    } else if (ch == "\t") {
      width += fontSize * 1.32f;
      previousWasDecimalDigit = false;
    } else if (ch == "\uFE0F" || (ch.size() >= 4 && static_cast<unsigned char>(ch[0]) == 0xF0)) {
      width += fontSize;
      previousWasDecimalDigit = false;
    } else {
      std::size_t cpIdx = 0;
      uint32_t cp = DecodeUtf8Codepoint(ch, cpIdx);
      bool isDigit = (cp >= '0' && cp <= '9');
      if (isDigit) {
        width += fontSize * (previousWasDecimalDigit ? 0.48f : 0.52f);
        previousWasDecimalDigit = true;
      } else if (IsCJKCodepoint(cp)) {
        width += fontSize;
        previousWasDecimalDigit = false;
      } else if (cp == '.' || cp == ',' || cp == '!' || cp == '?' || cp == ';' ||
                 cp == ':' || cp == '%' || cp == ')' || cp == ']' || cp == '}' ||
                 cp == '"' || cp == '-' || cp == 0x2026) {
        width += fontSize * 0.4f;
        previousWasDecimalDigit = false;
      } else {
        width += fontSize * 0.6f;
        previousWasDecimalDigit = false;
      }
    }
  }
  return width;
}

} // namespace raym3::v2
