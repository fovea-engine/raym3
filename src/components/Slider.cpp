#include "raym3/components/Slider.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Icon.h"
#include "raym3/components/Tooltip.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/ClipScope.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

struct SliderState {
  bool wasFocused = false;
  int lastActiveFrame = -1;
};

static int activeFieldId_ = -1;
static int currentFieldId_ = 0;
static int focusedFieldId_ = -1;
static std::map<int, SliderState> sliderStates_;
static int currentFrame_ = 0;

void SliderComponent::ResetFieldId() { 
  currentFieldId_ = 0;
  currentFrame_++;
  
  for (auto it = sliderStates_.begin(); it != sliderStates_.end();) {
    if (it->second.lastActiveFrame < currentFrame_ - 1) {
      it = sliderStates_.erase(it);
    } else {
      ++it;
    }
  }
}

static ComponentState GetSliderState(Rectangle bounds, Rectangle thumbRect) {
  Vector2 mousePos = GetMousePosition();
  // Check both bounds and specific hit areas
  Rectangle hitRect = {bounds.x, thumbRect.y - 10, bounds.width,
                       thumbRect.height + 20};
#if RAYM3_USE_INPUT_LAYERS
  bool canProcessInput = InputLayerManager::ShouldProcessMouseInput(bounds);
  bool isHovered = canProcessInput && CheckCollisionPointRec(mousePos, hitRect);
#else
  bool isHovered = CheckCollisionPointRec(mousePos, hitRect);
#endif
  bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);

  // We can't easily check if *this* specific slider is dragging here without
  // fieldId But we can check if *any* slider is dragging (activeFieldId_ != -1)
  // Ideally GetState should take fieldId or we rely on the Render loop to set
  // state. For now, let's just rely on isPressed/isHovered and let Render
  // override if dragging.

  if (activeFieldId_ != -1) {
    // If dragging, we might be the one.
    // Render handles the specific override.
    // Here we just return Default if someone else is dragging?
    // Or just standard logic.
  }

  if (isPressed)
    return ComponentState::Pressed;
  if (isHovered)
    return ComponentState::Hovered;
  return ComponentState::Default;
}

float SliderComponent::Render(Rectangle bounds, float value, float min,
                              float max, const char *label) {
  return Render(bounds, value, min, max, label, SliderOptions{});
}

float SliderComponent::Render(Rectangle bounds, float value, float min,
                              float max, const char *label,
                              const SliderOptions &options) {
  ColorScheme &scheme = Theme::GetColorScheme();

  // M3 Expressive measurements
  float trackHeight = 16.0f; // M3 spec: 16dp track
  float thumbWidth = 4.0f;   // User requested hard minimum 4px
  // Make thumbHeight respect bounds.height (clamped between min usable and M3 spec)
  float thumbHeight = std::min(44.0f, std::max(24.0f, bounds.height - 4.0f));

  Rectangle trackBounds = GetTrackBounds(bounds, label != nullptr);

  // Inset Icons Logic (MD3)
  // Icons are inside the track, not outside.
  // We do NOT shrink the track bounds.
  // We render the icon inside the track on the left (startIcon).
  float itemSize = 24.0f;
  float padding = 4.0f; // Closer to track edges
  float centerY = trackBounds.y + trackHeight / 2.0f;

  // Calculate normalized value and splitX
  float normalizedValue = (value - min) / (max - min);
  normalizedValue = std::clamp(normalizedValue, 0.0f, 1.0f);
  float splitX = trackBounds.x + (trackBounds.width * normalizedValue);

  Rectangle thumbRect = {splitX - thumbWidth / 2.0f,
                         trackBounds.y +
                             (trackBounds.height - thumbHeight) / 2.0f,
                         thumbWidth, thumbHeight};

  // Check interaction
  ComponentState state = GetSliderState(bounds, thumbRect);

  bool inputBlocked =
      DialogComponent::IsActive() && !DialogComponent::IsRendering();
  if (inputBlocked) {
    state = ComponentState::Default;
  }

  // Handle input using same logic as before...
  int fieldId = currentFieldId_++;
  bool isDraggingThis = (activeFieldId_ == fieldId);
  bool isFocused = (focusedFieldId_ == fieldId);
  
  SliderState &sliderState = sliderStates_[fieldId];
  sliderState.lastActiveFrame = currentFrame_;

  Vector2 mousePos = GetMousePosition();
  Rectangle hitRect = {trackBounds.x, trackBounds.y - 10, trackBounds.width,
                       trackBounds.height + 20};
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

  if (isDraggingThis && mouseDown) {
    RequestCursor(MOUSE_CURSOR_RESIZE_EW);
  } else if (mouseOverHit) {
    RequestCursor(MOUSE_CURSOR_POINTING_HAND);
  }

  if (mousePressed && mouseOverHit) {
    focusedFieldId_ = fieldId;
    isFocused = true;
  }
  
  // Lose focus when clicking anywhere outside (raw check, bypass input layers)
  if (isFocused && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mousePos, hitRect)) {
    focusedFieldId_ = -1;
    isFocused = false;
  }

  if (!inputBlocked) {
    if (mousePressed && mouseOverHit) {
      activeFieldId_ = fieldId;
      isDraggingThis = true;
#if RAYM3_USE_INPUT_LAYERS
      InputLayerManager::ConsumeInput();
#endif
    }

    if (mouseReleased && isDraggingThis) {
      if (options.onRelease)
        options.onRelease();
      activeFieldId_ = -1;
      isDraggingThis = false;
    }

    if (isDraggingThis) {
      state = ComponentState::Pressed; // Override state while dragging
      if (mouseDown) {
        float normalized = GetValueFromPosition(trackBounds, mousePos.x);
        value = min + normalized * (max - min);
        // Discrete mode: snap to step
        if (options.stepValue > 0.0f) {
          value = std::round((value - min) / options.stepValue) *
                      options.stepValue +
                  min;
        }
        value = std::clamp(value, min, max);
        // Recalculate normalized for drawing
        normalizedValue = (value - min) / (max - min);
        splitX = trackBounds.x + (trackBounds.width * normalizedValue);
        thumbRect.x = splitX - thumbWidth / 2.0f;
#if RAYM3_USE_INPUT_LAYERS
        InputLayerManager::ConsumeInput();
#endif
      } else {
        activeFieldId_ = -1;
        isDraggingThis = false;
      }
    }
  } else {
    // Force stop dragging if blocked
    if (isDraggingThis) {
      activeFieldId_ = -1;
      isDraggingThis = false;
    }
  }
  
  // Keyboard control when focused
  if (isFocused && !isDraggingThis && !inputBlocked) {
    bool isCtrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool isShiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    bool isSuperDown = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    bool isCmdDown = isCtrlDown || isSuperDown;
    
    float range = max - min;
    float step = options.stepValue > 0.0f ? options.stepValue : (range * 0.01f);
    if (isShiftDown) step *= 10.0f;
    
    bool valueChanged = false;
    
    // Arrow keys
    if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_DOWN)) {
      value = std::max(min, value - step);
      valueChanged = true;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_UP)) {
      value = std::min(max, value + step);
      valueChanged = true;
    }
    
    // Page Up/Down (20% of range)
    if (IsKeyPressed(KEY_PAGE_UP)) {
      value = std::min(max, value + range * 0.2f);
      valueChanged = true;
    }
    if (IsKeyPressed(KEY_PAGE_DOWN)) {
      value = std::max(min, value - range * 0.2f);
      valueChanged = true;
    }
    
    // Home/End
    if (IsKeyPressed(KEY_HOME)) {
      value = min;
      valueChanged = true;
    }
    if (IsKeyPressed(KEY_END)) {
      value = max;
      valueChanged = true;
    }
    
    // Mouse wheel when focused
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f && mouseOverHit) {
      float wheelStep = step;
      if (isCmdDown) wheelStep *= 0.1f; // Fine control
      value += wheel * wheelStep;
      value = std::clamp(value, min, max);
      valueChanged = true;
    }
    
    if (valueChanged) {
      if (options.stepValue > 0.0f) {
        value = std::round((value - min) / options.stepValue) * options.stepValue + min;
      }
      value = std::clamp(value, min, max);
      normalizedValue = (value - min) / (max - min);
      splitX = trackBounds.x + (trackBounds.width * normalizedValue);
      thumbRect.x = splitX - thumbWidth / 2.0f;
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
  float cornerRadius = trackHeight / 2.0f; // M3: full pill (8dp for 16dp track)

  // 1. Inactive Track (Right side)
  // Draw full rounded rect first as background (inactive part visible on right)
  Renderer::DrawRoundedRectangle(trackBounds, cornerRadius, inactiveColor);

  // 2. Active Track (Left side)
  if (normalizedValue > 0.0f) {
    float scissorWidth = trackBounds.width * normalizedValue;
    if (scissorWidth > 0.0f && trackHeight > 0.0f) {
      PushClipRect(
          {trackBounds.x, trackBounds.y, scissorWidth, trackHeight});
      Renderer::DrawRoundedRectangle(trackBounds, cornerRadius, activeColor);
      PopClipRect();
    }
  }

  // Render Start Icon (Inset) if present
  // It is rendered ON TOP of the active/inactive track.
  // If normalizedValue covers the icon, it should be contrasted against
  // activeColor (e.g. onPrimary). If not, contrasted against inactiveColor
  // (e.g. onSurfaceVariant). Actually, MD3 usually puts the icon on the active
  // track side.
  if (options.startIcon) {
    Rectangle itemBounds = {trackBounds.x + padding, centerY - itemSize / 2.0f,
                            itemSize, itemSize};

    // Determine icon color based on coverage.
    // If the splitX is past the icon's center, it's mostly covered by active
    // track.
    bool covered = (splitX > (itemBounds.x + itemSize / 2.0f));
    Color iconColor = covered ? scheme.onPrimary : scheme.onSurfaceVariant;

    // If activeTrackColor was custom, we should probably pick a contrasting
    // color. For simplicity, sticking to theme semantics unless overridden if
    // we added iconColor option.

    raym3::IconComponent::Render(options.startIcon, itemBounds,
                                 IconVariation::Filled, iconColor);
  }

  // Render End Icon (Inset logic, though usually not standard MD3 slider,
  // supporting per request)
  if (options.endIcon) {
    Rectangle itemBounds = {trackBounds.x + trackBounds.width - itemSize -
                                padding,
                            centerY - itemSize / 2.0f, itemSize, itemSize};
    // Usually end icon is on inactive part
    bool covered = (splitX > (itemBounds.x + itemSize / 2.0f));
    Color iconColor = covered ? scheme.onPrimary : scheme.onSurfaceVariant;
    raym3::IconComponent::Render(options.endIcon, itemBounds,
                                 IconVariation::Filled, iconColor);
  }

  // Draw End Dot (only if requested and not replaced by end icon? Actually dot
  // is usually stop indicator) MD3: Stop indicators are dots.
  if (options.showEndDot && !options.endIcon) {
    float dotRadius = 1.5f;
    Vector2 dotPos = {trackBounds.x + trackBounds.width - 6.0f,
                      trackBounds.y + trackHeight / 2.0f};
    // Dot color should be activeColor? Or onSurfaceVariant?
    // If track is filled up to there (1.0), it's on active. But at 1.0 thumb
    // covers it. Usually it's on the inactive part.
    DrawCircleV(dotPos, dotRadius, activeColor);
  }

  // Draw Stop Indicators at min/max (M3 feature)
  if (options.showStopIndicators) {
    float stopDotRadius = 2.0f;
    float stopInset = 6.0f;
    // Start stop (at min)
    if (normalizedValue > 0.02f) { // Only show if thumb isn't covering it
      Vector2 startDotPos = {trackBounds.x + stopInset,
                             trackBounds.y + trackHeight / 2.0f};
      DrawCircleV(startDotPos, stopDotRadius, activeColor);
    }
    // End stop (at max)
    if (normalizedValue < 0.98f) { // Only show if thumb isn't covering it
      Vector2 endDotPos = {trackBounds.x + trackBounds.width - stopInset,
                           trackBounds.y + trackHeight / 2.0f};
      DrawCircleV(endDotPos, stopDotRadius, inactiveColor);
    }
  }

  // Draw Tick Marks for discrete mode
  if (options.showTickMarks && options.stepValue > 0.0f) {
    int numSteps = (int)((max - min) / options.stepValue);
    float tickRadius = 1.5f;
    float tickInset = 6.0f; // Inset from track edges
    float tickableWidth = trackBounds.width - (tickInset * 2);
    for (int i = 0; i <= numSteps; i++) {
      float tickNorm = (float)i / (float)numSteps;
      float tickX = trackBounds.x + tickInset + tickableWidth * tickNorm;
      // Skip ticks covered by thumb
      if (std::abs(tickX - splitX) < thumbWidth + 4.0f)
        continue;
      Vector2 tickPos = {tickX, trackBounds.y + trackHeight / 2.0f};
      Color tickColor =
          (tickNorm < normalizedValue) ? scheme.onPrimary : activeColor;
      DrawCircleV(tickPos, tickRadius, tickColor);
    }
  }

  // Gap Mask (Surface color) - M3: 4dp gap
  float gapSize = 4.0f;
  Rectangle maskRect = {thumbRect.x - gapSize,
                        thumbRect.y, // Match thumb y
                        thumbRect.width + (gapSize * 2), thumbRect.height};
  DrawRectangleRec(maskRect, scheme.surface);

  // Draw Handle (Thumb)
  Renderer::DrawRoundedRectangle(thumbRect, thumbWidth / 2.0f, handleColor);

  // Draw Value Indicator Bubble (if active or requested)
  // MD3 spec: Bubble appears on press/drag.
  if (isDraggingThis && options.showValueIndicator) {
    char valueStr[32];
    snprintf(valueStr, sizeof(valueStr),
             options.valueFormat ? options.valueFormat : "%.0f", value);

    // Bubble dimensions
    float bubbleWidth = 48.0f;  // Roughly
    float bubbleHeight = 32.0f; // roughly
    float triangleHeight = 6.0f;
    float bubbleY = thumbRect.y - bubbleHeight - triangleHeight - 4.0f;
    float bubbleX = thumbRect.x + thumbRect.width / 2.0f - bubbleWidth / 2.0f;

    Rectangle bubbleRect = {bubbleX, bubbleY, bubbleWidth, bubbleHeight};

    // Draw Bubble Shape (Inverse pill?)
    // Usually it's an inverted teardrop shape. Use rounded rect for now.
    Color bubbleColor = scheme.inverseSurface;
    Color valueColor = scheme.inverseOnSurface;

    Renderer::DrawRoundedRectangle(bubbleRect, bubbleHeight / 2.0f,
                                   bubbleColor);

    // Little triangle pointer
    Vector2 p1 = {bubbleRect.x + bubbleRect.width / 2.0f - 6.0f,
                  bubbleRect.y + bubbleRect.height};
    Vector2 p2 = {bubbleRect.x + bubbleRect.width / 2.0f + 6.0f,
                  bubbleRect.y + bubbleRect.height};
    Vector2 p3 = {bubbleRect.x + bubbleRect.width / 2.0f,
                  bubbleRect.y + bubbleRect.height + triangleHeight};
    DrawTriangle(p1, p2, p3, bubbleColor); // Note: Raylib DrawTriangle order
                                           // might matter for culling
    // Actually DrawTriangle draws counter-clockwise by default? Or clockwise?
    // Let's ensure it draws.
    DrawTriangle(p1, p3, p2, bubbleColor);

    // Draw Value
    Vector2 textSize =
        Renderer::MeasureText(valueStr, 14.0f, FontWeight::Medium);
    Vector2 textPos = {bubbleRect.x + (bubbleRect.width - textSize.x) / 2.0f,
                       bubbleRect.y + (bubbleRect.height - 14.0f) / 2.0f};
    Renderer::DrawText(valueStr, textPos, 14.0f, valueColor,
                       FontWeight::Medium);
  }

  if (label) {
    Vector2 labelPos = {bounds.x, bounds.y};
    Renderer::DrawText(label, labelPos, 14.0f, scheme.onSurface,
                       FontWeight::Regular);
  }
  
  // Draw focus ring if focused
  if (isFocused && !isDraggingThis) {
    float focusInset = -4.0f;
    Rectangle focusRect = {thumbRect.x + focusInset, thumbRect.y + focusInset,
                           thumbRect.width - focusInset * 2, thumbRect.height - focusInset * 2};
    DrawRectangleLinesEx(focusRect, 2.0f, ColorAlpha(scheme.primary, 0.5f));
  }
  
  // Tooltip support
  if (options.tooltip && (mouseOverHit || isFocused)) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options.tooltipPlacement;
    tooltipOpts.delayMs = isFocused && !mouseOverHit ? 100.0f : 500.0f;
    Tooltip(thumbRect, options.tooltip, tooltipOpts);
  }

  return value;
}

Rectangle SliderComponent::GetTrackBounds(Rectangle bounds, bool hasLabel) {
  float trackHeight = 16.0f; // M3 spec: 16dp track

  // Only apply yOffset when a label is present
  float yOffset = hasLabel ? 24.0f : 0.0f;
  float availableHeight = bounds.height - yOffset;

  return {bounds.x,
          bounds.y + yOffset + (availableHeight - trackHeight) / 2.0f,
          bounds.width, trackHeight};
}

float SliderComponent::GetValueFromPosition(Rectangle trackBounds, float x) {
  float normalized = (x - trackBounds.x) / trackBounds.width;
  normalized = std::clamp(normalized, 0.0f, 1.0f);
  return normalized;
}

} // namespace raym3
