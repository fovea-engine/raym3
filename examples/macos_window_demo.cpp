#include <raylib.h>
#include <algorithm>

#include "raym3/fonts/FontManager.h"
#include "raym3/platform/MacOSWindow.h"

int main() {
  // raylib must request a transparent framebuffer before GLFW creates the
  // Cocoa window. Setting NSWindow opaque=NO afterward is not sufficient.
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_TRANSPARENT |
                 FLAG_MSAA_4X_HINT);
  InitWindow(760, 600, "raym3 macOS Transparency Demo");
  SetTargetFPS(60);
  raym3::FontManager::Initialize();
  Font uiFont = raym3::FontManager::LoadFont(raym3::FontWeight::Regular,
                                            raym3::FontStyle::Normal, 20);
  Font headingFont = raym3::FontManager::LoadFont(raym3::FontWeight::Medium,
                                                 raym3::FontStyle::Normal, 28);

  raym3::platform::MacOSWindowOptions options;
  options.transparentBackground = true;
  options.transparentTitlebar = true;
  options.fullSizeContentView = true;
  options.showWindowBorder = true;
  options.windowBorderColor = Color{245, 248, 255, 230};
  options.windowBorderWidth = 1.5f;
  options.windowBorderCornerWidth = 3.0f;
  raym3::platform::ConfigureMacOSWindow(options);
  raym3::platform::SetMacOSWindowControlsPosition(
      {Vector2{20, 30}, Vector2{42, 30}, Vector2{64, 30}});
  raym3::platform::SetMacOSWindowMaterialRegions({
      // Match raylib's radius formula: min(width, height) * roundness / 2.
      {{8, 8, 744, 88}, raym3::platform::MacOSWindowMaterial::Hud, 22},
      {{28, 116, 704, 450}, raym3::platform::MacOSWindowMaterial::Hud, 12.38f},
      {{54, 148, 652, 120}, raym3::platform::MacOSWindowMaterial::Sidebar, 7.2f},
      {{54, 294, 300, 170}, raym3::platform::MacOSWindowMaterial::Popover, 8.5f},
      {{378, 294, 328, 170}, raym3::platform::MacOSWindowMaterial::UnderWindowBackground, 8.5f},
  });
  // The full titlebar component is draggable except for native controls.
  raym3::platform::SetMacOSWindowDragRegions({{{0, 0, 760, 104}}});

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(Color{0, 0, 0, 0});

    // Distinct alpha levels make the compositor result easy to see.
    DrawRectangleRounded(Rectangle{8, 8, 744, 88}, 0.5f, 24,
                         Color{22, 28, 42, 46});
    // App mark and title in a double-height frosted titlebar.
    DrawCircle(162, 52, 19, Color{90, 154, 255, 220});
    DrawTextEx(headingFont, "R", Vector2{153, 37}, 27, 0, RAYWHITE);
    DrawTextEx(headingFont, "raym3", Vector2{194, 30}, 24, 0, RAYWHITE);
    DrawTextEx(uiFont, "Window materials demo", Vector2{194, 58}, 16, 0,
               Color{240, 244, 255, 225});

    DrawRectangleRounded(Rectangle{28, 116, 704, 450}, 0.055f, 24,
                         Color{14, 20, 32, 90});
    DrawRectangleRounded(Rectangle{54, 148, 652, 120}, 0.12f, 18,
                         Color{90, 154, 255, 42});
    DrawTextEx(headingFont, "macOS window materials", Vector2{76, 174}, 28, 0,
               RAYWHITE);
    DrawTextEx(uiFont, "AppKit blur regions sit behind raylib drawing.",
               Vector2{76, 214}, 20, 0, Color{240, 244, 255, 235});

    DrawRectangleRounded(Rectangle{54, 294, 300, 170}, 0.1f, 18,
                         Color{250, 250, 255, 24});
    DrawTextEx(uiFont, "SIDEBAR MATERIAL", Vector2{76, 316}, 15, 0,
               Color{250, 250, 255, 230});
    DrawTextEx(headingFont, "Per-region blur", Vector2{76, 350}, 22, 0, RAYWHITE);
    DrawRectangleRounded(Rectangle{378, 294, 328, 170}, 0.1f, 18,
                         Color{10, 12, 18, 70});
    DrawTextEx(uiFont, "UNDER WINDOW", Vector2{402, 316}, 15, 0,
               Color{250, 250, 255, 230});
    DrawTextEx(headingFont, "Adjust alpha", Vector2{402, 350}, 22, 0,
               RAYWHITE);
    DrawTextEx(uiFont, "Drag over wallpaper to inspect the frosted regions.",
               Vector2{76, 516}, 16, 0, Color{240, 244, 255, 240});
    EndDrawing();
  }

  if (uiFont.texture.id != 0) raym3::FontManager::UnloadFont(uiFont);
  if (headingFont.texture.id != 0) raym3::FontManager::UnloadFont(headingFont);
  raym3::FontManager::Shutdown();
  CloseWindow();
  return 0;
}
