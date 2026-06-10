#pragma once

#include "raym3/v2/TextEngine.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace raym3::v2 {

// Cross-thread text measurement seam: the JS/layout thread may request shaped
// metrics; the render thread owns glyph atlas writes. Render-thread-only caches
// (EmojiFont::clusterCache_, Node::preparedTextCache) must not be touched from
// the JS thread — use this cache for layout-time measurement instead.
struct TextMeasureKey {
  std::string text;
  std::string fontFamily;
  float fontSize = 16.0f;
  bool bold = false;

  bool operator==(const TextMeasureKey &o) const;
};

struct TextMeasureKeyHash {
  size_t operator()(const TextMeasureKey &k) const;
};

struct TextMeasureEntry {
  float width = 0.0f;
  float height = 0.0f;
  int lineCount = 1;
};

class TextMeasureCache {
public:
  bool lookup(const TextMeasureKey &key, TextMeasureEntry &out) const;
  void store(const TextMeasureKey &key, const TextMeasureEntry &entry);
  void clear();

  // Layout thread: measure using shaped runs (mutex held briefly).
  TextMeasureEntry measure(const TextMeasureKey &key, float maxWidth);

private:
  mutable std::mutex mutex_;
  std::unordered_map<TextMeasureKey, TextMeasureEntry, TextMeasureKeyHash> cache_;
};

TextMeasureCache &GlobalTextMeasureCache();

} // namespace raym3::v2
