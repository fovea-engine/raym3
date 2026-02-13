#include "raym3/components/View3D.h"
#include "raym3/components/TabBar.h"
#include "raym3/ClipScope.h"
#include "raym3/layout/Layout.h"
#include <algorithm>
#include <raylib.h>
#include <rlgl.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

// Embedded Shader Sources
// Vertex shader for GLSL 330 (desktop OpenGL 3.3)
static const char *vertShader330 = R"(#version 330
in vec2 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 0.0, 1.0);
}
)";

// Vertex shader for GLSL ES 300 (WebGL 2.0 / OpenGL ES 3.0)
static const char *vertShader300es = R"(#version 300 es
in vec2 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 0.0, 1.0);
}
)";

static const char *vertShader100 = R"(
attribute vec2 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec4 vertexColor;
uniform mat4 mvp;
varying vec2 fragTexCoord;
varying vec4 fragColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 0.0, 1.0);
}
)";

// Fragment shader for GLSL 330 (desktop)
static const char *fragShader330 = R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float radius;
void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec2 pixelPos = fragTexCoord * resolution;
    vec2 center = resolution / 2.0;
    vec2 halfSize = resolution / 2.0;
    vec2 p = pixelPos - center;
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 b = halfSize - vec2(r);
    float d = length(max(abs(p) - b, 0.0)) - r;
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) { discard; }
    finalColor = texColor * colDiffuse;
    finalColor.a *= alpha;
}
)";

// Fragment shader for GLSL ES 300 (WebGL 2.0)
static const char *fragShader300es = R"(#version 300 es
precision highp float;
in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float radius;
void main() {
    vec4 texColor = texture(texture0, fragTexCoord);
    vec2 pixelPos = fragTexCoord * resolution;
    vec2 center = resolution / 2.0;
    vec2 halfSize = resolution / 2.0;
    vec2 p = pixelPos - center;
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 b = halfSize - vec2(r);
    float d = length(max(abs(p) - b, 0.0)) - r;
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) { discard; }
    finalColor = texColor * colDiffuse;
    finalColor.a *= alpha;
}
)";

// Fragment shader for GLSL 100 (WebGL 1.0 fallback)
static const char *fragShader100 = R"(#version 100
precision mediump float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 resolution;
uniform float radius;
void main() {
    vec4 texColor = texture2D(texture0, fragTexCoord);
    vec2 pixelPos = fragTexCoord * resolution;
    vec2 center = resolution / 2.0;
    vec2 halfSize = resolution / 2.0;
    vec2 p = pixelPos - center;
    float r = min(radius, min(halfSize.x, halfSize.y));
    vec2 b = halfSize - vec2(r);
    float d = length(max(abs(p) - b, 0.0)) - r;
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);
    if (alpha <= 0.0) { discard; }
    gl_FragColor = texColor * colDiffuse;
    gl_FragColor.a *= alpha;
}
)";

View3D::View3D() {
  // Initialize to zero/null
  target_ = {0};
  shader_ = {0};
}

View3D::~View3D() {
  if (target_.id != 0) {
    UnloadRenderTexture(target_);
  }
  if (postProcessTarget_.id != 0) {
    UnloadRenderTexture(postProcessTarget_);
  }
  if (shaderLoaded_) {
    UnloadShader(shader_);
  }
}

void View3D::LoadRoundedShader() {
  if (shaderLoaded_)
    return;

#ifdef __EMSCRIPTEN__
  shader_ = LoadShaderFromMemory(vertShader300es, fragShader300es);
  if (shader_.id == rlGetShaderIdDefault()) {
    shader_ = LoadShaderFromMemory(vertShader100, fragShader100);
  }
#else
  shader_ = LoadShaderFromMemory(vertShader330, fragShader330);
  if (shader_.id == rlGetShaderIdDefault()) {
    shader_ = LoadShaderFromMemory(vertShader100, fragShader100);
  }
#endif

  if (shader_.id == rlGetShaderIdDefault()) {
    return;
  }

  shaderLocResolution_ = GetShaderLocation(shader_, "resolution");
  shaderLocRadius_ = GetShaderLocation(shader_, "radius");
  shaderLoaded_ = true;
}

void View3D::EnsureTextureSize(int width, int height) {
  if (target_.id == 0 || target_.texture.width != width ||
      target_.texture.height != height) {
    if (target_.id != 0) {
      UnloadRenderTexture(target_);
    }
    target_ = LoadRenderTexture(width, height);
    SetTextureFilter(target_.texture, TEXTURE_FILTER_BILINEAR);
  }
  
  if (postProcessTarget_.id == 0 || postProcessTarget_.texture.width != width ||
      postProcessTarget_.texture.height != height) {
    if (postProcessTarget_.id != 0) {
      UnloadRenderTexture(postProcessTarget_);
    }
    postProcessTarget_ = LoadRenderTexture(width, height);
    SetTextureFilter(postProcessTarget_.texture, TEXTURE_FILTER_BILINEAR);
  }
}

void View3D::Reset() {
  if (target_.id != 0) {
    UnloadRenderTexture(target_);
    target_ = {0};
  }
  if (postProcessTarget_.id != 0) {
    UnloadRenderTexture(postProcessTarget_);
    postProcessTarget_ = {0};
  }
}

void View3D::SetCornerRadius(float radius) { cornerRadius_ = radius; }

int View3D::Render(Rectangle bounds, std::function<void()> renderCallback,
                   Shader postProcessShader,
                   std::function<void(int width, int height)> setPostProcessUniforms) {
  if (!shaderLoaded_) {
    LoadRoundedShader();
  }

  int width = (int)bounds.width;
  int height = (int)bounds.height;

  // Early return if bounds are invalid - don't render or register input
  if (width <= 0 || height <= 0 || bounds.x < 0 || bounds.y < 0) {
    layerId_ = -1;
    return -1;
  }

#if RAYM3_USE_INPUT_LAYERS
  // Don't push/pop layer - use current layer like other raym3 components (Card, Button, etc.)
  // The caller (EditorLayout) manages layers, View3D just registers on current layer
  layerId_ = InputLayerManager::GetCurrentLayerId();
  
  // Only register viewport as blocking region if bounds are valid
  // This prevents the viewport from blocking input when it's hidden/shrunk
  if (bounds.width > 0 && bounds.height > 0) {
    InputLayerManager::RegisterBlockingRegion(bounds, true);
  }
#else
  layerId_ = -1;
#endif

  EnsureTextureSize(width, height);

  // 1. Render scene to texture
  // IMPORTANT: We must disable any active scissor (from UI layout) because
  // it uses Screen Coordinates, which don't map correctly to FBO Coordinates.
  // Flush any pending draw commands before switching to FBO
  // This ensures all UI drawn so far is committed to the screen framebuffer,
  // preventing flickering on WebGL where EndTextureMode() resets to framebuffer 0.
  rlDrawRenderBatchActive();
  SuspendClipScissor();

  BeginTextureMode(target_);
  ClearBackground(BLANK); // Clear with transparent
  if (renderCallback) {
    renderCallback();
  }
  
  // 2. Apply post-process shader if provided (render to postProcessTarget_)
  Texture2D finalTexture = target_.texture;
  if (postProcessShader.id != 0 && postProcessShader.id != rlGetShaderIdDefault()) {
    // Switch directly from target_ FBO to postProcessTarget_ FBO without going through screen FB
    // This eliminates flickering on WebGL by avoiding unnecessary screen-framebuffer round-trips
    rlDrawRenderBatchActive();
    rlEnableFramebuffer(postProcessTarget_.id);
    rlViewport(0, 0, postProcessTarget_.texture.width, postProcessTarget_.texture.height);
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0, postProcessTarget_.texture.width, postProcessTarget_.texture.height, 0, 0.0f, 1.0f);
    rlMatrixMode(RL_MODELVIEW);
    rlLoadIdentity();
    
    ClearBackground(BLANK);
    BeginShaderMode(postProcessShader);
    int texture0Loc = GetShaderLocation(postProcessShader, "texture0");
    if (texture0Loc >= 0) {
      SetShaderValueTexture(postProcessShader, texture0Loc, target_.texture);
    }
    if (setPostProcessUniforms) {
      setPostProcessUniforms(width, height);
    }
    Rectangle source = {0.0f, 0.0f, (float)target_.texture.width, -(float)target_.texture.height};
    Rectangle dest = {0.0f, 0.0f, (float)width, (float)height};
    DrawTexturePro(target_.texture, source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
    
    EndShaderMode();
    finalTexture = postProcessTarget_.texture;
  }
  
  EndTextureMode();

  // 3. Draw texture to screen (optionally with rounded corner shader)
  if (bounds.width <= 0 || bounds.height <= 0) {
    return layerId_;
  }
  
  // Intersect viewport bounds with parent scissor (TabContent, scroll containers)
  Rectangle parentScissor = Layout::GetActiveScissorBounds();
  float left = std::max(bounds.x, parentScissor.x);
  float top = std::max(bounds.y, parentScissor.y);
  float right = std::min(bounds.x + bounds.width, parentScissor.x + parentScissor.width);
  float bottom = std::min(bounds.y + bounds.height, parentScissor.y + parentScissor.height);
  
  bool hasValidScissor = (right > left && bottom > top);
  if (!hasValidScissor) {
    layerId_ = -1;
    return -1;
  }

  ApplyClipRectToGpu({left, top, right - left, bottom - top});
  
  // Apply rounded corner shader if loaded
  if (shaderLoaded_ && shader_.id != rlGetShaderIdDefault()) {
    BeginShaderMode(shader_);
    float resolution[2] = {(float)width, (float)height};
    SetShaderValue(shader_, shaderLocResolution_, resolution, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader_, shaderLocRadius_, &cornerRadius_, SHADER_UNIFORM_FLOAT);
  }

  Rectangle source = {0.0f, 0.0f, (float)finalTexture.width,
                      -(float)finalTexture.height};
  Rectangle dest = bounds;
  Vector2 origin = {0.0f, 0.0f};

  DrawTexturePro(finalTexture, source, dest, origin, 0.0f, WHITE);

  if (shaderLoaded_ && shader_.id != rlGetShaderIdDefault()) {
    EndShaderMode();
  }
  
  ResumeClipScissor();

  return layerId_;
}

bool View3D::CanProcessContinuousInput(Rectangle bounds, int layerId) {
#if RAYM3_USE_INPUT_LAYERS
  // Use raym3 input system to check if viewport can process input
  // This handles bounds checking and layer blocking automatically
  // Use View3D's actual layer ID, not currentLayerId_ (which might be different)
  return InputLayerManager::ShouldProcessMouseInput(bounds, layerId);
#else
  // Fallback: simple bounds check
  Vector2 mousePos = GetMousePosition();
  return CheckCollisionPointRec(mousePos, bounds);
#endif
}

bool View3D::ShouldProcessInput(Rectangle bounds, int layerId) {
  Vector2 mousePos = GetMousePosition();
  bool mouseInBounds = CheckCollisionPointRec(mousePos, bounds);
  
  // Validate that mouse button was up then pressed within bounds
  // This ensures interactions only occur when the press started in the viewport
  bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                      IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                      IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);
  
  bool mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                   IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
                   IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);
  
#if RAYM3_USE_INPUT_LAYERS
  // Use input capture with requireStartInBounds to ensure drags must start in viewport
  // BeginInputCapture with requireStartInBounds=true validates:
  // 1. Mouse button was pressed (up -> down transition) this frame
  // 2. Mouse is currently within bounds
  // 3. No higher layer is blocking
  // 4. Interaction started on View3D's layer (validated by BeginInputCapture with layerId)
  if (mouseDown && !mousePressed) {
    // Mouse is down but wasn't just pressed - drag started elsewhere
    // Check if we already have capture (started in viewport on View3D's layer)
    if (InputLayerManager::IsInputCaptured() && 
        InputLayerManager::IsInputCapturedBy(bounds, layerId)) {
      // We already have capture, continue processing
      return true;
    }
    // No capture means drag didn't start in viewport, deny
    return false;
  }
  
  // Use input capture system for validation with View3D's layer ID
  return InputLayerManager::BeginInputCapture(bounds, true, layerId);
#else
  // Fallback: validate that mouse button was pressed (not just down) within bounds
  // This prevents interactions when mouse was already down before entering viewport
  if (mouseDown && !mousePressed) {
    // Mouse is down but wasn't just pressed - this means drag started elsewhere
    // Don't allow interaction unless mouse was just pressed in bounds
    return false;
  }
  
  // Only allow if mouse was just pressed (transition from up to down) within bounds
  return mouseInBounds && mousePressed;
#endif
}

} // namespace raym3
