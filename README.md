# raym3 - Material Design 3 UI Library for Raylib

raym3 is a Material Design 3 inspired immediate-mode GUI library built on raylib. It provides a subset of Material Design 3 components with automatic resource management, making it easy to create modern, beautiful user interfaces in C++ applications.

**Author:** Jordan Miller ([@nanofuxion](https://github.com/nanofuxion))

## Demo

![raym3 demo](media/demo.gif)

## Features

- **Material Design 3 Inspired** - Implements Material Design 3 design principles and components
- **Immediate-Mode API** - Simple, intuitive API similar to raygui
- **Automatic Resource Management** - Icons and fonts are automatically discovered or can be embedded
- **Optional Yoga Layout** - Flexbox layout support for advanced UI composition
- **Roboto Font Support** - Embedded Roboto fonts with custom font loading
- **Light and Dark Themes** - Full theme support with Material Design 3 color system
- **SVG Icon Support** - Material Design icons with multiple variations (filled, outlined, round, sharp, two-tone)
- **Zero External Dependencies** - Can be built as a standalone library (raylib is fetched automatically)

## Components

raym3 currently implements the following Material Design 3 components:

### Input Components
- **Button** - Text, Filled, Outlined, Tonal, Elevated variants
- **IconButton** - Buttons with Material Design icons
- **TextField** - Single-line text input with label, password mode, input masking, undo/redo support, and automatic cursor color inversion for custom backgrounds
- **Checkbox** - Standard checkbox with label
- **Switch** - Toggle switch
- **RadioButton** - Radio button with label
- **Slider** - Continuous value slider with optional label

### Display Components
- **Card** - Elevated surface container with multiple variants
- **Dialog** - Modal dialog with customizable buttons
- **Modal** - Full-screen modal component with backdrop and text input support
- **Menu** - Dropdown menu with leading/trailing icons, dividers, gaps, icon-only mode, and disabled items
- **List** - Material Design list component with expandable items, icons, and selection callbacks
- **SegmentedButton** - Segmented button groups
- **ProgressIndicator** - Circular and linear progress indicators
- **Divider** - Horizontal and vertical dividers
- **Text** - Typography component with multiple weights and alignments
- **Icon** - Material Design icons with multiple variations

### Layout Components
- **Container** - Flexbox-based layout container (requires Yoga)
- **View3D** - 3D viewport component

**Note:** This is a partial implementation of Material Design 3. Many components from the full specification (such as AppBar, BottomNavigation, NavigationDrawer, Tabs, Chips, DataTables, Snackbars, Tooltips, FloatingActionButton, BottomSheet, Date/Time Pickers, etc.) are not yet implemented.

## Quick Start

```cpp
#include "raym3/raym3.h"
#include <raylib.h>

int main() {
    InitWindow(800, 600, "raym3 Example");
    SetTargetFPS(60);
    
    raym3::Initialize();
    raym3::SetTheme(false); // Light mode
    
    char textBuffer[256] = "";
    bool checked = false;
    float sliderValue = 50.0f;
    int selectedMenuItem = 0;
    
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(raym3::Theme::GetColorScheme().surface);
        
        raym3::BeginFrame();
        
        if (raym3::Button("Click Me", {100, 100, 120, 40})) {
            // Button clicked
        }
        
        raym3::IconButton("home", {100, 150, 48, 48}, 
                         raym3::ButtonVariant::Filled,
                         raym3::IconVariation::Filled);
        
        // TextField with custom background and automatic cursor inversion
        raym3::TextFieldOptions options;
        options.backgroundColor = PURPLE;
        options.textColor = WHITE;
        raym3::TextField(textBuffer, sizeof(textBuffer), {100, 210, 200, 56}, "Label", options);
        
        raym3::Checkbox("Check me", {100, 280, 200, 24}, &checked);
        sliderValue = raym3::Slider({100, 320, 200, 40}, sliderValue, 0.0f, 100.0f, "Slider");
        
        // Menu with icons, dividers, and gaps
        static raym3::MenuItem menuItems[] = {
            {"Home", "home"},
            {"Settings", "settings"},
            {nullptr, nullptr, nullptr, nullptr, true}, // Divider
            {"Profile", "person"},
            {nullptr, nullptr, nullptr, nullptr, false, true}, // Gap
            {"Logout", "logout"}
        };
        raym3::Menu({100, 380, 200, 240}, menuItems, 6, &selectedMenuItem);
        
        raym3::EndFrame();
        EndDrawing();
    }
    
    raym3::Shutdown();
    CloseWindow();
    return 0;
}
```

### TextField Features

The TextField component supports several advanced features:

- **Custom Colors**: Set `backgroundColor` and `textColor` in `TextFieldOptions`
- **Automatic Cursor Inversion**: Cursor color automatically inverts based on background luminance
- **Password Mode**: Set `passwordMode = true` to mask input
- **Input Masking**: Use regex patterns via `inputMask` for validation
- **Undo/Redo**: Built-in support with configurable history depth via `maxUndoHistory`
- **Icons**: Add `leadingIcon` and `trailingIcon` with click callbacks
- **Read-Only**: Set `readOnly = true` to prevent editing

### Menu Component Features

The Menu component supports:

- **Leading Icons**: Display icons on the left side of menu items
- **Trailing Text/Icons**: Show additional text or icons on the right
- **Dividers**: Add visual separators with `isDivider = true`
- **Gaps**: Create spacing between items with `isGap = true`
- **Icon-Only Mode**: Set `iconOnly = true` for compact horizontal menus
- **Disabled Items**: Set `disabled = true` to prevent interaction

## Building

### Basic Usage (CMake)

```cmake
add_subdirectory(raym3)
target_link_libraries(your_target raym3)
```

### With Yoga Layout Support

```cmake
add_subdirectory(yoga)
set(RAYM3_USE_YOGA ON CACHE BOOL "" FORCE)
add_subdirectory(raym3)
target_link_libraries(your_target raym3)
```

### With Input Layers Support

To enable the layer-based input system with automatic input blocking and capture:

```cmake
set(RAYM3_USE_INPUT_LAYERS ON CACHE BOOL "" FORCE)
add_subdirectory(raym3)
target_link_libraries(your_target raym3)
```

**Input Layers Features:**
- **Automatic Input Blocking**: Higher Z-order layers automatically block input to lower layers
- **Input Capture**: Drag operations must start within component bounds (prevents accidental drags)
- **Explicit Z-Ordering**: Use `PushLayer(zOrder)` and `PopLayer()` to control rendering order
- **Component Blocking**: Cards and panels automatically block input to elements beneath them

**Usage Example:**
```cpp
raym3::BeginFrame();

// Layer 0 - Background
raym3::Button("Background", {100, 100, 120, 40});

// Layer 1 - Overlay panel
raym3::PushLayer(1);
raym3::Card({200, 150, 400, 300}, raym3::CardVariant::Elevated);
raym3::Button("Overlay Button", {220, 170, 120, 40});
raym3::PopLayer();

raym3::EndFrame();
```

**Input Capture for Viewports:**
```cpp
Rectangle viewportBounds = {100, 100, 400, 300};

// Register as blocking region
raym3::InputLayerManager::RegisterBlockingRegion(viewportBounds, true);

// Only process camera controls if drag started in viewport
bool canProcessInput = raym3::InputLayerManager::BeginInputCapture(
    viewportBounds, 
    true  // requireStartInBounds = true
);

if (canProcessInput) {
    // Process camera rotation/zoom
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        // Rotate camera
    }
}
```

### Embed Resources into Library

To embed SVG icons and fonts directly into the static library (no external files needed):

```cmake
set(RAYM3_EMBED_RESOURCES ON CACHE BOOL "" FORCE)
add_subdirectory(raym3)
target_link_libraries(your_target raym3)
```

**Icon Optimization:** By default, when `RAYM3_EMBED_RESOURCES=ON`, only icons that are actually used in your code are embedded. This significantly reduces the library size. If you need all icons embedded (e.g., for dynamic icon loading), set:

```cmake
set(RAYM3_EMBED_RESOURCES ON CACHE BOOL "" FORCE)
set(RAYM3_EMBED_ALL_ICONS ON CACHE BOOL "" FORCE)
add_subdirectory(raym3)
```

## Resource Management

raym3 automatically discovers resources in the following order:

1. **Embedded Resources** (if `RAYM3_EMBED_RESOURCES=ON`)
2. **CMake-defined resource directory** (set at build time)
3. **Relative paths** (`./resources/icons`, `./raym3/resources/icons`, etc.)

Icons are expected in the structure:
```
resources/
  icons/
    filled/
      icon_name.svg
    outlined/
      icon_name.svg
    round/
      icon_name.svg
    sharp/
      icon_name.svg
    two-tone/
      icon_name.svg
```

Fonts are expected in:
```
resources/
  fonts/
    Roboto/
      Roboto-Regular.ttf
      Roboto-Bold.ttf
      ...
```

## Dependencies

- **raylib** (required) - Automatically fetched via CMake FetchContent
- **yoga** (optional) - Flexbox layout engine, enabled via `RAYM3_USE_YOGA`

## Credits and Acknowledgments

raym3 is built using the following excellent open-source projects:

### Core Libraries

- **[raylib](https://github.com/raysan5/raylib)** - A simple and easy-to-use library to enjoy videogames programming
  - License: zlib/libpng
  - Copyright (c) 2013-2024 Ramon Santamaria (@raysan5)

- **[NanoSVG](https://github.com/memononen/nanosvg)** - Simple stupid SVG parser
  - License: zlib/libpng
  - Copyright (c) 2013-14 Mikko Mononen memon@inside.org
  - Used for parsing and rasterizing SVG icons

- **[Yoga](https://github.com/facebook/yoga)** - A cross-platform layout engine
  - License: MIT
  - Copyright (c) Meta Platforms, Inc. and affiliates
  - Optional dependency for flexbox layout support

### Resources

- **[Material Design Icons](https://github.com/google/material-design-icons)** - Material Design icon set
  - License: Apache License 2.0
  - Copyright (c) Google LLC
  - Over 10,000 SVG icons included in multiple variations (filled, outlined, round, sharp, two-tone)

- **[Roboto Font Family](https://github.com/google/roboto)** - Material Design typeface
  - License: Apache License 2.0
  - Copyright (c) Google LLC
  - Embedded font files for consistent typography

### Design System

- **[Material Design 3](https://m3.material.io/)** - Google's Material Design system
  - Design guidelines and specifications
  - Color system, typography, and component specifications

## License

raym3 is licensed under the **zlib/libpng license**, the same as raylib.

Copyright (c) 2025 Jordan Miller ([@nanofuxion](https://github.com/nanofuxion))

See [LICENSE](LICENSE) file for full license text.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

We're particularly interested in contributions that add more Material Design 3 components to expand the library's functionality. Some high-priority components that would be valuable additions include:
- AppBar/TopAppBar
- BottomNavigation
- NavigationDrawer
- Tabs
- Chips
- DataTables
- Snackbars
- Tooltips
- FloatingActionButton (FAB)
- BottomSheet
- Date/Time Pickers

## Support

If you find raym3 useful and would like to support its development, you can:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P41DFXP6)

## Project Status

raym3 is an independent, self-contained project. All resources (icons and fonts) are included in the repository, making it easy to use in any project without external dependencies.

**Current Status:** Partial implementation of Material Design 3 components. The library is functional and ready to use, but many components from the full Material Design 3 specification are not yet implemented.

## Known Issues / TODO

- **List Component Input Blocking**: The List component currently still reacts to clicks even when there are higher layers above it. This should be fixed to verify blocking.

## Debugging

raym3 provides built-in tools for debugging layouts and understanding component bounds:

### Layout Debug Visualization

The layout system includes a powerful debug visualization tool that helps you understand how your UI is being laid out:

```cpp
// Enable debug visualization
raym3::Layout::SetDebug(true);

// In your render loop, after Layout::End():
raym3::Layout::DrawDebug();
```

**Features:**
- **Color-Coded Boxes**: Each layout node gets a unique color generated from its index, making it easy to distinguish between different components
- **Hover Highlighting**: When you hover over a component, it darkens and becomes more opaque with a darker outline for clear identification
- **Low-Opacity Overlays**: Non-hovered components use very low opacity (5%) so they don't obscure your actual UI
- **Visual Hierarchy**: Helps you understand container nesting, padding, gaps, and flexbox behavior

**Usage Tips:**
- Enable debug mode when your layout isn't behaving as expected
- Hover over components to see their exact bounds
- Use it to verify padding, gaps, and alignment are working correctly
- Toggle it on/off at runtime to compare the visual layout with the debug overlay

## Interaction Model

- **Click on Release**: Components (Buttons, etc.) trigger their primary action on **mouse release** while hovering, rather than on press. This matches standard UI behavior and allows users to cancel a click by moving the mouse away before releasing.
- **Input Capture**: Drag operations (sliders, scrollbars) capture input, allowing the user to drag outside the component bounds once the gesture has started.

---

*Note: This README was generated with the assistance of AI and may contain errors. Please verify all information and report any issues.*
