#include "raym3/input/InputLayer.h"

#if RAYM3_USE_INPUT_LAYERS

#include <algorithm>

namespace raym3 {

std::vector<BlockingRegion> InputLayerManager::blockingRegions_;
std::vector<BlockingRegion> InputLayerManager::activeBlockingRegions_;
int InputLayerManager::currentLayerId_ = 0;
std::vector<int> InputLayerManager::layerStack_ = {0};
int InputLayerManager::registrationOrder_ = 0;
InputCapture InputLayerManager::currentCapture_ = {
    .captureBounds = {0, 0, 0, 0},
    .captureLayerId = -1,
    .isActive = false,
    .captureStartPos = {0, 0}};

void InputLayerManager::Initialize() {
  blockingRegions_.clear();
  activeBlockingRegions_.clear();
  currentLayerId_ = 0;
  layerStack_ = {0};
  registrationOrder_ = 0;
  currentCapture_.isActive = false;
}

void InputLayerManager::BeginFrame() {
  // Move current frame's regions to active for this frame's queries
  // (Double-buffering so Update() sees regions registered during last frame's
  // Render())
  activeBlockingRegions_ = std::move(blockingRegions_);
  blockingRegions_.clear();
  registrationOrder_ = 0;

  // Reset layer stack to base layer
  layerStack_ = {0};
  currentLayerId_ = 0;

  // Check if capture should be released (mouse button up)
  if (currentCapture_.isActive) {
    // Fix: Use !IsMouseButtonDown instead of IsMouseButtonReleased to prevent
    // stuck capture if the release frame was missed (e.g. lag, focus loss)
    bool leftUp = !IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool rightUp = !IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    bool middleUp = !IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

    if (leftUp && rightUp && middleUp) {
      currentCapture_.isActive = false;
    }
  }
}

void InputLayerManager::EndFrame() {
  // Cleanup happens automatically at start of next frame
}

int InputLayerManager::PushLayer(int zOrder) {
  // Defensive: Ensure layer stack is initialized
  if (layerStack_.empty()) {
    layerStack_ = {0};
    currentLayerId_ = 0;
  }

  // If zOrder is specified, use it directly
  // Otherwise, just increment from current layer
  if (zOrder > 0) {
    currentLayerId_ = zOrder;
  } else {
    currentLayerId_++;
  }
  layerStack_.push_back(currentLayerId_);
  return currentLayerId_;
}

void InputLayerManager::PopLayer() {
  // Defensive: Ensure layer stack is never empty
  if (layerStack_.empty()) {
    layerStack_ = {0};
    currentLayerId_ = 0;
    return;
  }

  if (layerStack_.size() > 1) {
    layerStack_.pop_back();
    currentLayerId_ = layerStack_.back();
  } else {
    // Don't pop the base layer, just reset to ensure consistency
    layerStack_ = {0};
    currentLayerId_ = 0;
  }
}

int InputLayerManager::GetCurrentLayerId() {
  if (layerStack_.empty()) {
    return 0;
  }
  return layerStack_.back();
}

int InputLayerManager::GetHighestLayerId() {
  int highest = 0;

  // Check all active blocking regions to find the highest layer ID
  for (const auto &region : activeBlockingRegions_) {
    if (region.layerId > highest) {
      highest = region.layerId;
    }
  }

  // Also check current layer stack (layers that have been pushed)
  if (!layerStack_.empty()) {
    for (int layerId : layerStack_) {
      if (layerId > highest) {
        highest = layerId;
      }
    }
  }

  // The currentLayerId_ is the highest ID that has been assigned
  // (even if not yet used in blocking regions)
  if (currentLayerId_ > highest) {
    highest = currentLayerId_;
  }

  return highest;
}

void InputLayerManager::RegisterBlockingRegion(Rectangle bounds,
                                               bool blocksInput) {
  if (layerStack_.empty()) {
    layerStack_ = {0};
    currentLayerId_ = 0;
  }
  BlockingRegion region;
  region.bounds = bounds;
  region.layerId = GetCurrentLayerId();
  region.registrationOrder = registrationOrder_++;
  region.blocksInput = blocksInput;
  blockingRegions_.push_back(region);
}

bool InputLayerManager::BeginInputCapture(Rectangle bounds,
                                          bool requireStartInBounds,
                                          int layerId) {
  Vector2 mousePos = GetMousePosition();
  bool mouseInBounds = CheckCollisionPointRec(mousePos, bounds);

  // Use provided layerId or fall back to current layer
  int componentLayerId = (layerId >= 0) ? layerId : currentLayerId_;

  // If already captured by someone else, deny
  if (currentCapture_.isActive &&
      !IsInputCapturedBy(bounds, componentLayerId)) {
    return false;
  }

  // If not captured yet, check if we should capture on this frame
  if (!currentCapture_.isActive) {
    bool anyMouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT) ||
                        IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ||
                        IsMouseButtonDown(MOUSE_BUTTON_MIDDLE);

    if (anyMouseDown) {
      if (requireStartInBounds) {
        // Only capture if mouse was pressed IN bounds AND on the correct layer
        bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
                            IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) ||
                            IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE);

        if (mousePressed && mouseInBounds) {
          // Check if blocked by a higher layer
          // Temporarily set layer context to component's layer for validation
          int savedLayerId = currentLayerId_;
          currentLayerId_ = componentLayerId;
          bool canProcess = ShouldProcessMouseInput(bounds);
          currentLayerId_ = savedLayerId;

          if (!canProcess) {
            return false;
          }

          // Capture! Store the component's layer ID
          // Note: componentLayerId might differ from currentLayerId_ if View3D
          // has already popped its layer This is fine - we store the layer ID
          // that the component is on
          currentCapture_.captureBounds = bounds;
          currentCapture_.captureLayerId = componentLayerId;
          currentCapture_.isActive = true;
          currentCapture_.captureStartPos = mousePos;
          return true;
        }

        return false; // Mouse down but not in bounds
      } else {
        // Always allow if in bounds (legacy mode)
        int savedLayerId = currentLayerId_;
        currentLayerId_ = componentLayerId;
        bool canProcess = mouseInBounds && ShouldProcessMouseInput(bounds);
        currentLayerId_ = savedLayerId;

        if (canProcess) {
          currentCapture_.captureBounds = bounds;
          currentCapture_.captureLayerId = componentLayerId;
          currentCapture_.isActive = true;
          currentCapture_.captureStartPos = mousePos;
          return true;
        }
        return false;
      }
    }

    // No mouse down, allow passive input (hover, scroll wheel, etc.)
    int savedLayerId = currentLayerId_;
    currentLayerId_ = componentLayerId;
    bool canProcess = mouseInBounds && ShouldProcessMouseInput(bounds);
    currentLayerId_ = savedLayerId;
    return canProcess;
  }

  // Already captured by us, verify it's still valid
  if (currentCapture_.captureLayerId == componentLayerId) {
    return true;
  }

  // Capture is for a different layer, deny
  return false;
}

bool InputLayerManager::IsInputCaptured() { return currentCapture_.isActive; }

bool InputLayerManager::IsInputCapturedBy(Rectangle bounds, int layerId) {
  if (!currentCapture_.isActive)
    return false;

  // Use provided layerId or fall back to current layer
  int componentLayerId = (layerId >= 0) ? layerId : currentLayerId_;

  return currentCapture_.captureBounds.x == bounds.x &&
         currentCapture_.captureBounds.y == bounds.y &&
         currentCapture_.captureBounds.width == bounds.width &&
         currentCapture_.captureBounds.height == bounds.height &&
         currentCapture_.captureLayerId == componentLayerId;
}

void InputLayerManager::ReleaseCapture() { currentCapture_.isActive = false; }

bool InputLayerManager::ShouldProcessMouseInput(Rectangle bounds, int layerId) {
  Vector2 mousePos = GetMousePosition();

  if (!CheckCollisionPointRec(mousePos, bounds)) {
    return false;
  }

  int askingLayerId = (layerId >= 0) ? layerId : currentLayerId_;

  // Check active blocking regions (from last frame)
  for (const auto &region : activeBlockingRegions_) {
    if (!region.blocksInput)
      continue;
    if (!CheckCollisionPointRec(mousePos, region.bounds))
      continue;

    if (region.layerId > askingLayerId) {
      return false;
    }
  }

  return true;
}

bool InputLayerManager::IsBlockedByHigherLayer(int layerId, Vector2 mousePos) {
  // Check all active blocking regions
  for (const auto &region : activeBlockingRegions_) {
    if (!region.blocksInput)
      continue;
    if (!CheckCollisionPointRec(mousePos, region.bounds))
      continue;

    // Is this blocking region above the specified layer?
    if (region.layerId > layerId) {
      return true;
    }
  }

  return false;
}

void InputLayerManager::ConsumeInput() {
  // Mark that input was consumed by the current component
  // This helps with debugging but the blocking regions handle the actual
  // blocking
}

} // namespace raym3

#endif // RAYM3_USE_INPUT_LAYERS
