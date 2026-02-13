#pragma once

#ifndef RAYM3_USE_INPUT_LAYERS
#define RAYM3_USE_INPUT_LAYERS 0
#endif

#include <raylib.h>
#include <functional>
#include <vector>

namespace raym3 {

#if RAYM3_USE_INPUT_LAYERS

enum class ComponentType {
  Button,
  TextField,
  Checkbox,
  Switch,
  Slider,
  Card,
  Text,
  Icon,
  Custom
};

struct RenderCommand {
  std::function<void()> renderFunc;
  Rectangle bounds;
  Rectangle clipRect;
  int layerId;
  int zOrder;
  ComponentType type;
  bool consumesInput;
  unsigned long long registrationOrder;
  bool culled = false;
};

class RenderQueue {
public:
  static void Initialize();
  static void BeginFrame();
  static void ExecuteRenderQueue();
  static void Clear();
  
  // Layer management
  static void PushLayer(int zOrder = 0);
  static void PopLayer();
  static int GetCurrentLayerId();
  
  // Component registration
  static Rectangle RegisterComponent(
      ComponentType type,
      std::function<void(Rectangle)> renderFunc,
      int layerId = 0,
      bool consumesInput = true);
  
  // Check if a bounds should receive input
  static bool ShouldReceiveInput(Rectangle bounds, Rectangle clipRect, int layerId);

private:
  static std::vector<RenderCommand> renderQueue_;
  static int currentLayerId_;
  static std::vector<int> layerStack_;
  static int registrationCounter_;
  static int nextComponentId_;
  static std::vector<int> inputBlockingLayers_;
  
  static void BuildInputBlockingMap();
};

#else // RAYM3_USE_INPUT_LAYERS == 0

// Stub - immediate mode (current behavior)
enum class ComponentType {
  Button, TextField, Checkbox, Switch, Slider, Card, Text, Icon, Custom
};

class RenderQueue {
public:
  static void Initialize() {}
  static void BeginFrame() {}
  static void ExecuteRenderQueue() {}
  static void Clear() {}
  static void PushLayer(int = 0) {}
  static void PopLayer() {}
  static int GetCurrentLayerId() { return 0; }
  static Rectangle RegisterComponent(ComponentType, std::function<void(Rectangle)>, int = 0, bool = true) {
    return {0, 0, 0, 0};
  }
  static bool ShouldReceiveInput(Rectangle, int) { return true; }
};

#endif // RAYM3_USE_INPUT_LAYERS

} // namespace raym3

