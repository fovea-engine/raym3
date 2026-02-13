#include "raym3/ClipScope.h"

#include <algorithm>
#include <cmath>

namespace raym3 {

static std::vector<Rectangle> s_clipStack;
static std::vector<Rectangle> s_debugRects;
static bool s_gpuClipActive = false;
static int s_suspendDepth = 0;
static bool s_debugEnabled = false;
static int s_underflowCount = 0;

static Rectangle ScreenBounds() {
  return {0.0f, 0.0f, (float)GetScreenWidth(), (float)GetScreenHeight()};
}

static Rectangle IntersectRects(Rectangle a, Rectangle b) {
  float left = std::max(a.x, b.x);
  float top = std::max(a.y, b.y);
  float right = std::min(a.x + a.width, b.x + b.width);
  float bottom = std::min(a.y + a.height, b.y + b.height);
  if (right <= left || bottom <= top) {
    return {0, 0, 0, 0};
  }
  return {left, top, right - left, bottom - top};
}

void ApplyClipRectToGpu(Rectangle rect) {
  if (s_suspendDepth > 0) {
    return;
  }

  if (s_gpuClipActive) {
    EndScissorMode();
    s_gpuClipActive = false;
  }

  if (rect.width <= 0 || rect.height <= 0) {
    return;
  }

  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();
  if (screenW <= 0 || screenH <= 0) {
    return;
  }

  float scaleX = (float)GetRenderWidth() / (float)screenW;
  float scaleY = (float)GetRenderHeight() / (float)screenH;
  float left = rect.x * scaleX;
  float top = rect.y * scaleY;
  float right = (rect.x + rect.width) * scaleX;
  float bottom = (rect.y + rect.height) * scaleY;

  int x1 = (int)std::floor(left);
  int y1 = (int)std::floor(top);
  int x2 = (int)std::ceil(right);
  int y2 = (int)std::ceil(bottom);
  int w = x2 - x1;
  int h = y2 - y1;
  if (w <= 0 || h <= 0) {
    return;
  }

  BeginScissorMode(x1, y1, w, h);
  s_gpuClipActive = true;
}

void PushClipRect(Rectangle bounds) {
  Rectangle base = s_clipStack.empty() ? ScreenBounds() : s_clipStack.back();
  Rectangle clipped = IntersectRects(base, bounds);
  s_clipStack.push_back(clipped);
  if (s_debugEnabled) {
    s_debugRects.push_back(clipped);
  }
  ApplyClipRectToGpu(clipped);
}

void PopClipRect() {
  if (s_clipStack.empty()) {
    s_underflowCount++;
    return;
  }

  s_clipStack.pop_back();
  if (s_clipStack.empty()) {
    if (s_suspendDepth == 0 && s_gpuClipActive) {
      EndScissorMode();
      s_gpuClipActive = false;
    }
    return;
  }

  ApplyClipRectToGpu(s_clipStack.back());
}

Rectangle GetCurrentClipRect() {
  if (s_clipStack.empty()) {
    return ScreenBounds();
  }
  return s_clipStack.back();
}

bool HasActiveClipRect() { return !s_clipStack.empty(); }

bool IsRectInClip(Rectangle bounds) {
  Rectangle clip = GetCurrentClipRect();
  return CheckCollisionRecs(bounds, clip);
}

bool IsPointInClip(Vector2 point) {
  Rectangle clip = GetCurrentClipRect();
  return CheckCollisionPointRec(point, clip);
}

void ClearClipStack() {
  s_clipStack.clear();
  if (s_suspendDepth == 0 && s_gpuClipActive) {
    EndScissorMode();
    s_gpuClipActive = false;
  }
}

void SuspendClipScissor() {
  s_suspendDepth++;
  if (s_gpuClipActive) {
    EndScissorMode();
    s_gpuClipActive = false;
  }
}

void ResumeClipScissor() {
  if (s_suspendDepth <= 0) {
    s_suspendDepth = 0;
    return;
  }

  s_suspendDepth--;
  if (s_suspendDepth == 0 && !s_clipStack.empty()) {
    ApplyClipRectToGpu(s_clipStack.back());
  }
}

void SetClipDebug(bool enabled) { s_debugEnabled = enabled; }

bool IsClipDebug() { return s_debugEnabled; }

void GetClipDebugRects(std::vector<Rectangle> &out) { out = s_debugRects; }

void ClearClipDebugRects() { s_debugRects.clear(); }

int GetClipDepth() { return (int)s_clipStack.size(); }
int GetClipUnderflowCount() { return s_underflowCount; }
void ClearClipUnderflowCount() { s_underflowCount = 0; }

} // namespace raym3
