#pragma once

// Segmentation aligned with pretext/src/analysis.ts (Tier 1+2 subset).
// Reference: github.com/chenglou/pretext — sync when porting break/glue rules.

#include "raym3/v2/TextEngine.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace raym3::v2 {

struct AnalyzedSegment {
  std::string text;
  SegmentBreakKind kind = SegmentBreakKind::Text;
  std::size_t byteStart = 0;
  std::size_t byteEnd = 0;
  bool wordLike = true;
};

struct TextAnalysis {
  std::string normalized;
  std::vector<AnalyzedSegment> segments;
};

struct AnalysisProfile {
  bool carryCJKAfterClosingQuote = true;
  bool breakKeepAllAfterPunctuation = true;
};

AnalysisProfile DefaultAnalysisProfile();

std::string NormalizeTextWhitespace(std::string_view text, WhiteSpace mode);
bool IsCJKCodepoint(uint32_t cp);
bool IsCJKText(std::string_view text);
uint32_t DecodeUtf8Codepoint(std::string_view s, std::size_t &i);

bool CanContinueKeepAllTextRun(std::string_view previousText,
                               bool breakAfterPunctuation);

TextAnalysis AnalyzeText(std::string_view text, const AnalysisProfile &profile,
                         WhiteSpace whiteSpace, WordBreak wordBreak);

} // namespace raym3::v2
