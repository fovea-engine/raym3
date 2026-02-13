#include "raym3/input/RenderQueue.h"

#if RAYM3_USE_INPUT_LAYERS

#include "raym3/ClipScope.h"
#include "raym3/layout/Layout.h"
#include <algorithm>

namespace raym3 {

std::vector<RenderCommand> RenderQueue::renderQueue_;
int RenderQueue::currentLayerId_ = 0;
std::vector<int> RenderQueue::layerStack_ = {0};
int RenderQueue::registrationCounter_ = 0;
int RenderQueue::nextComponentId_ = 0;
std::vector<int> RenderQueue::inputBlockingLayers_;

void RenderQueue::Initialize() {
  Clear();
}

void RenderQueue::BeginFrame() {
  Clear();
  currentLayerId_ = 0;
  layerStack_ = {0};
  registrationCounter_ = 0;
  nextComponentId_ = 0;
}

void RenderQueue::Clear() {
  renderQueue_.clear();
  inputBlockingLayers_.clear();
}

void RenderQueue::PushLayer(int zOrder) {
  currentLayerId_++;
  layerStack_.push_back(currentLayerId_);
}

void RenderQueue::PopLayer() {
  if (layerStack_.empty()) {
    layerStack_ = {0};
    currentLayerId_ = 0;
    return;
  }
  if (layerStack_.size() > 1) {
    layerStack_.pop_back();
    currentLayerId_ = layerStack_.back();
  }
}

int RenderQueue::GetCurrentLayerId() {
  if (layerStack_.empty()) {
    return 0;
  }
  return layerStack_.back();
}

Rectangle RenderQueue::RegisterComponent(
    ComponentType type,
    std::function<void(Rectangle)> renderFunc,
    int layerId,
    bool consumesInput) {
  
  // Allocate space in the layout system
  // This returns bounds from the PREVIOUS frame
  Rectangle bounds = Layout::Alloc(Layout::Flex(0));
  
  // Capture everything needed for rendering
  RenderCommand cmd;
  cmd.type = type;
  cmd.bounds = bounds;
  cmd.layerId = (layerId == 0) ? currentLayerId_ : layerId;
  cmd.zOrder = cmd.layerId;
  cmd.consumesInput = consumesInput;
  cmd.registrationOrder = (unsigned long long)registrationCounter_++;
  cmd.clipRect = GetCurrentClipRect();
  cmd.culled = (cmd.bounds.width <= 0 || cmd.bounds.height <= 0 ||
                cmd.clipRect.width <= 0 || cmd.clipRect.height <= 0 ||
                !CheckCollisionRecs(cmd.bounds, cmd.clipRect));
  cmd.renderFunc = [renderFunc, bounds]() {
    renderFunc(bounds);
  };
  
  renderQueue_.push_back(cmd);
  
  return bounds;
}

void RenderQueue::BuildInputBlockingMap() {
  inputBlockingLayers_.clear();
  inputBlockingLayers_.resize(renderQueue_.size(), -1);
  
  Vector2 mousePos = GetMousePosition();
  
  // Find the topmost layer under the mouse that consumes input
  int topmostLayerUnderMouse = -1;
  
  for (size_t i = 0; i < renderQueue_.size(); i++) {
    const auto& cmd = renderQueue_[i];
    if (!cmd.consumesInput || cmd.culled) {
      continue;
    }
    if (!CheckCollisionPointRec(mousePos, cmd.bounds)) {
      continue;
    }
    if (!CheckCollisionPointRec(mousePos, cmd.clipRect)) {
      continue;
    }
    if (cmd.consumesInput) {
      if (topmostLayerUnderMouse == -1 || cmd.zOrder > topmostLayerUnderMouse) {
        topmostLayerUnderMouse = cmd.zOrder;
      }
    }
  }
  
  // Now mark which components should receive input
  for (size_t i = 0; i < renderQueue_.size(); i++) {
    const auto& cmd = renderQueue_[i];
    if (topmostLayerUnderMouse != -1 && cmd.zOrder < topmostLayerUnderMouse) {
      // This component is blocked by a higher layer
      inputBlockingLayers_[i] = topmostLayerUnderMouse;
    }
  }
}

bool RenderQueue::ShouldReceiveInput(Rectangle bounds, Rectangle clipRect,
                                     int layerId) {
  Vector2 mousePos = GetMousePosition();
  if (!CheckCollisionPointRec(mousePos, bounds) ||
      !CheckCollisionPointRec(mousePos, clipRect)) {
    return false;
  }

  for (const auto& cmd : renderQueue_) {
    if (!cmd.consumesInput || cmd.culled) {
      continue;
    }
    if (cmd.zOrder <= layerId) {
      continue;
    }
    if (!CheckCollisionPointRec(mousePos, cmd.bounds) ||
        !CheckCollisionPointRec(mousePos, cmd.clipRect)) {
      continue;
    }
    return false;
  }

  return true;
}

void RenderQueue::ExecuteRenderQueue() {
  // Build input blocking map first
  BuildInputBlockingMap();
  
  // Sort by Z-order (back to front)
  // Lower zOrder = rendered first (in back)
  // Higher zOrder = rendered last (in front)
  std::stable_sort(renderQueue_.begin(), renderQueue_.end(),
    [](const RenderCommand& a, const RenderCommand& b) {
      if (a.zOrder != b.zOrder) {
        return a.zOrder < b.zOrder;
      }
      // Same layer - maintain registration order
      return a.registrationOrder < b.registrationOrder;
    });
  
  // Execute render commands in order with per-command clip
  for (auto& cmd : renderQueue_) {
    if (cmd.culled) {
      continue;
    }
    ApplyClipRectToGpu(cmd.clipRect);
    cmd.renderFunc();
    ApplyClipRectToGpu(GetCurrentClipRect());
  }
}

} // namespace raym3

#endif // RAYM3_USE_INPUT_LAYERS

