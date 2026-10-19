#pragma once

#include "raym3/components/AppBar.h"
#include "raym3/components/Badge.h"
#include "raym3/components/Divider.h"         // Include for enum definition
#include "raym3/components/Chip.h"
#include "raym3/components/FloatingActionButton.h"
#include "raym3/components/List.h"            // Include for List component
#include "raym3/components/Navigation.h"
#include "raym3/components/RangeSlider.h"     // Include for RangeSlider
#include "raym3/components/SearchBar.h"
#include "raym3/components/ButtonGroup.h"
#include "raym3/components/SegmentedButton.h" // Include for struct definition
#include "raym3/components/Snackbar.h"
#include "raym3/components/TabBar.h"          // Include for TabBar component
#include "raym3/components/Tooltip.h"         // Include for Tooltip API
#include "raym3/components/View3D.h" // Include for View3D class
#include "raym3/styles/Theme.h"
#include "raym3/types.h"
#include "raym3/v2.h"
#include <raylib.h>
#include <vector>

namespace raym3 {

void Initialize();
void Shutdown();

void BeginScissor(Rectangle bounds);
void PushScissor(Rectangle bounds);
void PopScissor();
void PushRoundedStencil(Rectangle bounds, float radius);
// Push a rounded stencil in the current modelview space while constraining it
// to a screen-space scissor. This is used by transformed CSS overflow boxes.
void PushRoundedStencilWithScissor(Rectangle bounds, float radius,
                                   Rectangle scissorBounds);
void PopRoundedStencil();
Rectangle GetCurrentScissorBounds();
// Rebuild the current logical scissor/rounded-stencil state after a renderer
// target switch without mutating either logical stack.
void ReplayCurrentClips();

void SetScissorDebug(bool enabled);
bool IsScissorDebug();
void DrawScissorDebug();

void BeginFrame();
void EndFrame();

// Paint a web-style range input track and circular thumb. Layout and input
// remain owned by the host; colors and geometry come from CSS.
void DrawCssSlider(Rectangle bounds, float progress, Color accent,
                   Color track, float trackThickness = 4.0f,
                   float thumbRadius = 8.0f);

// Paint a single continuous horizontal gradient row. The color function is
// supplied by the CSS host; keeping the row in one Raylib draw call avoids a
// raster seam at the gradient midpoint.
void DrawCssGradientRow(float x, float y, float width, float height,
                        Color left, Color right);
void DrawCssEllipseBorder(Rectangle bounds, Color color, float thickness,
                          bool dashed);
void DrawCssInsetEllipse(Rectangle bounds, Color color, float offsetY,
                         float thickness);
void DrawCssSuitMark(Rectangle bounds, int suit, Color color);
void DrawCssFullscreenIcon(Rectangle bounds, Color color);
void DrawCssBoxFill(Rectangle bounds, float cornerRadius, Color color);
void DrawCssBoxBorder(Rectangle bounds, float cornerRadius, float thickness,
                      Color color);

void RequestCursor(int cursor);

#if RAYM3_USE_INPUT_LAYERS
void PushLayer(int zOrder = 0);
void PopLayer();
#endif

void SetTheme(bool darkMode);
bool IsDarkMode();

void BeginContainer(Rectangle bounds,
                    LayoutDirection direction = LayoutDirection::Column);
void EndContainer();

void SetIconBasePath(const char *path);

bool Button(const char *text, Rectangle bounds,
            ButtonVariant variant = ButtonVariant::Filled);
bool IconButton(const char *iconName, Rectangle bounds,
                ButtonVariant variant = ButtonVariant::Text,
                IconVariation iconVariation = IconVariation::Filled);

void Badge(Rectangle anchorBounds, const char *label = nullptr,
           const BadgeOptions &options = BadgeOptions{});
bool Chip(const char *label, Rectangle bounds,
          const ChipOptions &options = ChipOptions{});
bool Chip(const char *label, Rectangle bounds, bool *selected,
          const ChipOptions &options = ChipOptions{});
bool FloatingActionButton(const char *iconName, Rectangle bounds,
                          const FabOptions &options = FabOptions{});
bool ExtendedFloatingActionButton(const char *label, const char *iconName,
                                  Rectangle bounds,
                                  const FabOptions &options = FabOptions{});
int AppBar(Rectangle bounds, const char *title,
           const AppBarOptions &options = AppBarOptions{});
bool SearchBar(char *buffer, int bufferSize, Rectangle bounds,
               const char *placeholder = "Search",
               const SearchBarOptions &options = SearchBarOptions{});
bool NavigationBar(Rectangle bounds, const NavigationItem *items, int itemCount,
                   int *selectedIndex,
                   const NavigationOptions &options = NavigationOptions{});
bool NavigationRail(Rectangle bounds, const NavigationItem *items, int itemCount,
                    int *selectedIndex,
                    const NavigationOptions &options = NavigationOptions{});

bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label = nullptr);
bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label, const TextFieldOptions &options);
bool Checkbox(const char *label, Rectangle bounds, bool *checked);
bool Switch(const char *label, Rectangle bounds, bool *checked);
bool RadioButton(const char *label, Rectangle bounds, bool selected);
float Slider(Rectangle bounds, float value, float min, float max,
             const char *label = nullptr);
float Slider(Rectangle bounds, float value, float min, float max,
             const char *label, const SliderOptions &options);

// Range slider (multi-thumb)
std::vector<float> RangeSlider(Rectangle bounds,
                               const std::vector<float> &values, float min,
                               float max, const char *label = nullptr,
                               const RangeSliderOptions &options = {});

void CircularProgressIndicator(Rectangle bounds, float value = 0.0f,
                               bool indeterminate = false, Color color = BLANK,
                               float wiggleAmplitude = 2.0f,
                               float wiggleWavelength = 20.0f);
void LinearProgressIndicator(Rectangle bounds, float value = 0.0f,
                             bool indeterminate = false, Color color = BLANK,
                             float wiggleAmplitude = 2.0f,
                             float wiggleWavelength = 20.0f);

void Icon(const char *name, Rectangle bounds,
          IconVariation variation = IconVariation::Filled, Color color = BLACK);

void Text(const char *text, Rectangle bounds, float fontSize, Color color,
          FontWeight weight = FontWeight::Regular,
          TextAlignment alignment = TextAlignment::Left);

void Card(Rectangle bounds, CardVariant variant = CardVariant::Elevated);
bool Dialog(const char *title, const char *message, const char *buttons);
struct MenuItem {
  const char *text = nullptr;
  const char *leadingIcon = nullptr;
  const char *trailingText = nullptr;
  const char *trailingIcon = nullptr;
  bool isDivider = false;
  bool isGap = false;
  bool disabled = false;
  const char *tooltip = nullptr;
  TooltipPlacement tooltipPlacement = TooltipPlacement::Auto;
};

void Menu(Rectangle bounds, const MenuItem *items, int itemCount,
          int *selectedIndex, bool iconOnly = false);

struct SegmentedButtonItem; // Forward declaration
bool SegmentedButton(Rectangle bounds, const SegmentedButtonItem *items,
                     int itemCount, int *selectedIndex,
                     bool multiSelect = false);

struct ButtonGroupItem; // Forward declaration
int ButtonGroup(Rectangle bounds, const ButtonGroupItem *items, int count,
                ButtonVariant variant = ButtonVariant::Outlined);

void Divider(Rectangle bounds,
             DividerVariant variant = DividerVariant::Horizontal);

enum class DividerVariant; // Forward declaration
void Divider(Rectangle bounds,
             DividerVariant variant); // Default value in cpp or overload?
// Better to include the enum definition or use int?
// Let's include the header in raym3.h like SegmentedButton?
// Or just redefine enum? No.
// Let's include "raym3/components/Divider.h" in raym3.h

} // namespace raym3
