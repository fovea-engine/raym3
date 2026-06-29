#include "raym3/raym3.h"
#include "raym3/v2/EmojiFont.h"
#include <rlgl.h>

// Raw OpenGL for stencil operations (not wrapped by rlgl)
#if defined(RAYLIB_USE_RLMT) || defined(RAYLIB_USE_RLWG)
  // rlmt (Metal) and rlwg (WebGPU) have no GL context — route stencil/clear through
  // the rlgl-level wrappers their backends implement (rlwg stubs them as no-ops for
  // now; stencil-based rounded clipping is a later rlwg feature).
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
  if (applied.width < 1 || applied.height < 1)
    return;
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
#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLWG) || defined(RAYM3_WEBGPU)
  // Non-GL backends without a real color-mask + stencil implementation must skip
  // rounded clipping. Otherwise the WHITE mask quad
  // (DrawRectangleRounded(..., WHITE)) leaks into the color buffer and paints
  // elevated/rounded elements white.
  // The rlgl stencil wrappers don't reliably support the nested INCR/DECR clip
  // (it blanks the frame), and BeginScissorMode works in framebuffer pixels
  // while these bounds are dp (the Android render is dp-scaled), so a scissor
  // fallback can clip the wrong region and also blank. Until rounded clipping is
  // implemented correctly for these backends, skip it entirely: no mask draw
  // (no white leak), no stencil, no scissor. Cost: ripples/overflow are not
  // clipped to the rounded shape.
  (void)bounds;
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
#if defined(RAYLIB_USE_RLVK) || defined(RAYLIB_USE_RLWG) || defined(RAYM3_WEBGPU)
  return; // rounded clipping disabled for this backend (see PushRoundedStencil)
#endif
  if (s_stencilStack.empty()) return;
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
}

Rectangle GetCurrentScissorBounds() {
  if (s_scissorStack.empty()) {
    int w = std::max(1, GetScreenWidth());
    int h = std::max(1, GetScreenHeight());
    return {0, 0, (float)w, (float)h};
  }
  return s_scissorStack.back();
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

#if defined(__APPLE__)
  // Wire the OS emoji rasterizer on Apple platforms (macOS now; iOS future).
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
