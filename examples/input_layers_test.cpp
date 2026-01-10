#include "raym3/raym3.h"
#include <raylib.h>
#include <string>
#include <vector>

int main() {
  InitWindow(900, 650, "raym3 Input Layers Test");
  SetTargetFPS(60);
  
  raym3::Initialize();
  raym3::SetTheme(false); // Light mode
  
  char textBuffer[256] = "Click me!";
  bool checked = false;
  float sliderValue = 50.0f;
  int clickCount = 0;
  
  // Tab bar state
  std::vector<raym3::TabItem> tabs = {
    {"dashboard", "Dashboard", "dashboard"},
    {"settings", "Settings", "settings"},
    {"profile", "Profile", "person"},
    {"help", "Help", "help"}
  };
  int selectedTab = 0;
  
  while (!WindowShouldClose()) {
    BeginDrawing();
    raym3::ColorScheme& scheme = raym3::Theme::GetColorScheme();
    ClearBackground(scheme.surfaceContainerHighest);
    
    raym3::BeginFrame();
    
    // TabBar at top
    Rectangle tabBarBounds = {0, 0, (float)GetScreenWidth(), 34};
    raym3::TabBarOptions tabOpts;
    tabOpts.activeTabColor = scheme.surface;
    tabOpts.inactiveTabColor = scheme.surfaceContainerHighest;
    tabOpts.maxTabWidth = 180.0f;  // Limit tab width so they don't stretch
    
    int closedTab = -1;
    int clickedTab = raym3::TabBar(tabBarBounds, tabs, selectedTab, tabOpts, &closedTab);
    if (clickedTab >= 0) selectedTab = clickedTab;
    if (closedTab >= 0 && tabs.size() > 1) {
      tabs.erase(tabs.begin() + closedTab);
      if (selectedTab >= (int)tabs.size()) selectedTab = (int)tabs.size() - 1;
    }
    
    // Tab content area
    Rectangle contentBounds = {0, 34, (float)GetScreenWidth(), (float)GetScreenHeight() - 34};
    raym3::TabContentBegin(contentBounds, scheme.surface);
    
    // Display different content based on selected tab
    if (selectedTab < (int)tabs.size()) {
      std::string tabId = tabs[selectedTab].id;
      
      if (tabId == "dashboard") {
        // Dashboard content - show the original demo
        Rectangle bgBtnBounds = {100, 80, 200, 40};
        if (raym3::Button("Background Button (Layer 0)", bgBtnBounds)) {
          clickCount++;
        }
        raym3::Tooltip(bgBtnBounds, "This button is on the base layer");
        
        raym3::PushLayer(1);
        raym3::Card({200, 130, 400, 350}, raym3::CardVariant::Elevated);
        
        Rectangle overlayBtnBounds = {220, 150, 120, 40};
        if (raym3::Button("Overlay Button", overlayBtnBounds)) {
          clickCount++;
        }
        raym3::Tooltip(overlayBtnBounds, "Button on elevated layer");
        
        Rectangle textFieldBounds = {220, 210, 200, 56};
        raym3::TextField(textBuffer, sizeof(textBuffer), textFieldBounds, "Name");
        raym3::Tooltip(textFieldBounds, "Enter your name here");
        
        Rectangle checkboxBounds = {220, 280, 200, 24};
        raym3::Checkbox("Check me", checkboxBounds, &checked);
        raym3::Tooltip(checkboxBounds, "Toggle this option");
        
        Rectangle sliderBounds = {220, 320, 200, 40};
        sliderValue = raym3::Slider(sliderBounds, sliderValue, 0.0f, 100.0f, "Slider");
        raym3::TooltipOptions sliderOpts;
        sliderOpts.title = "Volume Control";
        sliderOpts.actionText = "Reset";
        sliderOpts.onAction = [&sliderValue]{ sliderValue = 50.0f; };
        raym3::Tooltip(sliderBounds, "Adjust the volume level", sliderOpts);
        
        Rectangle iconBtn1 = {220, 380, 40, 40};
        Rectangle iconBtn2 = {270, 380, 40, 40};
        Rectangle iconBtn3 = {320, 380, 40, 40};
        raym3::IconButton("settings", iconBtn1);
        raym3::Tooltip(iconBtn1, "Settings");
        raym3::IconButton("delete", iconBtn2);
        raym3::TooltipOptions deleteOpts;
        deleteOpts.title = "Delete Item";
        raym3::Tooltip(iconBtn2, "Remove this item permanently", deleteOpts);
        raym3::IconButton("help", iconBtn3);
        raym3::Tooltip(iconBtn3, "Help");
        
        raym3::PopLayer();
        
        raym3::Text(("Clicks: " + std::to_string(clickCount)).c_str(), 
                    {100, 510, 200, 30}, 16, scheme.onSurface);
        raym3::Text("Hover over components to see tooltips", 
                    {100, 535, 400, 20}, 12, scheme.onSurfaceVariant);
                    
      } else if (tabId == "settings") {
        raym3::Text("Settings Panel", {50, 50, 300, 40}, 24, scheme.onSurface, raym3::FontWeight::Bold);
        raym3::Text("Configure your application preferences here.", 
                    {50, 100, 400, 24}, 14, scheme.onSurfaceVariant);
                    
      } else if (tabId == "profile") {
        raym3::Text("User Profile", {50, 50, 300, 40}, 24, scheme.onSurface, raym3::FontWeight::Bold);
        raym3::Text("View and edit your profile information.", 
                    {50, 100, 400, 24}, 14, scheme.onSurfaceVariant);
                    
      } else if (tabId == "help") {
        raym3::Text("Help Center", {50, 50, 300, 40}, 24, scheme.onSurface, raym3::FontWeight::Bold);
        raym3::Text("Get help and documentation.", 
                    {50, 100, 400, 24}, 14, scheme.onSurfaceVariant);
      }
    }
    
    raym3::TabContentEnd();
    
    raym3::EndFrame();
    EndDrawing();
  }
  
  raym3::Shutdown();
  CloseWindow();
  return 0;
}
