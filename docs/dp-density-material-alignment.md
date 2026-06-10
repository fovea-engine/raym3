# raym3 v2 DP and Material Alignment

raym3 v2 uses raylib for rendering and input. Flutter Material is a visual
reference for Material 3 defaults, not a runtime or rendering dependency.

## Unit Contract

- `1 raym3::v2` layout unit is `1dp`, equivalent to Flutter's logical pixel.
- Component defaults, padding, typography sizes, elevation geometry, hit targets,
  and safe-area values are stored in dp.
- `raym3::v2::Density` is the canonical conversion API:
  - `SetPlatformDensity()` stores the real platform density.
  - `SetLayoutDensity()` stores the active raym3 layout/raster density.
  - `DpToPx()` and `PxToDp()` convert only at platform/render boundaries.
  - `RasterPixels()` gives crisp font/icon texture sizes.

## Rayact Android Density Policy

Android `DisplayMetrics.density` is real platform density. Rayact currently
chooses layout density from the surface width normalized to a 390dp viewport:

```text
layoutDensity = surfaceWidthPx / 390dp
```

Safe-area insets arrive from Android in real dp (`px / realDensity`) and are
converted to raym3 layout dp before Yoga/layout sees them:

```text
layoutInsetDp = androidInsetDp * realDensity / layoutDensity
```

This keeps Material component dp sizes stable across phone widths while fonts
and icons still rasterize at the active pixel density.

## Flutter Material Reference Areas

- Buttons and icon buttons: `button_style_button.dart`, `filled_button.dart`,
  `outlined_button.dart`, `text_button.dart`, `icon_button.dart`.
- Text fields and search: `input_decorator.dart`, `text_field.dart`,
  `search_anchor.dart`.
- Selection controls: `checkbox.dart`, `radio.dart`, `switch.dart`.
- Navigation: `navigation_bar.dart`, `navigation_rail.dart`, `tabs.dart`.
- Feedback and progress: `progress_indicator.dart`, `snack_bar.dart`,
  `dialog.dart`, `bottom_sheet.dart`, `tooltip.dart`.

Adopt Flutter Material dimensions and state behavior into raym3 tokens first,
then render them with raylib-native painters.
