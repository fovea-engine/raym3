#pragma once

#include "raym3/types.h"
#include "raym3/v2/Style.h"
#include "raym3/v2/TextEngine.h"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace raym3::v2 {

enum class NodeKind {
  View,
  Text,
  TextInput,
  Button,
  Custom,
  // First-class interactive controls. Their painting and interaction live in
  // raym3 core (Controls.cpp + Input.cpp) so pure C++ apps get working
  // sliders/toggles without a host bridge.
  Slider,
  RangeSlider,
  Switch,
  Checkbox,
  RadioButton
};

// Marks containers that need post-layout decoration the flexbox model can't
// express on its own (e.g. a tab's active indicator that slides between the
// selected child's bounds).
enum class NodeRole { None, Tabs, NavItem, NavigationBar, NavigationRail,
                      ButtonGroupContainer, ButtonGroupConnected, SplitButton,
                      BottomSheet };

enum class PopoverPlacement { Auto, Below, Above };

struct ViewProps {
  std::string id;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  bool disabled = false;
  bool capturesInput = false;
  int zIndex = 0;
  std::optional<Rectangle> anchorRect;
  PopoverPlacement placement = PopoverPlacement::Auto;
  std::function<void()> onPress;
};

struct TextProps {
  std::string id;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  bool disabled = false;
};

struct TextInputProps {
  std::string id;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  char *buffer = nullptr;
  int bufferSize = 0;
  std::string *value = nullptr;
  std::string label;
  std::string placeholder;
  bool passwordMode = false;
  bool readOnly = false;
  bool disabled = false;
  TextFieldVariant variant = TextFieldVariant::Outlined;
  bool drawBackground = true;
  bool drawOutline = true;
  // When false, the borderless field paints no self hover/focus highlight; the
  // parent (e.g. a search bar) owns the single state layer. See TextFieldOptions.
  bool drawStateLayer = true;
  std::function<void(const std::string &)> onChange;
  std::function<void()> onFocus;
  std::function<void()> onBlur;
};

struct ButtonProps {
  std::string id;
  std::string label;
  ButtonVariant variant = ButtonVariant::Filled;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  bool disabled = false;
  std::function<void()> onPress;
};

// Interaction + value state for first-class controls (Slider/Switch/etc.).
// Migrated out of rayact's bridge so the logic lives in raym3 core.
struct ControlState {
  bool checked = false;
  // Eased 0..1 toggle progress (-1 = uninitialized) for checkbox/switch/radio.
  float anim = -1.0f;
  float animFrom = 0.0f;
  float animTarget = -1.0f;
  float animElapsedMs = 0.0f;
  // Slider.
  float value = 0.0f;
  float minValue = 0.0f;
  float maxValue = 1.0f;
  float step = 0.0f; // 0 = continuous
  bool dragging = false;
  int draggingThumb = -1; // RangeSlider: 0 = start thumb, 1 = end thumb
  float startValue = 0.25f;
  float endValue = 0.75f;
  float sliderTrackH = 16.0f;
  float sliderHandleH = 44.0f;
};

// Minimal accessibility hooks (no screen-reader bridge yet; pre-wires a later
// pass). Mirrors RN accessibility* props.
struct AccessibilityInfo {
  std::string role;
  std::string label;
  bool hasState = false;
  bool stateChecked = false;
  bool stateDisabled = false;
  bool stateSelected = false;
  bool stateExpanded = false;
};

struct TextEditState {
  int cursor = 0;
  int selectionStart = -1;
  int selectionEnd = -1;
  float scrollOffsetX = 0.0f;
  float labelAnim = 0.0f;
  float lastBlinkTime = 0.0f;
  double backspaceTimer = 0.0;
  double deleteTimer = 0.0;
  double arrowLeftTimer = 0.0;
  double arrowRightTimer = 0.0;
  double undoTimer = 0.0;
  double redoTimer = 0.0;
  bool wasFocused = false;
  // Mouse selection state (drag select, double/triple click).
  bool isSelecting = false;
  int selectionAnchor = -1;
  int clickCount = 0;
  double lastClickTime = 0.0;
  int lastClickPos = -1;
};

class Node;
using NodePtr = std::shared_ptr<Node>;

class Node {
public:
  explicit Node(NodeKind kind);

  NodeKind kind;
  std::string id;
  Style style;
  StateStyles stateStyles;
  MotionSpec motion;
  bool disabled = false;
  int zIndex = 0;
  bool inkRipple = false;
  std::function<void()> onPress;

  std::string text;
  ButtonVariant buttonVariant = ButtonVariant::Filled;
  TextInputProps textInput;
  TextEditState textEdit;
  std::function<void(Rectangle)> customRender;

  Rectangle layout = {0, 0, 0, 0};
  Rectangle previousLayout = {0, 0, 0, 0};
  // Animation state — interpolated per-frame, persists across frames because the
  // node tree is retained. animStateAlpha eases the state-layer opacity (M3
  // state layers cross-fade rather than snap); animSelect eases selection
  // transitions (switch thumb, tab indicator, checkbox check).
  float animStateAlpha = 0.0f;
  float animSelect = 0.0f;
  bool animInitialized = false;
  // Container role + selection used for post-layout decoration (tab indicator).
  NodeRole role = NodeRole::None;
  bool selected = false;
  float animIndicatorX = -1.0f;
  float animIndicatorY = -1.0f;
  float animIndicatorW = 0.0f;
  float animIndicatorFade = 0.0f;
  bool animIndicatorLastSelected = false;
  bool animIndicatorSelectionInitialized = false;
  float scrollOffsetX = 0.0f;
  float scrollOffsetY = 0.0f;
  float scrollContentWidth = 0.0f;
  float scrollContentHeight = 0.0f;
  float scrollVelocityX = 0.0f;
  float scrollVelocityY = 0.0f;
  bool scrollMomentumEnabled = false;
  // Android-style fling simulation state (ClampingScrollSimulation).
  bool flingActive = false;
  double flingStartTime = 0.0;
  float flingStartOffsetY = 0.0f;
  float flingDuration = 0.0f;
  float flingDistance = 0.0f;
  std::function<void()> onScroll;
  // Continuously time-driven paint (indeterminate/wavy progress, loading
  // spinners): the frame scheduler must keep rendering while this node exists.
  bool alwaysAnimates = false;
  ComponentState state = ComponentState::Default;
  std::vector<NodePtr> children;

  std::string inputScratch;
  std::vector<char> inputBuffer;

  // Pretext-style two-phase text cache: prepare once (segment + measure),
  // layout many times (pure arithmetic). Invalidated when text or font changes.
  mutable std::optional<PreparedText> preparedTextCache;
  mutable std::string  preparedTextKey;   // text + fontSize + weight fingerprint

  bool inNavigationRail = false;
  bool inNavigationBar = false;
  float navigationRailOpenAmount = 0.0f;
  // True for modal overlay components (Dialog, BottomSheet, SideSheet) — causes
  // a full-screen scrim to be drawn behind the node in the fixed-position pass.
  bool hasScrim = false;
  bool capturesInput = false;
  float overlayDragOffsetY = 0.0f;

  std::optional<Rectangle> anchorRect;
  PopoverPlacement placement = PopoverPlacement::Auto;
  int anchorId = 0;

  // --- First-class control state + callbacks (raym3 core owns interaction) ---
  ControlState control;
  std::function<void(float)> onValueChange; // Slider / RangeSlider
  std::function<void(bool)> onToggle;       // Checkbox / Switch / RadioButton
  std::function<void()> onRequestClose;     // Modal/picker scrim dismiss

  // --- RN-class responder/gesture edges (driven by Input.cpp) ---
  std::function<void()> onPressIn;
  std::function<void()> onPressOut;
  std::function<void()> onLongPress;
  std::function<void()> onHoverIn;
  std::function<void()> onHoverOut;
  std::function<void(Vector2)> onHoverMove;   // pointer pos (dp)
  std::function<void(Vector2)> onDragStart;   // start pos (dp)
  std::function<void(Vector2)> onDragMove;    // delta since start (dp)
  std::function<void(Vector2)> onDragEnd;     // final delta (dp)

  // --- Layout reporting + focus + accessibility ---
  std::function<void(Rectangle)> onLayout; // fired when measured rect changes
  std::function<void()> onFocus;
  std::function<void()> onBlur;
  bool focusable = false;
  int tabIndex = 0;
  AccessibilityInfo accessibility;

  // Transient per-frame interaction bookkeeping (set by Input.cpp).
  bool hovered = false;
  Rectangle reportedLayout = {0, 0, 0, 0};
  bool reportedLayoutValid = false;
};

NodePtr View(const ViewProps &props, std::vector<NodePtr> children = {});
NodePtr Text(std::string text, const TextProps &props = {});
NodePtr TextInput(const TextInputProps &props);
NodePtr Button(const ButtonProps &props, std::vector<NodePtr> children = {});
NodePtr Custom(const ViewProps &props, std::function<void(Rectangle)> render,
               std::vector<NodePtr> children = {});

} // namespace raym3::v2
