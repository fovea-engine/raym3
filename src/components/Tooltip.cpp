#include "raym3/components/Tooltip.h"
#include "raym3/components/Button.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cstring>
#include <raylib.h>

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#endif

namespace raym3 {

// Static member definitions
bool TooltipManager::hasRequest_ = false;
Rectangle TooltipManager::anchorBounds_ = {0, 0, 0, 0};
Rectangle TooltipManager::tooltipBounds_ = {0, 0, 0, 0};
std::string TooltipManager::text_;
int TooltipManager::sourceLayer_ = 0;
TooltipOptions TooltipManager::options_;
float TooltipManager::hoverTimer_ = 0.0f;
float TooltipManager::dismissTimer_ = 0.0f;
Rectangle TooltipManager::lastAnchor_ = {0, 0, 0, 0};
bool TooltipManager::isVisible_ = false;
bool TooltipManager::mouseOverTooltip_ = false;

//-----------------------------------------------------------------------------
// Helper: Check if two rectangles are the same anchor
//-----------------------------------------------------------------------------
static bool IsSameAnchor(Rectangle a, Rectangle b) {
  return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

//-----------------------------------------------------------------------------
// TooltipManager implementation
//-----------------------------------------------------------------------------

void TooltipManager::Update() {
  RenderPendingTooltip();
  // Reset request flag for next frame
  hasRequest_ = false;
}

void TooltipManager::DismissAll() {
  isVisible_ = false;
  hoverTimer_ = 0.0f;
  dismissTimer_ = 0.0f;
  hasRequest_ = false;
  mouseOverTooltip_ = false;
}

bool TooltipManager::IsAnyVisible() { return isVisible_; }

int TooltipManager::GetActiveTooltipSourceLayer() { return sourceLayer_; }

void TooltipManager::RequestTooltip(Rectangle anchor, const char *text,
                                    int sourceLayer,
                                    const TooltipOptions &options) {
  // Only process one tooltip request per frame (first one wins)
  if (hasRequest_)
    return;

  Vector2 mousePos = GetMousePosition();
  bool isHoveredAnchor = CheckCollisionPointRec(mousePos, anchor);
  
  // For rich tooltips that are visible, also check if mouse is over tooltip
  bool isHoveredTooltip = false;
  if (isVisible_ && options_.IsRich() && IsSameAnchor(anchor, lastAnchor_)) {
    isHoveredTooltip = CheckCollisionPointRec(mousePos, tooltipBounds_);
    mouseOverTooltip_ = isHoveredTooltip;
  }
  
  // Mouse is over anchor OR over the tooltip itself (for rich tooltips)
  bool isHovered = isHoveredAnchor || isHoveredTooltip;

  if (!isHovered) {
    // Mouse left both anchor and tooltip
    if (IsSameAnchor(anchor, lastAnchor_)) {
      if (isVisible_ && options_.IsRich()) {
        // Rich tooltip: delay dismissal to allow mouse travel
        dismissTimer_ += GetFrameTime() * 1000.0f;
        if (dismissTimer_ >= kDismissDelayMs) {
          hoverTimer_ = 0.0f;
          dismissTimer_ = 0.0f;
          isVisible_ = false;
          mouseOverTooltip_ = false;
        } else {
          // Keep tooltip visible during grace period
          hasRequest_ = true;
        }
      } else {
        // Plain tooltip: dismiss immediately
        hoverTimer_ = 0.0f;
        isVisible_ = false;
      }
    }
    return;
  }

  // Mouse is hovering - reset dismiss timer
  dismissTimer_ = 0.0f;

  // Hovering - update timer
  if (IsSameAnchor(anchor, lastAnchor_)) {
    // Same anchor, accumulate time
    hoverTimer_ += GetFrameTime() * 1000.0f; // Convert to ms
  } else {
    // New anchor, reset timer
    lastAnchor_ = anchor;
    hoverTimer_ = 0.0f;
    isVisible_ = false;
    mouseOverTooltip_ = false;
  }

  // Check if we should show the tooltip
  if (hoverTimer_ >= options.delayMs) {
    hasRequest_ = true;
    anchorBounds_ = anchor;
    text_ = text ? text : "";
    sourceLayer_ = sourceLayer;
    options_ = options;
    isVisible_ = true;
  }
}

Rectangle TooltipManager::ComputePosition(Rectangle anchor, Vector2 contentSize,
                                          TooltipPlacement placement) {
  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();
  float gap = 8.0f;

  Rectangle result = {0, 0, contentSize.x, contentSize.y};

  // Determine initial position based on placement
  switch (placement) {
  case TooltipPlacement::Above:
    result.x = anchor.x + (anchor.width - contentSize.x) / 2.0f;
    result.y = anchor.y - contentSize.y - gap;
    break;

  case TooltipPlacement::Below:
    result.x = anchor.x + (anchor.width - contentSize.x) / 2.0f;
    result.y = anchor.y + anchor.height + gap;
    break;

  case TooltipPlacement::Left:
    result.x = anchor.x - contentSize.x - gap;
    result.y = anchor.y + (anchor.height - contentSize.y) / 2.0f;
    break;

  case TooltipPlacement::Right:
    result.x = anchor.x + anchor.width + gap;
    result.y = anchor.y + (anchor.height - contentSize.y) / 2.0f;
    break;

  case TooltipPlacement::Auto:
  default:
    // Default: below anchor, centered
    result.x = anchor.x + (anchor.width - contentSize.x) / 2.0f;
    result.y = anchor.y + anchor.height + gap;

    // Flip above if too close to bottom
    if (result.y + result.height > screenH - gap) {
      result.y = anchor.y - contentSize.y - gap;
    }
    break;
  }

  // Clamp to screen edges (apply to all placements)
  if (result.x + result.width > screenW - gap) {
    result.x = screenW - result.width - gap;
  }
  if (result.x < gap) {
    result.x = gap;
  }
  if (result.y + result.height > screenH - gap) {
    result.y = screenH - result.height - gap;
  }
  if (result.y < gap) {
    result.y = gap;
  }

  return result;
}

void TooltipManager::RenderPlainTooltip(Rectangle bounds, const char *text) {
  ColorScheme &scheme = Theme::GetColorScheme();

  // MD3 Plain Tooltip:
  // - Background: inverseSurface
  // - Text: inverseOnSurface, 14sp
  // - Corner radius: 4dp (extra small)
  float cornerRadius = 4.0f;

  // Draw background with slight elevation
  Renderer::DrawElevatedRectangle(bounds, cornerRadius, 2, scheme.inverseSurface);

  // Draw text centered
  float padding = 8.0f;
  Vector2 textPos = {bounds.x + padding,
                     bounds.y + (bounds.height - 14.0f) / 2.0f};
  Renderer::DrawText(text, textPos, 14.0f, scheme.inverseOnSurface,
                     FontWeight::Regular);
}

void TooltipManager::RenderRichTooltip(Rectangle bounds, const char *title,
                                       const char *text,
                                       const TooltipOptions &options) {
  ColorScheme &scheme = Theme::GetColorScheme();

  // MD3 Rich Tooltip:
  // - Background: surfaceContainerLow
  // - Title: onSurface, headline small (24sp but we'll use 16sp for tooltip)
  // - Body: onSurfaceVariant, 14sp
  // - Corner radius: 12dp (medium)
  float cornerRadius = 12.0f;
  float padding = 12.0f;

  // Draw background
  Renderer::DrawElevatedRectangle(bounds, cornerRadius, 3,
                                  scheme.surfaceContainerLow);

  float y = bounds.y + padding;

  // Title
  if (title && strlen(title) > 0) {
    Vector2 titlePos = {bounds.x + padding, y};
    Renderer::DrawText(title, titlePos, 16.0f, scheme.onSurface,
                       FontWeight::Medium);
    y += 22.0f;
  }

  // Body text
  if (text && strlen(text) > 0) {
    Vector2 textPos = {bounds.x + padding, y};
    Renderer::DrawText(text, textPos, 14.0f, scheme.onSurfaceVariant,
                       FontWeight::Regular);
    y += 20.0f;
  }

  // Action button
  if (options.actionText && strlen(options.actionText) > 0) {
    y += 4.0f; // Gap before button

    Vector2 btnTextSize =
        Renderer::MeasureText(options.actionText, 14.0f, FontWeight::Medium);
    float btnWidth = btnTextSize.x + 16.0f;
    float btnHeight = 32.0f;

    Rectangle btnBounds = {bounds.x + bounds.width - padding - btnWidth, y,
                           btnWidth, btnHeight};

    // Check for click on action button
    Vector2 mousePos = GetMousePosition();
    bool isHovered = CheckCollisionPointRec(mousePos, btnBounds);
    bool isPressed = isHovered && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    bool wasClicked = isHovered && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);

    // Draw hover state
    if (isHovered) {
      Color hoverColor = ColorAlpha(scheme.primary, 0.08f);
      DrawRectangleRounded(btnBounds, 0.5f, 4, hoverColor);
    }
    if (isPressed) {
      Color pressColor = ColorAlpha(scheme.primary, 0.12f);
      DrawRectangleRounded(btnBounds, 0.5f, 4, pressColor);
    }

    // Draw button text
    Vector2 btnTextPos = {btnBounds.x + 8.0f,
                          btnBounds.y + (btnBounds.height - btnTextSize.y) / 2.0f};
    Renderer::DrawText(options.actionText, btnTextPos, 14.0f, scheme.primary,
                       FontWeight::Medium);

    // Handle click
    if (wasClicked && options.onAction) {
      options.onAction();
      DismissAll(); // Close tooltip after action
    }
  }
}

void TooltipManager::RenderPendingTooltip() {
  if (!isVisible_ || text_.empty())
    return;

  // Calculate content size based on tooltip type
  Vector2 contentSize;
  float padding = options_.IsRich() ? 12.0f : 8.0f;
  float maxWidth = options_.IsRich() ? 312.0f : options_.maxWidth;

  if (options_.IsRich()) {
    // Rich tooltip size calculation
    float width = padding * 2;
    float height = padding * 2;

    if (options_.title) {
      Vector2 titleSize =
          Renderer::MeasureText(options_.title, 16.0f, FontWeight::Medium);
      width = std::max(width, titleSize.x + padding * 2);
      height += 22.0f;
    }

    if (!text_.empty()) {
      Vector2 textSize =
          Renderer::MeasureText(text_.c_str(), 14.0f, FontWeight::Regular);
      width = std::max(width, textSize.x + padding * 2);
      height += 20.0f;
    }

    if (options_.actionText) {
      Vector2 btnSize =
          Renderer::MeasureText(options_.actionText, 14.0f, FontWeight::Medium);
      width = std::max(width, btnSize.x + 16.0f + padding * 2);
      height += 36.0f; // Button height + gap
    }

    width = std::min(width, maxWidth);
    contentSize = {width, height};
  } else {
    // Plain tooltip size calculation
    Vector2 textSize =
        Renderer::MeasureText(text_.c_str(), 14.0f, FontWeight::Regular);
    contentSize.x = std::min(textSize.x + padding * 2, maxWidth);
    contentSize.y = 28.0f; // Fixed height for plain tooltip
  }

  // Compute position with edge avoidance
  Rectangle tooltipBounds =
      ComputePosition(anchorBounds_, contentSize, options_.placement);
  
  // Store bounds for mouse-over detection in RequestTooltip
  tooltipBounds_ = tooltipBounds;

  // Push to high layer for rendering
#if RAYM3_USE_INPUT_LAYERS
  // Use layer 8000 (above most content, below modals at 9999)
  // But always sourceLayer_ + 100 if source is already elevated
  int tooltipLayer = std::max(8000, sourceLayer_ + 100);
  InputLayerManager::PushLayer(tooltipLayer);
  // Register blocking region only for rich tooltips with action buttons
  if (options_.IsRich() && options_.actionText) {
    InputLayerManager::RegisterBlockingRegion(tooltipBounds, true);
  }
#endif

  // Render the appropriate tooltip style
  if (options_.IsRich()) {
    RenderRichTooltip(tooltipBounds, options_.title, text_.c_str(), options_);
  } else {
    RenderPlainTooltip(tooltipBounds, text_.c_str());
  }

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::PopLayer();
#endif
}

//-----------------------------------------------------------------------------
// Public API
//-----------------------------------------------------------------------------

void Tooltip(Rectangle anchor, const char *text) {
  TooltipOptions options;
  Tooltip(anchor, text, options);
}

void Tooltip(Rectangle anchor, const char *text, const TooltipOptions &options) {
#if RAYM3_USE_INPUT_LAYERS
  int sourceLayer = InputLayerManager::GetCurrentLayerId();
#else
  int sourceLayer = 0;
#endif
  TooltipManager::RequestTooltip(anchor, text, sourceLayer, options);
}

} // namespace raym3
