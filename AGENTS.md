# Notes for coding agents

## Checking what the UI looks like

Capture the app from inside raylib. Do not screenshot the desktop: it shows the user's private windows.

- `TakeScreenshot("name.png")` after `EndDrawing()` saves the framebuffer. raylib treats the argument as a file name
  under its working directory; an absolute path is turned into a nested name, so use `cd` or a relative name.
- For a path you choose, or to check pixels in code: `Image img = LoadImageFromScreen();` then
  `ExportImage(img, "/abs/path.png");` or `GetImageColor(img, x, y)`, then `UnloadImage(img)`.
  `tests/overflow_transform_test.cpp` does both.
- Gate it behind an env var (e.g. capture on frame N when `APP_SHOT=/path.png` is set) so the app needs no input and
  nothing stays in release behavior. Wait a few frames: fonts, icons and layout settle after the first frames.
- Captures are in framebuffer pixels (2x on Retina) and keep alpha. On a transparent window, areas meant to be opaque
  should read alpha 255; anything lower shows the desktop through.
- AppKit layers behind the framebuffer (the `NSVisualEffectView` blur from `MacOSWindow`, traffic lights) are not in
  the framebuffer. Only when those must be checked, capture that one window: `screencapture -o -l<windowid> out.png`
  (window id from `CGWindowListCopyWindowInfo`). Never a full-screen capture.
