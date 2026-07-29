#include "raym3/v2/TextEngine.h"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef RAYM3_GOLDEN_FIXTURE_PATH
#define RAYM3_GOLDEN_FIXTURE_PATH "tests/fixtures/text_layout_goldens.json"
#endif

using raym3::v2::DeterministicTestMeasure;
using raym3::v2::LayoutText;
using raym3::v2::PrepareText;
using raym3::v2::TextLayoutOptions;
using raym3::v2::TextLayoutResult;
using raym3::v2::WhiteSpace;
using raym3::v2::WordBreak;

namespace {

struct GoldenCase {
  std::string label;
  std::string text;
  float maxWidth = 0.0f;
  float lineHeight = 19.0f;
  float letterSpacing = 0.0f;
  WhiteSpace whiteSpace = WhiteSpace::Normal;
  WordBreak wordBreak = WordBreak::Normal;
  int lineCount = 0;
  float height = 0.0f;
  float maxLineWidth = 0.0f;
  std::vector<std::string> lines;
};

static std::string ReadFile(const char *path) {
  std::ifstream in(path);
  if (!in)
    return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

static std::size_t SkipWs(const std::string &json, std::size_t i) {
  while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i])))
    ++i;
  return i;
}

static bool Match(const std::string &json, std::size_t &i, char c) {
  i = SkipWs(json, i);
  if (i >= json.size() || json[i] != c)
    return false;
  ++i;
  return true;
}

static std::string ParseString(const std::string &json, std::size_t &i) {
  i = SkipWs(json, i);
  if (i >= json.size() || json[i] != '"')
    return {};
  ++i;
  std::string out;
  while (i < json.size()) {
    char c = json[i++];
    if (c == '"')
      break;
    if (c == '\\' && i < json.size()) {
      char esc = json[i++];
      if (esc == 'n')
        out.push_back('\n');
      else if (esc == 't')
        out.push_back('\t');
      else
        out.push_back(esc);
      continue;
    }
    out.push_back(c);
  }
  return out;
}

static double ParseNumber(const std::string &json, std::size_t &i) {
  i = SkipWs(json, i);
  std::size_t start = i;
  while (i < json.size() &&
         (std::isdigit(static_cast<unsigned char>(json[i])) || json[i] == '.' ||
          json[i] == '-' || json[i] == '+' || json[i] == 'e' || json[i] == 'E'))
    ++i;
  return std::stod(json.substr(start, i - start));
}

static WhiteSpace ParseWhiteSpaceEnum(const std::string &value) {
  return value == "pre-wrap" ? WhiteSpace::PreWrap : WhiteSpace::Normal;
}

static WordBreak ParseWordBreakEnum(const std::string &value) {
  if (value == "keep-all")
    return WordBreak::KeepAll;
  if (value == "break-word")
    return WordBreak::BreakWord;
  return WordBreak::Normal;
}

static std::vector<std::string> ParseStringArray(const std::string &json,
                                                 std::size_t &i) {
  std::vector<std::string> values;
  if (!Match(json, i, '['))
    return values;
  while (true) {
    i = SkipWs(json, i);
    if (i < json.size() && json[i] == ']') {
      ++i;
      break;
    }
    values.push_back(ParseString(json, i));
    i = SkipWs(json, i);
    if (i < json.size() && json[i] == ',')
      ++i;
  }
  return values;
}

static GoldenCase ParseCaseObject(const std::string &json, std::size_t &i) {
  GoldenCase testCase;
  if (!Match(json, i, '{'))
    return testCase;

  while (true) {
    i = SkipWs(json, i);
    if (i < json.size() && json[i] == '}') {
      ++i;
      break;
    }
    std::string key = ParseString(json, i);
    if (!Match(json, i, ':'))
      break;

    if (key == "label")
      testCase.label = ParseString(json, i);
    else if (key == "text")
      testCase.text = ParseString(json, i);
    else if (key == "maxWidth")
      testCase.maxWidth = static_cast<float>(ParseNumber(json, i));
    else if (key == "lineHeight")
      testCase.lineHeight = static_cast<float>(ParseNumber(json, i));
    else if (key == "letterSpacing")
      testCase.letterSpacing = static_cast<float>(ParseNumber(json, i));
    else if (key == "whiteSpace")
      testCase.whiteSpace = ParseWhiteSpaceEnum(ParseString(json, i));
    else if (key == "wordBreak")
      testCase.wordBreak = ParseWordBreakEnum(ParseString(json, i));
    else if (key == "lineCount")
      testCase.lineCount = static_cast<int>(ParseNumber(json, i));
    else if (key == "height")
      testCase.height = static_cast<float>(ParseNumber(json, i));
    else if (key == "maxLineWidth")
      testCase.maxLineWidth = static_cast<float>(ParseNumber(json, i));
    else if (key == "lines")
      testCase.lines = ParseStringArray(json, i);
    else {
      i = SkipWs(json, i);
      if (i < json.size() && json[i] == '"')
        (void)ParseString(json, i);
      else if (i < json.size() && json[i] == '[')
        (void)ParseStringArray(json, i);
      else
        (void)ParseNumber(json, i);
    }

    i = SkipWs(json, i);
    if (i < json.size() && json[i] == ',')
      ++i;
  }
  return testCase;
}

static std::vector<GoldenCase> LoadGoldens(const std::string &json) {
  std::vector<GoldenCase> cases;
  const std::string marker = "\"cases\"";
  std::size_t pos = json.find(marker);
  if (pos == std::string::npos)
    return cases;
  pos = json.find('[', pos);
  if (pos == std::string::npos)
    return cases;
  std::size_t i = pos + 1;
  while (true) {
    i = SkipWs(json, i);
    if (i >= json.size() || json[i] == ']')
      break;
    cases.push_back(ParseCaseObject(json, i));
    i = SkipWs(json, i);
    if (i < json.size() && json[i] == ',')
      ++i;
  }
  return cases;
}

static bool NearlyEqual(float a, float b, float epsilon = 0.05f) {
  return std::fabs(a - b) <= epsilon;
}

static TextLayoutResult LayoutGoldenCase(const GoldenCase &golden, float fontSize) {
  TextLayoutOptions opts;
  opts.fontSize = fontSize;
  opts.lineHeight = golden.lineHeight;
  opts.letterSpacing = golden.letterSpacing;
  opts.whiteSpace = golden.whiteSpace;
  opts.wordBreak = golden.wordBreak;

  auto measure = [](std::string_view text, const TextLayoutOptions &options) {
    return DeterministicTestMeasure(text, options);
  };

  auto prepared = PrepareText(golden.text, opts, measure);
  return LayoutText(prepared, golden.maxWidth, measure);
}

// numberOfLines / ellipsizeMode. Both were declared on the JS side for a long
// time and read by nothing, so an unbreakable label wrapped forever instead of
// ending in an ellipsis.
static int RunLineClampChecks() {
  int failures = 0;
  auto fail = [&](const std::string &label, const std::string &message) {
    std::cerr << "FAIL [" << label << "] " << message << '\n';
    ++failures;
  };

  const std::string text = "the quick brown fox jumps over the lazy dog";
  const float maxWidth = 120.0f;

  auto layout = [&](int maxLines, raym3::v2::TextOverflow overflow) {
    TextLayoutOptions opts;
    opts.fontSize = 16.0f;
    opts.lineHeight = 19.0f;
    opts.maxLines = maxLines;
    opts.overflow = overflow;
    return LayoutText(PrepareText(text, opts, DeterministicTestMeasure), maxWidth,
                      DeterministicTestMeasure);
  };

  const TextLayoutResult unlimited =
      layout(0, raym3::v2::TextOverflow::Clip);
  if (unlimited.lines.size() < 3)
    fail("unclamped", "expected the sample to wrap onto 3+ lines, got " +
                          std::to_string(unlimited.lines.size()));

  const TextLayoutResult clipped = layout(2, raym3::v2::TextOverflow::Clip);
  if (clipped.lines.size() != 2)
    fail("clip", "expected 2 lines, got " + std::to_string(clipped.lines.size()));
  if (!NearlyEqual(clipped.height, 38.0f))
    fail("clip", "expected height 38, got " + std::to_string(clipped.height));
  if (!clipped.lines.empty() &&
      clipped.lines.back().text != unlimited.lines[1].text)
    fail("clip", "clip must not rewrite the last line");

  const TextLayoutResult tail = layout(2, raym3::v2::TextOverflow::Ellipsis);
  if (tail.lines.size() != 2)
    fail("tail", "expected 2 lines, got " + std::to_string(tail.lines.size()));
  if (!tail.lines.empty()) {
    const std::string &last = tail.lines.back().text;
    if (last.size() < 3 || last.compare(last.size() - 3, 3, "\xE2\x80\xA6") != 0)
      fail("tail", "last line should end in an ellipsis, got \"" + last + "\"");
    if (tail.lines.back().width > maxWidth + 0.1f)
      fail("tail", "ellipsized line must still fit the width");
  }

  const TextLayoutResult single = layout(1, raym3::v2::TextOverflow::Ellipsis);
  if (single.lines.size() != 1)
    fail("single", "expected 1 line, got " + std::to_string(single.lines.size()));

  // A single unbreakable token: the reported symptom was one character per line.
  TextLayoutOptions wordOpts;
  wordOpts.fontSize = 16.0f;
  wordOpts.lineHeight = 19.0f;
  wordOpts.maxLines = 1;
  wordOpts.overflow = raym3::v2::TextOverflow::Ellipsis;
  wordOpts.wordBreak = WordBreak::BreakWord;
  const TextLayoutResult unbreakable = LayoutText(
      PrepareText("codegemma:2b-instruct-q4", wordOpts, DeterministicTestMeasure),
      60.0f, DeterministicTestMeasure);
  if (unbreakable.lines.size() != 1)
    fail("unbreakable", "expected 1 line, got " +
                            std::to_string(unbreakable.lines.size()));
  if (!unbreakable.lines.empty() && unbreakable.lines[0].width > 60.1f)
    fail("unbreakable", "clamped line must fit the width");

  return failures;
}

} // namespace

int main() {
  const std::string json = ReadFile(RAYM3_GOLDEN_FIXTURE_PATH);
  if (json.empty()) {
    std::cerr << "Failed to read golden fixture: " << RAYM3_GOLDEN_FIXTURE_PATH
              << '\n';
    return 1;
  }

  const std::vector<GoldenCase> cases = LoadGoldens(json);
  if (cases.empty()) {
    std::cerr << "No golden cases parsed\n";
    return 1;
  }

  int failures = 0;
  for (const GoldenCase &golden : cases) {
    TextLayoutResult result = LayoutGoldenCase(golden, 16.0f);

    auto fail = [&](const std::string &message) {
      std::cerr << "FAIL [" << golden.label << "] " << message << '\n';
      ++failures;
    };

    if (static_cast<int>(result.lines.size()) != golden.lineCount) {
      fail("lineCount expected " + std::to_string(golden.lineCount) + " got " +
           std::to_string(result.lines.size()));
      continue;
    }
    if (!NearlyEqual(result.height, golden.height)) {
      fail("height expected " + std::to_string(golden.height) + " got " +
           std::to_string(result.height));
    }
    if (!NearlyEqual(result.width, golden.maxLineWidth, 0.1f)) {
      fail("maxLineWidth expected " + std::to_string(golden.maxLineWidth) +
           " got " + std::to_string(result.width));
    }
    for (std::size_t i = 0; i < golden.lines.size(); ++i) {
      if (i >= result.lines.size()) {
        fail("missing line " + std::to_string(i));
        break;
      }
      if (result.lines[i].text != golden.lines[i]) {
        fail("line " + std::to_string(i) + " expected \"" + golden.lines[i] +
             "\" got \"" + result.lines[i].text + "\"");
      }
    }
  }

  failures += RunLineClampChecks();

  if (failures > 0) {
    std::cerr << failures << " case(s) failed\n";
    return 1;
  }

  std::cout << "All " << cases.size()
            << " text layout golden cases passed, plus line-clamp checks\n";
  return 0;
}
