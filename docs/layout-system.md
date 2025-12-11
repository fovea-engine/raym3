# Layout System

The Layout System provides flexbox-based automatic layout using Yoga. This allows you to create responsive UIs without manually calculating positions and sizes.

## Overview

The Layout System uses an immediate-mode approach where you declare your layout structure, and raym3 calculates the positions and sizes automatically. Layout calculations happen in two phases:

1. **Declaration Phase**: You declare your layout structure using `BeginContainer()`, `Alloc()`, etc.
2. **Calculation Phase**: After `Layout::End()`, all bounds are calculated and available for the next frame.

## Basic Usage

```cpp
#include "raym3/layout/Layout.h"

// Start a layout frame
Rectangle screen = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
raym3::Layout::Begin(screen);

// Create a row container
raym3::LayoutStyle rowStyle = raym3::Layout::Row();
rowStyle.padding = 20;
rowStyle.gap = 10;
raym3::Layout::BeginContainer(rowStyle);

// Allocate space for a button (width: 150, height: 40)
Rectangle buttonBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(150, 40));
raym3::Button("Click Me", buttonBounds);

// End the container
raym3::Layout::EndContainer();

// Finalize layout
raym3::Layout::End();
```

## Layout Style

`LayoutStyle` controls how containers and components are sized and positioned:

```cpp
struct LayoutStyle {
    float width = -1;        // Fixed width, or -1 for auto
    float height = -1;       // Fixed height, or -1 for auto
    float flexGrow = 0;      // How much space to take (0 = don't grow)
    float flexShrink = 1;    // How much to shrink (1 = can shrink)
    int direction = 1;       // 0 = Row, 1 = Column
    float gap = 0;           // Space between children
    float padding = 0;       // Padding on all sides
    float paddingTop = 0;    // Individual padding values
    float paddingRight = 0;
    float paddingBottom = 0;
    float paddingLeft = 0;
    int justifyContent = 0;  // Alignment along main axis
    int alignItems = 0;      // Alignment along cross axis
    int flexWrap = 0;        // 0 = NoWrap, 1 = Wrap, 2 = WrapReverse
};
```

## Helper Functions

raym3 provides convenient helpers for common layout patterns:

- **`Layout::Row()`** - Creates a horizontal container style
- **`Layout::Column()`** - Creates a vertical container style  
- **`Layout::Flex(float grow = 1.0f)`** - Creates a flexible size that grows
- **`Layout::Fixed(float width, float height)`** - Creates a fixed-size element

## Container Types

### Regular Container

```cpp
raym3::LayoutStyle style = raym3::Layout::Column();
style.gap = 10;
style.padding = 20;
raym3::Layout::BeginContainer(style);

// Add components here
Rectangle bounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
raym3::Button("Button", bounds);

raym3::Layout::EndContainer();
```

### Scroll Container

For scrollable content areas:

```cpp
raym3::LayoutStyle scrollStyle = raym3::Layout::Column();
scrollStyle.gap = 10;
// Begin scroll container (scrollX = false, scrollY = true)
raym3::Layout::BeginScrollContainer(scrollStyle, false, true);

// Add many items that will scroll
for (int i = 0; i < 20; i++) {
    Rectangle itemBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 50));
    raym3::Button(("Item " + std::to_string(i)).c_str(), itemBounds);
}

// End scroll container (automatically handles scissor mode)
EndScissorMode();
raym3::Layout::EndContainer();
```

## Common Patterns

### Sidebar + Content Layout

```cpp
raym3::Layout::Begin(screen);

// Main row container
raym3::LayoutStyle mainStyle = raym3::Layout::Row();
mainStyle.padding = 20;
mainStyle.gap = 20;
raym3::Layout::BeginContainer(mainStyle);

// Sidebar (fixed width)
raym3::LayoutStyle sidebarStyle = raym3::Layout::Column();
sidebarStyle.width = 200;
sidebarStyle.gap = 10;
raym3::Layout::BeginScrollContainer(sidebarStyle, false, true);

// Sidebar items
for (int i = 0; i < 10; i++) {
    Rectangle btnBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
    raym3::Button(("Menu " + std::to_string(i)).c_str(), btnBounds);
}

EndScissorMode();
raym3::Layout::EndContainer();

// Content area (flexible, takes remaining space)
raym3::LayoutStyle contentStyle = raym3::Layout::Column();
contentStyle.flexGrow = 1;  // Takes all remaining space
contentStyle.gap = 20;
raym3::Layout::BeginContainer(contentStyle);

// Content components here
Rectangle textBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
raym3::Text("Content", textBounds, 32, scheme.onSurface);

raym3::Layout::EndContainer();
raym3::Layout::EndContainer();
raym3::Layout::End();
```

### Cards in a Row

```cpp
raym3::LayoutStyle cardsStyle = raym3::Layout::Row();
cardsStyle.gap = 20;
cardsStyle.height = 150;
raym3::Layout::BeginScrollContainer(cardsStyle, true, false);  // Horizontal scroll

for (int i = 0; i < 10; i++) {
    Rectangle cardBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(200, -1));
    raym3::LayoutCard::BeginCard(cardBounds, raym3::CardVariant::Elevated);
    // Card content
    raym3::LayoutCard::EndCard();
}

EndScissorMode();
raym3::Layout::EndContainer();
```

## Debugging Layouts

If your layout isn't behaving as expected, you can enable debug mode to visualize the computed bounds of every container and item.

### Enabling Debug Mode

```cpp
// 1. Enable debug mode (can be toggled at runtime)
raym3::Layout::SetDebug(true);

// 2. Draw debug visualization at the end of your frame (after Layout::End())
raym3::Layout::DrawDebug();
```

### Debug Visualization Features

The debug overlay provides several helpful features:

- **Unique Colors**: Each layout node gets a distinct color generated from its index using a hue-based algorithm. This makes it easy to distinguish between different components and containers.

- **Hover Highlighting**: When you hover your mouse over a component:
  - The component darkens (lower HSV value)
  - Opacity increases to 15% (from 5%)
  - Outline becomes fully opaque and darker for clear identification
  
- **Low-Opacity Overlays**: Non-hovered components use very low opacity (5%) so they don't obscure your actual UI while still showing the layout structure.

- **Visual Hierarchy**: The overlay helps you understand:
  - Container nesting and parent-child relationships
  - Padding and gap spacing
  - Flexbox grow/shrink behavior
  - Alignment and justification

### Common Debugging Scenarios

**Problem: Component not appearing**
- Enable debug mode to see if the component has zero width/height
- Check if the component is being allocated space but rendering outside the visible area

**Problem: Unexpected spacing**
- Use hover to identify which container is adding the extra space
- Verify padding and gap values in your LayoutStyle

**Problem: Flexbox not working as expected**
- Check if parent container has enough space to distribute
- Verify flexGrow/flexShrink values
- Ensure you're using the correct direction (Row vs Column)

### Example: Debugging a Complex Layout

```cpp
raym3::BeginFrame();

// Enable debug mode
raym3::Layout::SetDebug(true);

Rectangle screen = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
raym3::Layout::Begin(screen);

// Your layout code here...
raym3::LayoutStyle mainStyle = raym3::Layout::Row();
raym3::Layout::BeginContainer(mainStyle);
// ... components ...
raym3::Layout::EndContainer();

raym3::Layout::End();

// Draw debug overlay AFTER Layout::End()
raym3::Layout::DrawDebug();

raym3::EndFrame();
```

**Pro Tip**: You can toggle debug mode at runtime based on a key press:

```cpp
if (IsKeyPressed(KEY_F3)) {
    static bool debugEnabled = false;
    debugEnabled = !debugEnabled;
    raym3::Layout::SetDebug(debugEnabled);
}
```

## Important Notes

1. **Frame-Based Calculation**: Layout bounds are calculated from the *previous* frame. On the first frame, bounds may be `{0, 0, 0, 0}` until the layout is calculated.

2. **Deterministic Order**: The layout system relies on components being declared in the same order each frame. Changing the order will cause incorrect layouts.

3. **Scissor Mode**: Scroll containers automatically enable scissor mode. You must call `EndScissorMode()` after ending a scroll container if you've manually enabled scissor mode.

4. **Yoga Required**: The Layout System requires Yoga to be enabled. Set `RAYM3_USE_YOGA=ON` in CMake.

See `examples/layout_test.cpp` for a complete working example.

