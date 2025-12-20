#include "raym3/raym3.h"
#include <cstdio>
#include <raylib.h>

int main() {
  InitWindow(1200, 1000, "raym3 Example");
  SetTargetFPS(60);

  raym3::Initialize();
  raym3::SetTheme(false);

  char textBuffer[256] = "";
  bool checkboxChecked = false;
  bool switchChecked = false;
  float sliderValue = 50.0f;
  int selectedMenuItem = 0;
  bool showDialog = false;

  while (!WindowShouldClose()) {
    BeginDrawing();

    raym3::ColorScheme &scheme = raym3::Theme::GetColorScheme();
    ClearBackground(scheme.surface);

    raym3::BeginFrame();

    float y = 20.0f;
    float spacing = 60.0f;

    if (raym3::Button("Filled Button", {20, y, 150, 40},
                      raym3::ButtonVariant::Filled)) {
      showDialog = true;
    }
    y += spacing;

    raym3::Button("Text Button", {20, y, 150, 40}, raym3::ButtonVariant::Text);
    y += spacing;

    if (raym3::Button("Outlined Button", {20, y, 150, 40},
                      raym3::ButtonVariant::Outlined)) {
      raym3::SnackbarComponent::Show(
          "Message Sent", 4.0f, {"UNDO", []() { printf("Undo clicked\n"); }});
    }
    y += spacing;

    raym3::TextField(textBuffer, sizeof(textBuffer), {20, y, 250, 56},
                     "Text Field");
    y += spacing + 20;

    static char outlinedWithIconsBuffer[256] = "";
    static bool showPassword = false;

    raym3::TextFieldOptions outlinedIconsOpts;
    outlinedIconsOpts.variant = raym3::TextFieldVariant::Outlined;
    outlinedIconsOpts.leadingIcon = "person";
    outlinedIconsOpts.trailingIcon =
        showPassword ? "visibility" : "visibility_off";
    outlinedIconsOpts.placeholder = "Password";
    outlinedIconsOpts.passwordMode = !showPassword;
    outlinedIconsOpts.onTrailingIconClick = []() {
      showPassword = !showPassword;
      printf("Password visibility toggled: %s\n",
             showPassword ? "visible" : "hidden");
      return true;
    };
    raym3::TextField(outlinedWithIconsBuffer, sizeof(outlinedWithIconsBuffer),
                     {20, y, 300, 56}, "Outlined with Icons",
                     outlinedIconsOpts);
    y += spacing + 20;

    // Checkbox and Switch on same row
    raym3::Checkbox("Checkbox", {20, y, 120, 24}, &checkboxChecked);
    raym3::Switch("Switch", {150, y, 120, 24}, &switchChecked);
    y += spacing;

    sliderValue =
        raym3::Slider({20, y, 250, 40}, sliderValue, 0.0f, 100.0f, "Slider");
    y += spacing + 20;

    // Slider with inset icons (both ends)
    static float volumeValue = 50.0f;
    raym3::SliderOptions volumeOpts;
    volumeOpts.startIcon = "volume_mute";
    volumeOpts.endIcon = "volume_up";
    volumeOpts.showValueIndicator = true;
    volumeValue = raym3::Slider({20, y, 250, 40}, volumeValue, 0.0f, 100.0f,
                                "Volume", volumeOpts);
    y += spacing + 20;

    // Slider with value bubble
    static float rangeValue = 50.0f;
    raym3::SliderOptions rangeOpts;
    rangeOpts.showValueIndicator = true;
    rangeOpts.valueFormat = "%.0f%%";
    rangeValue = raym3::Slider({20, y, 250, 40}, rangeValue, 0.0f, 100.0f,
                               "Range", rangeOpts);
    y += spacing + 20;

    // Discrete slider with tick marks (M3 feature)
    static float discreteValue = 50.0f;
    raym3::SliderOptions discreteOpts;
    discreteOpts.stepValue = 10.0f;
    discreteOpts.showTickMarks = true;
    discreteOpts.showStopIndicators = true;
    discreteOpts.showValueIndicator = true;
    discreteValue = raym3::Slider({20, y, 250, 40}, discreteValue, 0.0f, 100.0f,
                                  "Discrete", discreteOpts);
    y += spacing + 20;

    // Range slider (multi-thumb)
    static std::vector<float> rangeValues = {25.0f, 75.0f};
    raym3::RangeSliderOptions rangeSliderOpts;
    rangeSliderOpts.showValueIndicators = true;
    rangeSliderOpts.minDistance = 10.0f;
    rangeValues = raym3::RangeSlider({20, y, 250, 40}, rangeValues, 0.0f,
                                     100.0f, "Range Slider", rangeSliderOpts);
    y += spacing + 20;

    // Moved RadioButtons and SegmentedButton to Column 1 flow
    static int selectedOption = 0;
    if (raym3::RadioButton("Option 1", {20, y, 120, 48}, selectedOption == 0))
      selectedOption = 0;
    if (raym3::RadioButton("Option 2", {140, y, 120, 48}, selectedOption == 1))
      selectedOption = 1;
    if (raym3::RadioButton("Option 3", {260, y, 120, 48}, selectedOption == 2))
      selectedOption = 2;
    y += 60;

    static int selectedSegment = 0;
    static raym3::SegmentedButtonItem segmentItems[] = {
        {"Day", "wb_sunny"},
        {"Week", "calendar_view_week"},
        {"Month", "calendar_month"},
        {"Year", "calendar_today"}};

    raym3::SegmentedButton({20, y, 320, 40}, segmentItems, 4, &selectedSegment);

    // --- Column 2 (x=420) ---
    float col2X = 420.0f;
    float col2Y = 20.0f;

    static raym3::MenuItem menuItems[] = {
        {.text = "Cut", .leadingIcon = "content_cut", .trailingText = "Cmd+X"},
        {.text = "Copy",
         .leadingIcon = "content_copy",
         .trailingText = "Cmd+C"},
        {.text = "Paste",
         .leadingIcon = "content_paste",
         .trailingText = "Cmd+V"},
        {.isDivider = true},
        {.text = "Settings", .leadingIcon = "settings"},
        {.text = "Help", .leadingIcon = "help", .disabled = true}};
    raym3::Menu({col2X, col2Y, 240, 300}, menuItems, 6, &selectedMenuItem);

    static raym3::MenuItem gapMenuItems[] = {
        {.text = "Group A Item 1"}, {.text = "Group A Item 2"}, {.isGap = true},
        {.text = "Group B Item 1"}, {.text = "Group B Item 2"},
    };
    static int selectedGapMenuItem = -1;
    raym3::Menu({col2X + 260, col2Y, 200, 250}, gapMenuItems, 5,
                &selectedGapMenuItem);

    // Icon-only menu demo (horizontal toolbar style)
    static raym3::MenuItem iconOnlyItems[] = {
        {.text = nullptr, .leadingIcon = "undo"},
        {.text = nullptr, .leadingIcon = "redo"},
        {.isDivider = true}, // Separator
        {.text = nullptr, .leadingIcon = "content_cut"},
        {.text = nullptr, .leadingIcon = "content_copy"},
        {.text = nullptr, .leadingIcon = "content_paste"},
        {.isDivider = true}, // Separator
        {.text = nullptr, .leadingIcon = "format_bold"},
        {.text = nullptr, .leadingIcon = "format_italic"},
        {.text = nullptr, .leadingIcon = "format_underlined"},
    };
    static int selectedIconMenuItem = -1;
    raym3::Menu({col2X, col2Y + 310, 396, 56}, iconOnlyItems, 10,
                &selectedIconMenuItem, true); // iconOnly = true

    col2Y += 390.0f;

    raym3::Card({col2X, col2Y, 350, 160}, raym3::CardVariant::Elevated);
    col2Y += 180.0f;

    static raym3::View3D view3D;
    static Camera3D camera = {{5.0f, 5.0f, 5.0f},
                              {0.0f, 0.0f, 0.0f},
                              {0.0f, 1.0f, 0.0f},
                              45.0f,
                              CAMERA_PERSPECTIVE};

    float time = (float)GetTime();
    camera.position.x = sinf(time) * 5.0f;
    camera.position.z = cosf(time) * 5.0f;

    view3D.Render({col2X, col2Y, 300, 200}, []() {
      BeginMode3D(camera);
      ClearBackground(RAYWHITE);
      DrawGrid(10, 1.0f);
      DrawCube({0, 0, 0}, 2.0f, 2.0f, 2.0f, RED);
      DrawCubeWires({0, 0, 0}, 2.0f, 2.0f, 2.0f, MAROON);
      EndMode3D();
    });
    // Text examples beside View3D
    float textExampleX = col2X + 320;
    float textExampleY = col2Y;
    raym3::Text("Roboto Regular", {textExampleX, textExampleY, 100, 24}, 16,
                scheme.onSurface, raym3::FontWeight::Regular);
    raym3::Text("Roboto Medium", {textExampleX, textExampleY + 25, 100, 24}, 16,
                scheme.onSurface, raym3::FontWeight::Medium);
    raym3::Text("Roboto Bold", {textExampleX, textExampleY + 50, 100, 24}, 16,
                scheme.onSurface, raym3::FontWeight::Bold);

    col2Y += 220.0f;

    raym3::CircularProgressIndicator({col2X, col2Y, 48, 48}, 0.0f, true,
                                     Color{0, 0, 0, 0});
    raym3::LinearProgressIndicator({col2X + 60, col2Y + 20, 200, 4}, 0.75f,
                                   false, Color{0, 0, 0, 0});
    raym3::LinearProgressIndicator({col2X + 60, col2Y + 40, 200, 4}, 0.0f, true,
                                   Color{0, 0, 0, 0});
    col2Y += 80.0f;

    raym3::Icon("home", {col2X, col2Y + 10, 24, 24},
                raym3::IconVariation::Filled, scheme.primary);
    raym3::Icon("settings", {col2X + 40, col2Y + 10, 24, 24},
                raym3::IconVariation::Outlined, scheme.primary);

    if (raym3::IconButton("favorite", {col2X + 80, col2Y, 48, 48},
                          raym3::ButtonVariant::Text,
                          raym3::IconVariation::Filled)) {
      printf("Heart clicked!\n");
    }
    raym3::IconButton("add", {col2X + 140, col2Y, 48, 48},
                      raym3::ButtonVariant::Filled,
                      raym3::IconVariation::Filled);
    raym3::IconButton("edit", {col2X + 200, col2Y, 48, 48},
                      raym3::ButtonVariant::Tonal,
                      raym3::IconVariation::Filled);
    raym3::IconButton("delete", {col2X + 260, col2Y, 48, 48},
                      raym3::ButtonVariant::Outlined,
                      raym3::IconVariation::Filled);
    col2Y += 60.0f;

    // --- Column 3 (x=960) ---
    float col3X = 960.0f;
    float col3Y = 20.0f;
    // Lists are rendered below

    // List Component Demo
    static raym3::ListItem diningChildren[] = {{.text = "Breakfast & brunch"},
                                               {.text = "New American"},
                                               {.text = "Sushi bars"},
                                               {.text = "Filipino food"}};

    static raym3::ListItem rootItems[] = {
        {.text = "Attractions", .leadingIcon = "movie"},
        {.text = "Dining",
         .leadingIcon = "restaurant",
         .children = diningChildren,
         .childCount = 4,
         .expanded = true},
        {.text = "Education", .leadingIcon = "school"},
        {.text = "Health", .leadingIcon = "favorite"},
        {.text = "Family", .leadingIcon = "group"},
        {.text = "Office",
         .leadingIcon = "content_cut"}, // Using content_cut as placeholder for
                                        // scissors/office
        {.text = "Promotions", .leadingIcon = "label"}};

    float listHeight = 0;
    // Render list on the right side of the split menu
    raym3::List({col3X, col3Y, 220, 600}, rootItems, 7, &listHeight);

    // Deep Nesting Example
    static raym3::ListItem level3[] = {{.text = "Level 3 Item 1"},
                                       {.text = "Level 3 Item 2"}};

    static raym3::ListItem level2[] = {{.text = "Level 2 Item 1",
                                        .children = level3,
                                        .childCount = 2,
                                        .expanded = true},
                                       {.text = "Level 2 Item 2",
                                        .backgroundColor = PURPLE,
                                        .textColor = WHITE}};

    static raym3::ListItem level1[] = {{.text = "Root Item",
                                        .children = level2,
                                        .childCount = 2,
                                        .expanded = true},
                                       {.text = "Custom Color Item",
                                        .textColor = RED,
                                        .iconColor = BLUE,
                                        .leadingIcon = "star"}};

    float deepListHeight = 0;
    raym3::List({col3X, col3Y + listHeight + 20, 220, 300}, level1, 2,
                &deepListHeight);

    if (showDialog) {
      if (raym3::Dialog("Dialog", "This is a Material Design 3 dialog!",
                        "Cancel;OK")) {
        showDialog = false;
      }
    }

    raym3::SnackbarComponent::Render(
        {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()});

    raym3::EndFrame();
    EndDrawing();
  }

  raym3::Shutdown();
  CloseWindow();
  return 0;
}
