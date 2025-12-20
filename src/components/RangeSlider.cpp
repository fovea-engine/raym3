#include "raym3/components/RangeSlider.h"
#include "raym3/components/Dialog.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

// Static state for immediate mode tracking
static int activeFieldId_ = -1;
static int activeThumbIndex_ = -1;
static int currentFieldId_ = 0;

void RangeSliderComponent::ResetFieldId() { currentFieldId_ = 0; }

std::vector<float>
RangeSliderComponent::Render(Rectangle bounds, const std::vector<float> &values,
                             float min, float max, const char *label,
                             const RangeSliderOptions &options) {

  if (values.empty()) {
    return values;
  }

  ColorScheme &scheme = Theme::GetColorScheme();
  std::vector<float> result = values;

  // M3 Expressive measurements
  float trackHeight = 16.0f;
  float thumbWidth = 4.0f;
  float thumbHeight = 44.0f;

  Rectangle trackBounds = GetTrackBounds(bounds);
  float centerY = trackBounds.y + trackHeight / 2.0f;

  // Calculate thumb positions
  std::vector<float> normalizedValues;
  std::vector<Rectangle> thumbRects;
  for (size_t i = 0; i < result.size(); i++) {
    float normalized = (result[i] - min) / (max - min);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    normalizedValues.push_back(normalized);

    float splitX = trackBounds.x + (trackBounds.width * normalized);
    Rectangle thumbRect = {splitX - thumbWidth / 2.0f,
                           trackBounds.y + (trackHeight - thumbHeight) / 2.0f,
                           thumbWidth, thumbHeight};
    thumbRects.push_back(thumbRect);
  }

  // Check interaction
  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  int fieldId = currentFieldId_++;
  bool isDraggingThis = (activeFieldId_ == fieldId);

  Vector2 mousePos = GetMousePosition();
  Rectangle hitRect = {trackBounds.x, trackBounds.y - 15, trackBounds.width,
                       trackBounds.height + 30};

#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  bool mouseOverHit =
      canProcessInput && CheckCollisionPointRec(mousePos, hitRect);
  bool mouseDown = canProcessInput && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool mousePressed =
      canProcessInput && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  bool mouseReleased =
      canProcessInput && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#else
  bool mouseOverHit = CheckCollisionPointRec(mousePos, hitRect);
  bool mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  bool mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
#endif

  if (!inputBlocked) {
    if (mousePressed && mouseOverHit) {
      activeFieldId_ = fieldId;
      activeThumbIndex_ =
          GetClosestThumbIndex(trackBounds, result, min, max, mousePos);
      isDraggingThis = true;
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
    }

    if (mouseReleased && isDraggingThis) {
      activeFieldId_ = -1;
      activeThumbIndex_ = -1;
      isDraggingThis = false;
    }

    if (isDraggingThis && activeThumbIndex_ >= 0 &&
        activeThumbIndex_ < (int)result.size()) {
      if (mouseDown) {
        float normalized = GetValueFromPosition(trackBounds, mousePos.x);
        float newValue = min + normalized * (max - min);

        // Discrete mode: snap to step
        if (options.stepValue > 0.0f) {
          newValue = std::round((newValue - min) / options.stepValue) *
                         options.stepValue +
                     min;
        }
        newValue = std::clamp(newValue, min, max);

        // Enforce ordering: don't let thumbs cross each other
        float minVal = (activeThumbIndex_ > 0)
                           ? result[activeThumbIndex_ - 1] + options.minDistance
                           : min;
        float maxVal = (activeThumbIndex_ < (int)result.size() - 1)
                           ? result[activeThumbIndex_ + 1] - options.minDistance
                           : max;
        newValue = std::clamp(newValue, minVal, maxVal);

        result[activeThumbIndex_] = newValue;

        // Recalculate for drawing
        float updatedNorm = (newValue - min) / (max - min);
        normalizedValues[activeThumbIndex_] = updatedNorm;
        float splitX = trackBounds.x + (trackBounds.width * updatedNorm);
        thumbRects[activeThumbIndex_].x = splitX - thumbWidth / 2.0f;

#if RAYM3_USE_INPUT_LAYERS
        InputLayerManager::ConsumeInput();
#endif
      } else {
        activeFieldId_ = -1;
        activeThumbIndex_ = -1;
        isDraggingThis = false;
      }
    }
  } else {
    if (isDraggingThis) {
      activeFieldId_ = -1;
      activeThumbIndex_ = -1;
      isDraggingThis = false;
    }
  }

  // Determine colors
  Color activeColor = options.activeTrackColor.a > 0 ? options.activeTrackColor
                                                     : scheme.primary;
  Color inactiveColor = options.inactiveTrackColor.a > 0
                            ? options.inactiveTrackColor
                            : scheme.surfaceVariant;
  Color handleColor =
      options.handleColor.a > 0 ? options.handleColor : scheme.primary;
  float cornerRadius = trackHeight / 2.0f;

  // 1. Draw inactive track (full background)
  Renderer::DrawRoundedRectangle(trackBounds, cornerRadius, inactiveColor);

  // 2. Draw active track (between first and last thumb)
  if (result.size() >= 2) {
    float startNorm = normalizedValues[0];
    float endNorm = normalizedValues[result.size() - 1];
    float startX = trackBounds.x + trackBounds.width * startNorm;
    float endX = trackBounds.x + trackBounds.width * endNorm;

    if (endX > startX) {
      BeginScissorMode((int)startX, (int)trackBounds.y, (int)(endX - startX),
                       (int)trackHeight);
      Renderer::DrawRoundedRectangle(trackBounds, cornerRadius, activeColor);
      EndScissorMode();
    }
  } else if (result.size() == 1) {
    // Single thumb: fill from start to thumb
    float norm = normalizedValues[0];
    if (norm > 0.0f) {
      BeginScissorMode((int)trackBounds.x, (int)trackBounds.y,
                       (int)(trackBounds.width * norm), (int)trackHeight);
      Renderer::DrawRoundedRectangle(trackBounds, cornerRadius, activeColor);
      EndScissorMode();
    }
  }

  // 3. Draw Stop Indicators
  if (options.showStopIndicators) {
    float stopDotRadius = 2.0f;
    float stopInset = 6.0f;
    float minNorm = (result.size() > 0) ? normalizedValues[0] : 1.0f;
    float maxNorm =
        (result.size() > 0) ? normalizedValues[result.size() - 1] : 0.0f;

    if (minNorm > 0.02f) {
      Vector2 startDotPos = {trackBounds.x + stopInset, centerY};
      DrawCircleV(startDotPos, stopDotRadius, activeColor);
    }
    if (maxNorm < 0.98f) {
      Vector2 endDotPos = {trackBounds.x + trackBounds.width - stopInset,
                           centerY};
      DrawCircleV(endDotPos, stopDotRadius, inactiveColor);
    }
  }

  // 4. Draw Tick Marks for discrete mode
  if (options.showTickMarks && options.stepValue > 0.0f) {
    int numSteps = (int)((max - min) / options.stepValue);
    float tickRadius = 1.5f;
    float tickInset = 6.0f; // Inset from track edges
    float tickableWidth = trackBounds.width - (tickInset * 2);
    for (int i = 0; i <= numSteps; i++) {
      float tickNorm = (float)i / (float)numSteps;
      float tickX = trackBounds.x + tickInset + tickableWidth * tickNorm;

      // Skip ticks covered by any thumb
      bool covered = false;
      for (const auto &rect : thumbRects) {
        if (std::abs(tickX - (rect.x + rect.width / 2.0f)) <
            thumbWidth + 4.0f) {
          covered = true;
          break;
        }
      }
      if (covered)
        continue;

      Vector2 tickPos = {tickX, centerY};
      // Determine tick color based on whether it's in active range
      bool inRange = (result.size() >= 2)
                         ? (tickNorm >= normalizedValues[0] &&
                            tickNorm <= normalizedValues[result.size() - 1])
                         : (tickNorm < normalizedValues[0]);
      Color tickColor = inRange ? scheme.onPrimary : activeColor;
      DrawCircleV(tickPos, tickRadius, tickColor);
    }
  }

  // 5. Draw Gap Masks and Thumbs
  float gapSize = 4.0f;
  for (size_t i = 0; i < thumbRects.size(); i++) {
    const auto &thumbRect = thumbRects[i];

    // Gap Mask
    Rectangle maskRect = {thumbRect.x - gapSize, thumbRect.y,
                          thumbRect.width + (gapSize * 2), thumbRect.height};
    DrawRectangleRec(maskRect, scheme.surface);

    // Thumb
    Renderer::DrawRoundedRectangle(thumbRect, thumbWidth / 2.0f, handleColor);
  }

  // 6. Draw Value Indicators
  if (isDraggingThis && options.showValueIndicators && activeThumbIndex_ >= 0 &&
      activeThumbIndex_ < (int)result.size()) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr),
             options.valueFormat ? options.valueFormat : "%.0f",
             result[activeThumbIndex_]);

    float bubbleWidth = 48.0f;
    float bubbleHeight = 32.0f;
    float triangleHeight = 6.0f;
    const auto &thumbRect = thumbRects[activeThumbIndex_];
    float bubbleY = thumbRect.y - bubbleHeight - triangleHeight - 4.0f;
    float bubbleX = thumbRect.x + thumbRect.width / 2.0f - bubbleWidth / 2.0f;

    Rectangle bubbleRect = {bubbleX, bubbleY, bubbleWidth, bubbleHeight};
    Color bubbleColor = scheme.inverseSurface;
    Color valueColor = scheme.inverseOnSurface;

    Renderer::DrawRoundedRectangle(bubbleRect, bubbleHeight / 2.0f,
                                   bubbleColor);

    Vector2 p1 = {bubbleRect.x + bubbleRect.width / 2.0f - 6.0f,
                  bubbleRect.y + bubbleRect.height};
    Vector2 p2 = {bubbleRect.x + bubbleRect.width / 2.0f + 6.0f,
                  bubbleRect.y + bubbleRect.height};
    Vector2 p3 = {bubbleRect.x + bubbleRect.width / 2.0f,
                  bubbleRect.y + bubbleRect.height + triangleHeight};
    DrawTriangle(p1, p2, p3, bubbleColor);
    DrawTriangle(p1, p3, p2, bubbleColor);

    Vector2 textSize =
        Renderer::MeasureText(valueStr, 14.0f, FontWeight::Medium);
    Vector2 textPos = {bubbleRect.x + (bubbleRect.width - textSize.x) / 2.0f,
                       bubbleRect.y + (bubbleRect.height - 14.0f) / 2.0f};
    Renderer::DrawText(valueStr, textPos, 14.0f, valueColor,
                       FontWeight::Medium);
  }

  // Draw label
  if (label) {
    Vector2 labelPos = {bounds.x, bounds.y};
    Renderer::DrawText(label, labelPos, 14.0f, scheme.onSurface,
                       FontWeight::Regular);
  }

  return result;
}

Rectangle RangeSliderComponent::GetTrackBounds(Rectangle bounds) {
  float trackHeight = 16.0f;
  float labelHeight = 20.0f;
  float yOffset = 24.0f;

  return {bounds.x,
          bounds.y + yOffset + (bounds.height - yOffset - trackHeight) / 2.0f,
          bounds.width, trackHeight};
}

float RangeSliderComponent::GetValueFromPosition(Rectangle trackBounds,
                                                 float x) {
  float normalized = (x - trackBounds.x) / trackBounds.width;
  return std::clamp(normalized, 0.0f, 1.0f);
}

int RangeSliderComponent::GetClosestThumbIndex(Rectangle trackBounds,
                                               const std::vector<float> &values,
                                               float min, float max,
                                               Vector2 mousePos) {
  if (values.empty())
    return -1;

  int closestIdx = 0;
  float closestDist = std::numeric_limits<float>::max();

  for (size_t i = 0; i < values.size(); i++) {
    float normalized = (values[i] - min) / (max - min);
    float thumbX = trackBounds.x + trackBounds.width * normalized;
    float dist = std::abs(mousePos.x - thumbX);
    if (dist < closestDist) {
      closestDist = dist;
      closestIdx = (int)i;
    }
  }

  return closestIdx;
}

} // namespace raym3
