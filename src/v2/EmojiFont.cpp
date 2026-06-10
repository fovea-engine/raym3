#include "raym3/v2/EmojiFont.h"

#include "raym3/config.h"
#include "raym3/fonts/FontManager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace raym3::v2 {

// ── big-endian readers ────────────────────────────────────────────────────
namespace {
inline std::uint16_t RdU16(const std::uint8_t *p) {
  return (std::uint16_t)((p[0] << 8) | p[1]);
}
inline std::int16_t RdS16(const std::uint8_t *p) { return (std::int16_t)RdU16(p); }
inline std::uint32_t RdU32(const std::uint8_t *p) {
  return ((std::uint32_t)p[0] << 24) | ((std::uint32_t)p[1] << 16) |
         ((std::uint32_t)p[2] << 8) | p[3];
}
inline std::uint32_t MakeTag(const char *t) {
  return ((std::uint32_t)(std::uint8_t)t[0] << 24) |
         ((std::uint32_t)(std::uint8_t)t[1] << 16) |
         ((std::uint32_t)(std::uint8_t)t[2] << 8) | (std::uint8_t)t[3];
}

// UTS#51-ish cluster helpers — font-independent.
inline bool IsRegionalIndicator(std::uint32_t cp) {
  return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}
inline bool IsEmojiExtend(std::uint32_t cp) {
  return cp == 0x200D ||
         (cp >= 0xFE00 && cp <= 0xFE0F) ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) ||
         cp == 0x20E3 ||
         (cp >= 0xE0020 && cp <= 0xE007F);
}
inline bool IsEmojiStart(std::uint32_t cp) {
  return (cp >= 0x1F000 && cp <= 0x1FAFF) ||
         (cp >= 0x2600  && cp <= 0x27BF)  ||
         (cp >= 0x2B00  && cp <= 0x2BFF)  ||
         (cp >= 0x2300  && cp <= 0x23FF)  ||
         cp == 0x203C || cp == 0x2049 ||
         (cp == 0x2194 || cp == 0x2195 || cp == 0x2196 || cp == 0x2197 ||
          cp == 0x2198 || cp == 0x2199 || cp == 0x21A9 || cp == 0x21AA) ||
         cp == 0x2122 || cp == 0x2139 ||
         (cp >= 0x25AA && cp <= 0x25FF) ||
         (cp >= 0x2934 && cp <= 0x2935) ||
         cp == 0x3030 || cp == 0x303D ||
         cp == 0x3297 || cp == 0x3299 ||
         IsRegionalIndicator(cp);
}
inline bool IsKeycapBase(std::uint32_t cp) {
  return cp == 0x23 || cp == 0x2A || (cp >= 0x30 && cp <= 0x39);
}
inline bool IsNonStandalone(std::uint32_t cp) {
  return cp == 0x200D || (cp >= 0xFE00 && cp <= 0xFE0F) ||
         (cp >= 0x1F3FB && cp <= 0x1F3FF) || cp == 0x20E3 ||
         (cp >= 0xE0020 && cp <= 0xE007F);
}

struct Cp {
  std::uint32_t cp;
  std::size_t byteStart, byteEnd;
};

std::uint32_t DecodeUtf8(std::string_view s, std::size_t &i) {
  unsigned char c = (unsigned char)s[i];
  std::uint32_t cp;
  std::size_t extra;
  if (c < 0x80) { cp = c; extra = 0; }
  else if (c < 0xC0) { i++; return 0xFFFD; }
  else if (c < 0xE0) { cp = c & 0x1F; extra = 1; }
  else if (c < 0xF0) { cp = c & 0x0F; extra = 2; }
  else { cp = c & 0x07; extra = 3; }
  i++;
  for (std::size_t k = 0; k < extra && i < s.size(); ++k, ++i)
    cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
  return cp;
}

// Canonical raster size for OS/CBDT backends — drawn at this px, scaled at draw time.
static constexpr int kRasterPx = 128;

} // namespace

// ─────────────────────────────────────────────────────────────────────────────

EmojiFont &EmojiFont::Instance() {
  static EmojiFont inst;
  return inst;
}

void EmojiFont::SetRasterizer(Rasterizer fn) {
  rasterizer_ = std::move(fn);
  // Reset so EnsureBackend re-evaluates with the new rasterizer.
  backend_ = Backend::Unresolved;
  ResetTextureCache();
}

bool EmojiFont::Available() {
  EnsureBackend();
  return backend_ != Backend::None;
}

void EmojiFont::EnsureBackend() {
  if (backend_ != Backend::Unresolved) return;

  if (rasterizer_) {
    backend_ = Backend::Raster;
    TraceLog(LOG_INFO, "EmojiFont: using OS rasterizer backend");
    return;
  }

  if (LoadCbdtFont()) {
    backend_ = Backend::Cbdt;
    return;
  }

  backend_ = Backend::None;
  TraceLog(LOG_INFO, "EmojiFont: no emoji backend available");
}

bool EmojiFont::LoadCbdtFont() {
  if (fontParsed_) return !cmap_.empty() && !strike_.empty();
  fontParsed_ = true;

  const std::vector<std::string> candidates = {
      // Linux: system emoji fonts (checked before bundled to avoid shipping 10 MB)
      "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",        // Ubuntu/Debian
      "/usr/share/fonts/google-noto-emoji/NotoColorEmoji.ttf",    // Fedora
      "/usr/share/fonts/noto/NotoColorEmoji.ttf",                 // openSUSE / Arch
      "/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf",
      "/usr/share/fonts/NotoColorEmoji.ttf",
      // Bundled fallback (shipped with the app on Linux when system font absent;
      // always present on non-Linux since CBDT is only used there as last resort)
      std::string(RAYM3_RESOURCE_DIR) + "/fonts/NotoColorEmoji.ttf",
      "./resources/fonts/NotoColorEmoji.ttf",
      "./raym3/resources/fonts/NotoColorEmoji.ttf",
      "NotoColorEmoji.ttf",
  };
  std::string path;
  for (const auto &c : candidates)
    if (std::filesystem::exists(c)) { path = c; break; }
  if (path.empty()) return false;

  FILE *f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long sz = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (sz <= 0) { std::fclose(f); return false; }
  data_.resize((std::size_t)sz);
  if (std::fread(data_.data(), 1, (std::size_t)sz, f) != (std::size_t)sz) {
    std::fclose(f); data_.clear(); return false;
  }
  std::fclose(f);

  const std::uint8_t *base = data_.data();
  const std::size_t total = data_.size();
  if (total < 12) return false;

  std::uint16_t numTables = RdU16(base + 4);
  std::uint32_t cmapOff = 0, cblcOff = 0, gsubOff = 0;
  std::uint32_t cmapLen = 0, cblcLen = 0, gsubLen = 0;
  std::size_t rec = 12;
  for (std::uint16_t i = 0; i < numTables; ++i, rec += 16) {
    if (rec + 16 > total) break;
    std::uint32_t tag = RdU32(base + rec);
    std::uint32_t off = RdU32(base + rec + 8);
    std::uint32_t len = RdU32(base + rec + 12);
    if (tag == MakeTag("cmap")) { cmapOff = off; cmapLen = len; }
    else if (tag == MakeTag("CBLC")) { cblcOff = off; cblcLen = len; }
    else if (tag == MakeTag("CBDT")) { cbdtOffset_ = off; cbdtLength_ = len; }
    else if (tag == MakeTag("GSUB")) { gsubOff = off; gsubLen = len; }
  }
  (void)cmapLen; (void)cblcLen; (void)gsubLen;
  if (!cmapOff || !cblcOff || !cbdtOffset_) return false;

  // ── cmap ──────────────────────────────────────────────────────────────
  {
    const std::uint8_t *cm = base + cmapOff;
    std::uint16_t nSub = RdU16(cm + 2);
    std::uint32_t best = 0;
    int bestScore = -1;
    for (std::uint16_t i = 0; i < nSub; ++i) {
      const std::uint8_t *r = cm + 4 + i * 8;
      std::uint16_t plat = RdU16(r), enc = RdU16(r + 2);
      std::uint32_t subOff = RdU32(r + 4);
      const std::uint8_t *s = cm + subOff;
      std::uint16_t fmt = RdU16(s);
      int score = -1;
      if (fmt == 12 && plat == 3 && enc == 10) score = 5;
      else if (fmt == 12 && plat == 0) score = 4;
      else if (fmt == 12) score = 3;
      else if (fmt == 4 && plat == 3 && enc == 1) score = 2;
      else if (fmt == 4) score = 1;
      if (score > bestScore) { bestScore = score; best = subOff; }
    }
    if (best) {
      const std::uint8_t *s = cm + best;
      std::uint16_t fmt = RdU16(s);
      if (fmt == 12) {
        std::uint32_t nGroups = RdU32(s + 12);
        const std::uint8_t *g = s + 16;
        cmap_.reserve(nGroups);
        for (std::uint32_t i = 0; i < nGroups; ++i, g += 12)
          cmap_.push_back({RdU32(g), RdU32(g + 4), RdU32(g + 8)});
      } else if (fmt == 4) {
        std::uint16_t segX2 = RdU16(s + 6);
        std::uint16_t segCount = segX2 / 2;
        const std::uint8_t *endCodes = s + 14;
        const std::uint8_t *startCodes = endCodes + segX2 + 2;
        const std::uint8_t *idDelta = startCodes + segX2;
        const std::uint8_t *idRange = idDelta + segX2;
        for (std::uint16_t i = 0; i < segCount; ++i) {
          std::uint16_t endC = RdU16(endCodes + i * 2);
          std::uint16_t startC = RdU16(startCodes + i * 2);
          std::int16_t delta = RdS16(idDelta + i * 2);
          std::uint16_t rangeOff = RdU16(idRange + i * 2);
          if (rangeOff == 0) {
            for (std::uint32_t c = startC; c <= endC && c != 0xFFFF; ++c)
              cmap_.push_back({c, c, (std::uint16_t)(c + delta)});
          } else {
            for (std::uint32_t c = startC; c <= endC && c != 0xFFFF; ++c) {
              const std::uint8_t *gp = idRange + i * 2 + rangeOff + (c - startC) * 2;
              if (gp + 2 > base + total) break;
              std::uint16_t gi = RdU16(gp);
              if (gi != 0) gi = (std::uint16_t)(gi + delta);
              cmap_.push_back({c, c, gi});
            }
          }
        }
      }
    }
    std::sort(cmap_.begin(), cmap_.end(),
              [](const CmapRange &a, const CmapRange &b) { return a.start < b.start; });
  }

  // ── GSUB ligatures (type 4 + extension type 7) ────────────────────────
  if (gsubOff) {
    const std::uint8_t *g = base + gsubOff;
    std::uint32_t lookupListOff = RdU16(g + 8);
    const std::uint8_t *ll = g + lookupListOff;
    std::uint16_t lookupCount = RdU16(ll);
    for (std::uint16_t li = 0; li < lookupCount; ++li) {
      std::uint16_t lookupOff = RdU16(ll + 2 + li * 2);
      const std::uint8_t *lk = ll + lookupOff;
      std::uint16_t lookupType = RdU16(lk);
      std::uint16_t subCount = RdU16(lk + 4);
      for (std::uint16_t si = 0; si < subCount; ++si) {
        std::uint16_t subOff = RdU16(lk + 6 + si * 2);
        const std::uint8_t *st = lk + subOff;
        std::uint16_t effType = lookupType;
        const std::uint8_t *ligSub = st;
        if (lookupType == 7) {
          effType = RdU16(st + 2);
          std::uint32_t extOff = RdU32(st + 4);
          ligSub = st + extOff;
        }
        if (effType != 4) continue;
        std::uint16_t substFormat = RdU16(ligSub);
        if (substFormat != 1) continue;
        std::uint16_t covOff = RdU16(ligSub + 2);
        std::uint16_t ligSetCount = RdU16(ligSub + 4);
        const std::uint8_t *cov = ligSub + covOff;
        std::vector<std::uint16_t> covGlyphs;
        std::uint16_t covFmt = RdU16(cov);
        if (covFmt == 1) {
          std::uint16_t gc = RdU16(cov + 2);
          for (std::uint16_t i = 0; i < gc; ++i)
            covGlyphs.push_back(RdU16(cov + 4 + i * 2));
        } else if (covFmt == 2) {
          std::uint16_t rc = RdU16(cov + 2);
          for (std::uint16_t i = 0; i < rc; ++i) {
            const std::uint8_t *rr = cov + 4 + i * 6;
            std::uint16_t startG = RdU16(rr), endG = RdU16(rr + 2);
            for (std::uint32_t gg = startG; gg <= endG; ++gg)
              covGlyphs.push_back((std::uint16_t)gg);
          }
        }
        for (std::uint16_t i = 0; i < ligSetCount && i < (std::uint16_t)covGlyphs.size(); ++i) {
          std::uint16_t firstGlyph = covGlyphs[i];
          std::uint16_t ligSetOff = RdU16(ligSub + 6 + i * 2);
          const std::uint8_t *ls = ligSub + ligSetOff;
          std::uint16_t ligCount = RdU16(ls);
          for (std::uint16_t j = 0; j < ligCount; ++j) {
            std::uint16_t ligOff = RdU16(ls + 2 + j * 2);
            const std::uint8_t *lig = ls + ligOff;
            std::uint16_t ligGlyph = RdU16(lig);
            std::uint16_t compCount = RdU16(lig + 2);
            Ligature out;
            out.ligatureGlyph = ligGlyph;
            out.components.push_back(firstGlyph);
            for (std::uint16_t k = 1; k < compCount; ++k)
              out.components.push_back(RdU16(lig + 4 + (k - 1) * 2));
            ligatures_[firstGlyph].push_back(std::move(out));
          }
        }
      }
    }
    for (auto &[k, v] : ligatures_)
      std::sort(v.begin(), v.end(), [](const Ligature &a, const Ligature &b) {
        return a.components.size() > b.components.size();
      });
  }

  // ── CBLC: pick largest strike ─────────────────────────────────────────
  {
    const std::uint8_t *cb = base + cblcOff;
    std::uint32_t numSizes = RdU32(cb + 4);
    const std::uint8_t *bst = cb + 8;
    const std::size_t kBitmapSizeTable = 48;
    std::uint32_t bestArrOff = 0, bestNumSub = 0;
    float bestPpem = 0;
    for (std::uint32_t i = 0; i < numSizes; ++i) {
      const std::uint8_t *e = bst + i * kBitmapSizeTable;
      std::uint32_t arrOff = RdU32(e + 0);
      std::uint32_t numSub = RdU32(e + 8);
      std::uint8_t ppemX = e[44];
      if ((float)ppemX > bestPpem) {
        bestPpem = (float)ppemX;
        bestArrOff = arrOff; bestNumSub = numSub;
      }
    }
    strikePpem_ = bestPpem > 0 ? bestPpem : 128.0f;
    const std::uint8_t *arr = cb + bestArrOff;
    for (std::uint32_t i = 0; i < bestNumSub; ++i) {
      const std::uint8_t *a = arr + i * 8;
      std::uint16_t firstG = RdU16(a), lastG = RdU16(a + 2);
      std::uint32_t addOff = RdU32(a + 4);
      const std::uint8_t *ist = arr + addOff;
      IndexSubTable t;
      t.firstGlyph = firstG; t.lastGlyph = lastG;
      t.indexFormat = RdU16(ist); t.imageFormat = RdU16(ist + 2);
      t.imageDataOffset = RdU32(ist + 4);
      std::uint32_t n = (std::uint32_t)(lastG - firstG + 1);
      if (t.indexFormat == 1) {
        for (std::uint32_t k = 0; k <= n; ++k)
          t.offsets.push_back(RdU32(ist + 8 + k * 4));
      } else if (t.indexFormat == 3) {
        for (std::uint32_t k = 0; k <= n; ++k)
          t.offsets.push_back(RdU16(ist + 8 + k * 2));
      } else if (t.indexFormat == 2) {
        t.imageSize = RdU32(ist + 8);
      } else if (t.indexFormat == 4) {
        std::uint32_t numG = RdU32(ist + 8);
        for (std::uint32_t k = 0; k <= numG; ++k) {
          const std::uint8_t *p = ist + 12 + k * 4;
          t.glyphArray.push_back(RdU16(p));
          t.offsets.push_back(RdU16(p + 2));
        }
      } else if (t.indexFormat == 5) {
        t.imageSize = RdU32(ist + 8);
        std::uint32_t numG = RdU32(ist + 24);
        for (std::uint32_t k = 0; k < numG; ++k)
          t.glyphArray.push_back(RdU16(ist + 28 + k * 2));
      }
      strike_.push_back(std::move(t));
    }
  }

  bool ok = !cmap_.empty() && !strike_.empty();
  if (ok) {
    bool isSystem = path.find(RAYM3_RESOURCE_DIR) == std::string::npos &&
                    path.find("./") != 0 && path != "NotoColorEmoji.ttf";
    TraceLog(LOG_INFO,
             "EmojiFont: loaded CBDT %s (%s, cmap ranges=%zu, ligatures=%zu, strike ppem=%.0f)",
             path.c_str(), isSystem ? "system" : "bundled",
             cmap_.size(), ligatures_.size(), strikePpem_);
  }
  return ok;
}

// ── CBDT glyph helpers ────────────────────────────────────────────────────

std::uint16_t EmojiFont::GlyphForCodepoint(std::uint32_t cp) const {
  std::size_t lo = 0, hi = cmap_.size();
  while (lo < hi) {
    std::size_t mid = (lo + hi) / 2;
    if (cp < cmap_[mid].start) hi = mid;
    else if (cp > cmap_[mid].end) lo = mid + 1;
    else return (std::uint16_t)(cmap_[mid].startGlyph + (cp - cmap_[mid].start));
  }
  return 0;
}

std::size_t EmojiFont::MatchLigature(const std::vector<std::uint16_t> &glyphs,
                                     std::size_t pos,
                                     std::uint16_t &outGlyph) const {
  auto it = ligatures_.find(glyphs[pos]);
  if (it == ligatures_.end()) return 0;
  for (const Ligature &lig : it->second) {
    std::size_t n = lig.components.size();
    if (pos + n > glyphs.size()) continue;
    bool ok = true;
    for (std::size_t k = 0; k < n; ++k)
      if (glyphs[pos + k] != lig.components[k]) { ok = false; break; }
    if (ok) { outGlyph = lig.ligatureGlyph; return n; }
  }
  return 0;
}

// Map a UTF-8 emoji cluster string to a single CBDT glyph id via cmap + GSUB.
std::uint16_t EmojiFont::ShapeCluster(std::string_view clusterUtf8) const {
  std::vector<std::uint16_t> glyphs;
  for (std::size_t i = 0; i < clusterUtf8.size();) {
    std::uint32_t cp = DecodeUtf8(clusterUtf8, i);
    std::uint16_t g = GlyphForCodepoint(cp);
    if (g != 0) glyphs.push_back(g);
  }
  if (glyphs.empty()) return 0;
  std::size_t k = 0;
  while (k < glyphs.size()) {
    std::uint16_t ligGlyph = 0;
    std::size_t consumed = MatchLigature(glyphs, k, ligGlyph);
    if (consumed >= 2) {
      glyphs.erase(glyphs.begin() + (std::ptrdiff_t)k,
                   glyphs.begin() + (std::ptrdiff_t)(k + consumed));
      glyphs.insert(glyphs.begin() + (std::ptrdiff_t)k, ligGlyph);
    }
    ++k;
  }
  return glyphs.empty() ? 0 : glyphs[0];
}

bool EmojiFont::DecodeGlyphBitmap(std::uint16_t glyphId, Glyph &out) const {
  const IndexSubTable *t = nullptr;
  for (const auto &s : strike_)
    if (glyphId >= s.firstGlyph && glyphId <= s.lastGlyph) { t = &s; break; }
  if (!t) return false;

  std::uint32_t recOff = 0;
  if (t->indexFormat == 1 || t->indexFormat == 3) {
    std::uint32_t gi = (std::uint32_t)(glyphId - t->firstGlyph);
    if (gi + 1 >= t->offsets.size()) return false;
    if (t->offsets[gi + 1] == t->offsets[gi]) return false;
    recOff = t->imageDataOffset + t->offsets[gi];
  } else if (t->indexFormat == 2) {
    std::uint32_t gi = (std::uint32_t)(glyphId - t->firstGlyph);
    recOff = t->imageDataOffset + t->imageSize * gi;
  } else if (t->indexFormat == 4 || t->indexFormat == 5) {
    std::size_t pos = SIZE_MAX;
    for (std::size_t k = 0; k < t->glyphArray.size(); ++k)
      if (t->glyphArray[k] == glyphId) { pos = k; break; }
    if (pos == SIZE_MAX) return false;
    if (t->indexFormat == 4) recOff = t->imageDataOffset + t->offsets[pos];
    else recOff = t->imageDataOffset + t->imageSize * (std::uint32_t)pos;
  } else {
    return false;
  }

  const std::uint8_t *base = data_.data();
  const std::uint8_t *rec = base + cbdtOffset_ + recOff;
  if (rec + 4 > base + data_.size()) return false;

  std::uint32_t metricsLen = 0;
  if (t->imageFormat == 17) {
    out.pngHeight = rec[0]; out.pngWidth = rec[1];
    metricsLen = 5;
  } else if (t->imageFormat == 18) {
    out.pngHeight = rec[0]; out.pngWidth = rec[1];
    metricsLen = 8;
  } else if (t->imageFormat == 19) {
    metricsLen = 0;
  } else {
    return false;
  }

  const std::uint8_t *p = rec + metricsLen;
  if (p + 4 > base + data_.size()) return false;
  std::uint32_t dataLen = RdU32(p);
  p += 4;
  if (p + dataLen > base + data_.size()) return false;

  Image img = LoadImageFromMemory(".png", p, (int)dataLen);
  if (img.data == nullptr || img.width <= 0) return false;
  out.pngWidth = img.width;
  out.pngHeight = img.height;
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img);
  if (tex.id == 0) return false;
  SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
  out.texture = tex;
  return true;
}

// ── Public API ────────────────────────────────────────────────────────────

// Segmentation is purely Unicode-based and font-independent (works for both
// Raster and CBDT backends without accessing the font data).
std::vector<EmojiFont::Segment> EmojiFont::SegmentText(std::string_view utf8) {
  std::vector<Segment> segs;
  if (utf8.empty()) return segs;

  std::vector<Cp> cps;
  for (std::size_t i = 0; i < utf8.size();) {
    std::size_t start = i;
    std::uint32_t cp = DecodeUtf8(utf8, i);
    cps.push_back({cp, start, i});
  }

  std::size_t textStart = std::string_view::npos;
  auto flushText = [&](std::size_t end) {
    if (textStart != std::string_view::npos && end > textStart)
      segs.push_back({false, textStart, end});
    textStart = std::string_view::npos;
  };

  std::size_t n = cps.size(), i = 0;
  while (i < n) {
    std::uint32_t cp = cps[i].cp;
    bool startsEmoji = IsEmojiStart(cp);
    if (!startsEmoji && IsKeycapBase(cp)) {
      if (i + 1 < n && (cps[i + 1].cp == 0xFE0F || cps[i + 1].cp == 0x20E3))
        startsEmoji = true;
    }
    if (!startsEmoji) {
      if (textStart == std::string_view::npos) textStart = cps[i].byteStart;
      ++i;
      continue;
    }

    std::size_t j = i + 1;
    while (j < n) {
      std::uint32_t c = cps[j].cp;
      if (IsEmojiExtend(c)) { ++j; continue; }
      if (cps[j - 1].cp == 0x200D && (IsEmojiStart(c) || IsKeycapBase(c))) { ++j; continue; }
      if (IsRegionalIndicator(cps[i].cp) && IsRegionalIndicator(c) && (j - i) < 2) { ++j; continue; }
      break;
    }

    flushText(cps[i].byteStart);
    segs.push_back({true, cps[i].byteStart, cps[j - 1].byteEnd});
    i = j;
  }
  flushText(utf8.size());

  // merge adjacent plain-text segments
  std::vector<Segment> merged;
  for (auto &s : segs) {
    if (!s.isEmoji && !merged.empty() && !merged.back().isEmoji &&
        merged.back().byteEnd == s.byteStart)
      merged.back().byteEnd = s.byteEnd;
    else
      merged.push_back(s);
  }
  return merged;
}

const EmojiFont::Glyph *EmojiFont::GetCluster(std::string_view clusterUtf8) {
  EnsureBackend();
  if (backend_ == Backend::None) return nullptr;

  std::string key(clusterUtf8);
  auto it = clusterCache_.find(key);
  if (it != clusterCache_.end())
    return it->second.failed ? nullptr : &it->second;

  Glyph g;

  if (backend_ == Backend::Raster) {
    int w = 0, h = 0;
    unsigned char *rgba = rasterizer_(key.c_str(), kRasterPx, &w, &h);
    if (!rgba || w <= 0 || h <= 0) {
      if (rgba) std::free(rgba);
      g.failed = true;
      clusterCache_[key] = g;
      return nullptr;
    }
    Image img;
    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    Texture2D tex = LoadTextureFromImage(img);
    std::free(rgba);
    if (tex.id == 0) {
      g.failed = true;
      clusterCache_[key] = g;
      return nullptr;
    }
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    g.texture = tex;
    g.pngWidth = w;
    g.pngHeight = h;
  } else {
    // Backend::Cbdt
    std::uint16_t glyphId = ShapeCluster(clusterUtf8);
    if (glyphId == 0 || !DecodeGlyphBitmap(glyphId, g)) {
      g.failed = true;
      clusterCache_[key] = g;
      return nullptr;
    }
  }

  auto &stored = clusterCache_[key] = g;
  return &stored;
}

void EmojiFont::ResetTextureCache() {
  for (auto &[k, g] : clusterCache_)
    if (g.texture.id != 0) UnloadTexture(g.texture);
  clusterCache_.clear();
}

// ── Draw / measure helpers ─────────────────────────────────────────────────

void DrawTextWithEmoji(Font font, std::string_view utf8, Vector2 pos,
                       float fontSize, float spacing, Color tint) {
  EmojiFont &ef = EmojiFont::Instance();
  auto segs = ef.SegmentText(utf8);
  bool anyEmoji = false;
  for (const auto &s : segs) if (s.isEmoji) { anyEmoji = true; break; }
  if (!anyEmoji || !ef.Available()) {
    DrawTextEx(font, std::string(utf8).c_str(), pos, fontSize, spacing, tint);
    return;
  }

  float x = pos.x;
  for (const auto &s : segs) {
    std::string sub(utf8.substr(s.byteStart, s.byteEnd - s.byteStart));
    if (!s.isEmoji) {
      DrawTextEx(font, sub.c_str(), {x, pos.y}, fontSize, spacing, tint);
      x += MeasureTextEx(font, sub.c_str(), fontSize, spacing).x + spacing;
    } else {
      const float box = fontSize;
      const EmojiFont::Glyph *g = ef.GetCluster(sub);
      if (g && g->texture.id != 0) {
        float aspect = g->pngHeight > 0 ? (float)g->pngWidth / (float)g->pngHeight : 1.0f;
        float h = box, w = box * aspect;
        float dx = x + (box - w) * 0.5f;
        float dy = pos.y + fontSize * 0.08f;
        Rectangle src = {0, 0, (float)g->pngWidth, (float)g->pngHeight};
        Rectangle dst = {dx, dy, w, h};
        DrawTexturePro(g->texture, src, dst, {0, 0}, 0.0f, WHITE);
      }
      x += box + spacing;
    }
  }
}

Vector2 MeasureTextWithEmoji(Font font, std::string_view utf8, float fontSize,
                             float spacing) {
  EmojiFont &ef = EmojiFont::Instance();
  auto segs = ef.SegmentText(utf8);
  bool anyEmoji = false;
  for (const auto &s : segs) if (s.isEmoji) { anyEmoji = true; break; }
  if (!anyEmoji || !ef.Available())
    return MeasureTextEx(font, std::string(utf8).c_str(), fontSize, spacing);

  float w = 0, h = fontSize;
  for (const auto &s : segs) {
    if (!s.isEmoji) {
      std::string sub(utf8.substr(s.byteStart, s.byteEnd - s.byteStart));
      Vector2 m = MeasureTextEx(font, sub.c_str(), fontSize, spacing);
      w += m.x + spacing;
      h = std::max(h, m.y);
    } else {
      w += fontSize + spacing;
    }
  }
  return {w, h};
}

// Grapheme boundaries aware of emoji clusters — used by TextField for cursor
// movement, selection, and backspace so clusters are treated as atomic units.
std::vector<std::size_t> EmojiAwareGraphemeBoundaries(std::string_view utf8) {
  std::vector<std::size_t> bounds;
  bounds.push_back(0);

  if (utf8.empty()) return bounds;

  // Decode to codepoints with byte spans.
  struct CpSpan { std::uint32_t cp; std::size_t start, end; };
  std::vector<CpSpan> cps;
  for (std::size_t i = 0; i < utf8.size();) {
    std::size_t s = i;
    std::uint32_t cp = DecodeUtf8(utf8, i);
    cps.push_back({cp, s, i});
  }

  std::size_t n = cps.size(), i = 0;
  while (i < n) {
    std::uint32_t cp = cps[i].cp;
    bool startsEmoji = IsEmojiStart(cp);
    if (!startsEmoji && IsKeycapBase(cp) && i + 1 < n &&
        (cps[i + 1].cp == 0xFE0F || cps[i + 1].cp == 0x20E3))
      startsEmoji = true;

    if (startsEmoji) {
      // Consume the whole emoji cluster as one grapheme.
      std::size_t j = i + 1;
      while (j < n) {
        std::uint32_t c = cps[j].cp;
        if (IsEmojiExtend(c)) { ++j; continue; }
        if (cps[j - 1].cp == 0x200D && (IsEmojiStart(c) || IsKeycapBase(c))) { ++j; continue; }
        if (IsRegionalIndicator(cps[i].cp) && IsRegionalIndicator(c) && (j - i) < 2) { ++j; continue; }
        break;
      }
      bounds.push_back(cps[j - 1].end);
      i = j;
    } else {
      // Plain grapheme: single codepoint (simplified — no Unicode combining classes,
      // but sufficient for Latin + common CJK text alongside emoji).
      bounds.push_back(cps[i].end);
      ++i;
    }
  }

  return bounds;
}

} // namespace raym3::v2
