#include "raym3/components/Badge.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include <algorithm>
#include <cstring>
#include <string>

namespace raym3 {

void BadgeComponent::Render(Rectangle anchorBounds, const char *label,
                            const BadgeOptions &options) {
  ColorScheme &scheme = Theme::GetColorScheme();
  const bool hasLabel = label && label[0] != '\0';
  Color containerColor = (options.color.a > 0) ? options.color : scheme.error;
  Color labelColor =
      (options.textColor.a > 0) ? options.textColor : scheme.onError;

  std::string visibleLabel;
  if (hasLabel) {
    visibleLabel = label;
    if (options.maxCharacterCount > 0 &&
        static_cast<int>(visibleLabel.size()) > options.maxCharacterCount) {
      visibleLabel = visibleLabel.substr(0, options.maxCharacterCount - 1);
      visibleLabel += "+";
    }
  }

  const float badgeHeight = hasLabel ? 16.0f : 6.0f;
  float badgeWidth = badgeHeight;
  if (hasLabel) {
    Vector2 labelSize = Renderer::MeasureText(visibleLabel.c_str(), 11.0f,
                                              FontWeight::Medium);
    badgeWidth = std::max(16.0f, labelSize.x + 8.0f);
  }

  const bool topEnd = options.alignment == BadgeAlignment::TopEnd;
  float badgeX = topEnd ? anchorBounds.x + anchorBounds.width - badgeWidth +
                              options.horizontalOffset
                        : anchorBounds.x - options.horizontalOffset;
  float badgeY = anchorBounds.y + options.verticalOffset;
  Rectangle badgeBounds = {badgeX, badgeY, badgeWidth, badgeHeight};

  Renderer::DrawRoundedRectangle(badgeBounds, badgeHeight / 2.0f,
                                 containerColor);
  if (hasLabel) {
    Renderer::DrawTextCentered(visibleLabel.c_str(), badgeBounds, 11.0f,
                               labelColor, FontWeight::Medium);
  }
}

} // namespace raym3
