# October Rendering and Text Updates

This note describes the rendering and text improvements included after raym3 v2.0.0.

## SVG documents

`raym3::SvgRenderer::DrawDocument(source, bounds, opacity)` renders an inline SVG document into a texture. Rasterized document textures are cached by source and size with bounded cache usage. The same renderer supports native `raym3::v2` SVG views and ordinary icon rendering. SVG RGBA fill colors and combined fill opacity are retained.

## Native fonts and fallback

Native builds use the platform system UI font as the default body face. Emscripten uses bundled Roboto and does not look up host font files. Resource embedding affects bundled resources, not the native font default. CSS font family resolution observes requested family, weight, and style; a system fallback face can supply glyphs missing from the chosen face.

## Text selection

Text fields require completed stationary clicks to recognize multi-click selection, so batched press edges and drags do not accidentally trigger it. A double-click selects a word, while a triple-click selects all text. Dragging after a word selection extends by word; dragging after a triple-click extends at character granularity.

## Transformed overflow

Nested overflow clipping follows parent transforms, including rotation, scale, and rounded corners. Hit testing uses the visible clipped shape, so clipped descendants do not receive pointer input outside the visible region.

## Other renderer fixes

The renderer keeps clip-stack state balanced, handles atlas and texture frees requested mid-frame safely, and reports frame statistics from the context that rendered. Emoji font handling also uses platform font support and honors custom emoji font selection.

## Verification targets

With `RAYM3_BUILD_TESTS=ON`, CTest includes text input selection, text layout, SVG RGBA rasterization, and transformed overflow rendering/hit testing. The overflow test requires a working graphics context.
