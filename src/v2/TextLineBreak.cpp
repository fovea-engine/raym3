#include "raym3/v2/TextLineBreak.h"

#include "raym3/v2/EmojiFont.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>
#include <string>

namespace raym3::v2 {

namespace {

bool ConsumesAtLineStart(SegmentBreakKind kind) {
  return kind == SegmentBreakKind::Space;
}

bool BreaksAfter(SegmentBreakKind kind) {
  return kind == SegmentBreakKind::Space ||
         kind == SegmentBreakKind::PreservedSpace ||
         kind == SegmentBreakKind::Tab;
}

std::size_t NormalizeLineStartSegmentIndex(const PreparedText &prepared,
                                           std::size_t segmentIndex,
                                           std::size_t endSegmentIndex) {
  while (segmentIndex < endSegmentIndex) {
    if (!ConsumesAtLineStart(prepared.segments[segmentIndex].kind))
      break;
    ++segmentIndex;
  }
  return segmentIndex;
}

float GetTabAdvance(float lineWidth, float tabStopAdvance) {
  if (tabStopAdvance <= 0.0f)
    return 0.0f;
  float remainder = std::fmod(lineWidth, tabStopAdvance);
  if (std::fabs(remainder) <= 1e-6f)
    return tabStopAdvance;
  return tabStopAdvance - remainder;
}

float SegmentContribution(const PreparedText &prepared, std::size_t index,
                          float lineW) {
  const PreparedSegment &seg = prepared.segments[index];
  if (seg.kind == SegmentBreakKind::Tab)
    return GetTabAdvance(lineW, prepared.tabStopAdvance);
  return seg.width;
}

std::string MaterializeLineRange(const PreparedText &prepared,
                                 const LineBreakCursor &start,
                                 const LineBreakCursor &end) {
  if (start.segmentIndex == end.segmentIndex) {
    if (start.segmentIndex >= prepared.segments.size())
      return {};
    const std::string &segment = prepared.segments[start.segmentIndex].text;
    auto boundaries = EmojiAwareGraphemeBoundaries(segment);
    if (start.graphemeIndex >= boundaries.size() - 1)
      return {};
    std::size_t byteStart = boundaries[start.graphemeIndex];
    std::size_t byteEnd = end.graphemeIndex < boundaries.size() - 1
                              ? boundaries[end.graphemeIndex]
                              : segment.size();
    return segment.substr(byteStart, byteEnd - byteStart);
  }

  std::string result;
  for (std::size_t segmentIndex = start.segmentIndex;
       segmentIndex < end.segmentIndex; ++segmentIndex) {
    if (segmentIndex >= prepared.segments.size())
      break;
    const std::string &segment = prepared.segments[segmentIndex].text;
    if (prepared.segments[segmentIndex].kind == SegmentBreakKind::HardBreak)
      continue;
    if (segmentIndex == start.segmentIndex && start.graphemeIndex > 0) {
      auto boundaries = EmojiAwareGraphemeBoundaries(segment);
      if (start.graphemeIndex < boundaries.size() - 1) {
        result += segment.substr(boundaries[start.graphemeIndex]);
      }
    } else {
      result += segment;
    }
  }

  if (end.graphemeIndex > 0 && end.segmentIndex < prepared.segments.size()) {
    const std::string &segment = prepared.segments[end.segmentIndex].text;
    auto boundaries = EmojiAwareGraphemeBoundaries(segment);
    if (end.graphemeIndex < boundaries.size() - 1) {
      result += segment.substr(0, boundaries[end.graphemeIndex]);
    }
  }
  return result;
}

struct WalkerState {
  float lineW = 0.0f;
  bool hasContent = false;
  std::size_t lineStartSegmentIndex = 0;
  std::size_t lineStartGraphemeIndex = 0;
  std::size_t lineEndSegmentIndex = 0;
  std::size_t lineEndGraphemeIndex = 0;
  std::size_t pendingBreakSegmentIndex = static_cast<std::size_t>(-1);
  float pendingBreakPaintWidth = 0.0f;
  bool pendingBreakUseLineWidth = false;
};

using LineVisitor = std::function<void(float paintWidth, const LineBreakCursor &,
                                       const LineBreakCursor &)>;

std::size_t WalkPreparedLines(const PreparedText &prepared, float maxWidth,
                              float fitLimit, LineVisitor onLine) {
  const auto &segments = prepared.segments;
  if (segments.empty())
    return 0;

  WalkerState state;
  std::size_t lineCount = 0;

  auto clearPending = [&]() {
    state.pendingBreakSegmentIndex = static_cast<std::size_t>(-1);
    state.pendingBreakPaintWidth = 0.0f;
    state.pendingBreakUseLineWidth = false;
  };

  auto emitLine = [&](std::optional<std::size_t> endSeg = std::nullopt,
                      std::optional<std::size_t> endGrapheme = std::nullopt,
                      std::optional<float> width = std::nullopt) {
    if (!state.hasContent)
      return;
    std::size_t endSegmentIndex =
        endSeg.value_or(state.lineEndSegmentIndex);
    std::size_t endGraphemeIndex =
        endGrapheme.value_or(state.lineEndGraphemeIndex);
    float paintWidth = width.value_or(state.lineW);

    if (onLine) {
      LineBreakCursor start{state.lineStartSegmentIndex,
                            state.lineStartGraphemeIndex};
      LineBreakCursor end{endSegmentIndex, endGraphemeIndex};
      onLine(paintWidth, start, end);
    }

    ++lineCount;
    state.lineW = 0.0f;
    state.hasContent = false;
    clearPending();
  };

  auto startLineAtSegment = [&](std::size_t segmentIndex, float width) {
    state.hasContent = true;
    state.lineStartSegmentIndex = segmentIndex;
    state.lineStartGraphemeIndex = 0;
    state.lineEndSegmentIndex = segmentIndex + 1;
    state.lineEndGraphemeIndex = 0;
    state.lineW = width;
  };

  auto startLineAtGrapheme = [&](std::size_t segmentIndex,
                                std::size_t graphemeIndex, float width) {
    state.hasContent = true;
    state.lineStartSegmentIndex = segmentIndex;
    state.lineStartGraphemeIndex = graphemeIndex;
    state.lineEndSegmentIndex = segmentIndex;
    state.lineEndGraphemeIndex = graphemeIndex + 1;
    state.lineW = width;
  };

  auto appendWholeSegment = [&](std::size_t segmentIndex, float width) {
    if (!state.hasContent) {
      startLineAtSegment(segmentIndex, width);
      return;
    }
    state.lineW += width;
    state.lineEndSegmentIndex = segmentIndex + 1;
    state.lineEndGraphemeIndex = 0;
  };

  auto appendBreakableFrom = [&](std::size_t segmentIndex,
                                 std::size_t startGraphemeIndex) {
    const auto &fitAdvances =
        prepared.segments[segmentIndex].breakableFitAdvances;
    for (std::size_t g = startGraphemeIndex; g < fitAdvances.size(); ++g) {
      float gw = fitAdvances[g];
      if (!state.hasContent) {
        startLineAtGrapheme(segmentIndex, g, gw);
      } else if (state.lineW + gw > fitLimit) {
        emitLine();
        startLineAtGrapheme(segmentIndex, g, gw);
      } else {
        state.lineW += gw;
        state.lineEndSegmentIndex = segmentIndex;
        state.lineEndGraphemeIndex = g + 1;
      }
    }
    if (state.hasContent && state.lineEndSegmentIndex == segmentIndex &&
        state.lineEndGraphemeIndex == fitAdvances.size()) {
      state.lineEndSegmentIndex = segmentIndex + 1;
      state.lineEndGraphemeIndex = 0;
    }
  };

  std::size_t i = 0;
  while (i < segments.size()) {
    const PreparedSegment &seg = segments[i];

    if (seg.kind == SegmentBreakKind::HardBreak) {
      if (state.hasContent)
        emitLine();
      ++i;
      continue;
    }

    if (!state.hasContent) {
      i = NormalizeLineStartSegmentIndex(prepared, i, segments.size());
      if (i >= segments.size())
        break;
    }

    float w = SegmentContribution(prepared, i, state.lineW);
    bool breakAfter = BreaksAfter(seg.kind);

    if (!state.hasContent) {
      if (w > fitLimit && !seg.breakableFitAdvances.empty()) {
        appendBreakableFrom(i, 0);
      } else {
        startLineAtSegment(i, w);
      }
      if (breakAfter) {
        state.pendingBreakSegmentIndex = i + 1;
        state.pendingBreakPaintWidth = state.lineW - w;
        state.pendingBreakUseLineWidth = seg.kind == SegmentBreakKind::Tab;
      }
      ++i;
      continue;
    }

    float newW = state.lineW + w;
    if (newW > fitLimit) {
      if (breakAfter) {
        appendWholeSegment(i, w);
        emitLine(i + 1, 0, state.lineW - w);
        ++i;
        continue;
      }

      if (state.pendingBreakSegmentIndex != static_cast<std::size_t>(-1)) {
        if (state.lineEndSegmentIndex > state.pendingBreakSegmentIndex ||
            (state.lineEndSegmentIndex == state.pendingBreakSegmentIndex &&
             state.lineEndGraphemeIndex > 0)) {
          emitLine();
          continue;
        }
        emitLine(state.pendingBreakSegmentIndex, 0,
                 state.pendingBreakUseLineWidth ? state.lineW
                                                : state.pendingBreakPaintWidth);
        continue;
      }

      if (w > fitLimit && !seg.breakableFitAdvances.empty()) {
        emitLine();
        appendBreakableFrom(i, 0);
        ++i;
        continue;
      }

      emitLine();
      continue;
    }

    appendWholeSegment(i, w);
    if (breakAfter) {
      state.pendingBreakSegmentIndex = i + 1;
      state.pendingBreakPaintWidth = state.lineW - w;
      state.pendingBreakUseLineWidth = seg.kind == SegmentBreakKind::Tab;
    }
    ++i;
  }

  if (state.hasContent)
    emitLine();
  return lineCount;
}

TextLayoutResult LayoutInner(const PreparedText &prepared, float maxWidth,
                             float lineFitEpsilon, bool applyWholeLineGuard,
                             const MeasureTextCallback *measure) {
  TextLayoutResult result;
  if (prepared.segments.empty()) {
    if (prepared.source.empty())
      result.lines.push_back({"", 0, 0, 0.0f, 0.0f});
    return result;
  }

  float fitLimit =
      maxWidth > 0.0f ? maxWidth + lineFitEpsilon : std::numeric_limits<float>::max();

  float y = 0.0f;
  WalkPreparedLines(prepared, maxWidth, fitLimit,
                    [&](float paintWidth, const LineBreakCursor &start,
                        const LineBreakCursor &end) {
                      std::string text =
                          MaterializeLineRange(prepared, start, end);
                      float linePaintWidth = paintWidth;
                      std::size_t byteStart =
                          start.segmentIndex < prepared.segments.size()
                              ? prepared.segments[start.segmentIndex].byteStart
                              : 0;
                      std::size_t byteEnd =
                          end.segmentIndex > 0 &&
                                  end.segmentIndex <= prepared.segments.size()
                              ? prepared.segments[end.segmentIndex - 1].byteEnd
                              : prepared.source.size();
                      result.lines.push_back(
                          {text, byteStart, byteEnd, linePaintWidth, y});
                      result.width = std::max(result.width, linePaintWidth);
                      result.height += prepared.options.lineHeight;
                      y += prepared.options.lineHeight;
                    });

  if (applyWholeLineGuard && maxWidth > 0.0f && result.lines.size() > 1 &&
      measure && !prepared.source.empty()) {
    bool hasForcedBreaks = false;
    for (const PreparedSegment &seg : prepared.segments) {
      if (seg.kind == SegmentBreakKind::HardBreak ||
          seg.kind == SegmentBreakKind::Tab) {
        hasForcedBreaks = true;
        break;
      }
    }
    if (!hasForcedBreaks) {
      const float wholeWidth = (*measure)(prepared.source, prepared.options);
      if (wholeWidth <= maxWidth + lineFitEpsilon) {
        result.lines.clear();
        result.lines.push_back(
            {prepared.source, 0, prepared.source.size(), wholeWidth, 0.0f});
        result.width = wholeWidth;
        result.height = prepared.options.lineHeight;
        return result;
      }
    }
    TextLayoutResult single =
        LayoutInner(prepared, 0.0f, lineFitEpsilon, false, measure);
    if (single.lines.size() == 1 &&
        single.width <= maxWidth + lineFitEpsilon) {
      return single;
    }
  }

  return result;
}

} // namespace

TextLayoutResult LayoutPreparedText(const PreparedText &prepared, float maxWidth,
                                    MeasureTextCallback measure,
                                    float lineFitEpsilon) {
  const MeasureTextCallback *measurePtr = measure ? &measure : nullptr;
  return LayoutInner(prepared, maxWidth, lineFitEpsilon, true, measurePtr);
}

} // namespace raym3::v2
