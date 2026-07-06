#include "raym3/v2/TextAnalysis.h"

#include "raym3/v2/EmojiFont.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace raym3::v2 {

namespace detail {

bool IsCollapsibleAsciiSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool IsLatinLetterOrDigit(uint32_t cp) {
  if ((cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
      (cp >= '0' && cp <= '9'))
    return true;
  if (cp >= 0x00C0 && cp <= 0x024F)
    return true;
  if (cp >= 0x1E00 && cp <= 0x1EFF)
    return true;
  return false;
}

bool IsLeftStickyPunctuationChar(uint32_t cp) {
  static const std::unordered_set<uint32_t> sticky = {
      '.', ',', '!', '?', ':', ';', ')', ']', '}', '%', '"', 0x2026,
      0x060C, 0x061B, 0x061F, 0x0964, 0x0965, 0x104A, 0x104B, 0x104C,
      0x104D, 0x104F, 0x201D, 0x2019, 0x00BB, 0x203A,
  };
  return sticky.count(cp) > 0;
}

bool IsKinsokuStart(uint32_t cp) {
  static const std::unordered_set<uint32_t> start = {
      0xFF0C, 0xFF0E, 0xFF01, 0xFF1A, 0xFF1B, 0xFF1F, 0x3001, 0x3002,
      0x30FB, 0xFF09, 0x3015, 0x3009, 0x300B, 0x300D, 0x300F, 0x3011,
      0x3017, 0x3019, 0x301B, 0x30FC, 0x3005, 0x303B, 0x309D, 0x309E,
      0x30FD, 0x30FE,
  };
  return start.count(cp) > 0;
}

bool IsKinsokuEnd(uint32_t cp) {
  static const std::unordered_set<uint32_t> end = {
      '"', '(', '[', '{', 0x00A1, 0x00BF, 0x201C, 0x2018, 0x201A, 0x201E,
      0x00AB, 0x2039, 0x2E18, 0xFF08, 0x3014, 0x3008, 0x300A, 0x300C,
      0x300E, 0x3010, 0x3016, 0x3018, 0x301A,
  };
  return end.count(cp) > 0;
}

bool EndsWithClosingQuote(std::string_view text) {
  static const std::unordered_set<uint32_t> closing = {
      0x201D, 0x2019, 0x00BB, 0x203A, 0x300D, 0x300F, 0x3011, 0x300B,
      0x3009, 0x3015, 0xFF09,
  };
  std::size_t i = text.size();
  while (i > 0) {
    std::size_t start = i;
    uint32_t cp = DecodeUtf8Codepoint(text, start);
    if (closing.count(cp) || IsLeftStickyPunctuationChar(cp))
      return closing.count(cp) > 0;
    if (!IsLeftStickyPunctuationChar(cp))
      return false;
    i = start;
  }
  return false;
}

std::string GetLastCodepoint(std::string_view text) {
  if (text.empty())
    return {};
  std::size_t i = text.size();
  while (i > 0 && (static_cast<unsigned char>(text[i - 1]) & 0xC0) == 0x80)
    --i;
  if (i == 0)
    return {};
  std::size_t start = i - 1;
  while (start > 0 && (static_cast<unsigned char>(text[start - 1]) & 0xC0) == 0x80)
    --start;
  while (start > 0) {
    unsigned char b = static_cast<unsigned char>(text[start - 1]);
    if ((b & 0xC0) != 0x80)
      break;
    --start;
  }
  return std::string(text.substr(start));
}

bool EndsWithKeepAllGlue(std::string_view text) {
  std::string last = GetLastCodepoint(text);
  return last == "\u00A0" || last == "\u202F" || last == "\u2060" ||
         last == "\uFEFF";
}

bool EndsWithKeepAllDash(std::string_view text) {
  std::string last = GetLastCodepoint(text);
  return last == "-" || last == "\u2010" || last == "\u2013" || last == "\u2014";
}

bool EndsWithLineStartProhibited(std::string_view text) {
  std::string last = GetLastCodepoint(text);
  if (last.empty())
    return false;
  std::size_t i = 0;
  uint32_t cp = DecodeUtf8Codepoint(last, i);
  return IsKinsokuStart(cp) || IsLeftStickyPunctuationChar(cp);
}

bool CanContinueKeepAllTextRun(std::string_view previous,
                               bool breakAfterPunctuation) {
  if (EndsWithKeepAllGlue(previous))
    return false;
  if (!breakAfterPunctuation)
    return true;
  if (EndsWithLineStartProhibited(previous))
    return false;
  if (EndsWithKeepAllDash(previous))
    return false;
  return true;
}

bool IsLeftStickyPunctuationSegment(std::string_view segment) {
  if (segment.empty())
    return false;
  bool saw = false;
  for (std::size_t i = 0; i < segment.size();) {
    std::size_t start = i;
    uint32_t cp = DecodeUtf8Codepoint(segment, i);
    if (IsLeftStickyPunctuationChar(cp)) {
      saw = true;
      continue;
    }
    return false;
  }
  return saw;
}

SegmentBreakKind ClassifyBreakChar(char c, WhiteSpace whiteSpace) {
  if (whiteSpace == WhiteSpace::PreWrap) {
    if (c == ' ')
      return SegmentBreakKind::PreservedSpace;
    if (c == '\t')
      return SegmentBreakKind::Tab;
    if (c == '\n')
      return SegmentBreakKind::HardBreak;
  }
  if (c == ' ')
    return SegmentBreakKind::Space;
  return SegmentBreakKind::Text;
}

bool IsGlueChar(uint32_t cp) {
  return cp == 0x00A0 || cp == 0x202F || cp == 0x2060 || cp == 0xFEFF;
}

struct RawPiece {
  std::string text;
  bool wordLike = true;
  SegmentBreakKind kind = SegmentBreakKind::Text;
  std::size_t start = 0;
};

std::vector<RawPiece> SplitByBreakChars(std::string_view segment, bool wordLike,
                                        std::size_t start, WhiteSpace ws) {
  bool hasBreak = false;
  for (char c : segment) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\xA0' || c == '\xAD' ||
        (c == '\r'))
      hasBreak = true;
  }
  if (!hasBreak)
    return {{std::string(segment), wordLike, SegmentBreakKind::Text, start}};

  std::vector<RawPiece> pieces;
  std::string current;
  SegmentBreakKind currentKind = SegmentBreakKind::Text;
  bool currentWordLike = false;
  std::size_t currentStart = start;
  std::size_t offset = 0;

  for (std::size_t i = 0; i < segment.size();) {
    std::size_t cpStart = i;
    uint32_t cp = DecodeUtf8Codepoint(segment, i);
    SegmentBreakKind kind = SegmentBreakKind::Text;
    if (IsGlueChar(cp))
      kind = SegmentBreakKind::Glue;
    else if (cp == 0x200B)
      kind = SegmentBreakKind::Text;
    else if (cp == 0x00AD)
      kind = SegmentBreakKind::Text;
    else if (cpStart < segment.size()) {
      kind = ClassifyBreakChar(segment[cpStart], ws);
    }

    bool pieceWordLike = kind == SegmentBreakKind::Text && wordLike;

    if (!current.empty() && (kind != currentKind || pieceWordLike != currentWordLike)) {
      pieces.push_back({current, currentWordLike, currentKind, currentStart});
      current.clear();
      currentStart = start + offset;
    }

    currentKind = kind;
    currentWordLike = pieceWordLike;
    current.append(segment.substr(cpStart, i - cpStart));
    offset = i - start;
  }

  if (!current.empty())
    pieces.push_back({current, currentWordLike, currentKind, currentStart});
  return pieces;
}

std::vector<RawPiece> SegmentLatinWords(std::string_view normalized,
                                        WhiteSpace ws) {
  std::vector<RawPiece> pieces;
  std::string current;
  bool currentWordLike = false;
  std::size_t currentStart = 0;
  SegmentBreakKind currentKind = SegmentBreakKind::Text;

  auto flush = [&]() {
    if (current.empty())
      return;
    auto split = SplitByBreakChars(current, currentWordLike, currentStart, ws);
    pieces.insert(pieces.end(), split.begin(), split.end());
    current.clear();
  };

  const auto boundaries = EmojiAwareGraphemeBoundaries(normalized);
  for (std::size_t bi = 0; bi + 1 < boundaries.size(); ++bi) {
    std::size_t gStart = boundaries[bi];
    std::size_t gEnd = boundaries[bi + 1];
    std::string_view grapheme(normalized.data() + gStart, gEnd - gStart);

    if (grapheme == "\n" && ws == WhiteSpace::PreWrap) {
      flush();
      pieces.push_back({"\n", false, SegmentBreakKind::HardBreak, gStart});
      continue;
    }
    if (grapheme == " " || grapheme == "\t") {
      flush();
      SegmentBreakKind kind = ClassifyBreakChar(grapheme[0], ws);
      pieces.push_back({std::string(grapheme), false, kind, gStart});
      continue;
    }

    std::size_t cpIdx = 0;
    uint32_t cp = DecodeUtf8Codepoint(grapheme, cpIdx);
    if (IsGlueChar(cp)) {
      flush();
      pieces.push_back({std::string(grapheme), false, SegmentBreakKind::Glue, gStart});
      continue;
    }

    if (IsCJKCodepoint(cp) || IsCJKText(grapheme)) {
      flush();
      pieces.push_back({std::string(grapheme), true, SegmentBreakKind::Text, gStart});
      continue;
    }

    bool wordLike = IsLatinLetterOrDigit(cp) || grapheme == "'" || grapheme == "-";
    if (current.empty()) {
      current = std::string(grapheme);
      currentWordLike = wordLike;
      currentKind = SegmentBreakKind::Text;
      currentStart = gStart;
    } else if (wordLike == currentWordLike && currentKind == SegmentBreakKind::Text) {
      current.append(grapheme);
    } else {
      flush();
      current = std::string(grapheme);
      currentWordLike = wordLike;
      currentKind = SegmentBreakKind::Text;
      currentStart = gStart;
    }
  }
  flush();
  return pieces;
}

std::vector<AnalyzedSegment> MergePunctuationGlue(std::vector<RawPiece> pieces,
                                                  const AnalysisProfile &profile) {
  std::vector<AnalyzedSegment> out;
  for (const RawPiece &piece : pieces) {
    if (piece.kind != SegmentBreakKind::Text || piece.wordLike ||
        out.empty() || out.back().kind != SegmentBreakKind::Text) {
      out.push_back({piece.text, piece.kind, piece.start,
                     piece.start + piece.text.size(), piece.wordLike});
      continue;
    }

    bool prevCJK = IsCJKText(out.back().text);
    bool sticky = IsLeftStickyPunctuationSegment(piece.text) ||
                  (piece.text == "-" && out.back().wordLike);
    bool cjkProhibited =
        prevCJK && !piece.text.empty() &&
        ([&]() {
          std::size_t cpIdx = 0;
          uint32_t cp = DecodeUtf8Codepoint(piece.text, cpIdx);
          return IsKinsokuStart(cp) || IsLeftStickyPunctuationChar(cp);
        })();

    if (!prevCJK && sticky) {
      out.back().text += piece.text;
      out.back().byteEnd = piece.start + piece.text.size();
      out.back().wordLike = out.back().wordLike || piece.wordLike;
    } else if (profile.carryCJKAfterClosingQuote && prevCJK && cjkProhibited) {
      out.back().text += piece.text;
      out.back().byteEnd = piece.start + piece.text.size();
    } else {
      out.push_back({piece.text, piece.kind, piece.start,
                     piece.start + piece.text.size(), piece.wordLike});
    }
  }

  for (std::size_t i = 0; i + 1 < out.size(); ++i) {
    if (out[i].kind != SegmentBreakKind::Text || out[i + 1].kind != SegmentBreakKind::Text)
      continue;
    if (!IsCJKText(out[i].text) || !IsCJKText(out[i + 1].text))
      continue;
    if (!EndsWithClosingQuote(out[i].text))
      continue;
    out[i].text += out[i + 1].text;
    out[i].byteEnd = out[i + 1].byteEnd;
    out[i + 1].text.clear();
  }

  out.erase(std::remove_if(out.begin(), out.end(),
                           [](const AnalyzedSegment &s) { return s.text.empty(); }),
            out.end());
  return out;
}

std::vector<AnalyzedSegment>
MergeGlueRuns(const std::vector<AnalyzedSegment> &segments) {
  std::vector<AnalyzedSegment> out;
  for (std::size_t i = 0; i < segments.size(); ++i) {
    const auto &seg = segments[i];
    if (seg.kind != SegmentBreakKind::Glue) {
      out.push_back(seg);
      continue;
    }

    std::string glue = seg.text;
    std::size_t glueStart = seg.byteStart;
    ++i;
    while (i < segments.size() && segments[i].kind == SegmentBreakKind::Glue) {
      glue += segments[i].text;
      ++i;
    }
    if (i < segments.size() && segments[i].kind == SegmentBreakKind::Text) {
      out.push_back({glue + segments[i].text, SegmentBreakKind::Text, glueStart,
                     segments[i].byteEnd, segments[i].wordLike});
      continue;
    }
    out.push_back({glue, SegmentBreakKind::Glue, glueStart,
                   glueStart + glue.size(), false});
    if (i < segments.size())
      out.push_back(segments[i]);
  }
  return out;
}

std::vector<AnalyzedSegment>
MergeKeepAll(const std::string &normalized,
             const std::vector<AnalyzedSegment> &segments,
             bool breakAfterPunctuation) {
  if (segments.size() <= 1)
    return segments;

  std::vector<AnalyzedSegment> out;
  std::size_t groupStart = static_cast<std::size_t>(-1);
  bool groupCJK = false;

  auto flushGroup = [&](std::size_t end) {
    if (groupStart == static_cast<std::size_t>(-1))
      return;
    if (groupCJK) {
      if (groupStart + 1 == end) {
        out.push_back(segments[groupStart]);
      } else {
        std::size_t sourceStart = segments[groupStart].byteStart;
        std::size_t sourceEnd =
            end < segments.size() ? segments[end].byteStart : normalized.size();
        out.push_back({normalized.substr(sourceStart, sourceEnd - sourceStart),
                       SegmentBreakKind::Text, sourceStart, sourceEnd,
                       true});
      }
    } else {
      for (std::size_t j = groupStart; j < end; ++j)
        out.push_back(segments[j]);
    }
    groupStart = static_cast<std::size_t>(-1);
    groupCJK = false;
  };

  for (std::size_t i = 0; i < segments.size(); ++i) {
    const auto &seg = segments[i];
    if (seg.kind == SegmentBreakKind::Text) {
      if (groupStart != static_cast<std::size_t>(-1) &&
          !CanContinueKeepAllTextRun(segments[i - 1].text, breakAfterPunctuation))
        flushGroup(i);
      if (groupStart == static_cast<std::size_t>(-1))
        groupStart = i;
      groupCJK = groupCJK || IsCJKText(seg.text);
      continue;
    }
    flushGroup(i);
    out.push_back(seg);
  }
  flushGroup(segments.size());
  return out;
}

} // namespace detail

bool CanContinueKeepAllTextRun(std::string_view previous,
                               bool breakAfterPunctuation) {
  return detail::CanContinueKeepAllTextRun(previous, breakAfterPunctuation);
}

AnalysisProfile DefaultAnalysisProfile() {
  return {.carryCJKAfterClosingQuote = true,
          .breakKeepAllAfterPunctuation = true};
}

uint32_t DecodeUtf8Codepoint(std::string_view s, std::size_t &i) {
  if (i >= s.size())
    return 0;
  unsigned char c = static_cast<unsigned char>(s[i]);
  uint32_t cp;
  std::size_t extra;
  if (c < 0x80) {
    cp = c;
    extra = 0;
  } else if (c < 0xC0) {
    ++i;
    return 0xFFFD;
  } else if (c < 0xE0) {
    cp = c & 0x1F;
    extra = 1;
  } else if (c < 0xF0) {
    cp = c & 0x0F;
    extra = 2;
  } else {
    cp = c & 0x07;
    extra = 3;
  }
  ++i;
  for (std::size_t k = 0; k < extra && i < s.size(); ++k, ++i)
    cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
  return cp;
}

bool IsCJKCodepoint(uint32_t cp) {
  return (cp >= 0x4E00 && cp <= 0x9FFF) || (cp >= 0x3400 && cp <= 0x4DBF) ||
         (cp >= 0x20000 && cp <= 0x2A6DF) || (cp >= 0x2A700 && cp <= 0x2CEAF) ||
         (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0x3000 && cp <= 0x303F) ||
         (cp >= 0x3040 && cp <= 0x309F) || (cp >= 0x30A0 && cp <= 0x30FF) ||
         (cp >= 0x3130 && cp <= 0x318F) || (cp >= 0xAC00 && cp <= 0xD7AF) ||
         (cp >= 0xFF00 && cp <= 0xFFEF);
}

bool IsCJKText(std::string_view text) {
  for (std::size_t i = 0; i < text.size();) {
    if (IsCJKCodepoint(DecodeUtf8Codepoint(text, i)))
      return true;
  }
  return false;
}

std::string NormalizeTextWhitespace(std::string_view text, WhiteSpace mode) {
  if (mode == WhiteSpace::PreWrap) {
    std::string out(text);
    std::size_t pos = 0;
    while ((pos = out.find("\r\n", pos)) != std::string::npos) {
      out.replace(pos, 2, "\n");
      pos += 1;
    }
    for (char &c : out) {
      if (c == '\r' || c == '\f')
        c = '\n';
    }
    return out;
  }

  std::string normalized;
  normalized.reserve(text.size());
  bool previousWasSpace = false;
  bool atStart = true;
  for (char c : text) {
    if (detail::IsCollapsibleAsciiSpace(c)) {
      if (!previousWasSpace && !atStart)
        normalized.push_back(' ');
      previousWasSpace = true;
    } else {
      normalized.push_back(c);
      previousWasSpace = false;
      atStart = false;
    }
  }
  while (!normalized.empty() && normalized.front() == ' ')
    normalized.erase(normalized.begin());
  while (!normalized.empty() && normalized.back() == ' ')
    normalized.pop_back();
  return normalized;
}

TextAnalysis AnalyzeText(std::string_view text, const AnalysisProfile &profile,
                         WhiteSpace whiteSpace, WordBreak wordBreak) {
  TextAnalysis result;
  result.normalized = NormalizeTextWhitespace(text, whiteSpace);
  if (result.normalized.empty())
    return result;

  auto pieces = detail::SegmentLatinWords(result.normalized, whiteSpace);
  auto merged = detail::MergePunctuationGlue(std::move(pieces), profile);
  merged = detail::MergeGlueRuns(merged);
  if (wordBreak == WordBreak::KeepAll)
    merged = detail::MergeKeepAll(result.normalized, merged,
                          profile.breakKeepAllAfterPunctuation);

  result.segments = std::move(merged);
  return result;
}

} // namespace raym3::v2
