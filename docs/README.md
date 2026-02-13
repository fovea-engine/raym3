# raym3 Documentation

This directory contains documentation for advanced raym3 features and systems.

## Documentation

- **[Layout System](layout-system.md)** - Flexbox-based layout with Yoga
- **[Input Layer System](input-layer-system.md)** - Z-ordering and input blocking
- **[ClipScope System](clip-scope.md)** - Stack-based clipping, scissor, and SuspendClipScissor

## Combining Both Systems

You can combine both systems for complex UIs:

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

// Modal overlay with input layers
raym3::PushLayer(1);
raym3::Card({200, 200, 400, 300}, raym3::CardVariant::Elevated);
// Modal content
raym3::PopLayer();

raym3::EndFrame();
```

This combination allows you to create responsive layouts with proper input handling for overlays and modals.

