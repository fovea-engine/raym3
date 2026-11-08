#pragma once

#include <raylib.h>
#include <vector>

// Optional native macOS window customization for raylib's Cocoa window.
// Call after InitWindow(). These options do not change raylib's renderer.

namespace raym3::platform {

// Keep client-drawn material cutouts and rounded hit clips aligned with the
// native transparent window edge.
inline constexpr float kMacOSWindowCornerRadius = 12.0f;

struct MacOSWindowOptions {
  // Allow transparent pixels in the window's content to show the desktop.
  bool transparentBackground = false;
  // Hide the native titlebar while keeping the window controls available.
  bool hideTitlebar = false;
  // Draw content into the titlebar region. Useful with a custom titlebar.
  bool fullSizeContentView = false;
  // Remove the titlebar's opaque background when it is visible.
  bool transparentTitlebar = false;
  // Optional native edge around a transparent window; disabled by default.
  bool showWindowBorder = false;
  Color windowBorderColor = {255, 255, 255, 150};
  float windowBorderWidth = 1.0f;
  // Border thickness measured diagonally at the corners. 0 keeps it equal to
  // windowBorderWidth. Larger values make the corners heavier than the edges.
  float windowBorderCornerWidth = 0.0f;
};

// Top-left positions for native close, minimize, and zoom controls.
struct MacOSWindowControlsPosition {
  Vector2 close{};
  Vector2 minimize{};
  Vector2 zoom{};
};

enum class MacOSWindowMaterial {
  Sidebar,
  Hud,
  Menu,
  Popover,
  UnderWindowBackground,
  WindowBackground,
};

// A native frosted material region, specified in raylib window coordinates
// (origin at top-left). Regions are placed behind raylib's transparent view.
struct MacOSWindowMaterialRegion {
  Rectangle bounds{};
  MacOSWindowMaterial material = MacOSWindowMaterial::Sidebar;
  // In raylib logical points. Match the radius used to draw the raylib shape.
  float cornerRadius = 0.0f;
  bool tracksWindowHeight = false; // Keep full-height regions aligned during live resize.
  // Track a width that is constrained by maxWidth and the window's trailing inset.
  // Useful for sidebars that must follow narrow live resizes before the next render.
  bool tracksWindowWidth = false;
  float maxWidth = 0.0f;
  float windowRightInset = 0.0f;
};

// Blank/custom-titlebar areas that should initiate native window dragging.
struct MacOSWindowDragRegion {
  Rectangle bounds{};
};

// Apply native Cocoa window options. On non-macOS this function is a no-op.
// For desktop transparency, pass raylib's FLAG_WINDOW_TRANSPARENT to
// SetConfigFlags before InitWindow, then clear/draw transparent pixels with
// alpha zero. This window helper cannot change framebuffer hints after creation.
void ConfigureMacOSWindow(const MacOSWindowOptions& options);

// Replace the native material regions. Call after ConfigureMacOSWindow() and
// refresh the rectangles when layout changes. Regions may track window height,
// or a width capped by maxWidth and windowRightInset during live resize. No-op
// off macOS.
void SetMacOSWindowMaterialRegions(
    const std::vector<MacOSWindowMaterialRegion>& regions);

// Replace the native drag hit regions, in raylib coordinates. These regions
// intercept mouse input to start a native window drag, so keep custom controls
// out of them. The native close, minimize, and zoom buttons still receive
// clicks and hover when a region overlaps them. A double click in a region
// zooms or minimizes the window, following the user's titlebar setting.
// No-op off macOS.
void SetMacOSWindowDragRegions(const std::vector<MacOSWindowDragRegion>& regions);

// Position the native controls in raylib window coordinates. Call after
// configuring titlebar style. The OS retains native button behavior.
void SetMacOSWindowControlsPosition(
    const MacOSWindowControlsPosition& positions);

}  // namespace raym3::platform
