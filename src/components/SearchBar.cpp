#include "raym3/components/SearchBar.h"
#include "raym3/components/TextField.h"
#include "raym3/components/Tooltip.h"
#include "raym3/styles/Theme.h"

namespace raym3 {

bool SearchBarComponent::Render(char *buffer, int bufferSize, Rectangle bounds,
                                const char *placeholder,
                                const SearchBarOptions &options) {
  TextFieldOptions textOptions;
  textOptions.variant = TextFieldVariant::Filled;
  textOptions.placeholder = placeholder;
  textOptions.disabled = options.disabled;
  textOptions.leadingIcon = options.leadingIcon;
  textOptions.trailingIcon = options.trailingIcon;
  textOptions.drawOutline = false;
  textOptions.drawBackground = true;
  textOptions.backgroundColor = Theme::GetColorScheme().surfaceContainerHigh;
  textOptions.outlineColor = ColorAlpha(Theme::GetColorScheme().outline, 0.0f);
  textOptions.borderRadius = bounds.height > 0.0f ? bounds.height / 2.0f : 28.0f;

  bool changed =
      TextFieldComponent::Render(buffer, bufferSize, bounds, nullptr, textOptions);

  if (options.tooltip && CheckCollisionPointRec(GetMousePosition(), bounds)) {
    TooltipOptions tooltipOpts;
    tooltipOpts.placement = options.tooltipPlacement;
    Tooltip(bounds, options.tooltip, tooltipOpts);
  }

  return changed;
}

} // namespace raym3
