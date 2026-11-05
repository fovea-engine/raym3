#include "raym3/raym3.h"
#include "raym3/rendering/Renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <raylib.h>
#include <string>
#include <vector>

namespace {

using raym3::v2::PointerInput;

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

void TriggerAll(std::array<double, kMotionCount> &starts,
                std::array<int, kMotionCount> &clicks, double now) {
  starts.fill(now);
  for (int &count : clicks)
    ++count;
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

// Returns the two keyframes to blend and the blend amount.
void LoadingBlobFrames(float progress, BlobKeyframe &from, BlobKeyframe &to,
                       float &t) {
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
  from = frames[index];
  to = frames[index + 1];
  t = Smooth(scaled - static_cast<float>(index));
}

// Point on one keyframe's outline. Every frequency is a whole number, so the
// outline closes at theta = 2*pi without a seam.
Vector2 BlobPoint(const BlobKeyframe &f, float theta, float outerRadius) {
  float rotatedTheta = theta - f.rotation;
  float organicLobes = std::round(f.lobes * 0.52f + 1.7f);
  float radius = (outerRadius * f.radiusScale) *
                 (1.0f + f.amplitude * std::cos(f.lobes * rotatedTheta + f.phase) +
                  f.secondaryAmplitude *
                      std::sin(f.secondaryLobes * rotatedTheta + f.phase * 1.7f) +
                  f.wobbleAmplitude *
                      std::sin(organicLobes * rotatedTheta + f.phase * 2.4f));
  return {std::cos(theta) * radius * f.aspectX, std::sin(theta) * radius * f.aspectY};
}

void DrawLoadingBlob(Vector2 center, float progress, float outerRadius,
                     bool contained) {
  BlobKeyframe from, to;
  float t = 0.0f;
  LoadingBlobFrames(progress, from, to, t);
  constexpr int perimeterCount = 120;
  std::vector<Vector2> points;
  points.reserve(perimeterCount);

  // Blend outlines, not lobe counts: a fractional lobe count tears a notch
  // into the shape where the angle wraps around.
  const float scale = contained ? 0.98f : 1.0f;
  for (int i = 0; i < perimeterCount; ++i) {
    float theta = static_cast<float>(i) / static_cast<float>(perimeterCount) *
                  2.0f * PI;
    Vector2 a = BlobPoint(from, theta, outerRadius);
    Vector2 b = BlobPoint(to, theta, outerRadius);
    points.push_back({center.x + (a.x + (b.x - a.x) * t) * scale,
                      center.y + (a.y + (b.y - a.y) * t) * scale});
  }

  const Color color = PlumShape();
  for (int i = 0; i < perimeterCount; ++i) {
    const Vector2 &a = points[i];
    const Vector2 &b = points[(i + 1) % perimeterCount];
    DrawTriangle(center, b, a, color); // CCW on screen, or raylib culls it
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

void DrawShapeMorph(Rectangle bounds, float progress, bool morphed) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Shape morph", "click to toggle", progress > 0.0f && progress < 1.0f);
  // Each click flips the target shape; progress eases from the old one.
  float e = Smooth(progress);
  float t = morphed ? e : 1.0f - e;
  float pop = std::sin(e * PI) * 0.06f;
  // Pill button -> wider square-cornered button.
  float w = 124.0f + 56.0f * t, h = 48.0f + 8.0f * t;
  Rectangle button = {bounds.x + bounds.width * 0.5f - w * 0.5f * (1.0f + pop),
                      bounds.y + 96.0f - h * 0.5f * (1.0f + pop),
                      w * (1.0f + pop), h * (1.0f + pop)};
  raym3::Renderer::DrawRoundedRectangle(button, 24.0f + (8.0f - 24.0f) * t,
                                        scheme.primaryContainer);
  raym3::Renderer::DrawTextCentered(t > 0.5f ? "Selected" : "Action", button, 14.0f,
                                    scheme.onPrimaryContainer,
                                    raym3::FontWeight::Medium);
  // FAB: rounded square -> circle while the plus turns into a close mark.
  const float size = 56.0f * (1.0f + pop);
  const Vector2 c = {bounds.x + bounds.width - 60.0f, bounds.y + 96.0f};
  Rectangle fab = {c.x - size * 0.5f, c.y - size * 0.5f, size, size};
  raym3::Renderer::DrawRoundedRectangle(fab, 16.0f + (size * 0.5f - 16.0f) * t,
                                        scheme.tertiaryContainer);
  const float angle = 45.0f * t;
  for (float bar : {0.0f, 90.0f}) {
    DrawRectanglePro({c.x, c.y, 20.0f, 2.5f}, {10.0f, 1.25f}, angle + bar,
                     scheme.onTertiaryContainer);
  }
}

void DrawSheetEnter(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Sheet enter", "click to replay", progress > 0.0f && progress < 1.0f);
  Rectangle stage = {bounds.x + 18.0f, bounds.y + 62.0f, bounds.width - 36.0f,
                     bounds.height - 70.0f};
  raym3::Renderer::DrawRoundedRectangle(stage, 8.0f,
                                        scheme.surfaceContainer);
  float enter = Smooth(progress);
  constexpr float sheetHeight = 66.0f;
  float y = stage.y + stage.height - sheetHeight * enter;
  Rectangle sheet = {stage.x + 22.0f, y, stage.width - 44.0f, sheetHeight};
  BeginScissorMode(static_cast<int>(stage.x), static_cast<int>(stage.y),
                   static_cast<int>(stage.width), static_cast<int>(stage.height));
  raym3::Renderer::DrawRoundedRectangle(sheet, 24.0f,
                                        scheme.surfaceContainerHigh);
  raym3::Renderer::DrawRoundedRectangle({sheet.x + sheet.width * 0.5f - 18.0f,
                                         sheet.y + 10.0f, 36.0f, 4.0f},
                                        2.0f, scheme.outline);
  raym3::Renderer::DrawText("Bottom sheet", {sheet.x + 18.0f, sheet.y + 28.0f},
                            13.0f, scheme.onSurface,
                            raym3::FontWeight::Medium);
  EndScissorMode();
}

void DrawCarousel(Rectangle bounds, float position) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Carousel", "click to scroll", false);
  Rectangle clip = {bounds.x + 14.0f, bounds.y + 64.0f, bounds.width - 28.0f, 74.0f};
  BeginScissorMode(static_cast<int>(clip.x), static_cast<int>(clip.y),
                   static_cast<int>(clip.width), static_cast<int>(clip.height));
  // M3 multi-browse keylines: slot -1 is off the left edge, 4 off the right.
  constexpr float gap = 8.0f, small = 40.0f;
  const float large = (clip.width - small - gap * 3.0f) / 2.5f;
  const float widths[6] = {small, large, large, large * 0.5f, small, small};
  float lefts[6];
  lefts[0] = -small - gap;
  for (int k = 1; k < 6; ++k)
    lefts[k] = k == 1 ? 0.0f : lefts[k - 1] + widths[k - 1] + gap;
  constexpr int count = 8;
  for (int i = 0; i < count; ++i) {
    float slot = std::fmod(i - position + count * 64.0f, count) - 1.0f; // [-1, 7)
    if (slot >= 4.0f)
      continue;
    int k = static_cast<int>(std::floor(slot)) + 1;
    float f = slot + 1.0f - static_cast<float>(k);
    float w = widths[k] + (widths[k + 1] - widths[k]) * f;
    float x = lefts[k] + (lefts[k + 1] - lefts[k]) * f;
    Rectangle card = {clip.x + x, clip.y + 2.0f, w, 70.0f};
    bool primary = i % 2 == 0;
    raym3::Renderer::DrawRoundedRectangle(card, std::min(18.0f, w * 0.5f),
                                          primary ? scheme.primaryContainer
                                                  : scheme.tertiaryContainer);
    if (w > 72.0f) {
      raym3::Renderer::DrawTextCentered(TextFormat("Item %d", i + 1), card, 13.0f,
                                        primary ? scheme.onPrimaryContainer
                                                : scheme.onTertiaryContainer,
                                        raym3::FontWeight::Medium);
    }
  }
  EndScissorMode();
}

void DrawCheckboxMotion(Rectangle bounds, float progress) {
  const auto &scheme = raym3::Theme::GetColorScheme();
  DrawPanel(bounds, "Checkbox selection", "click to replay", progress > 0.0f && progress < 1.0f);
  float t = Smooth(progress);
  Vector2 center = {bounds.x + bounds.width * 0.5f, bounds.y + 108.0f};
  DrawCircleV(center, 34.0f * t, WithAlpha(scheme.primary, 0.12f * (1.0f - t)));
  Rectangle box = {center.x - 16.0f, center.y - 16.0f, 32.0f, 32.0f};
  if (t > 0.45f) {
    raym3::Renderer::DrawRoundedRectangle(box, 4.0f, scheme.primary);
    Vector2 a = {box.x + 7.0f, box.y + 16.0f};
    Vector2 b = {box.x + 13.0f, box.y + 22.0f};
    Vector2 c = {box.x + 26.0f, box.y + 9.0f};
    float checkAlpha = Smooth((t - 0.45f) / 0.55f);
    DrawLineEx(a, b, 4.0f, WithAlpha(scheme.onPrimary, checkAlpha));
    DrawLineEx(b, c, 4.0f, WithAlpha(scheme.onPrimary, checkAlpha));
  } else {
    raym3::Renderer::DrawRoundedRectangleEx(box, 4.0f, scheme.outline, 2.0f);
  }
}

// Scripted pointer for `--autoplay` (README recording): double-click "word",
// drag to extend by words, then click to collapse. Same path as a real mouse.
PointerInput ScriptedPointer(int frame) {
  frame -= 120; // leave time for a screen recorder to attach
  PointerInput p;
  p.pos = {470.0f, 134.0f};
  p.pressed = frame == 60 || frame == 66 || frame == 200;
  p.released = frame == 61 || frame == 120 || frame == 201;
  p.down = p.pressed || (frame > 66 && frame < 120);
  if (frame > 70 && frame <= 120)
    p.pos.x += std::min(frame - 70, 30) * 4.0f;
  if (frame >= 200)
    p.pos.x = 440.0f;
  return p;
}

} // namespace

int main(int argc, char **argv) {
  const bool autoplay = argc > 1 && std::string(argv[1]) == "--autoplay";
  if (autoplay)
    SetConfigFlags(FLAG_WINDOW_UNFOCUSED | FLAG_WINDOW_TOPMOST); // visible, but never steals the keyboard
  InitWindow(1200, 900, "raym3 v2 component and motion demo");
  SetTargetFPS(60);

  raym3::Initialize();

  using namespace raym3::v2;

  std::string value = "Double-click a word to select it";
  std::array<double, kMotionCount> animationStarts;
  animationStarts.fill(0.0);
  std::array<int, kMotionCount> clicks;
  clicks.fill(1); // launch plays every motion once

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

  Style actionStyle;
  actionStyle.width = 160.0f;
  actionStyle.height = 40.0f;
  actionStyle.borderRadius = 20.0f;
  actionStyle.backgroundColor = raym3::Theme::GetColorScheme().primary;
  actionStyle.text.color = raym3::Theme::GetColorScheme().onPrimary;

  Style inputStyle;
  inputStyle.width = 390.0f;
  inputStyle.height = 56.0f;

  // Retained tree: built once, like a DOM. Focus, selection and click counts
  // live on the nodes, so rebuilding every frame would forget them.
  auto root = View({.id = "root", .style = rootStyle},
                   {
                       Text("raym3 v2  |  Components, text, and motion", {.style = titleStyle}),
                       Text("Badge · selected chip · TextInput: double-click a word, then drag · click panels to replay",
                            {.style = captionStyle}),
                       View({.id = "controls", .style = controlsStyle},
                            {
                                Button({.label = "Action",
                                        .style = actionStyle,
                                        .onPress = [&animationStarts, &clicks]() {
                                          TriggerAll(animationStarts, clicks, GetTime());
                                        }}),
                                MaterialComponent(M3Component::Badge, {.label = "2"}),
                                MaterialComponent(M3Component::Chip,
                                                  {.label = "Selected chip",
                                                   .selected = true}),
                                TextInput({.id = "showcase-text-input",
                                           .style = inputStyle,
                                           .value = &value,
                                           .placeholder = "Text input",
                                           .selectTextOnFocus = true,
                                           .autoFocus = true}),
                            }),
                       Custom({.id = "motion-gallery",
                               .style = {.height = 480.0f}},
                              [&animationStarts, &clicks](Rectangle bounds) {
                                const float gap = 14.0f;
                                const float panelW = (bounds.width - gap) * 0.5f;
                                const float panelH = (bounds.height - gap * 2.0f) / 3.0f;
                                const double time = GetTime();
                                Rectangle p[6];
                                for (int i = 0; i < 6; ++i) {
                                  p[i] = {bounds.x + (i % 2) * (panelW + gap),
                                          bounds.y + (i / 2) * (panelH + gap),
                                          panelW, panelH};
                                }
                                if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                                  Vector2 mouse = GetMousePosition();
                                  for (int i = 0; i < kMotionCount; ++i) {
                                    if (CheckCollisionPointRec(mouse, p[i])) {
                                      animationStarts[i] = time;
                                      ++clicks[i];
                                    }
                                  }
                                }
                                DrawLoading(p[0], LoadingProgressFor(animationStarts[0], time));
                                DrawActiveIndicator(p[1], ProgressFor(animationStarts[1], time));
                                DrawShapeMorph(p[2], ProgressFor(animationStarts[2], time),
                                               clicks[2] % 2 == 1);
                                DrawSheetEnter(p[3], ProgressFor(animationStarts[3], time));
                                DrawCarousel(p[4], clicks[4] - 1 +
                                                       Smooth(ProgressFor(animationStarts[4], time)));
                                DrawCheckboxMotion(p[5], ProgressFor(animationStarts[5], time));
                              }),
                   });

  for (int frame = 0; !WindowShouldClose(); ++frame) {
    const Rectangle screen = {0, 0, static_cast<float>(GetScreenWidth()),
                              static_cast<float>(GetScreenHeight())};
    if (autoplay) {
      const PointerInput p = ScriptedPointer(frame);
      BeginInputFrame(p.pos, p.down, p.pressed, p.released, 0.0f);
      // After the text demo, play one panel every 1.5 s.
      if (frame >= 360 && (frame - 360) % 90 == 0) {
        const int panel = ((frame - 360) / 90) % kMotionCount;
        animationStarts[panel] = GetTime();
        ++clicks[panel];
      }
    } else {
      BeginInputFrame(GetMousePosition(), IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                      IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                      IsMouseButtonReleased(MOUSE_BUTTON_LEFT), GetMouseWheelMove());
    }

    BeginDrawing();
    ClearBackground(raym3::Theme::GetColorScheme().surface);
    raym3::BeginFrame();
    Render(root, screen);
    raym3::EndFrame();
    EndDrawing();

    // Same order a browser uses: hit-test the painted frame, then dispatch.
    ResolveInput(root);
    ResolveTextInput(root);
    ResolveScrollInput(root);
  }

  raym3::Shutdown();
  CloseWindow();
  return 0;
}
