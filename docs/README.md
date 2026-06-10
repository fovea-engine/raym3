# raym3 Documentation

This directory contains documentation for advanced raym3 features and systems.

## Documentation

- **[Layout System](layout-system.md)** - Flexbox-based layout with Yoga, scroll containers, and ClipScope usage
- **[Input Layer System](input-layer-system.md)** - Z-ordering and input blocking
- **[Scissor System](scissor-system.md)** - Stack-based clipping regions
- **[v2 Native UI Upgrade](v2-upgrade.md)** - Retained/composable v2 API, same-frame layout, dependency, icon, and text-engine foundation
- **[M3 Component Browser Audit](m3-component-browser-audit.md)** - Browser-reviewed Material 3 component and motion checklist

## Combining Layout, Input Layers, and Scissor

You can combine all three systems for complex UIs. Layout provides structure, scissor handles scroll clipping, and input layers manage overlay input. Use `PushLayer(100)` or higher for modals so they render above layout clipping:

```cpp
raym3::BeginFrame();

// Layout system for main UI
Rectangle screen = {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};
raym3::Layout::Begin(screen);

raym3::LayoutStyle mainStyle = raym3::Layout::Row();
raym3::Layout::BeginContainer(mainStyle);

// Sidebar with layout
raym3::LayoutStyle sidebarStyle = raym3::Layout::Column();
sidebarStyle.width = 200;
raym3::Layout::BeginContainer(sidebarStyle);

Rectangle btnBounds = raym3::Layout::Alloc(raym3::Layout::Fixed(-1, 40));
raym3::Button("Menu Item", btnBounds);

raym3::Layout::EndContainer();

// Content area
raym3::LayoutStyle contentStyle = raym3::Layout::Column();
contentStyle.flexGrow = 1;
raym3::Layout::BeginContainer(contentStyle);

// ... content components

raym3::Layout::EndContainer();
raym3::Layout::EndContainer();
raym3::Layout::End();

// Modal overlay - zOrder >= 100 bypasses scissor
raym3::PushLayer(100);
raym3::Card({200, 200, 400, 300}, raym3::CardVariant::Elevated);
// Modal content
raym3::PopLayer();

raym3::EndFrame();
```

This combination allows you to create responsive layouts with proper input handling and clipping for overlays and modals.
