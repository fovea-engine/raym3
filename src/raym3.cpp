#include "raym3/raym3.h"
#include "raym3/components/Button.h"
#include "raym3/components/Card.h"
#include "raym3/components/Checkbox.h"
#include "raym3/components/Dialog.h"
#include "raym3/components/Menu.h"
#include "raym3/components/Slider.h"
#include "raym3/components/Switch.h"
#include "raym3/components/TextField.h"
#include "raym3/layout/Container.h"
#include "raym3/styles/Theme.h"

#include "raym3/components/Divider.h"
#include "raym3/components/Icon.h"
#include "raym3/components/IconButton.h"
#include "raym3/components/ProgressIndicator.h"
#include "raym3/components/RadioButton.h"
#include "raym3/components/SegmentedButton.h"
#include "raym3/components/Text.h"
#include "raym3/rendering/SvgRenderer.h"

#if RAYM3_USE_INPUT_LAYERS
#include "raym3/input/InputLayer.h"
#include "raym3/input/RenderQueue.h"
#endif

namespace raym3 {

static bool initialized = false;
static bool darkMode = false;

void Initialize() {
  if (initialized)
    return;

  Theme::Initialize();
  SvgRenderer::Initialize(nullptr); // Auto-detect resource path

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::Initialize();
  RenderQueue::Initialize();
#endif

  initialized = true;
}

void Shutdown() {
  if (!initialized)
    return;

  SvgRenderer::Shutdown();
  Theme::Shutdown();
  initialized = false;
}

void BeginFrame() {
  if (!initialized)
    Initialize();
  TextFieldComponent::ResetFieldId();
  SliderComponent::ResetFieldId();

#if RAYM3_USE_INPUT_LAYERS
  InputLayerManager::BeginFrame();
  RenderQueue::BeginFrame();
#endif
}

void EndFrame() {
#if RAYM3_USE_INPUT_LAYERS
  RenderQueue::ExecuteRenderQueue();
  InputLayerManager::EndFrame();
#endif
}

#if RAYM3_USE_INPUT_LAYERS
void PushLayer(int zOrder) {
  InputLayerManager::PushLayer(zOrder);
  RenderQueue::PushLayer(zOrder);
}

void PopLayer() {
  InputLayerManager::PopLayer();
  RenderQueue::PopLayer();
}
#endif

void SetTheme(bool isDarkMode) {
  darkMode = isDarkMode;
  Theme::SetDarkMode(isDarkMode);
}

bool IsDarkMode() { return darkMode; }

void SetIconBasePath(const char *path) { SvgRenderer::Initialize(path); }

void BeginContainer(Rectangle bounds, LayoutDirection direction) {
  Container::Begin(bounds, direction);
}

void EndContainer() { Container::End(); }

bool Button(const char *text, Rectangle bounds, ButtonVariant variant) {
  return ButtonComponent::Render(text, bounds, variant);
}

bool IconButton(const char *iconName, Rectangle bounds, ButtonVariant variant,
                IconVariation iconVariation) {
  return IconButtonComponent::Render(iconName, bounds, variant, iconVariation);
}

bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label) {
  return TextFieldComponent::Render(buffer, bufferSize, bounds, label);
}

bool TextField(char *buffer, int bufferSize, Rectangle bounds,
               const char *label, const TextFieldOptions &options) {
  return TextFieldComponent::Render(buffer, bufferSize, bounds, label, options);
}

bool Checkbox(const char *label, Rectangle bounds, bool *checked) {
  return CheckboxComponent::Render(label, bounds, checked);
}

bool Switch(const char *label, Rectangle bounds, bool *checked) {
  return SwitchComponent::Render(label, bounds, checked);
}

bool RadioButton(const char *label, Rectangle bounds, bool selected) {
  return RadioButtonComponent::Render(label, bounds, selected);
}

float Slider(Rectangle bounds, float value, float min, float max,
             const char *label) {
  return SliderComponent::Render(bounds, value, min, max, label);
}

void Icon(const char *name, Rectangle bounds, IconVariation variation,
          Color color) {
  IconComponent::Render(name, bounds, variation, color);
}

void Text(const char *text, Rectangle bounds, float fontSize, Color color,
          FontWeight weight, TextAlignment alignment) {
  TextComponent::Render(text, bounds, fontSize, color, weight, alignment);
}

void CircularProgressIndicator(Rectangle bounds, float value,
                               bool indeterminate, Color color,
                               float wiggleAmplitude, float wiggleFrequency) {
  ProgressIndicator::Circular(bounds, value, indeterminate, color,
                              wiggleAmplitude, wiggleFrequency);
}

void LinearProgressIndicator(Rectangle bounds, float value, bool indeterminate,
                             Color color, float wiggleAmplitude,
                             float wiggleFrequency) {
  ProgressIndicator::Linear(bounds, value, indeterminate, color,
                            wiggleAmplitude, wiggleFrequency);
}

void Card(Rectangle bounds, CardVariant variant) {
  CardComponent::Render(bounds, variant);
}

bool Dialog(const char *title, const char *message, const char *buttons) {
  return DialogComponent::Render(title, message, buttons);
}

void Menu(Rectangle bounds, const MenuItem *items, int itemCount,
          int *selectedIndex, bool iconOnly) {
  MenuComponent::Render(bounds, items, itemCount, selectedIndex, iconOnly);
}

bool SegmentedButton(Rectangle bounds, const SegmentedButtonItem *items,
                     int itemCount, int *selectedIndex) {
  return SegmentedButtonComponent::Render(bounds, items, itemCount,
                                          selectedIndex);
}

void Divider(Rectangle bounds, DividerVariant variant) {
  DividerComponent::Render(bounds, variant);
}

} // namespace raym3
