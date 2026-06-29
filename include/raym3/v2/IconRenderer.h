#pragma once

#include <raylib.h>

namespace raym3::v2 {

void SetIconFontSearchPrefix(const char* prefix);
void RegisterMaterialIcon(int codepoint, int sizeDp, bool filled = true);
void DrawMaterialIcon(int codepoint, Rectangle bounds, Color color,
                      int sizeDp = 0, bool filled = true);
void ResetMaterialIconAtlas();
// Drop all GPU-side state WITHOUT unloading (the device was re-initialized,
// so held texture ids are stale and may already be reused). The atlas
// rebuilds lazily on the next DrawMaterialIcon.
void IconRendererResetDeviceCache();

} // namespace raym3::v2
