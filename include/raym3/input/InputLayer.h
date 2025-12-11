#pragma once

#ifndef RAYM3_USE_INPUT_LAYERS
#define RAYM3_USE_INPUT_LAYERS 0
#endif

#include <raylib.h>
#include <vector>

namespace raym3 {

#if RAYM3_USE_INPUT_LAYERS

struct BlockingRegion {
  Rectangle bounds;
  int layerId;
  int registrationOrder;
  bool blocksInput;
};

struct InputCapture {
  Rectangle captureBounds;
  int captureLayerId;
  bool isActive;
  Vector2 captureStartPos;
};

class InputLayerManager {
public:
  static void Initialize();
  static void BeginFrame();
  static void EndFrame();

  // Layer management
  static int PushLayer(int zOrder = 0);
  static void PopLayer();
  static int GetCurrentLayerId();
  static int GetHighestLayerId();

  // Blocking region registration
  static void RegisterBlockingRegion(Rectangle bounds, bool blocksInput = true);

  // Input capture (for drags/scrolls)
  // layerId: The layer ID that the component is on. If -1, uses currentLayerId_
  static bool BeginInputCapture(Rectangle bounds,
                                bool requireStartInBounds = true,
                                int layerId = -1);
  static bool IsInputCaptured();
  static bool IsInputCapturedBy(Rectangle bounds, int layerId = -1);
  static void ReleaseCapture();

  // Basic input check
  // layerId: The layer ID to check from. If -1, uses currentLayerId_
  static bool ShouldProcessMouseInput(Rectangle bounds, int layerId = -1);

  // Check if there's a blocking region above a specific layer at mouse position
  static bool IsBlockedByHigherLayer(int layerId, Vector2 mousePos);

  // Input consumption
  static void ConsumeInput();

private:
  static std::vector<BlockingRegion> blockingRegions_;
  static std::vector<BlockingRegion>
      activeBlockingRegions_; // For double-buffering
  static int currentLayerId_;
  static std::vector<int> layerStack_;
  static int registrationOrder_;
  static InputCapture currentCapture_;
};

#else // RAYM3_USE_INPUT_LAYERS == 0

// Stub implementation - always allows input
class InputLayerManager {
public:
  static void Initialize() {}
  static void BeginFrame() {}
  static void EndFrame() {}
  static int PushLayer(int = 0) { return 0; }
  static void PopLayer() {}
  static int GetCurrentLayerId() { return 0; }
  static int GetHighestLayerId() { return 0; }
  static void RegisterBlockingRegion(Rectangle, bool = true) {}
  static bool BeginInputCapture(Rectangle bounds, bool = true) {
    return CheckCollisionPointRec(GetMousePosition(), bounds);
  }
  static bool IsInputCaptured() { return false; }
  static bool IsInputCapturedBy(Rectangle) { return false; }
  static void ReleaseCapture() {}
  static bool ShouldProcessMouseInput(Rectangle bounds, int = -1) {
    return CheckCollisionPointRec(GetMousePosition(), bounds);
  }
  static bool IsBlockedByHigherLayer(int, Vector2) { return false; }
  static void ConsumeInput() {}
};

#endif // RAYM3_USE_INPUT_LAYERS

} // namespace raym3
