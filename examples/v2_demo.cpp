#include "raym3/raym3.h"
#include "raym3/rendering/Renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <raylib.h>
#include <string>
#include <vector>

namespace {

constexpr int kMotionCount = 6;
constexpr float kAnimationSeconds = 0.9f;

float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float Smooth(float value) {
  value = Clamp01(value);
  return value * value * (3.0f - 2.0f * value);
}

float ProgressFor(double startTime, double now, float seconds = kAnimationSeconds) {
  if (startTime < 0.0)
    return 0.0f;
  return Clamp01(static_cast<float>((now - startTime) / seconds));
}

float LoadingProgressFor(double startTime, double now) {
  constexpr float idleStarProgress = 1.0f / 7.0f;
  if (startTime < 0.0)
    return idleStarProgress;

  double elapsed = now - startTime;
  if (elapsed >= 4.0)
    return idleStarProgress;

  return Clamp01(static_cast<float>(elapsed / 4.0));
}

void TriggerAll(std::array<double, kMotionCount> &starts, double now) {
  for (double &start : starts) {
    start = now;
  }
}

Color WithAlpha(Color color, float alpha) { return ColorAlpha(color, alpha); }

Color LavenderBackground() { return {246, 240, 249, 255}; }
Color LavenderRing() { return {202, 180, 244, 255}; }
Color PlumShape() { return {93, 73, 133, 255}; }

struct BlobKeyframe {
  float lobes;
  float amplitude;
  float secondaryLobes;
  float secondaryAmplitude;
  float wobbleAmplitude;
  float phase;
  float rotation;
  float aspectX;
  float aspectY;
  float radiusScale;
};

BlobKeyframe MixBlob(const BlobKeyframe &from, const BlobKeyframe &to,
                     float t) {
  return {
      from.lobes + (to.lobes - from.lobes) * t,
      from.amplitude + (to.amplitude - from.amplitude) * t,
      from.secondaryLobes + (to.secondaryLobes - from.secondaryLobes) * t,
      from.secondaryAmplitude +
          (to.secondaryAmplitude - from.secondaryAmplitude) * t,
      from.wobbleAmplitude + (to.wobbleAmplitude - from.wobbleAmplitude) * t,
      from.phase + (to.phase - from.phase) * t,
      from.rotation + (to.rotation - from.rotation) * t,
      from.aspectX + (to.aspectX - from.aspectX) * t,
      from.aspectY + (to.aspectY - from.aspectY) * t,
      from.radiusScale + (to.radiusScale - from.radiusScale) * t,
  };
}

BlobKeyframe LoadingBlobFrame(float progress) {
  static const BlobKeyframe frames[] = {
      // Soft wavy circle.
      {11.0f, 0.145f, 3.0f, 0.025f, 0.015f, 0.20f, -0.12f, 1.02f,
       0.98f, 0.70f},
      // Rounded 10-point expressive star.
      {10.0f, 0.335f, 5.0f, 0.018f, 0.008f, 0.03f, -0.03f, 1.02f,
       1.00f, 0.60f},
      // Tilted rounded pentagon.
      {5.0f, 0.130f, 2.0f, 0.030f, 0.012f, 0.48f, 0.36f, 1.05f,
       0.96f, 0.72f},
      // Horizontal squeezed capsule.
      {2.0f, 0.090f, 4.0f, 0.015f, 0.006f, 0.00f, -0.04f, 1.48f,
       0.72f, 0.68f},
      // Four-lobed organic blob.
      {4.0f, 0.170f, 2.0f, 0.050f, 0.018f, 0.76f, 0.12f, 1.08f,
       1.03f, 0.67f},
      // Vertical oval.
      {2.0f, 0.045f, 4.0f, 0.010f, 0.004f, 1.55f, 0.02f, 0.76f,
       1.48f, 0.68f},
      // Return through the sharper star before looping.
      {10.0f, 0.335f, 5.0f, 0.018f, 0.008f, 0.03f, -0.03f, 1.02f,
       1.00f, 0.60f},
      // Seamless reset to the initial wavy circle.
      {11.0f, 0.145f, 3.0f, 0.025f, 0.015f, 0.20f, -0.12f, 1.02f,
       0.98f, 0.70f},
  };

  progress = Clamp01(progress);
  constexpr int segmentCount =
      static_cast<int>(sizeof(frames) / sizeof(frames[0])) - 1;
  float scaled = progress * static_cast<float>(segmentCount);
  int index = std::min(segmentCount - 1, static_cast<int>(std::floor(scaled)));
  float local = Smooth(scaled - static_cast<float>(index));
  return MixBlob(frames[index], frames[index + 1], local);
}

void DrawLoadingBlob(Vector2 center, float progress, float outerRadius,
                     bool contained) {
  const BlobKeyframe frame = LoadingBlobFrame(progress);
  constexpr int perimeterCount = 120;
  std::vector<Vector2> points;
  points.reserve(perimeterCount);

  for (int i = 0; i < perimeterCount; ++i) {
    float theta = static_cast<float>(i) / static_cast<float>(perimeterCount) *
                  2.0f * PI;
    float rotatedTheta = theta - frame.rotation;
    float primary = std::cos(frame.lobes * rotatedTheta + frame.phase);
    float secondary =
        std::sin(frame.secondaryLobes * rotatedTheta + frame.phase * 1.7f);
    float organic = std::sin((frame.lobes * 0.52f + 1.7f) * rotatedTheta +
                             frame.phase * 2.4f);
    float radius = (outerRadius * frame.radiusScale) *
                   (1.0f + frame.amplitude * primary +
                    frame.secondaryAmplitude * secondary +
                    frame.wobbleAmplitude * organic);

    if (contained) {
      radius *= 0.98f;
    }

    points.push_back({center.x + std::cos(theta) * radius * frame.aspectX,
                      center.y + std::sin(theta) * radius * frame.aspectY});
  }

  const Color color = PlumShape();
  for (int i = 0; i < perimeterCount; ++i) {
    const Vector2 &a = points[i];
    const Vector2 &b = points[(i + 1) % perimeterCount];
    DrawTriangle(center, a, b, color);
  }
}

void DrawPanel(Rectangle bounds, const char *title, const char *subtitle,
               bool active) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  raym3::Renderer::DrawRoundedRectangle(bounds, 8.0f, scheme.surfaceContainerLow);
  raym3::Renderer::DrawRoundedRectangleEx(bounds, 8.0f, scheme.outlineVariant,
                                          1.0f);
  if (active) {
    raym3::Renderer::DrawRoundedRectangleEx(bounds, 8.0f, scheme.primary, 2.0f);
  }
  raym3::Renderer::DrawText(title, {bounds.x + 16.0f, bounds.y + 14.0f}, 14.0f,
                            scheme.onSurface, raym3::FontWeight::Bold);
  raym3::Renderer::DrawText(subtitle, {bounds.x + 16.0f, bounds.y + 34.0f},
                            12.0f, scheme.onSurfaceVariant);
}

void DrawLoading(Rectangle bounds, float progress) {
  DrawPanel(bounds, "Loading", "click to replay 4s morph",
            progress > 0.0f && progress < 1.0f);

  Rectangle stage = {bounds.x + 14.0f, bounds.y + 50.0f, bounds.width - 28.0f,
                     bounds.height - 58.0f};
  raym3::Renderer::DrawRoundedRectangle(stage, 8.0f, LavenderBackground());

  const float outerRadius =
      std::min(stage.height * 0.42f, stage.width * 0.13f);
  Vector2 left = {stage.x + stage.width * 0.27f, stage.y + stage.height * 0.52f};
  Vector2 right = {stage.x + stage.width * 0.72f,
                   stage.y + stage.height * 0.52f};

  DrawCircleV(right, outerRadius, LavenderRing());
  DrawLoadingBlob(left, progress, outerRadius, false);
  DrawLoadingBlob(right, progress, outerRadius, true);
}

void DrawActiveIndicator(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Active indicator", "click to replay", progress > 0.0f && progress < 1.0f);
  Rectangle rail = {bounds.x + 22.0f, bounds.y + 86.0f, bounds.width - 44.0f,
                    48.0f};
  raym3::Renderer::DrawRoundedRectangle(rail, 24.0f, scheme.surfaceContainer);

  float item = rail.width / 4.0f;
  float local = Smooth(progress);
  float overshoot = std::sin(local * PI) * 9.0f;
  float x = rail.x + item * local + overshoot;
  Rectangle indicator = {x + 8.0f, rail.y + 7.0f, item - 16.0f, 34.0f};
  raym3::Renderer::DrawRoundedRectangle(indicator, 17.0f,
                                        scheme.secondaryContainer);

  for (int i = 0; i < 4; ++i) {
    const char *label = i == 0 ? "Home" : i == 1 ? "Search" : i == 2 ? "Work" : "Me";
    Color color = (progress > 0.5f ? i == 1 : i == 0)
                      ? scheme.onSecondaryContainer
                      : scheme.onSurfaceVariant;
    raym3::Renderer::DrawTextCentered(label,
                                      {rail.x + i * item, rail.y, item, rail.height},
                                      12.0f, color, raym3::FontWeight::Medium);
  }
}

void DrawShapeMorph(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Shape morph", "click to replay", progress > 0.0f && progress < 1.0f);
  float t = std::sin(Smooth(progress) * PI);
  float radius = 28.0f + (10.0f - 28.0f) * t;
  float scale = 1.0f + 0.08f * std::sin(t * PI);
  Rectangle button = {bounds.x + bounds.width * 0.5f - 62.0f * scale,
                      bounds.y + 80.0f - 24.0f * scale, 124.0f * scale,
                      48.0f * scale};
  raym3::Renderer::DrawRoundedRectangle(button, radius,
                                        scheme.primaryContainer);
  raym3::Renderer::DrawTextCentered("Action", button, 14.0f,
                                    scheme.onPrimaryContainer,
                                    raym3::FontWeight::Medium);
  DrawCircle(bounds.x + bounds.width - 54.0f, bounds.y + 112.0f,
             20.0f + 8.0f * t, scheme.tertiaryContainer);
  DrawText("+", static_cast<int>(bounds.x + bounds.width - 60.0f),
           static_cast<int>(bounds.y + 100.0f), 24, scheme.onTertiaryContainer);
}

void DrawSheetEnter(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Sheet enter", "click to replay", progress > 0.0f && progress < 1.0f);
  Rectangle stage = {bounds.x + 18.0f, bounds.y + 62.0f, bounds.width - 36.0f,
                     104.0f};
  raym3::Renderer::DrawRoundedRectangle(stage, 8.0f,
                                        scheme.surfaceContainer);
  float enter = Smooth(progress);
  float y = stage.y + stage.height - 68.0f * enter;
  Rectangle sheet = {stage.x + 22.0f, y, stage.width - 44.0f, 78.0f};
  raym3::Renderer::DrawRoundedRectangle(sheet, 24.0f,
                                        scheme.surfaceContainerHigh);
  raym3::Renderer::DrawRoundedRectangle({sheet.x + sheet.width * 0.5f - 18.0f,
                                         sheet.y + 10.0f, 36.0f, 4.0f},
                                        2.0f, scheme.outline);
  raym3::Renderer::DrawText("Bottom sheet", {sheet.x + 18.0f, sheet.y + 28.0f},
                            13.0f, scheme.onSurface,
                            raym3::FontWeight::Medium);
}

void DrawCarousel(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Carousel", "click to replay", progress > 0.0f && progress < 1.0f);
  Rectangle clip = {bounds.x + 14.0f, bounds.y + 68.0f, bounds.width - 28.0f,
                    88.0f};
  BeginScissorMode(static_cast<int>(clip.x), static_cast<int>(clip.y),
                   static_cast<int>(clip.width), static_cast<int>(clip.height));
  float offset = Smooth(progress) * 170.0f;
  for (int i = 0; i < 5; ++i) {
    float w = i % 2 == 0 ? 118.0f : 76.0f;
    float x = clip.x + i * 98.0f - offset;
    while (x + w < clip.x)
      x += 490.0f;
    Rectangle card = {x, clip.y + 4.0f, w, 80.0f};
    Color fill = i % 2 == 0 ? scheme.primaryContainer : scheme.tertiaryContainer;
    raym3::Renderer::DrawRoundedRectangle(card, 18.0f, fill);
    raym3::Renderer::DrawTextCentered(i % 2 == 0 ? "Large" : "Small", card,
                                      13.0f,
                                      i % 2 == 0 ? scheme.onPrimaryContainer
                                                 : scheme.onTertiaryContainer,
                                      raym3::FontWeight::Medium);
  }
  EndScissorMode();
}

void DrawCheckboxMotion(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Checkbox", "click to replay", progress > 0.0f && progress < 1.0f);
  float t = Smooth(progress);
  Vector2 center = {bounds.x + bounds.width * 0.5f, bounds.y + 108.0f};
  DrawCircleV(center, 34.0f * t, WithAlpha(scheme.primary, 0.12f * (1.0f - t)));
  Rectangle box = {center.x - 12.0f, center.y - 12.0f, 24.0f, 24.0f};
  raym3::Renderer::DrawRoundedRectangle(box, 3.0f, scheme.primary);
  Vector2 a = {box.x + 5.0f, box.y + 12.0f};
  Vector2 b = {box.x + 10.0f, box.y + 17.0f};
  Vector2 c = {box.x + 19.0f, box.y + 7.0f};
  DrawLineEx(a, b, 3.0f, scheme.onPrimary);
  if (t > 0.45f) {
    DrawLineEx(b, c, 3.0f, WithAlpha(scheme.onPrimary, Smooth((t - 0.45f) / 0.55f)));
  }
}

} // namespace

int main() {
  InitWindow(920, 680, "raym3 v2 motion demo");
  SetTargetFPS(60);

  raym3::Initialize();

  std::string value = "Editable text";
  std::array<double, kMotionCount> animationStarts;
  animationStarts.fill(-1.0);

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(raym3::Theme::GetColorScheme().surface);
    raym3::BeginFrame();

    using namespace raym3::v2;

    Style rootStyle;
    rootStyle.padding.all = 24.0f;
    rootStyle.gap = 14.0f;
    rootStyle.backgroundColor = raym3::Theme::GetColorScheme().surface;

    Style titleStyle;
    titleStyle.text.fontSize = 24.0f;
    titleStyle.text.lineHeight = 32.0f;
    titleStyle.text.weight = raym3::FontWeight::Bold;

    Style captionStyle;
    captionStyle.text.fontSize = 13.0f;
    captionStyle.text.lineHeight = 18.0f;
    captionStyle.text.color = raym3::Theme::GetColorScheme().onSurfaceVariant;

    Style controlsStyle;
    controlsStyle.flexDirection = FlexDirection::Row;
    controlsStyle.alignItems = Align::Center;
    controlsStyle.gap = 12.0f;
    controlsStyle.height = 64.0f;

    Style inputStyle;
    inputStyle.width = 320.0f;
    inputStyle.height = 56.0f;

    Style badgeStyle;
    badgeStyle.margin.left = 4.0f;

    double now = GetTime();

    auto root = View({.id = "root", .style = rootStyle},
                     {
                         Text("raym3 v2 M3 motion demo", {.style = titleStyle}),
                         Text("Click a panel to replay one animation, or click Action to replay all of them.",
                              {.style = captionStyle}),
                         View({.id = "controls", .style = controlsStyle},
                              {
                                  Button({.label = "Action",
                                          .style = {.width = 160.0f,
                                                    .height = 40.0f},
                                          .onPress = [&animationStarts, now]() {
                                            TriggerAll(animationStarts, now);
                                          }}),
                                  MaterialComponent(M3Component::Badge,
                                                    {.label = "9",
                                                     .style = badgeStyle}),
                                  TextInput({.style = inputStyle,
                                             .value = &value,
                                             .label = "Editable",
                                             .placeholder = "Type here"}),
                              }),
                         Custom({.id = "motion-gallery",
                                 .style = {.height = 480.0f}},
                                [&animationStarts](Rectangle bounds) {
                                  const float gap = 14.0f;
                                  const float panelW =
                                      (bounds.width - gap) * 0.5f;
                                  const float panelH =
                                      (bounds.height - gap * 2.0f) / 3.0f;
                                  const double time = GetTime();
                                  Rectangle p[6];
                                  for (int i = 0; i < 6; ++i) {
                                    p[i] = {bounds.x + (i % 2) * (panelW + gap),
                                            bounds.y +
                                                (i / 2) * (panelH + gap),
                                            panelW, panelH};
                                  }
                                  if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                    Vector2 mouse = GetMousePosition();
                                    for (int i = 0; i < kMotionCount; ++i) {
                                      if (CheckCollisionPointRec(mouse, p[i])) {
                                        animationStarts[i] = time;
                                      }
                                    }
                                  }
                                  DrawLoading(
                                      p[0], LoadingProgressFor(animationStarts[0],
                                                               time));
                                  DrawActiveIndicator(
                                      p[1], ProgressFor(animationStarts[1], time));
                                  DrawShapeMorph(
                                      p[2], ProgressFor(animationStarts[2], time));
                                  DrawSheetEnter(
                                      p[3], ProgressFor(animationStarts[3], time));
                                  DrawCarousel(
                                      p[4], ProgressFor(animationStarts[4], time));
                                  DrawCheckboxMotion(
                                      p[5], ProgressFor(animationStarts[5], time));
                                }),
                     });

    Render(root, {0, 0, static_cast<float>(GetScreenWidth()),
                  static_cast<float>(GetScreenHeight())});

    raym3::EndFrame();
    EndDrawing();
  }

  raym3::Shutdown();
  CloseWindow();
  return 0;
}
