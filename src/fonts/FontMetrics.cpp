#include "raym3/fonts/FontMetrics.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace raym3 {
namespace {

std::uint16_t ReadU16(const unsigned char *p) {
  return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

std::int16_t ReadS16(const unsigned char *p) {
  return static_cast<std::int16_t>(ReadU16(p));
}

std::uint32_t ReadU32(const unsigned char *p) {
  return (static_cast<std::uint32_t>(p[0]) << 24) |
         (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) |
         static_cast<std::uint32_t>(p[3]);
}

// Offset of a table in the first font of an sfnt or TrueType collection.
// Returns 0 when absent — a real table never starts at 0 (the header does).
std::uint32_t FindTable(const unsigned char *data, std::size_t len,
                        const char tag[4]) {
  if (len < 12) return 0;
  std::size_t dir = 0;
  if (std::memcmp(data, "ttcf", 4) == 0) {
    if (len < 16) return 0;
    dir = ReadU32(data + 12);   // first font in the collection
    if (dir + 12 > len) return 0;
  }
  const std::uint16_t numTables = ReadU16(data + dir + 4);
  const std::size_t recordsEnd =
      dir + 12 + static_cast<std::size_t>(numTables) * 16;
  if (recordsEnd > len) return 0;
  for (std::uint16_t i = 0; i < numTables; ++i) {
    const unsigned char *rec = data + dir + 12 + static_cast<std::size_t>(i) * 16;
    if (std::memcmp(rec, tag, 4) == 0) return ReadU32(rec + 8);
  }
  return 0;
}

std::mutex &CacheMutex() {
  static std::mutex m;
  return m;
}

std::unordered_map<std::string, FontVMetrics> &PathCache() {
  static std::unordered_map<std::string, FontVMetrics> cache;
  return cache;
}

} // namespace

FontVMetrics ReadFontVMetrics(const unsigned char *data, std::size_t len) {
  FontVMetrics metrics;
  if (!data || len < 12) return metrics;

  const std::uint32_t head = FindTable(data, len, "head");
  const std::uint32_t hhea = FindTable(data, len, "hhea");
  // head.unitsPerEm is at +18, hhea.ascender/descender at +4/+6.
  if (head == 0 || hhea == 0 || head + 20 > len || hhea + 10 > len)
    return metrics;

  const std::uint16_t unitsPerEm = ReadU16(data + head + 18);
  const std::int16_t ascender = ReadS16(data + hhea + 4);
  const std::int16_t descender = ReadS16(data + hhea + 6);
  if (unitsPerEm == 0) return metrics;

  const float band = static_cast<float>(ascender - descender);
  if (band <= 0.0f) return metrics;

  metrics.emRatio = band / static_cast<float>(unitsPerEm);
  metrics.ascent = static_cast<float>(ascender) / static_cast<float>(unitsPerEm);
  metrics.descent = -static_cast<float>(descender) / static_cast<float>(unitsPerEm);
  return metrics;
}

FontVMetrics ReadFontVMetricsFromFile(const std::string &path) {
  if (path.empty()) return {};
  {
    std::lock_guard<std::mutex> lock(CacheMutex());
    auto it = PathCache().find(path);
    if (it != PathCache().end()) return it->second;
  }

  FontVMetrics metrics;
  if (FILE *f = std::fopen(path.c_str(), "rb")) {
    // The tables this needs live in the first few KB of every face we ship, but
    // the directory itself can point anywhere, so read the whole file rather
    // than guess. It happens once per path.
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size > 0) {
      std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
      if (std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size())
        metrics = ReadFontVMetrics(bytes.data(), bytes.size());
    }
    std::fclose(f);
  }

  std::lock_guard<std::mutex> lock(CacheMutex());
  PathCache()[path] = metrics;
  return metrics;
}

} // namespace raym3
