# Flutter Material Component Audit

raym3 remains raylib-native. Flutter Material is used as the visual reference
for dp tokens, state behavior, shapes, and motion. `raym3::v2::Density` owns
dp-to-pixel conversion; `raym3::v2::MaterialDefaults` owns component metrics.

| raym3 component | Flutter Material reference | Status |
| --- | --- | --- |
| AppBar, Toolbar | `app_bar.dart` | tokenized defaults |
| BottomAppBar | `bottom_app_bar.dart` | tokenized defaults |
| Badge | `badge.dart` | tokenized defaults |
| Banner | `banner.dart` | tokenized defaults |
| Button, ButtonGroup, SplitButton | `button_style_button.dart`, `filled_button.dart`, `segmented_button.dart` | tokenized defaults |
| Card | `card.dart` | tokenized defaults |
| Carousel | `carousel.dart` | placeholder tokenized |
| Checkbox | `checkbox.dart` | centered visual vs 48dp tap target |
| Chip | `chip.dart`, `filter_chip.dart`, `action_chip.dart` | tokenized defaults |
| DatePicker | `date_picker.dart`, `date_picker_theme.dart` | placeholder tokenized |
| Dialog | `dialog.dart` | tokenized defaults |
| Divider | `divider.dart` | tokenized defaults |
| FAB, ExtendedFAB | `floating_action_button.dart` | tokenized defaults |
| IconButton | `icon_button.dart` | tokenized defaults |
| List | `list_tile.dart`, `list_tile_theme.dart` | tokenized defaults |
| LoadingIndicator, ProgressIndicator | `progress_indicator.dart` | tokenized defaults and painter constants |
| Menu, FabMenu | `menu_anchor.dart`, `menu_style.dart` | tokenized defaults |
| NavigationBar, NavigationBarItem | `navigation_bar.dart` | tokenized defaults |
| NavigationDrawer | `navigation_drawer.dart` | tokenized defaults |
| NavigationRail | `navigation_rail.dart` | tokenized defaults |
| RadioButton | `radio.dart` | centered visual vs 48dp tap target |
| Search | `search_anchor.dart` | tokenized defaults |
| SideSheet, BottomSheet | `bottom_sheet.dart` | tokenized defaults |
| Slider | `slider.dart`, `slider_theme.dart`, `slider_parts.dart` | 2024 M3 track/handle/gap tokens |
| Snackbar | `snack_bar.dart` | tokenized defaults |
| Switch | `switch.dart` | centered 52x32 visual vs 48dp tap target |
| Tabs | `tabs.dart` | tokenized defaults |
| TextField | `text_field.dart`, `input_decorator.dart` | tokenized defaults |
| TimePicker | `time_picker.dart`, `time_picker_theme.dart` | placeholder tokenized |
| Tooltip | `tooltip.dart` | tokenized defaults |

## Redesign pass (2026-06) — fixes landed

Verified end-to-end on Android via `apps/desktop/material-gallery.tsx`:

- Button bridge (`JS_createButton`) now routes through `MaterialComponent(Button)`
  so unstyled `<Button>` gets M3 defaults (primary fill, 40dp, 20dp pill,
  labelLarge) instead of an empty-style box.
- ExtendedFab/Button with a `label` + later-appended Icon child: `JS_appendChild`
  converts `node->text` into a trailing Text child so Yoga lays out [icon][label]
  as a row (was overlapping centered text + icon).
- Tabs restructured to container/item (by label presence, like SegmentedButton):
  container paints the sliding bottom indicator (`NodeRole::Tabs`); items are
  equal-width centered cells with NO fill. `ApplySelectionState` no longer fills
  the selected tab (was a tall secondaryContainer box).
- Wavy indicators (circular + linear) drew thick polylines with `DrawLineEx`,
  which leaves triangular "cracks" on the outer edge of every bend. Fixed by
  adding a `DrawCircleV(p, capR)` round-join at every vertex — in the inline
  `ActivityIndicator` painter, `PaintCircularLoading`, and `DrawWavyLine`.
  `ActivityIndicator` stays wavy-by-default (M3 expressive).
- Chip horizontal padding 16dp→8dp; RadioButton border uses `DrawRing` (was two
  imprecise `DrawCircleLines`); Badge dot variant (6dp) when no label; TextField
  filled-variant painter (top-only corners + 1dp bottom indicator); elevation
  tokens L4 (8dp) / L5 (12dp) added; dead `kSliderPressedHandleWidth` removed.

## Android-specific gotchas (cost real debugging time)

- **Vertex buffer**: RLVK `RL_DEFAULT_BATCH_BUFFER_ELEMENTS` defaulted to 2048 on
  the Android build → `frame vertex buffer full, dropping flush` silently dropped
  draw calls past ~2 cards. Set to 8192 in `apps/android/app/src/main/cpp/CMakeLists.txt`.
- **Scroll coords**: `processRaym3ScrollInput` must receive dp-space cursor
  (`mouseDp`), not raw pixels — scroll node layouts are dp. On Android density
  ~3.7x so pixel coords never hit the dp bounds (silent no-scroll).
- **ScrollView style clobber**: `JS_setStyle` plain-replace wiped `overflow=Scroll`
  set at creation. Scroll nodes (`g_scrollViewIds`) now MERGE like material nodes.
- **Root render**: a desktop entry needs an explicit `render(<App/>)` call;
  `export default` alone leaves `root=no` (black screen).
- **Icons**: an app must `import '../../resources/fonts/material_icons.js'` to
  populate the name→codepoint map, else icon names render as literal text.

## Remaining Work

- Promote more painters from tokenized defaults to exact Flutter paint math.
- Add screenshot comparisons for default, selected, disabled, hovered, focused,
  and pressed states.
- Keep Rayact bridge defaults sourced from `MaterialDefaults`, not local
  literals, so Android and desktop use the same dp model.
