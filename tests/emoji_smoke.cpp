#include "raym3/v2/EmojiFont.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/styles/Theme.h"
#include <raylib.h>
#include <cstdio>

using namespace raym3;
using namespace raym3::v2;

int main() {
  SetTraceLogLevel(LOG_INFO);
  InitWindow(900, 520, "emoji smoke");
  SetTargetFPS(60);
  FontManager::Initialize();
  Theme::Initialize();

  EmojiFont &ef = EmojiFont::Instance();
  printf("EmojiFont available: %d\n", (int)ef.Available());

  const char *lines[] = {
      "Hello world! \xF0\x9F\x98\x80",                          // 😀
      "Heart \xE2\x9D\xA4\xEF\xB8\x8F fire \xF0\x9F\x94\xA5",   // ❤️ 🔥
      "Family \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7", // 👨‍👩‍👧
      "Flag \xF0\x9F\x87\xBA\xF0\x9F\x87\xB8 thumbs \xF0\x9F\x91\x8D\xF0\x9F\x8F\xBD", // 🇺🇸 👍🏽
      "Keycap 1\xEF\xB8\x8F\xE2\x83\xA3 done",                 // 1️⃣
  };

  int frame = 0;
  while (!WindowShouldClose() && frame < 90) {
    BeginDrawing();
    ClearBackground(RAYWHITE);
    Font font = Theme::GetFont(32, FontWeight::Regular);
    float y = 30;
    for (const char *l : lines) {
      DrawTextWithEmoji(font, l, {30, y}, 32, 0, BLACK);
      y += 60;
    }
    EndDrawing();
    if (frame == 60) TakeScreenshot("emoji_smoke.png");
    frame++;
  }
  CloseWindow();
  return 0;
}
