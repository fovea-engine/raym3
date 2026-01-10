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

    // Sidebar buttons with tooltips
    Rectangle dashboardBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Dashboard", dashboardBounds, raym3::ButtonVariant::Tonal);
    raym3::Tooltip(dashboardBounds, "View your dashboard");

    Rectangle settingsBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Settings", settingsBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(settingsBounds, "Configure application settings");

    Rectangle profileBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Profile", profileBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(profileBounds, "Edit your profile");

    Rectangle analyticsBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Analytics", analyticsBounds, raym3::ButtonVariant::Text);
    // Rich tooltip for analytics
    raym3::TooltipOptions analyticsOpts;
    analyticsOpts.title = "Analytics Dashboard";
    raym3::Tooltip(analyticsBounds, "View detailed usage statistics and reports", analyticsOpts);

    Rectangle reportsBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Reports", reportsBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(reportsBounds, "Generate and download reports");

    Rectangle usersBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Users", usersBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(usersBounds, "Manage user accounts");

    Rectangle helpBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Help", helpBounds, raym3::ButtonVariant::Text);
    // Rich tooltip with action
    raym3::TooltipOptions helpOpts;
    helpOpts.title = "Need Help?";
    helpOpts.actionText = "Open Docs";
    helpOpts.onAction = []{ TraceLog(LOG_INFO, "Opening documentation..."); };
    raym3::Tooltip(helpBounds, "Get help and documentation", helpOpts);

    Rectangle messagesBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Messages", messagesBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(messagesBounds, "View your messages");

    Rectangle notifBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Notifications", notifBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(notifBounds, "View notifications");

    Rectangle calendarBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Calendar", calendarBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(calendarBounds, "View your calendar");

    Rectangle tasksBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Tasks", tasksBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(tasksBounds, "Manage your tasks");

    Rectangle docsBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Documents", docsBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(docsBounds, "Browse documents");

    Rectangle projectsBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Projects", projectsBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(projectsBounds, "Manage projects");

    Rectangle teamBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Team", teamBounds, raym3::ButtonVariant::Text);
    raym3::Tooltip(teamBounds, "Team management");

    Rectangle logoutBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button("Logout", logoutBounds, raym3::ButtonVariant::Outlined);
    // Rich tooltip for logout
    raym3::TooltipOptions logoutOpts;
    logoutOpts.title = "Sign Out";
    raym3::Tooltip(logoutBounds, "Sign out of your account", logoutOpts);

    EndScissorMode();
    raym3::Layout::EndContainer();

    raym3::LayoutStyle contentStyle = raym3::Layout::Column();
    contentStyle.flexGrow = 1;
    contentStyle.gap = 20;
    raym3::Layout::BeginContainer(contentStyle);

    raym3::Text("Welcome Back!",
                raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40)), 32,
                scheme.onSurface, raym3::FontWeight::Bold);

    Rectangle searchBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 56));
    raym3::TextField(textBuffer, sizeof(textBuffer), searchBounds,
                     "Search or enter text");
    raym3::Tooltip(searchBounds, "Type to search");

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
