#pragma once

#include <cstddef>
#include <string>

namespace raym3 {

// Vertical metrics of a font face, normalized to the em square.
//
// Why raym3 needs them: raylib bakes glyphs with stbtt_ScaleForPixelHeight(),
// which maps the *hhea ascent−descent band* onto the requested pixel size. CSS,
// React Native and Lynx instead treat font-size as the **em size**. The band is
// ~1.17–1.20 em on the UI faces we ship, so a raylib font asked for 14 px draws
// an em of only ~11.9 px — every label came out ~15% smaller than the same
// declared size on the web or in React Native. FontManager bakes at
// `size * emRatio` and rebases the font so a draw at `size` lands on a real em.
struct FontVMetrics {
  // (hhea.ascender − hhea.descender) / head.unitsPerEm. 1.0 means the band and
  // the em square coincide and no correction is needed.
  float emRatio = 1.0f;
  // hhea.ascender / unitsPerEm — distance from the glyph-drawing origin (which
  // raylib anchors at the ascender) down to the baseline, in em.
  float ascent = 1.0f;
  // −hhea.descender / unitsPerEm, in em (positive below the baseline).
  float descent = 0.0f;
};

// Parse `head` + `hhea` out of an sfnt (ttf/otf/ttc) buffer. Returns the neutral
// 1.0 metrics when the data is not a font this parser understands, so a caller
// can always use the result without branching.
FontVMetrics ReadFontVMetrics(const unsigned char *data, std::size_t len);

// Same, from a file. Results are cached per path — the sfnt header, the table
// directory and the two tables are all this reads, but the file open is not
// free and font baking asks for the same face at many sizes.
FontVMetrics ReadFontVMetricsFromFile(const std::string &path);

} // namespace raym3
