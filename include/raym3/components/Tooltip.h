#pragma once

#include <functional>
#include <raylib.h>
#include <string>

namespace raym3 {

enum class TooltipPlacement {
  Auto,  // Smart positioning to avoid window edges
  Above,
  Below,
  Left,
  Right
};

struct TooltipOptions {
  // Positioning
  TooltipPlacement placement = TooltipPlacement::Auto;
  float delayMs = 500.0f; // Hover delay before showing (default 500ms)

  // Plain tooltip settings
  float maxWidth = 280.0f; // Max width for text wrapping (plain: 280dp)

  // Rich tooltip fields (if ANY of these are set, tooltip becomes rich)
  const char *title = nullptr;            // Rich: displayed as headline
  const char *actionText = nullptr;       // Rich: action button text
  std::function<void()> onAction = nullptr; // Rich: action callback

  // Helper: returns true if this should be rendered as rich tooltip
  bool IsRich() const {
    return title != nullptr || actionText != nullptr || onAction != nullptr;
  }
};

class TooltipManager {
public:
  // Call once per frame at END of your component rendering (from EndFrame)
  static void Update();

  // Force dismiss all tooltips (called automatically when modal opens)
  static void DismissAll();

  // Check if any tooltip is currently showing
  static bool IsAnyVisible();

  // Get the layer ID that triggered current tooltip (for layer inheritance)
  static int GetActiveTooltipSourceLayer();

private:
  friend void Tooltip(Rectangle, const char *, const TooltipOptions &);

  static void RequestTooltip(Rectangle anchor, const char *text,
                             int sourceLayer, const TooltipOptions &options);
  static void RenderPendingTooltip();
  static void RenderPlainTooltip(Rectangle bounds, const char *text);
  static void RenderRichTooltip(Rectangle bounds, const char *title,
                                const char *text, const TooltipOptions &options);
  static Rectangle ComputePosition(Rectangle anchor, Vector2 contentSize,
                                   TooltipPlacement placement);

  // State
  static bool hasRequest_;
  static Rectangle anchorBounds_;
  static Rectangle tooltipBounds_;  // Computed tooltip position for hover check
  static std::string text_;
  static int sourceLayer_;
  static TooltipOptions options_;
  static float hoverTimer_;
  static float dismissTimer_;       // Delay before dismissing (for rich tooltips)
  static Rectangle lastAnchor_;
  static bool isVisible_;
  static bool mouseOverTooltip_;    // Is mouse currently over the tooltip?
  
  static constexpr float kDismissDelayMs = 300.0f; // Time to move mouse to tooltip
};

//-----------------------------------------------------------------------------
// Public API - unified Tooltip function
//-----------------------------------------------------------------------------

// Plain tooltip (just text)
void Tooltip(Rectangle anchor, const char *text);

// Tooltip with options - auto-detects plain vs rich based on options values
// If options.title, options.actionText, or options.onAction is set → Rich
// tooltip Otherwise → Plain tooltip
void Tooltip(Rectangle anchor, const char *text, const TooltipOptions &options);

} // namespace raym3
