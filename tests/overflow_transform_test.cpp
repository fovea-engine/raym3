#include "raym3/raym3.h"
#include "raym3/v2/Renderer.h"

#include <raylib.h>
#include <cstdlib>
#include <iostream>

using namespace raym3::v2;

namespace {

bool IsColor(Color actual, Color expected) {
  return actual.r == expected.r && actual.g == expected.g &&
         actual.b == expected.b && actual.a == expected.a;
}

} // namespace

int main() {
  constexpr int width = 320;
  constexpr int height = 280;
  const Color red = {240, 30, 30, 255};
  const Color green = {20, 180, 80, 255};

  SetTraceLogLevel(LOG_NONE);
  InitWindow(width, height, "RayM3 transformed overflow test");
  if (!IsWindowReady()) {
    std::cerr << "could not initialize an OpenGL context for overflow test\n";
    return 2;
  }
  raym3::Initialize();

  Style parentStyle;
  parentStyle.position = PositionType::Absolute;
  parentStyle.inset.left = 120.0f;
  parentStyle.inset.top = 100.0f;
  parentStyle.width = 120.0f;
  parentStyle.height = 100.0f;
  parentStyle.rotation = 30.0f;
  parentStyle.scale = 0.75f;
  parentStyle.borderRadius = 20.0f;
  parentStyle.overflow = Overflow::Hidden;
  parentStyle.backgroundColor = green;

  Style innerStyle;
  innerStyle.position = PositionType::Absolute;
  innerStyle.inset.left = 30.0f;
  innerStyle.inset.top = 25.0f;
  innerStyle.width = 60.0f;
  innerStyle.height = 50.0f;
  innerStyle.rotation = -20.0f;
  innerStyle.scale = 1.1f;
  innerStyle.borderRadius = 14.0f;
  innerStyle.overflow = Overflow::Hidden;
  innerStyle.backgroundColor = BLUE;

  Style childStyle;
  childStyle.position = PositionType::Absolute;
  childStyle.inset.left = -25.0f;
  childStyle.inset.top = -25.0f;
  childStyle.width = 110.0f;
  childStyle.height = 100.0f;
  childStyle.backgroundColor = red;
  NodePtr child = View({.id = "oversized-child",
                        .style = childStyle,
                        .onPress = [] {}});

  Style rootStyle;
  rootStyle.flexDirection = FlexDirection::Column;
  rootStyle.width = width;
  rootStyle.height = height;
  rootStyle.backgroundColor = BLACK;
  NodePtr root = View({.id = "root", .style = rootStyle},
                      {View({.id = "rotated-clip", .style = parentStyle},
                            {View({.id = "scaled-inner-clip", .style = innerStyle},
                                  {child})})});

  BeginDrawing();
  ClearBackground(BLACK);
  raym3::BeginFrame();
  Render(root, {0, 0, (float)width, (float)height});
  raym3::EndFrame();

  // Preserve an inspectable capture using Raylib's built-in screenshot path.
  const char *capture = std::getenv("RAYM3_OVERFLOW_SCREENSHOT");
  // Raylib's TakeScreenshot accepts a filename, then resolves it under its
  // storage base path; an absolute path is treated as a nested filename.
  TakeScreenshot(capture ? capture : "raym3-v2-overflow-transform.png");

  Image frame = LoadImageFromScreen();
  const Color center = GetImageColor(frame, 180, 150);
  // With the outer parent's 0.75 scale and 30-degree rotation, this point is
  // inside its transformed AABB but outside its rounded shape.
  const Color transformedCorner = GetImageColor(frame, 123, 96);
  // This point is inside the outer parent but outside the nested inner clip.
  const Color nestedClipEdge = GetImageColor(frame, 145, 150);
  UnloadImage(frame);

  // Check that clipped rounded corners reject descendant input, while a point
  // inside the visible portion of the same oversized child remains interactive.
  Style inputParentStyle;
  inputParentStyle.position = PositionType::Absolute;
  inputParentStyle.inset.left = 60.0f;
  inputParentStyle.inset.top = 60.0f;
  inputParentStyle.width = 120.0f;
  inputParentStyle.height = 100.0f;
  inputParentStyle.scale = 0.75f;
  inputParentStyle.borderRadius = 20.0f;
  inputParentStyle.overflow = Overflow::Hidden;
  Style inputChildStyle;
  inputChildStyle.position = PositionType::Absolute;
  inputChildStyle.inset.left = -20.0f;
  inputChildStyle.inset.top = -20.0f;
  inputChildStyle.width = 80.0f;
  inputChildStyle.height = 80.0f;
  NodePtr inputChild = View({.id = "input-child",
                             .style = inputChildStyle,
                             .onPress = [] {}});
  NodePtr inputRoot = View({.id = "input-root", .style = rootStyle},
                           {View({.id = "input-rounded-parent",
                                  .style = inputParentStyle},
                                 {inputChild})});
  Render(inputRoot, {0, 0, (float)width, (float)height});
  // These are transformed screen coordinates for local points (61,61) and
  // (90,90), respectively. The corner remains inside the untransformed
  // parent's rectangle, so only the inverse-transformed rounded test rejects it.
  const auto cornerOwner = InputOwnerAt({76, 73});
  const auto interiorOwner = InputOwnerAt({98, 95});
  EndDrawing();

  raym3::Shutdown();
  CloseWindow();

  if (!IsColor(center, red)) {
    std::cerr << "transformed clip hid its child interior: " << (int)center.r
              << "," << (int)center.g << "," << (int)center.b << "\n";
    return 1;
  }
  if (!IsColor(transformedCorner, BLACK)) {
    std::cerr << "rotated rounded clip leaked at its transformed corner: "
              << (int)transformedCorner.r << "," << (int)transformedCorner.g
              << "," << (int)transformedCorner.b << "\n";
    return 1;
  }
  if (!IsColor(nestedClipEdge, green)) {
    std::cerr << "nested transformed clip did not restore its parent: "
              << (int)nestedClipEdge.r << "," << (int)nestedClipEdge.g << ","
              << (int)nestedClipEdge.b << "\n";
    return 1;
  }
  if (cornerOwner == inputChild) {
    std::cerr << "rounded overflow corner still receives child input\n";
    return 1;
  }
  if (interiorOwner != inputChild) {
    std::cerr << "visible rounded overflow interior lost child input\n";
    return 1;
  }
  return 0;
}
