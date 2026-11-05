// raym3 v1 (immediate-mode) demo.
//
// Every frame calls the components directly, and Layout::Frame (Yoga flexbox)
// places them. Nothing is retained between frames except the app's own state.
// Pass --autoplay to drive a scripted walkthrough for recordings.
#include "raym3/raym3.h"
#include "raym3/components/Navigation.h"
#include "raym3/components/SegmentedButton.h"
#include "raym3/layout/Layout.h"
#include <cmath>
#include <raylib.h>
#include <string>

using raym3::Layout;
using raym3::LayoutStyle;

namespace {

LayoutStyle Row(float height, float gap = 12.0f) {
  LayoutStyle s = Layout::Row();
  s.height = height;
  s.gap = gap;
  s.align = 2; // center children vertically, like align-items: center
  return s;
}

} // namespace

int main(int argc, char **argv) {
  const bool autoplay = argc > 1 && std::string(argv[1]) == "--autoplay";
  if (autoplay)
    SetConfigFlags(FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_TOPMOST);
  InitWindow(1200, 760, "raym3 v1 immediate-mode demo");
  SetTargetFPS(60);
  raym3::Initialize();
  raym3::SetTheme(true);

  char text[256] = "Double-click a word to select it";
  bool filterSelected = true;
  bool checked = true;
  bool switchOn = false;
  int radio = 0;
  float volume = 40.0f;
  int segment = 1;
  int nav = 0;
  float progress = 0.0f;

  static raym3::SegmentedButtonItem segments[] = {
      {"Day", "wb_sunny"}, {"Week", "calendar_view_week"}, {"Month", "calendar_month"}};
  static raym3::NavigationItem navItems[] = {
      {.label = "Home", .icon = "home"},
      {.label = "Search", .icon = "search"},
      {.label = "Inbox", .icon = "mail", .badgeLabel = "3", .showBadge = true},
      {.label = "Me", .icon = "person"}};

  for (int frame = 0; !WindowShouldClose(); ++frame) {
    if (autoplay && frame >= 120 && frame % 60 == 0) {
      // Walk through state changes so a recording shows each control move.
      const int step = (frame - 120) / 60;
      switchOn = step % 2 == 0 ? !switchOn : switchOn;
      checked = step % 3 == 0 ? !checked : checked;
      radio = step % 3;
      segment = step % 3;
      nav = step % 4;
      filterSelected = step % 2 == 1;
    }
    volume = autoplay ? 50.0f + 40.0f * std::sin(static_cast<float>(GetTime()))
                      : volume;
    progress = std::fmod(static_cast<float>(GetTime()) * 0.2f, 1.0f);

    BeginDrawing();
    const raym3::ColorScheme &scheme = raym3::Theme::GetColorScheme();
    ClearBackground(scheme.surface);
    raym3::BeginFrame();

    const Rectangle screen = {0, 0, static_cast<float>(GetScreenWidth()),
                              static_cast<float>(GetScreenHeight())};
    // Two passes: measure, then draw with this frame's bounds. Components only
    // draw (and read input) when draw is true.
    Layout::Frame(screen, [&](bool draw) {
      LayoutStyle root = Layout::Column();
      root.flexGrow = 1;
      root.padding = 24.0f;
      root.gap = 16.0f;
      Layout::BeginContainer(root);

      Rectangle title = Layout::Alloc(Layout::Fixed(-1, 32));
      Rectangle caption = Layout::Alloc(Layout::Fixed(-1, 18));
      if (draw) {
        raym3::Text("raym3 v1  |  Immediate mode with flex layout", title, 24,
                    scheme.onSurface, raym3::FontWeight::Bold);
        raym3::Text("Same layout engine, input rules, and anti-aliasing as v2",
                    caption, 13, scheme.onSurfaceVariant);
      }

      // Row 1: actions, badge, chips, text field.
      Layout::BeginContainer(Row(64));
      Rectangle filled = Layout::Alloc(Layout::Fixed(140, 40));
      Rectangle tonal = Layout::Alloc(Layout::Fixed(140, 40));
      Rectangle bell = Layout::Alloc(Layout::Fixed(48, 48));
      Rectangle chipA = Layout::Alloc(Layout::Fixed(132, 32));
      Rectangle chipB = Layout::Alloc(Layout::Fixed(112, 32));
      Rectangle field = Layout::Alloc(Layout::Fixed(360, 56));
      Layout::EndContainer();
      if (draw) {
        raym3::Button("Filled", filled, raym3::ButtonVariant::Filled);
        raym3::Button("Tonal", tonal, raym3::ButtonVariant::Tonal);
        raym3::IconButton("notifications", bell, raym3::ButtonVariant::Text);
        raym3::Badge(bell, "2");
        raym3::Chip("Selected", chipA, &filterSelected,
                    {.variant = raym3::ChipVariant::Filter});
        raym3::Chip("Assist", chipB,
                    {.variant = raym3::ChipVariant::Assist, .leadingIcon = "event"});
        raym3::TextField(text, sizeof(text), field, "Text field");
      }

      // Row 2: selection controls.
      Layout::BeginContainer(Row(48, 20));
      Rectangle box = Layout::Alloc(Layout::Fixed(130, 40));
      Rectangle sw = Layout::Alloc(Layout::Fixed(130, 40));
      Rectangle r0 = Layout::Alloc(Layout::Fixed(110, 40));
      Rectangle r1 = Layout::Alloc(Layout::Fixed(110, 40));
      Rectangle r2 = Layout::Alloc(Layout::Fixed(110, 40));
      LayoutStyle sliderStyle = Layout::Flex(1);
      sliderStyle.height = 40.0f;
      Rectangle slider = Layout::Alloc(sliderStyle);
      Layout::EndContainer();
      if (draw) {
        raym3::Checkbox("Checkbox", box, &checked);
        raym3::Switch("Switch", sw, &switchOn);
        if (raym3::RadioButton("One", r0, radio == 0)) radio = 0;
        if (raym3::RadioButton("Two", r1, radio == 1)) radio = 1;
        if (raym3::RadioButton("Three", r2, radio == 2)) radio = 2;
        raym3::SliderOptions opts;
        opts.startIcon = "volume_mute";
        opts.endIcon = "volume_up";
        volume = raym3::Slider(slider, volume, 0.0f, 100.0f, nullptr, opts);
      }

      // Row 3: segmented button, FABs, progress.
      Layout::BeginContainer(Row(96, 20));
      Rectangle seg = Layout::Alloc(Layout::Fixed(340, 40));
      Rectangle fab = Layout::Alloc(Layout::Fixed(56, 56));
      Rectangle efab = Layout::Alloc(Layout::Fixed(150, 56));
      Rectangle spinner = Layout::Alloc(Layout::Fixed(48, 48));
      Rectangle bar = Layout::Alloc(Layout::Flex(1));
      Layout::EndContainer();
      if (draw) {
        raym3::SegmentedButton(seg, segments, 3, &segment);
        raym3::FloatingActionButton("add", fab);
        raym3::ExtendedFloatingActionButton("Compose", "edit", efab);
        raym3::CircularProgressIndicator(spinner, 0.0f, true);
        raym3::LinearProgressIndicator({bar.x, bar.y + bar.height * 0.5f - 2,
                                        bar.width, 4},
                                       progress);
      }

      // Row 4: card that fills the remaining height.
      LayoutStyle grow = Layout::Flex(1);
      Rectangle card = Layout::Alloc(grow);
      if (draw) {
        raym3::Card(card, raym3::CardVariant::Filled);
        raym3::Text("Card fills the leftover height (flexGrow: 1)",
                    {card.x + 20, card.y + 20, card.width - 40, 24}, 16,
                    scheme.onSurface, raym3::FontWeight::Medium);
        raym3::Text("Resize the window: every row reflows from the same "
                    "styles, with no hand-placed coordinates.",
                    {card.x + 20, card.y + 50, card.width - 40, 20}, 13,
                    scheme.onSurfaceVariant);
      }

      // Bottom navigation.
      Rectangle navBar = Layout::Alloc(Layout::Fixed(-1, 80));
      if (draw)
        raym3::NavigationBar(navBar, navItems, 4, &nav);

      Layout::EndContainer();
    });

    raym3::EndFrame();
    EndDrawing();
  }

  raym3::Shutdown();
  CloseWindow();
  return 0;
}
