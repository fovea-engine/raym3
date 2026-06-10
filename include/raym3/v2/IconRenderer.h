#pragma once

#include <raylib.h>

namespace raym3::v2 {

void RegisterMaterialIcon(int codepoint, int sizeDp, bool filled = true);
void DrawMaterialIcon(int codepoint, Rectangle bounds, Color color,
                      int sizeDp = 0, bool filled = true);
void ResetMaterialIconAtlas();

} // namespace raym3::v2
