# ClipScope System

The ClipScope system provides a stack-based clipping API for controlling what regions of the screen receive rendering. It handles DPI scaling, nested clips, and visibility checks.

## Overview

ClipScope manages a stack of clip rectangles. Each `PushClipRect()` intersects with the previous clip and applies the result to the GPU scissor. This enables scroll containers, overflow panels, and nested clipping regions.

## Basic Usage

```cpp
#include "raym3/raym3.h"

raym3::BeginFrame();

// Clip to a region
raym3::BeginScissor({100, 100, 400, 300});

// All drawing is clipped to this region
raym3::Button("Clipped Button", {120, 120, 120, 40});

raym3::PopScissor();

raym3::EndFrame();
```

## API (raym3.h)

- **`BeginScissor(Rectangle bounds)`** / **`PushScissor(Rectangle bounds)`** - Push a clip region onto the stack
- **`PopScissor()`** - Pop the current clip region
- **`GetCurrentScissorBounds()`** - Get the active clip rectangle
- **`IsVisible(Rectangle bounds)`** - Check if a rectangle intersects the current clip
- **`SetScissorDebug(bool enabled)`** / **`IsScissorDebug()`** - Debug visualization

## Low-Level API (ClipScope.h)

For advanced use:

- **`PushClipRect(Rectangle bounds)`** - Push clip (intersects with parent)
- **`PopClipRect()`** - Pop clip
- **`GetCurrentClipRect()`** - Current clip bounds
- **`HasActiveClipRect()`** - Returns true if stack is non-empty
- **`IsRectInClip(Rectangle bounds)`** - Visibility check for rectangle
- **`IsPointInClip(Vector2 point)`** - Visibility check for point
- **`ClearClipStack()`** - Reset stack (raym3 does this automatically at frame end if unbalanced)

## Suspend / Resume

When you need to draw outside the current clip (e.g., a dropdown menu, tooltip, or overlay that extends beyond a scroll area):

```cpp
#include "raym3/ClipScope.h"

raym3::PushScissor(scrollContentBounds);

// Draw clipped content
raym3::Button("Item", itemBounds);

// Temporarily disable clipping for overlay
raym3::SuspendClipScissor();

// Draw overlay that extends beyond clip (e.g., dropdown)
raym3::Card(dropdownBounds, raym3::CardVariant::Elevated);

raym3::ResumeClipScissor();

// Clipping is restored
raym3::PopScissor();
```

Use a guard for exception safety:

```cpp
{
  raym3::SuspendClipScissor();
  struct RestoreGuard {
    ~RestoreGuard() { raym3::ResumeClipScissor(); }
  } guard;
  // draw overlay
}
```

## Frame Validation

At `EndFrame()`, raym3 validates the clip stack. If the stack is not empty (unbalanced Push/Pop), it logs a warning and clears the stack. Underflow (Pop without Push) is also detected and reported.

## Debug Mode

Enable `SetScissorDebug(true)` to record clipped regions for debug visualization. Use `GetClipDebugRects()` to retrieve the list of applied clip rectangles.
