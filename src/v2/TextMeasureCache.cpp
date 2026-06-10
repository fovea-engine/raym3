#include "raym3/v2/TextMeasureCache.h"

#include "raym3/v2/TextEngine.h"

namespace raym3::v2 {

bool TextMeasureKey::operator==(const TextMeasureKey &o) const {
  return text == o.text && fontFamily == o.fontFamily &&
         fontSize == o.fontSize && bold == o.bold;
}

size_t TextMeasureKeyHash::operator()(const TextMeasureKey &k) const {
  size_t h = std::hash<std::string>{}(k.text);
  h ^= std::hash<std::string>{}(k.fontFamily) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<float>{}(k.fontSize) + 0x9e3779b9 + (h << 6) + (h >> 2);
  h ^= std::hash<bool>{}(k.bold) + 0x9e3779b9 + (h << 6) + (h >> 2);
  return h;
}

bool TextMeasureCache::lookup(const TextMeasureKey &key, TextMeasureEntry &out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = cache_.find(key);
  if (it == cache_.end()) return false;
  out = it->second;
  return true;
}

void TextMeasureCache::store(const TextMeasureKey &key, const TextMeasureEntry &entry) {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_[key] = entry;
}

void TextMeasureCache::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

TextMeasureEntry TextMeasureCache::measure(const TextMeasureKey &key, float maxWidth) {
  TextMeasureEntry cached;
  if (lookup(key, cached)) return cached;

  TextLayoutOptions opts;
  opts.fontSize = key.fontSize;
  opts.fontFamily = key.fontFamily;
  opts.weight = key.bold ? FontWeight::Bold : FontWeight::Regular;

  PreparedText prepared = PrepareText(key.text, opts);
  TextLayoutResult layout = LayoutText(prepared, maxWidth);

  TextMeasureEntry out{};
  out.width = layout.width;
  out.height = layout.height;
  out.lineCount = (int)layout.lines.size();
  if (out.lineCount < 1) out.lineCount = 1;
  store(key, out);
  return out;
}

TextMeasureCache &GlobalTextMeasureCache() {
  static TextMeasureCache cache;
  return cache;
}

} // namespace raym3::v2
