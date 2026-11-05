#include "raym3/raym3.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/v2/EmojiFont.h"
#include <rlgl.h>
#include <algorithm>
#include <array>

// Raw OpenGL for stencil operations (not wrapped by rlgl)
#if defined(RAYLIB_USE_RLMT) || defined(RAYLIB_USE_RLWG) || defined(RAYLIB_USE_RLVK)
  // rlmt (Metal), rlwg (WebGPU) and rlvk (Vulkan) have no GL context — route
  // stencil/clear through the rlgl-level wrappers those backends implement.
  // rlvk implements the full set (keyed stencil pipelines + dynamic reference);
  // rlwg stubs them as no-ops for now, so it stays on the scissor fallback.
  #define RAYM3_GL_STENCIL_TEST           0x0B90
  #define RAYM3_GL_EQUAL                  0x0202
  #define RAYM3_GL_KEEP                   0x1E00
  #define RAYM3_GL_INCR                   0x1E02
  #define RAYM3_GL_DECR                   0x1E03
  #define RAYM3_GL_TRUE                   1
  #define RAYM3_GL_FALSE                  0
  #define raym3_glEnable(x)               do { if ((x) == RAYM3_GL_STENCIL_TEST) rlEnableStencilTest(); } while(0)
  #define raym3_glDisable(x)              do { if ((x) == RAYM3_GL_STENCIL_TEST) rlDisableStencilTest(); } while(0)
  #define raym3_glStencilMask(m)          rlStencilMask(m)
  #define raym3_glStencilFunc(f,r,m)     rlStencilFunc(f,r,m)
  #define raym3_glStencilOp(f,d,p)        rlStencilOp(f,d,p)
  #define raym3_glColorMask(r,g,b,a)      rlColorMask(r,g,b,a)
  #define raym3_glDepthMask(m)            do { if (m) rlEnableDepthMask(); else rlDisableDepthMask(); } while(0)
  #define raym3_glClear(x)                do { if ((x) & 0x400) rlClearStencilBuffer(0); } while(0)
  #define GL_STENCIL_TEST                   RAYM3_GL_STENCIL_TEST
  #define GL_EQUAL                          RAYM3_GL_EQUAL
  #define GL_KEEP                           RAYM3_GL_KEEP
  #define GL_INCR                           RAYM3_GL_INCR
  #define GL_DECR                           RAYM3_GL_DECR
  #define GL_TRUE                           RAYM3_GL_TRUE
  #define GL_FALSE                          RAYM3_GL_FALSE
  #define GL_STENCIL_BUFFER_BIT             0x400
  #define glEnable                          raym3_glEnable
  #define glDisable                         raym3_glDisable
  #define glStencilMask                     raym3_glStencilMask
  #define glStencilFunc                     raym3_glStencilFunc
  #define glStencilOp                       raym3_glStencilOp
  #define glColorMask                       raym3_glColorMask
  #define glDepthMask                       raym3_glDepthMask
  #define glClear                           raym3_glClear
#elif defined(__ANDROID__)
  #include <GLES3/gl3.h>
  #include <GLES2/gl2ext.h>
#elif defined(__APPLE__)
  #define GL_SILENCE_DEPRECATION
  #include <OpenGL/gl3.h>
#else
  #include <GL/gl.h>
  #include <GL/glext.h>
#endif

#include "raym3/components/Button.h"
#include "raym3/components/Card.h"
#include "raym3/components/Checkbox.h"
#include "raym3/components/Chip.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/FloatingActionButton.h"
#include "raym3/components/Menu.h"
#include "raym3/components/Navigation.h"
#include "raym3/components/RangeSlider.h"
#include "raym3/components/SearchBar.h"
#include "raym3/components/Slider.h"
#include "raym3/components/Switch.h"
#include "raym3/components/TextField.h"
#include "raym3/components/Tooltip.h"
#include "raym3/layout/Container.h"
#include "raym3/styles/Theme.h"
#include "raym3/layout/Layout.h"

#include "raym3/components/Divider.h"
#include "raym3/components/AppBar.h"
#include "raym3/components/Badge.h"
#include "raym3/components/Icon.h"
#include "raym3/components/IconButton.h"
#include "raym3/components/ProgressIndicator.h"
#include "raym3/components/RadioButton.h"
#include "raym3/components/ButtonGroup.h"
#include "raym3/components/SegmentedButton.h"
#include "raym3/components/Text.h"
#include "raym3/rendering/SvgRenderer.h"
#include <algorithm>
#include <cmath>
#include <vector>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#include "raym3/input/RenderQueue.h"
#endif

namespace raym3 {

static std::array<Texture2D, 4> cssSuitTextures{};
static std::array<bool, 4> cssSuitTexturesReady{};

static Image MakeSuitImage(int suit) {
  constexpr int size = 64;
  const Color white = WHITE;
  const int cx = size / 2;
  Image image = GenImageColor(size, size, BLANK);
  auto circle = [&](float x, float y, float radius) {
    ImageDrawCircle(&image, static_cast<int>(std::round(x)),
                    static_cast<int>(std::round(y)),
                    static_cast<int>(std::round(radius)), white);
  };
  auto triangle = [&](Vector2 a, Vector2 b, Vector2 d) {
    ImageDrawTriangle(&image, a, b, d, white);
  };
  switch (suit) {
  case 0: // spade
    triangle({(float)cx - 27, 34}, {(float)cx + 27, 34}, {(float)cx, 60});
    circle(cx - 13, 28, 14); circle(cx + 13, 28, 14);
    ImageDrawRectangle(&image, cx - 6, 38, 12, 21, white);
    break;
  case 1: // heart
    circle(cx - 13, 20, 15); circle(cx + 13, 20, 15);
    triangle({(float)cx - 27, 23}, {(float)cx + 27, 23}, {(float)cx, 58});
    break;
  case 2: // diamond
    triangle({(float)cx, 8}, {(float)cx + 23, 32}, {(float)cx, 56});
    triangle({(float)cx, 8}, {(float)cx - 23, 32}, {(float)cx, 56});
    break;
  default: // club
    circle(cx, 18, 14); circle(cx - 17, 33, 14); circle(cx + 17, 33, 14);
    ImageDrawRectangle(&image, cx - 6, 36, 12, 22, white);
    break;
  }
  return image;
}

static void InitializeCssSuitTextures() {
  for (size_t i = 0; i < cssSuitTextures.size(); ++i) {
    if (cssSuitTexturesReady[i]) continue;
    Image image = MakeSuitImage(static_cast<int>(i));
    cssSuitTextures[i] = LoadTextureFromImage(image);
    UnloadImage(image);
    if (cssSuitTextures[i].id != 0) {
      SetTextureFilter(cssSuitTextures[i], TEXTURE_FILTER_BILINEAR);
      cssSuitTexturesReady[i] = true;
    }
  }
}

static int s_requestedCursor = MOUSE_CURSOR_DEFAULT;
static bool initialized = false;
static std::vector<Rectangle> s_scissorStack;
static bool s_scissorDebugEnabled = false;
static std::vector<Rectangle> s_scissorDebugRects;

struct StencilEntry { Rectangle bounds; float radius; };
static std::vector<StencilEntry> s_stencilStack;

static Rectangle IntersectAndClampScissor(Rectangle requested, Rectangle current) {
  float left = std::max(requested.x, current.x);
  float top = std::max(requested.y, current.y);
  float right = std::min(requested.x + requested.width, current.x + current.width);
  float bottom = std::min(requested.y + requested.height, current.y + current.height);
  if (right <= left || bottom <= top)
    return {0, 0, 0, 0};
  int renderW = std::max(1, GetScreenWidth());
  int renderH = std::max(1, GetScreenHeight());
  int x = (int)std::floor(left);
  int y = (int)std::floor(top);
  int w = (int)std::ceil(right - left);
  int h = (int)std::ceil(bottom - top);
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > renderW) w = renderW - x;
  if (y + h > renderH) h = renderH - y;
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  return {(float)x, (float)y, (float)w, (float)h};
}

void PushScissor(Rectangle bounds) {
  int renderW = std::max(1, GetScreenWidth());
  int renderH = std::max(1, GetScreenHeight());
  Rectangle current = s_scissorStack.empty()
    ? Rectangle{0, 0, (float)renderW, (float)renderH}
    : s_scissorStack.back();
  Rectangle applied = IntersectAndClampScissor(bounds, current);
  // A clip that does not overlap its parent is empty, not absent: push it so it
  // clips everything out. Returning early instead left the stack one entry
  // short while the caller still ran its matching PopScissor, so that pop took
  // the *ancestor's* scissor off and every later draw in the frame was clipped
  // by the wrong rect — content bleeding out of a card or a scroll view.
  if (applied.width < 1 || applied.height < 1)
    applied = {applied.x, applied.y, 0, 0};
  s_scissorStack.push_back(applied);
  BeginScissorMode((int)applied.x, (int)applied.y, (int)applied.width, (int)applied.height);
  if (s_scissorDebugEnabled)
    s_scissorDebugRects.push_back(applied);
}

void PopScissor() {
  if (s_scissorStack.empty())
    return;
  s_scissorStack.pop_back();
  if (s_scissorStack.empty()) {
    EndScissorMode();
    return;
  }
  Rectangle prev = s_scissorStack.back();
  BeginScissorMode((int)prev.x, (int)prev.y, (int)prev.width, (int)prev.height);
}

void PushRoundedStencil(Rectangle bounds, float radius) {
  PushRoundedStencilWithScissor(bounds, radius, bounds);
}

void PushRoundedStencilWithScissor(Rectangle bounds, float radius,
                                   Rectangle scissorBounds) {
  // A rounded stencil is the curved part of CSS overflow clipping, but keep a
  // rectangular scissor around it as well. Besides avoiding rasterization
  // fringe outside the clip box, this makes the API safe for callers that do
  // not separately push a scissor (the retained renderer does not). Nested
  // callers simply get another intersected scissor, which is intentional.
  PushScissor(scissorBounds);
#if defined(RAYLIB_USE_RLWG) || defined(RAYM3_WEBGPU)
  // rlwg stubs the stencil wrappers as no-ops, so a true rounded clip is not
  // possible there yet. Fall back to the rectangular scissor instead of
  // skipping clipping entirely — otherwise a child that overflows a rounded
  // container (ripples, translucent fills, images) is not clipped at all. The
  // scissor path is safe here: the platform sets FLAG_WINDOW_HIGHDPI, so
  // BeginScissorMode scales the dp rect to framebuffer pixels exactly like the
  // dp content-scale applied to geometry (same contract ScrollView clipping
  // relies on). Cost: the clip is square — content is confined to the box but
  // corner pixels inside the radius are not masked.
  // rlvk takes the real stencil path below: its rl* wrappers implement keyed
  // stencil pipelines with a dynamic reference (the historical "stencil blanks
  // the frame" on rlvk was a pipeline-cache key collision — raw GL enums
  // overflowing their bitfields — fixed in rlvk_gl33.inc).
  (void)radius;
  return;
#endif
  rlDrawRenderBatchActive(); // flush pending draws before touching GL state

  int parentLevel = (int)s_stencilStack.size();
  int newLevel    = parentLevel + 1;

  glEnable(GL_STENCIL_TEST);
  glStencilMask(0xFF);

  // Write into stencil only where the parent clip already passes
  glStencilFunc(GL_EQUAL, parentLevel, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_INCR); // increment on pass

  // Draw rounded rect into stencil only (no colour output)
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDepthMask(GL_FALSE);

  float minDim    = std::min(bounds.width, bounds.height);
  float roundness = (minDim > 0.0f) ? std::min(1.0f, (radius * 2.0f) / minDim) : 0.0f;
  DrawRectangleRounded(bounds, roundness, 32, WHITE);
  rlDrawRenderBatchActive();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);

  // Subsequent draws pass only inside the new rounded clip
  glStencilFunc(GL_EQUAL, newLevel, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilMask(0x00); // lock stencil during normal rendering

  s_stencilStack.push_back({bounds, radius});
}

void PopRoundedStencil() {
#if defined(RAYLIB_USE_RLWG) || defined(RAYM3_WEBGPU)
  PopScissor(); // the rectangular fallback also serves as the full clip
  return;
#endif
  if (s_stencilStack.empty()) {
    PopScissor();
    return;
  }
  StencilEntry entry = s_stencilStack.back();
  s_stencilStack.pop_back();

  rlDrawRenderBatchActive();

  int currentLevel = (int)s_stencilStack.size() + 1; // level we're leaving
  int parentLevel  = (int)s_stencilStack.size();

  glStencilMask(0xFF);
  glStencilFunc(GL_EQUAL, currentLevel, 0xFF);
  glStencilOp(GL_KEEP, GL_KEEP, GL_DECR); // undo the increment

  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDepthMask(GL_FALSE);

  float minDim    = std::min(entry.bounds.width, entry.bounds.height);
  float roundness = (minDim > 0.0f) ? std::min(1.0f, (entry.radius * 2.0f) / minDim) : 0.0f;
  DrawRectangleRounded(entry.bounds, roundness, 32, WHITE);
  rlDrawRenderBatchActive();

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDepthMask(GL_TRUE);

  if (s_stencilStack.empty()) {
    glDisable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
  } else {
    glStencilFunc(GL_EQUAL, parentLevel, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);
  }
  PopScissor();
}

Rectangle GetCurrentScissorBounds() {
  if (s_scissorStack.empty()) {
    int w = std::max(1, GetScreenWidth());
    int h = std::max(1, GetScreenHeight());
    return {0, 0, (float)w, (float)h};
  }
  return s_scissorStack.back();
}

void ReplayCurrentClips() {
#if defined(RAYLIB_USE_RLWG) || defined(RAYM3_WEBGPU)
  if (!s_scissorStack.empty()) {
    const Rectangle bounds = s_scissorStack.back();
    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)bounds.width, (int)bounds.height);
  }
  return;
#else
  rlDrawRenderBatchActive();
  EndScissorMode();
  if (s_stencilStack.empty()) {
    glDisable(GL_STENCIL_TEST);
  } else {
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    for (size_t index = 0; index < s_stencilStack.size(); ++index) {
      const StencilEntry &entry = s_stencilStack[index];
      glStencilFunc(GL_EQUAL, (int)index, 0xFF);
      glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
      glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glDepthMask(GL_FALSE);
      float minDim = std::min(entry.bounds.width, entry.bounds.height);
      float roundness = minDim > 0.0f
          ? std::min(1.0f, (entry.radius * 2.0f) / minDim) : 0.0f;
      DrawRectangleRounded(entry.bounds, roundness, 32, WHITE);
      rlDrawRenderBatchActive();
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilFunc(GL_EQUAL, (int)s_stencilStack.size(), 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0x00);
  }
  if (!s_scissorStack.empty()) {
    const Rectangle bounds = s_scissorStack.back();
    BeginScissorMode((int)bounds.x, (int)bounds.y,
                     (int)bounds.width, (int)bounds.height);
  }
#endif
}

void SetScissorDebug(bool enabled) { s_scissorDebugEnabled = enabled; }
bool IsScissorDebug() { return s_scissorDebugEnabled; }

void BeginScissor(Rectangle bounds) {
  PushScissor(bounds);
}
static bool darkMode = false;

void Initialize() {
  if (initialized)
    return;

  Theme::Initialize();
  SvgRenderer::Initialize(nullptr); // Auto-detect resource path
  InitializeCssSuitTextures();

#if defined(__APPLE__) || defined(_WIN32)
  // Apple: CoreText (Apple Color Emoji). Windows: DirectWrite/D2D (Segoe UI Emoji).
  raym3::v2::EmojiFont::Instance().SetRasterizer(raym3::v2::PlatformRasterizeEmoji);
#endif

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::Initialize();
  RenderQueue::Initialize();
#endif

  initialized = true;
}

void Shutdown() {
  if (!initialized)
    return;

  for (size_t i = 0; i < cssSuitTextures.size(); ++i) {
    if (cssSuitTexturesReady[i]) {
      UnloadTexture(cssSuitTextures[i]);
      cssSuitTexturesReady[i] = false;
    }
  }

  SvgRenderer::Shutdown();
  Theme::Shutdown();
  initialized = false;
}

void RequestCursor(int cursor) {
  s_requestedCursor = cursor;
}

void BeginFrame() {
  if (!initialized)
    Initialize();
  s_requestedCursor = MOUSE_CURSOR_DEFAULT;
  s_scissorDebugRects.clear();
  s_stencilStack.clear();
  // Clear stencil buffer at frame start so rounded clips from previous frame don't leak
  glClear(GL_STENCIL_BUFFER_BIT);
  TextFieldComponent::ResetFieldId();
  SliderComponent::ResetFieldId();
  RangeSliderComponent::ResetFieldId();
  ButtonComponent::ResetIds();
  ButtonGroupComponent::ResetIds();
  SegmentedButtonComponent::ResetIds();
  IconButtonComponent::ResetIds();
  CheckboxComponent::ResetIds();
  SwitchComponent::ResetIds();
  RadioButtonComponent::ResetIds();
  ChipComponent::ResetIds();
  FloatingActionButtonComponent::ResetIds();

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::BeginFrame();
  RenderQueue::BeginFrame();
#endif
}

void DrawScissorDebug() {
  if (!s_scissorDebugEnabled || s_scissorDebugRects.empty())
    return;
  while (!s_scissorStack.empty()) {
    s_scissorStack.pop_back();
  }
  EndScissorMode();
  for (const Rectangle& r : s_scissorDebugRects) {
    DrawRectangleRec(r, (Color){0, 255, 0, 35});
    DrawRectangleLinesEx(r, 2.0f, (Color){0, 255, 0, 180});
  }
  s_scissorDebugRects.clear();
}

void EndFrame() {
  SetMouseCursor(s_requestedCursor);

  TooltipManager::Update();

  while (!s_scissorStack.empty()) {
    s_scissorStack.pop_back();
  }
  EndScissorMode();

#if RAYM3_USE_INPUT_LAYERS
  RenderQueue::ExecuteRenderQueue();
  InputLayerManager::EndFrame();
#endif
}

void DrawCssSlider(Rectangle bounds, float progress, Color accent, Color track,
                   float trackThickness, float thumbRadius) {
  const float cy = bounds.y + bounds.height * 0.5f;
  const float left = bounds.x;
  const float right = bounds.x + bounds.width;
  const float x = left + std::clamp(progress, 0.0f, 1.0f) *
                            std::max(0.0f, right - left);
  DrawLineEx({left, cy}, {right, cy}, trackThickness, track);
  DrawLineEx({left, cy}, {x, cy}, trackThickness, accent);
  DrawCircleV({x, cy}, thumbRadius, accent);
}

void DrawCssGradientRow(float x, float y, float width, float height,
                        Color left, Color right) {
  if (width <= 0.0f || height <= 0.0f) return;
  const int ix = static_cast<int>(std::floor(x));
  const int iy = static_cast<int>(std::floor(y));
  const int iw = std::max(1, static_cast<int>(std::ceil(width)));
  const int ih = std::max(1, static_cast<int>(std::ceil(height)));
  DrawRectangleGradientH(ix, iy, iw, ih, left, right);
}

void DrawCssEllipseBorder(Rectangle bounds, Color color, float thickness,
                          bool dashed) {
  const float cx = bounds.x + bounds.width * 0.5f;
  const float cy = bounds.y + bounds.height * 0.5f;
  const float rx = std::max(0.0f, bounds.width * 0.5f - thickness * 0.5f);
  const float ry = std::max(0.0f, bounds.height * 0.5f - thickness * 0.5f);
  if (rx <= 0.0f || ry <= 0.0f) return;
  if (!dashed) {
    const int layers = std::max(1, static_cast<int>(std::ceil(thickness)));
    for (int i = 0; i < layers; ++i)
      DrawEllipseLines(static_cast<int>(std::round(cx)),
                       static_cast<int>(std::round(cy)),
                       std::max(1, static_cast<int>(std::round(rx - i))),
                       std::max(1, static_cast<int>(std::round(ry - i))), color);
    return;
  }
  const float perimeter = PI * (3.0f * (rx + ry) -
      std::sqrt(std::max(0.0f, (3.0f * rx + ry) * (rx + 3.0f * ry))));
  const int dashes = std::max(8, static_cast<int>(perimeter / 7.0f));
  const int samples = std::max(2, static_cast<int>(std::ceil(perimeter / 2.0f)));
  for (int i = 0; i < samples; ++i) {
    const float phase = static_cast<float>(i) / samples * dashes;
    if (phase - std::floor(phase) >= 0.62f) continue;
    const float a0 = 2.0f * PI * static_cast<float>(i) / samples;
    const float a1 = 2.0f * PI * static_cast<float>(i + 1) / samples;
    DrawLineEx({cx + rx * std::cos(a0), cy + ry * std::sin(a0)},
               {cx + rx * std::cos(a1), cy + ry * std::sin(a1)},
               thickness, color);
  }
}

void DrawCssInsetEllipse(Rectangle bounds, Color color, float offsetY,
                         float thickness) {
  const float cx = bounds.x + bounds.width * 0.5f;
  const float cy = bounds.y + bounds.height * 0.5f + offsetY;
  const float rx = std::max(0.0f, bounds.width * 0.5f - thickness * 0.5f);
  const float ry = std::max(0.0f, bounds.height * 0.5f - thickness * 0.5f);
  if (rx > 0.0f && ry > 0.0f) {
    const int layers = std::max(1, static_cast<int>(std::ceil(thickness)));
    for (int i = 0; i < layers; ++i)
      DrawEllipseLines(static_cast<int>(std::round(cx)),
                       static_cast<int>(std::round(cy)),
                       std::max(1, static_cast<int>(std::round(rx - i))),
                       std::max(1, static_cast<int>(std::round(ry - i))), color);
  }
}

void DrawCssSuitMark(Rectangle bounds, int suit, Color color) {
  if (suit < 0 || suit >= static_cast<int>(cssSuitTextures.size()) ||
      bounds.width <= 0.0f || bounds.height <= 0.0f)
    return;
  const size_t index = static_cast<size_t>(suit);
  if (!cssSuitTexturesReady[index]) return;
  const Texture2D texture = cssSuitTextures[index];
  DrawTexturePro(texture, {0, 0, static_cast<float>(texture.width),
                           static_cast<float>(texture.height)},
                 bounds, {0, 0}, 0.0f, color);
}

// Vector replacement for U+26F6 (SQUARE FOUR CORNERS) — the full-screen glyph.
// Browsers draw this codepoint with a solid white outline and corner squares;
// the thin anti-aliased fallback face renders it as faint grey specks, so the
// runtime paints a crisp vector icon instead of trusting glyph fallbacks.
void DrawCssFullscreenIcon(Rectangle bounds, Color color) {
  const float side = std::min(bounds.width, bounds.height);
  if (side <= 2.0f) return;
  const float cx = bounds.x + bounds.width * 0.5f;
  const float cy = bounds.y + bounds.height * 0.5f;
  const float k = std::max(1.5f, side * 0.15f);
  const float stroke = std::max(1.0f, side * 0.085f);
  const float box = std::max(2.0f, side - (k * 2.0f + stroke));
  const float bx = cx - box * 0.5f;
  const float by = cy - box * 0.5f;
  const Rectangle outline = {bx, by, box, box};
  DrawRectangleRoundedLinesEx(outline, 0.18f, 10, stroke, color);
  const float off = stroke * 0.75f;
  const Rectangle corners[4] = {
      {bx - off - k, by - off - k, k, k},
      {bx + box + off, by - off - k, k, k},
      {bx - off - k, by + box + off, k, k},
      {bx + box + off, by + box + off, k, k}};
  for (const Rectangle &corner : corners) DrawRectangleRec(corner, color);
}

void DrawCssBoxFill(Rectangle bounds, float cornerRadius, Color color) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f || color.a == 0) return;
  Renderer::DrawRoundedRectangle(bounds, std::max(0.0f, cornerRadius), color);
}

void DrawCssBoxBorder(Rectangle bounds, float cornerRadius, float thickness,
                      Color color) {
  if (bounds.width <= 0.0f || bounds.height <= 0.0f || thickness <= 0.0f ||
      color.a == 0)
    return;
  Renderer::DrawRoundedRectangleEx(bounds, std::max(0.0f, cornerRadius), color,
                                   thickness);
}

#if RAYM3_USE_INPUT_LAYERS
static constexpr int OVERLAY_LAYER_THRESHOLD = 100;

void PushLayer(int zOrder) {
  InputLayerManager::PushLayer(zOrder);
  RenderQueue::PushLayer(zOrder);

  if (zOrder >= OVERLAY_LAYER_THRESHOLD) {
    EndScissorMode();
  }
}

void PopLayer() {
  InputLayerManager::PopLayer();
  RenderQueue::PopLayer();
  
  // If we returned to the base layer (or if we need to restore layout clipping context)
  // We check if there's an active layout scissor and restore it.
  // Note: This logic assumes we mostly use Layers for Overlays on top of Layouts.
  // If Layouts are nested in Layers, this might need refinement.
  
  if (InputLayerManager::GetCurrentLayerId() <= 0 && !s_scissorStack.empty()) {
    Rectangle s = s_scissorStack.back();
    BeginScissorMode((int)s.x, (int)s.y, (int)s.width, (int)s.height);
  }
}
#endif

void SetTheme(bool isDarkMode) {
  darkMode = isDarkMode;
  Theme::SetDarkMode(isDarkMode);
}

bool IsDarkMode() { return darkMode; }

void SetIconBasePath(const char *path) { SvgRenderer::Initialize(path); }

void BeginContainer(Rectangle bounds, LayoutDirection direction) {
  Container::Begin(bounds, direction);
}

void EndContainer() { Container::End(); }

bool Button(const char *text, Rectangle bounds, ButtonVariant variant) {
  return ButtonComponent::Render(text, bounds, variant);
}

bool IconButton(const char *iconName, Rectangle bounds, ButtonVariant variant,
                IconVariation iconVariation) {
  return IconButtonComponent::Render(iconName, bounds, variant, iconVariation);
}

void Badge(Rectangle anchorBounds, const char *label,
           const BadgeOptions &options) {
  BadgeComponent::Render(anchorBounds, label, options);
}

bool Chip(const char *label, Rectangle bounds, const ChipOptions &options) {
  return ChipComponent::Render(label, bounds, options);
}

bool Chip(const char *label, Rectangle bounds, bool *selected,
          const ChipOptions &options) {
  return ChipComponent::Render(label, bounds, selected, options);
}

bool FloatingActionButton(const char *iconName, Rectangle bounds,
                          const FabOptions &options) {
  return FloatingActionButtonComponent::Render(iconName, bounds, options);
}

bool ExtendedFloatingActionButton(const char *label, const char *iconName,
                                  Rectangle bounds,
                                  const FabOptions &options) {
  return FloatingActionButtonComponent::RenderExtended(label, iconName, bounds,
                                                       options);
}

int AppBar(Rectangle bounds, const char *title,
           const AppBarOptions &options) {
  return AppBarComponent::Render(bounds, title, options);
}

bool SearchBar(char *buffer, int bufferSize, Rectangle bounds,
               const char *placeholder, const SearchBarOptions &options) {
  return SearchBarComponent::Render(buffer, bufferSize, bounds, placeholder,
                                    options);
}

bool NavigationBar(Rectangle bounds, const NavigationItem *items, int itemCount,
                   int *selectedIndex, const NavigationOptions &options) {
  return NavigationComponent::Bar(bounds, items, itemCount, selectedIndex,
                                  options);
}

bool NavigationRail(Rectangle bounds, const NavigationItem *items, int itemCount,
                    int *selectedIndex, const NavigationOptions &options) {
  return NavigationComponent::Rail(bounds, items, itemCount, selectedIndex,
                                   options);
}

bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label) {
  return TextFieldComponent::Render(buffer, bufferSize, bounds, label);
}

bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label, const TextFieldOptions &options) {
  return TextFieldComponent::Render(buffer, bufferSize, bounds, label, options);
}

bool Checkbox(const char *label, Rectangle bounds, bool *checked) {
  return CheckboxComponent::Render(label, bounds, checked);
}

bool Switch(const char *label, Rectangle bounds, bool *checked) {
  return SwitchComponent::Render(label, bounds, checked);
}

bool RadioButton(const char *label, Rectangle bounds, bool selected) {
  return RadioButtonComponent::Render(label, bounds, selected);
}

float Slider(Rectangle bounds, float value, float min, float max,
             const char *label) {
  return SliderComponent::Render(bounds, value, min, max, label);
}

float Slider(Rectangle bounds, float value, float min, float max,
             const char *label, const SliderOptions &options) {
  return SliderComponent::Render(bounds, value, min, max, label, options);
}

std::vector<float> RangeSlider(Rectangle bounds,
                               const std::vector<float> &values, float min,
                               float max, const char *label,
                               const RangeSliderOptions &options) {
  return RangeSliderComponent::Render(bounds, values, min, max, label, options);
}

void Icon(const char *name, Rectangle bounds, IconVariation variation,
          Color color) {
  IconComponent::Render(name, bounds, variation, color);
}

void Text(const char *text, Rectangle bounds, float fontSize, Color color,
          FontWeight weight, TextAlignment alignment) {
  TextComponent::Render(text, bounds, fontSize, color, weight, alignment);
}

void CircularProgressIndicator(Rectangle bounds, float value,
                               bool indeterminate, Color color,
                               float wiggleAmplitude, float wiggleFrequency) {
  ProgressIndicator::Circular(bounds, value, indeterminate, color,
                              wiggleAmplitude, wiggleFrequency);
}

void LinearProgressIndicator(Rectangle bounds, float value, bool indeterminate,
                             Color color, float wiggleAmplitude,
                             float wiggleFrequency) {
  ProgressIndicator::Linear(bounds, value, indeterminate, color,
                            wiggleAmplitude, wiggleFrequency);
}

void Card(Rectangle bounds, CardVariant variant) {
  CardComponent::Render(bounds, variant);
}

bool Dialog(const char *title, const char *message, const char *buttons) {
  return DialogComponent::Render(title, message, buttons);
}

void Menu(Rectangle bounds, const MenuItem *items, int itemCount,
          int *selectedIndex, bool iconOnly) {
  MenuComponent::Render(bounds, items, itemCount, selectedIndex, iconOnly);
}

bool SegmentedButton(Rectangle bounds, const SegmentedButtonItem *items,
                     int itemCount, int *selectedIndex, bool multiSelect) {
  return SegmentedButtonComponent::Render(bounds, items, itemCount,
                                          selectedIndex, multiSelect);
}

int ButtonGroup(Rectangle bounds, const ButtonGroupItem *items, int count,
                ButtonVariant variant) {
  return ButtonGroupComponent::Render(bounds, items, count, variant);
}

void Divider(Rectangle bounds, DividerVariant variant) {
  DividerComponent::Render(bounds, variant);
}

} // namespace raym3
