#include "raym3/layout/Layout.h"
#include "raym3/layout/LayoutCard.h"
#include "raym3/raym3.h"
#include <raylib.h>

int main() {
  InitWindow(800, 600, "raym3 Layout Example");
  SetTargetFPS(60);
  SetWindowState(FLAG_WINDOW_RESIZABLE);

  raym3::Initialize();
  raym3::SetTheme(false);

  char textBuffer[256] = "";

  while (!WindowShouldClose()) {
    BeginDrawing();

    raym3::ColorScheme &scheme = raym3::Theme::GetColorScheme();
    ClearBackground(scheme.surface);

    raym3::BeginFrame();

    Rectangle screen = {0, 0, (float)GetScreenWidth(),
                        (float)GetScreenHeight()};

    raym3::Layout::Begin(screen);

    raym3::LayoutStyle mainStyle = raym3::Layout::Row();
    mainStyle.padding = 20;
    mainStyle.gap = 20;
    raym3::Layout::BeginContainer(mainStyle);

    raym3::LayoutStyle sidebarStyle = raym3::Layout::Column();
    sidebarStyle.width = 200;
    sidebarStyle.gap = 10;
    raym3::Layout::BeginScrollContainer(sidebarStyle, false, true);

    raym3::Button("Dashboard",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Tonal);
    raym3::Button("Settings",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Profile", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Analytics",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Reports", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Users", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Settings2",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Help", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Messages",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Notifications",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Calendar",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Tasks", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Documents",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Projects",
                  raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Team", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Text);
    raym3::Button("Logout", raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)),
                  raym3::ButtonVariant::Outlined);

    EndScissorMode();
    raym3::Layout::EndContainer();

    raym3::LayoutStyle contentStyle = raym3::Layout::Column();
    contentStyle.flexGrow = 1;
    contentStyle.gap = 20;
    raym3::Layout::BeginContainer(contentStyle);

    raym3::Text("Welcome Back!",
                raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)), 32,
                scheme.onSurface, raym3::FontWeight::Bold);

    raym3::TextField(textBuffer, sizeof(textBuffer),
                     raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 56)),
                     "Search or enter text");

    raym3::LayoutStyle cardsStyle = raym3::Layout::Row();
    cardsStyle.gap = 20;
    cardsStyle.height = 150;
    raym3::Layout::BeginScrollContainer(cardsStyle, true, false);

    for (int i = 0; i < 10; i++) {
      raym3::LayoutCard::BeginCard(raym3::Layout::Fixed(200, -1),
                                   i % 3 == 0   ? raym3::CardVariant::Elevated
                                   : i % 3 == 1 ? raym3::CardVariant::Filled
                                                : raym3::CardVariant::Outlined);
      raym3::LayoutCard::EndCard();
    }

    EndScissorMode();
    raym3::Layout::EndContainer();

    raym3::LayoutCard::BeginCard(raym3::Layout::Flex(),
                                 raym3::CardVariant::Outlined);
    raym3::LayoutCard::EndCard();

    raym3::Layout::EndContainer();

    raym3::Layout::EndContainer();

    raym3::Layout::End();

    raym3::EndFrame();
    EndDrawing();
  }

  raym3::Shutdown();
  CloseWindow();
  return 0;
}
