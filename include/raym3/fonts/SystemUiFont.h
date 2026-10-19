#pragma once

#include "raym3/types.h"
#include <string>
#include <vector>

namespace raym3 {

// Resolve a loadable system UI font file for the default body face.
// Returns false when no suitable system face exists (web never calls this —
// it uses embedded Roboto instead).
bool ResolveSystemUiFontPath(FontWeight weight, FontStyle style,
                             std::string &outPath);

// Resolve a named CSS family to a platform font file, including the requested
// weight. Returns false when the family is unavailable so callers can advance
// to the next family in the CSS stack.
bool ResolveCssFontPath(const std::string &family, FontWeight weight,
                        FontStyle style, std::string &outPath);

// Resolve a system face that covers the requested codepoints. The returned
// face is used only for text runs whose selected CSS font lacks those glyphs.
bool ResolveSystemFallbackFontPath(const std::vector<int> &codepoints,
                                   std::string &outPath);

} // namespace raym3
