# raym3 v2 Native UI Upgrade

This branch starts the v2 architecture without removing the existing
immediate-mode API.

## Implemented foundation

- `raym3::v2` namespace with `Style`, `TextStyle`, `MotionSpec`, prop structs,
  composable `View`, `Text`, `TextInput`, `Button`, `Custom`, and
  `MaterialComponent` entry points.
- Same-frame v2 layout/render pipeline. The v2 renderer computes layout before
  drawing, so it does not depend on previous-frame bounds.
- Legacy `Layout::Frame(rootBounds, callback)` helper for immediate-mode code
  that wants a two-pass layout frame to avoid one-frame layout stutter.
- Configurable raylib/Yoga fetch tags. Defaults are raylib `6.0` and Yoga
  `v3.2.1`.
- Lightweight icon manifest flow. `resources/icons/core-icons.txt` is the
  default subset for embedding/installing icons; full catalog embedding/install
  is opt-in.
- Initial text layout scaffold with UTF-8 boundary tracking, whitespace
  normalization, wrapping, and cached prepared text structures.

## Build options

- `RAYM3_RAYLIB_TAG`: raylib tag/branch/commit to fetch when no `raylib` target
  is provided. Default: `6.0`.
- `RAYM3_YOGA_TAG`: Yoga tag/branch/commit to fetch when no `yogacore` target is
  provided. Default: `v3.2.1`.
- `RAYM3_FETCH_DEPS`: fetch missing dependencies. Default: `ON`.
- `RAYM3_BUILD_EXAMPLES`: build example apps. Default: `ON`.
- `RAYM3_BUILD_TESTS`: build CTest tests. Default: `OFF`.
- `RAYM3_ICON_MANIFEST`: newline-delimited icon names to embed/install.
- `RAYM3_EMBED_ALL_ICONS`: embed every icon variation. Default: `OFF`.
- `RAYM3_INSTALL_ALL_ICONS`: install every icon variation. Default: `OFF`.

## Current limitations

The v2 API is a foundation, not full Material 3 compliance yet. The complete M3
catalog is represented by `M3Component`, but many entries currently render with
generic token defaults while their exact spec behavior, accessibility semantics,
and motion details are implemented incrementally.

The text engine scaffold is UTF-8 aware but does not yet include HarfBuzz,
FriBidi, Unicode line-breaking, IME composition, or native platform text
services.
