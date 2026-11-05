# raym3 v2 Best Practices

raym3 v2 aims to behave like a browser: flexbox layout, DOM-style retained
nodes, and HTML input semantics. Most "it looks wrong" or "it doesn't respond"
bugs come from breaking one of the rules below. Each rule lists the symptom you
see when it is broken, so this doubles as a troubleshooting guide.

`examples/v2_demo.cpp` follows every rule here and is the reference app.

## 1. Build configuration

| Option | Default | Why it matters |
| --- | --- | --- |
| `RAYM3_USE_YOGA` | `ON` | Real flexbox. With `OFF`, a basic stacking fallback lays children top-left and **ignores `alignItems`, `justifyContent`, and stretch**. |
| `RAYM3_ANTIALIASING` | `ON` | Requests 4x MSAA before `main()`. With `OFF`, raw raylib shapes (`DrawTriangle`, `DrawCircle`, `DrawLineEx`) get jagged edges. |

**Symptom of Yoga being off:** every child in a row sits at the top-left of
its parent, badges show their digit off-center, chip text rides high. If you
see that, check `RAYM3_USE_YOGA` in your `CMakeCache.txt` before touching any
styles.

Anti-aliasing is a window hint. It has to be requested before `InitWindow`,
so it cannot be toggled at runtime. Turn it off at build time:

```cmake
set(RAYM3_ANTIALIASING OFF CACHE BOOL "" FORCE)
add_subdirectory(raym3)
```

## 2. Build the tree once (retain it like a DOM)

Focus, caret, selection, click counts, hover, and animation state all live
**on the nodes**. Input identity (`NodeId`) is the node's address.

```cpp
// Good: built once, before the loop.
auto root = View({.id = "root", .style = rootStyle}, { /* ... */ });
while (!WindowShouldClose()) {
  // ... frame ...
  Render(root, screen);
}
```

```cpp
// Bad: a brand-new tree every frame.
while (!WindowShouldClose()) {
  auto root = View(/* ... */); // focus and selection are forgotten each frame
  Render(root, screen);
}
```

**Symptoms of rebuilding per frame:** the text field never keeps focus,
double-click never registers, hover states flash.

Values that change over time belong in state the tree reads, not in the tree
itself:

- Capture state **by reference** in callbacks (`[&state]`), never a per-frame
  snapshot like `[now]`. Read the clock (`GetTime()`) inside the callback.
- Bind text fields to a long-lived `std::string` through `.value = &value`.
- Use `Custom` nodes for per-frame drawing. Their draw callback runs every
  frame against the retained node.

## 3. Drive the input system every frame

`Render` only paints. Input is a separate pass, and the host must call it.
This is the same order a browser uses: paint, hit-test the painted frame,
then dispatch.

```cpp
while (!WindowShouldClose()) {
  BeginInputFrame(GetMousePosition(), IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                  IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                  IsMouseButtonReleased(MOUSE_BUTTON_LEFT), GetMouseWheelMove());

  BeginDrawing();
  ClearBackground(raym3::Theme::GetColorScheme().surface);
  raym3::BeginFrame();
  Render(root, screen);
  raym3::EndFrame();
  EndDrawing();

  ResolveInput(root);       // hover, press, focus, onPress
  ResolveTextInput(root);   // caret, selection, keyboard, I-beam cursor
  ResolveScrollInput(root); // scroll gestures, after controls keep capture
}
```

**Symptom of skipping this:** buttons never fire `onPress`, and the text
field ignores clicks. It shows no I-beam cursor and never gets focus.

Pointer coordinates passed to `BeginInputFrame` must already be in dp. On a
1x display that is just `GetMousePosition()`. HiDPI hosts convert first, as
rayact does with `Density`.

Touch hosts call `SetPointerIsMouse(false)`. That switches text fields to
touch rules: a press waits so scrolling can win, long-press selects a word, and
selection handles appear. Mouse hosts keep the default `true`. Mouse rules match
HTML: mousedown focuses and places the caret, and nothing shows handles.

## 4. Let layout do the alignment

Write styles the way you would write CSS flexbox:

- Center children with `alignItems` (cross axis) and `justifyContent` (main
  axis) on the **parent**. Don't hand-place them with margins.
- Let components size to their content. Material defaults already encode
  the spec: a badge is a 16dp circle that grows into a pill, and a chip hugs
  its label. Forcing `width` on these is how a badge becomes "too wide" with
  an off-center digit.
- Label text is a content-sized text node, like an HTML text node. Never give
  a label the container's `width`/`height`. That makes it overflow its
  siblings, which squashed the chip checkmark to 0 width. It also pins the
  glyphs to the top of the box instead of centering them.

## 5. Text fields behave like `<input>`

With the input loop from section 3 and a mouse pointer:

| Action | Result |
| --- | --- |
| Single click | Focus and place the caret, on mousedown. |
| Double click | Select the word. |
| Double click, then drag | Extend the selection a whole word at a time. |
| Triple click | Select all. |
| Click inside a selection | Collapse to a caret at that point. |
| Hover | I-beam cursor, set by `ResolveTextInput`. |
| `autoFocus = true` | Focused when the node is created. |

Cursor ownership: `ResolveTextInput` sets the I-beam directly. `raym3::EndFrame`
applies `RequestCursor` only when the request changes. Don't call
`SetMouseCursor` every frame from app code. Any per-frame write fights the
text field and makes the cursor flicker between the arrow and the I-beam.

## 6. Custom drawing with raw raylib

`Custom` nodes often draw with raylib directly. Three rules:

- **Winding order.** `DrawTriangle` only fills counter-clockwise triangles
  *as seen on screen*. With y pointing down, a fan built with increasing
  angle is clockwise, so pass `(center, next, current)`. Triangles in the
  wrong order are silently culled. The shape just disappears.
- **Morph shapes by blending outlines, not parameters.** When a shape is
  defined by lobe counts or frequencies, interpolating the counts gives
  fractional values. The outline then fails to close at 2π and tears a notch.
  Compute each keyframe's point at the same angle, then lerp the points. Keep
  every frequency an integer.
- **Clip moving content.** Anything that slides inside a panel, such as a
  sheet entering or carousel items scrolling, needs a scissor around the
  stage, or it overflows its parent.

## 7. Motion that reads correctly

- A "replay" animation that ends where it started (`sin(πt)`) looks like
  nothing happened when it is subtle. For state changes such as a morph, select,
  or toggle, animate **to a new resting state** and flip it on each click.
- Carousels follow M3 multi-browse keylines (large, large, medium, small).
  Items change width as they move between keylines. They don't just slide.
- Animate to a resting state the user can inspect. Then verify both
  directions.

## 8. Verifying changes

- **Layout:** dump `node->layout` for the subtree in question. Off-center
  content with correct styles almost always means Yoga is off, or a child has a
  forced size.
- **Input:** script `BeginInputFrame` with synthetic presses. The demo's
  `--autoplay` mode does this. It exercises the same path as a real mouse,
  and you can assert on `node->textEdit` (`cursor`, `selectionStart`,
  `selectionEnd`, `clickCount`).
- **Recording the README GIF:** run `example_v2_demo --autoplay`. That window
  floats on top without taking keyboard focus, so typing elsewhere doesn't
  land in the demo. Capture it, then encode with a generated palette and
  dithering so gradients don't band:

```sh
ffmpeg -i capture.mp4 -vf "fps=15,scale=800:-1:flags=lanczos,split[a][b];\
[a]palettegen=max_colors=256:stats_mode=diff[p];\
[b][p]paletteuse=dither=sierra2_4a:diff_mode=rectangle" media/demo.gif
```
