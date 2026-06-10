#include "raym3/v2/TextEngine.h"

#include "raym3/fonts/FontManager.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/EmojiFont.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace raym3::v2 {

static std::size_t NextUtf8Boundary(std::string_view text, std::size_t index) {
  if (index >= text.size())
    return text.size();

  unsigned char first = static_cast<unsigned char>(text[index]);
  std::size_t step = 1;
  if ((first & 0x80u) == 0) {
    step = 1;
  } else if ((first & 0xE0u) == 0xC0u) {
    step = 2;
  } else if ((first & 0xF0u) == 0xE0u) {
    step = 3;
  } else if ((first & 0xF8u) == 0xF0u) {
    step = 4;
  }

  return std::min(text.size(), index + step);
}

std::vector<std::size_t> GraphemeBoundaries(std::string_view text) {
  return raym3::v2::EmojiAwareGraphemeBoundaries(text);
}

static float DefaultMeasure(std::string_view text,
                            const TextLayoutOptions &options) {
  std::string materialized(text);
  Vector2 size;
  if (!options.fontFamily.empty()) {
    Font font = FontManager::LoadFontByFamily(options.fontFamily, (int)options.fontSize);
    size = MeasureTextWithEmoji(font, materialized, options.fontSize, options.letterSpacing);
  } else {
    Font font = Theme::GetFont(options.fontSize, options.weight);
    size = MeasureTextWithEmoji(font, materialized, options.fontSize, options.letterSpacing);
  }
  if (options.letterSpacing != 0.0f && !options.fontFamily.empty() &&
      materialized.size() > 1) {
    // letterSpacing already applied via MeasureTextEx spacing param above
  } else if (options.letterSpacing != 0.0f && materialized.size() > 1) {
    size.x += options.letterSpacing *
              static_cast<float>(GraphemeBoundaries(materialized).size() - 2);
  }
  return size.x;
}

static bool IsAsciiSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// CJK Unified Ideographs and common extension blocks — each character is its
// own break opportunity (same policy as pretext's isCJK check).
static bool IsCJKCodepoint(uint32_t cp) {
  return (cp >= 0x4E00  && cp <= 0x9FFF)  ||  // CJK Unified Ideographs
         (cp >= 0x3400  && cp <= 0x4DBF)  ||  // Extension A
         (cp >= 0x20000 && cp <= 0x2A6DF) ||  // Extension B
         (cp >= 0x2A700 && cp <= 0x2CEAF) ||  // Extension C/D/E
         (cp >= 0xF900  && cp <= 0xFAFF)  ||  // Compatibility Ideographs
         (cp >= 0x3000  && cp <= 0x303F)  ||  // CJK Symbols & Punctuation
         (cp >= 0x3040  && cp <= 0x309F)  ||  // Hiragana
         (cp >= 0x30A0  && cp <= 0x30FF)  ||  // Katakana
         (cp >= 0xAC00  && cp <= 0xD7AF);     // Hangul
}

// Decode first UTF-8 codepoint at s[i], advance i past it.
static uint32_t DecodeUtf8(std::string_view s, std::size_t &i) {
  unsigned char c = static_cast<unsigned char>(s[i]);
  uint32_t cp;
  std::size_t extra;
  if      (c < 0x80)   { cp = c;          extra = 0; }
  else if (c < 0xC0)   { cp = 0xFFFD;     extra = 0; i++; return cp; }
  else if (c < 0xE0)   { cp = c & 0x1F;   extra = 1; }
  else if (c < 0xF0)   { cp = c & 0x0F;   extra = 2; }
  else                 { cp = c & 0x07;   extra = 3; }
  i++;
  for (std::size_t k = 0; k < extra && i < s.size(); ++k, ++i)
    cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
  return cp;
}

static std::string NormalizeWhitespace(std::string_view text,
                                       WhiteSpace whiteSpace) {
  if (whiteSpace == WhiteSpace::PreWrap) {
    return std::string(text);
  }

  std::string normalized;
  normalized.reserve(text.size());
  bool previousWasSpace = false;
  for (char c : text) {
    if (IsAsciiSpace(c)) {
      if (!previousWasSpace) {
        normalized.push_back(' ');
      }
      previousWasSpace = true;
    } else {
      normalized.push_back(c);
      previousWasSpace = false;
    }
  }
  return normalized;
}

PreparedText PrepareText(std::string text, const TextLayoutOptions &options,
                         MeasureTextCallback measure) {
  PreparedText prepared;
  prepared.options = options;
  prepared.source = NormalizeWhitespace(text, options.whiteSpace);
  prepared.graphemeBoundaries = GraphemeBoundaries(prepared.source);

  MeasureTextCallback measureFn =
      measure ? std::move(measure) : MeasureTextCallback(DefaultMeasure);

  // Walk source, emitting runs. Each word/CJK-char/space/hard-break is one run.
  // CJK characters are individual break opportunities (pretext: isCJK policy).
  // Spaces are their own run (trailing space on a line is discarded by LayoutText).
  std::string_view src(prepared.source);
  std::size_t i = 0;
  std::size_t wordStart = std::string_view::npos;

  auto flushWord = [&](std::size_t end) {
    if (wordStart == std::string_view::npos || end <= wordStart) return;
    std::string_view runText = src.substr(wordStart, end - wordStart);
    prepared.runs.push_back({std::string(runText), wordStart, end,
                              measureFn(runText, options)});
    wordStart = std::string_view::npos;
  };

  while (i < src.size()) {
    unsigned char c = static_cast<unsigned char>(src[i]);

    if (src[i] == '\n') {                       // hard break
      flushWord(i);
      prepared.runs.push_back({"\n", i, i + 1, 0.0f});
      i++;
    } else if (IsAsciiSpace(src[i])) {          // collapsible space
      flushWord(i);
      std::size_t spaceStart = i++;
      while (i < src.size() && IsAsciiSpace(src[i]) && src[i] != '\n') i++;
      std::string_view spaceText = src.substr(spaceStart, i - spaceStart);
      prepared.runs.push_back({std::string(spaceText), spaceStart, i,
                                measureFn(spaceText, options)});
    } else if (c >= 0x80) {                     // multi-byte — check CJK
      std::size_t cpStart = i;
      uint32_t cp = DecodeUtf8(src, i);
      if (IsCJKCodepoint(cp)) {                 // CJK: flush pending word, emit alone
        flushWord(cpStart);
        std::string_view cpText = src.substr(cpStart, i - cpStart);
        prepared.runs.push_back({std::string(cpText), cpStart, i,
                                  measureFn(cpText, options)});
      } else {                                  // non-CJK multi-byte: accumulate into word
        if (wordStart == std::string_view::npos) wordStart = cpStart;
      }
    } else {                                    // ASCII non-space: accumulate into word
      if (wordStart == std::string_view::npos) wordStart = i;
      i++;
    }
  }
  flushWord(src.size());

  return prepared;
}

TextLayoutResult LayoutText(const PreparedText &prepared, float maxWidth,
                            MeasureTextCallback measure) {
  TextLayoutResult result;
  MeasureTextCallback measureFn =
      measure ? std::move(measure) : MeasureTextCallback(DefaultMeasure);

  std::string current;
  std::size_t lineStart = 0;
  std::size_t lineEnd = 0;
  float currentWidth = 0.0f;
  float y = 0.0f;

  auto pushLine = [&]() {
    result.lines.push_back({current, lineStart, lineEnd, currentWidth, y});
    result.width = std::max(result.width, currentWidth);
    result.height += prepared.options.lineHeight;
    y += prepared.options.lineHeight;
    current.clear();
    currentWidth = 0.0f;
    lineStart = lineEnd;
  };

  for (const TextRun &run : prepared.runs) {
    if (run.text == "\n") {
      lineEnd = run.byteEnd;
      pushLine();
      lineStart = run.byteEnd;
      lineEnd = run.byteEnd;
      continue;
    }

    bool overflows =
        maxWidth > 0.0f && currentWidth > 0.0f &&
        currentWidth + run.width > maxWidth;

    if (overflows) {
      pushLine();
      lineStart = run.byteStart;
    }

    if (maxWidth > 0.0f && run.width > maxWidth &&
        prepared.options.wordBreak == WordBreak::BreakWord) {
      for (std::size_t i = 0; i + 1 < prepared.graphemeBoundaries.size(); ++i) {
        std::size_t start = prepared.graphemeBoundaries[i];
        std::size_t end = prepared.graphemeBoundaries[i + 1];
        if (start < run.byteStart || end > run.byteEnd)
          continue;

        std::string_view grapheme(prepared.source.data() + start, end - start);
        float width = measureFn(grapheme, prepared.options);
        if (currentWidth > 0.0f && currentWidth + width > maxWidth) {
          pushLine();
          lineStart = start;
        }
        current.append(grapheme);
        currentWidth += width;
        lineEnd = end;
      }
      continue;
    }

    current += run.text;
    currentWidth += run.width;
    lineEnd = run.byteEnd;
  }

  if (!current.empty() || prepared.source.empty()) {
    pushLine();
  }

  return result;
}

std::string TextCacheKey(const std::string &text, float fontSize, FontWeight weight,
                         const std::string &fontFamily) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.1f:%d:", fontSize, static_cast<int>(weight));
  std::string key(buf);
  if (!fontFamily.empty()) { key += fontFamily; key += ':'; }
  return key + text;
}

} // namespace raym3::v2
