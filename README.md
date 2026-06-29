# raym3 - Material Design 3 UI Library for Raylib

raym3 is a Material Design 3 inspired immediate-mode GUI library built on raylib. It provides a subset of Material Design 3 components with automatic resource management, making it easy to create modern, beautiful user interfaces in C++ applications.

**Author:** Jordan Miller ([@nanofuxion](https://github.com/nanofuxion))

## Demo

![raym3 demo](media/demo.gif)

## Features

- **Material Design 3 Inspired** - Implements Material Design 3 design principles and components
- **v2 Composable API** - Retained `raym3::v2` View tree with style objects, same-frame layout, a Material component catalog, text input editing, CSS transitions, a mutation-batch protocol, and Flutter-style CSS density scaling
- **Immediate-Mode API** - Simple, intuitive API similar to raygui
- **Automatic Resource Management** - Icons and fonts are automatically discovered or can be embedded
- **Optional Yoga Layout** - Flexbox layout support for advanced UI composition
- **Roboto Font Support** - Roboto v3 (from roboto-3-classic); can be loaded from disk or embedded for portable deployment
- **Light and Dark Themes** - Full theme support with Material Design 3 color system
- **SVG Icon Support** - Material Design icons with multiple variations (filled, outlined, round, sharp, two-tone)
- **Zero External Dependencies** - Can be built as a standalone library (raylib is fetched automatically)
- **Scissor Stack API** - Stack-based clipping with `PushScissor`/`PopScissor`, debug visualization

## Components

raym3 currently implements the following Material Design 3 components:

### Input Components
- **Button** - Text, Filled, Outlined, Tonal, Elevated variants with keyboard activation and optional tooltips
- **IconButton** - Buttons with Material Design icons, keyboard activation, and optional tooltips
- **TextField** - Single-line text input with word/line navigation, drag selection, triple-click select all, Escape to revert, and I-beam cursor
- **SearchBar** - M3 contained search input with leading/trailing icon slots
- **Checkbox** - Standard checkbox with label, keyboard activation (Space/Enter), pointer cursor, and optional tooltips
- **Switch** - Toggle switch with keyboard activation (Space/Enter), pointer cursor, and optional tooltips
- **RadioButton** - Radio button with label, keyboard activation (Space/Enter), pointer cursor, and optional tooltips
- **Slider** - Value slider with keyboard navigation (Arrow/Page/Home/End), mouse wheel, focus ring, pointer/resize cursors, and optional tooltips
- **RangeSlider** - Multi-thumb range slider with Tab key thumb cycling, keyboard navigation, focus ring, and optional tooltips
- **Chip** - Assist, filter, input, and suggestion chips with M3 selection and state layers
- **FloatingActionButton** - FAB and extended FAB with regular/medium/large M3 Expressive sizes and color roles

### Display Components
- **AppBar** - Small, center-aligned, medium, and large app bars with navigation/actions
- **Badge** - Small status dots and large labeled badges for icons/navigation
- **Card** - Elevated surface container with multiple variants
- **Dialog** - Modal dialog with customizable buttons
- **Modal** - Full-screen modal component with backdrop and text input support
- **Menu** - Dropdown menu with leading/trailing icons, dividers, gaps, icon-only mode, disabled items, pointer cursor, and per-item tooltips
- **List** - Material Design list component with keyboard navigation (Arrow/Page/Home/End), Shift multi-select, Ctrl+A select all, typeahead search, drag reorder, pointer cursor, and per-item tooltips
- **NavigationBar / NavigationRail** - M3 destination navigation with active indicators, labels, and badges
- **SegmentedButton** - Segmented button groups
- **ProgressIndicator** - Circular and linear progress indicators
- **Divider** - Horizontal and vertical dividers
- **Text** - Typography component with multiple weights and alignments
- **Icon** - Material Design icons with multiple variations

### Layout Components
- **Container** - Flexbox-based layout container (requires Yoga)
- **View3D** - 3D viewport component

### Feedback Components
- **Tooltip** - Hover tooltips with smart timing, configurable placement, and developer-defined text
- **Snackbar** - Temporary notification messages
- **TabBar** - Browser-style tab bar with closeable tabs, drag reorder, icons, and tooltips

**Note:** This is a partial implementation of Material Design 3. Some components from the full specification (such as NavigationDrawer, DataTables, BottomSheet, SideSheet, Carousel, Date/Time Pickers, etc.) are not yet implemented as first-class immediate-mode components.

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
    std::vector<float> rangeValues = {20.0f, 80.0f};
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
        
        // Slider with discrete mode and tick marks
        raym3::SliderOptions sliderOpts;
        sliderOpts.stepValue = 10.0f;
        sliderOpts.showTickMarks = true;
        sliderOpts.showStopIndicators = true;
        sliderValue = raym3::Slider({100, 320, 200, 40}, sliderValue, 0.0f, 100.0f, "Slider", sliderOpts);
        
        // Range slider for selecting a range
        raym3::RangeSliderOptions rangeOpts;
        rangeOpts.showValueIndicators = true;
        rangeValues = raym3::RangeSlider({100, 380, 200, 40}, rangeValues, 0.0f, 100.0f, "Range", rangeOpts);
        
        // Menu with icons, dividers, and gaps
        static raym3::MenuItem menuItems[] = {
            {"Home", "home"},
            {"Settings", "settings"},
            {nullptr, nullptr, nullptr, nullptr, true}, // Divider
            {"Profile", "person"},
            {nullptr, nullptr, nullptr, nullptr, false, true}, // Gap
            {"Logout", "logout"}
        };
        raym3::Menu({100, 440, 200, 240}, menuItems, 6, &selectedMenuItem);
        
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

### Slider Component Features

The Slider component supports M3 Expressive customization:

- **Discrete Mode**: Set `stepValue > 0` to snap to specific increments
- **Tick Marks**: Enable `showTickMarks` to display step indicators in discrete mode
- **Stop Indicators**: Enable `showStopIndicators` to show dots at min/max positions
- **Inset Icons**: Add icons *inside* the track (`startIcon`, `endIcon`)
- **Value Indicator**: Show a value bubble above the thumb on drag (`showValueIndicator`)
- **Start/End Text**: Add text labels
- **Track Colors**: Customize active/inactive track colors
- **Handle Color**: Customize the handle thumb color
- **End Dot**: Customize or hide the end-of-track dot

### RangeSlider Component Features

The RangeSlider component provides multi-thumb range selection:

- **Multiple Thumbs**: Support for any number of thumbs via `std::vector<float>`
- **Range Fill**: Active track fill between first and last thumb
- **Thumb Constraints**: Thumbs cannot cross each other
- **Minimum Distance**: Set `minDistance` to enforce spacing between thumbs
- **Discrete Mode**: Supports `stepValue` and `showTickMarks` like Slider
- **Value Indicators**: Show value bubbles for all thumbs when dragging
- **Stop Indicators**: Display min/max position markers

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

### Font Configuration (Project-Agnostic)

raym3 uses Roboto v3.015 from [roboto-3-classic](https://github.com/googlefonts/roboto-3-classic). The setup works for any project using raym3 (submodule or standalone) on macOS, Windows, Linux, Android, and Web/WASM:

- **Default**: Fonts are downloaded at configure time to the build directory and loaded from disk at runtime.
- **Embedded** (recommended for packaged apps): Set `RAYM3_EMBED_FONTS=ON` to embed Roboto into the binary—no font files needed at runtime.
- **Offline/Override**: Set `ROBOTO_FONTS_DIR` to a path with `Roboto-Regular.ttf` and `Roboto-Bold.ttf` to use pre-downloaded fonts.

```cmake
set(RAYM3_EMBED_FONTS ON CACHE BOOL "" FORCE)  # For portable deployment
add_subdirectory(raym3)
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
- **Overlay Layer Threshold**: Layers with `zOrder >= 100` bypass layout scissor clipping (for menus, tooltips, dialogs)

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

### Emscripten / WebAssembly

raym3 supports building for Emscripten. The CMake configuration automatically sets `GRAPHICS_API_OPENGL_ES3` for WebAssembly targets.

### Embed Resources into Library

To embed SVG icons and fonts directly into the static library (no external files needed):

```cmake
set(RAYM3_EMBED_RESOURCES ON CACHE BOOL "" FORCE)
add_subdirectory(raym3)
target_link_libraries(your_target raym3)
```

**Icon Optimization:** By default, when `RAYM3_EMBED_RESOURCES=ON`, raym3 embeds the lightweight manifest in `resources/icons/core-icons.txt`, or a custom newline-delimited manifest provided with `RAYM3_ICON_MANIFEST`. This avoids embedding the full 10,000+ SVG catalog. If you need all icons embedded (e.g., for dynamic icon loading), set:

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

## Icon Manifests & Autocomplete

raym3 embeds and installs a lightweight icon subset by default. To generate an app-specific manifest from source usage:

```bash
cd raym3 && python3 tools/generate_icon_manifest.py --root ../your-app --output resources/icons/app-icons.txt
```

Then configure with:

```cmake
set(RAYM3_ICON_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/raym3/resources/icons/app-icons.txt" CACHE FILEPATH "" FORCE)
```

Use `RAYM3_EMBED_ALL_ICONS=ON` or `RAYM3_INSTALL_ALL_ICONS=ON` only when dynamic icon names require the full catalog.

## Dependencies

- **raylib** (required) - Automatically fetched via CMake FetchContent, default tag `6.0`
- **yoga** (optional) - Flexbox layout engine, enabled via `RAYM3_USE_YOGA`, default tag `v3.2.1`

See `docs/v2-upgrade.md` for the new v2 API, same-frame layout, dependency, and icon manifest details.

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

- **[Roboto Font Family](https://github.com/googlefonts/roboto-3-classic)** (v3.015) - Material Design typeface
  - License: Apache License 2.0
  - Copyright (c) Google LLC
  - Downloaded at configure time or embedded via `RAYM3_EMBED_FONTS`

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
- NavigationDrawer
- DataTables
- BottomSheet
- SideSheet
- Carousel
- Date/Time Pickers

## Support

If you find raym3 useful and would like to support its development, you can:

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/P5P41DFXP6)

## Project Status

raym3 is an independent, self-contained project. Icons come from the material-design-icons submodule. Fonts (Roboto) are auto-downloaded at configure time or can be embedded—no manual setup required for any platform.

**Current Status:** Partial implementation of Material Design 3 components. The library is functional and ready to use, but many components from the full Material Design 3 specification are not yet implemented.

## Changelog

### v1.5.0 - Scissor API & Emscripten Support
- **Scissor Stack API**: Public `PushScissor`, `PopScissor`, `BeginScissor`, `GetCurrentScissorBounds` for custom clipping regions. Scissors intersect and stack correctly.
- **Scissor Debug**: `SetScissorDebug`, `IsScissorDebug`, `DrawScissorDebug` for visualizing active scissor regions.
- **Overlay Layer Threshold**: Input layers with `zOrder >= 100` bypass layout scissor (menus, tooltips, dialogs render above clipped content).
- **Emscripten**: Automatic `GRAPHICS_API_OPENGL_ES3` for WebAssembly builds.
- **Layout**: Scroll containers and `GetActiveScissorBounds` now use the shared scissor stack.
- **Slider, TextField, RangeSlider**: Use `PushClipRect`/`PopClipRect` to restore parent clip. For custom content in scroll containers, call `PushScissor(Layout::GetActiveScissorBounds())` and `PopScissor()`.

### v1.4.0 - Stable Layout IDs
- **Layout Flicker Fix**: Eliminated UI flicker when layout structure changes (adding/removing elements, switching tabs) by implementing stable hash-based node IDs instead of sequential numbering.
- **Stable Node IDs**: Layout node IDs now use FNV-1a hashing based on tree hierarchy rather than sequential order. IDs remain stable when siblings are added/removed.
- **New Layout API**: Added `PushId(const char*)`, `PushId(int)`, and `PopId()` for advanced manual ID control in complex layouts.
- **Backward Compatible**: All existing code continues to work. `SetIdOffset` maintains compatibility while integrating with the hash-based system.

### v1.3.0 - UX Overhaul & Tooltip System
- **Buffered Cursor System**: Added `RequestCursor()` API - cursor is set once per frame in `EndFrame()` to prevent flickering when multiple components render. Components call `RequestCursor()` instead of `SetMouseCursor()` directly.
- **Tooltip Smart Timing**: Tooltips show instantly (50ms) after the first tooltip in a session. Returns to normal delay after 2 seconds of inactivity.
- **Tooltip Support**: Added optional `tooltip` and `tooltipPlacement` fields to `ButtonOptions`, `SliderOptions`, `RangeSliderOptions`, `IconButtonOptions`, `CheckboxOptions`, `SwitchOptions`, `RadioButtonOptions`, `ListItem`, `MenuItem`, and `TabItem`.
- **TextField Enhancements**: Reduced horizontal padding, fade gradient for overflow text, triple-click select all, Escape to revert, Cmd+Backspace/Alt+Backspace word/line deletion, auto-scroll during drag selection, I-beam cursor on hover.
- **Slider & RangeSlider**: Added focus management, keyboard navigation (Arrow keys with Shift 10x, Page Up/Down 20%, Home/End min/max), mouse wheel support, focus ring, pointer/resize cursors.
- **RangeSlider**: Tab key cycles between thumbs when focused.
- **List Component**: Added keyboard navigation (Up/Down/Home/End/Page Up/Page Down), Shift+Arrow for multi-select range, Ctrl+A select all, Escape to deselect, Enter to activate, Space to toggle, typeahead search with 500ms timeout.
- **Menu Component**: Added pointer cursor on hover for icon-only and standard menus.
- **TabBar**: Added tooltip support for individual tabs.
- **Button & IconButton**: Added keyboard activation (Space/Enter) when focused, pointer cursor on hover.
- **Checkbox, RadioButton, Switch**: Added keyboard activation (Space/Enter), pointer cursor, focus management with click-away blur.
- **Focus Management**: All focusable components lose focus when clicking outside their bounds.

### v1.2.0 - Tooltip and TabBar
- **Tooltip Component**: New `Tooltip()` function and `TooltipManager` for deferred tooltip rendering
- **TabBar Component**: Browser-style tab bar with closeable tabs, drag reorder, loading/audio indicators, add tab callback, and text truncation

### v1.1.0
- **Removed Native Text Input**: The `useNativeInput` option in `TextFieldOptions` has been removed along with the `RAYM3_ENABLE_NATIVE_TEXT_INPUT` CMake option. The TextField component now provides full native-like text editing behavior (keyboard shortcuts, word/line navigation, selection, undo/redo) without requiring platform-specific backends. This simplifies cross-platform deployment and removes the Cocoa framework dependency on macOS.

## Scissor API

raym3 provides a stack-based scissor (clipping) API for custom clipping regions:

- **`PushScissor(Rectangle bounds)`** - Push a clipping region; intersects with current scissor
- **`PopScissor()`** - Pop the current scissor and restore the previous one
- **`BeginScissor(Rectangle bounds)`** - Convenience for `PushScissor(bounds)`
- **`GetCurrentScissorBounds()`** - Get the current active scissor rectangle
- **`SetScissorDebug(bool enabled)`** / **`IsScissorDebug()`** - Enable scissor debug overlay
- **`DrawScissorDebug()`** - Draw green outlines of active scissor regions (call after your frame content)

The layout system and scroll containers use this API internally. Use layers with `zOrder >= 100` for overlays (menus, tooltips, dialogs) so they render above layout clipping.

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

### Multi-Tab / MDI Layout State Isolation

When building tabbed interfaces or MDI (Multiple Document Interface) applications, each tab needs its own isolated layout state. Use these methods to prevent layout bleeding between tabs:

```cpp
// Set a unique ID offset for each tab (e.g., tabIndex * 1000)
raym3::Layout::SetIdOffset(activeTabIndex * 1000);

// When switching tabs, invalidate previous frame bounds to force fresh layout
raym3::Layout::InvalidatePreviousFrame();
```

**When to use:**
- Call `SetIdOffset()` before `Layout::Begin()` when rendering each tab's content
- Call `InvalidatePreviousFrame()` when switching between tabs to reset layout state

## Interaction Model

- **Click on Release**: Components (Buttons, etc.) trigger their primary action on **mouse release** while hovering, rather than on press. This matches standard UI behavior and allows users to cancel a click by moving the mouse away before releasing.
- **Input Capture**: Drag operations (sliders, scrollbars) capture input, allowing the user to drag outside the component bounds once the gesture has started.
- **Buffered Cursor**: Cursor changes are buffered via `RequestCursor()` during the frame and applied once in `EndFrame()`. This prevents flickering when multiple components render.
- **Focus Management**: Interactive components track focus state. Clicking a component gives it focus; clicking away removes focus. Focused components respond to keyboard input (Space/Enter for activation, Arrow keys for navigation).
- **Tooltip Smart Timing**: After the first tooltip is shown, subsequent tooltips appear instantly (50ms) for 2 seconds, then revert to the normal delay. This matches native OS tooltip behavior.

---

*Note: This README was generated with the assistance of AI and may contain errors. Please verify all information and report any issues.*
