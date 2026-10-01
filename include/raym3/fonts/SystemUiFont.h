#pragma once

#include "raym3/types.h"
#include <string>

namespace raym3 {

// Resolve a loadable system UI font file for the default body face.
// Returns false when no suitable system face exists (web never calls this —
// it uses embedded Roboto instead).
bool ResolveSystemUiFontPath(FontWeight weight, FontStyle style,
                             std::string &outPath);

} // namespace raym3
