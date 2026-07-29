#include "raym3/v2/Renderer.h"
#include "raym3/v2/TextSelectionOverlay.h"
#include "raym3/v2/RenderContext.h"
#include "raym3/v2/TextInput.h"

#include "raym3/components/Button.h"
#include "raym3/fonts/FontManager.h"
#include "raym3/raym3.h"
#include "raym3/rendering/Renderer.h"
#include "raym3/styles/Theme.h"
#include "raym3/v2/Controls.h"
#include "raym3/v2/Density.h"
#include "raym3/v2/Input.h"
#include "raym3/v2/MaterialTokens.h"
#include "raym3/v2/Ripple.h"
#include "raym3/v2/TextEngine.h"
#include "raym3/v2/EmojiFont.h"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>

#ifndef RAYM3_USE_YOGA
#define RAYM3_USE_YOGA 0
#endif

#if RAYM3_USE_YOGA
#include <yoga/Yoga.h>
#endif

namespace raym3::v2 {

// M3 default backdrop scrim alpha (style.scrimOpacity overrides per-overlay).
static constexpr float kDefaultScrimOpacity = 0.32f;
static thread_local float g_renderOpacity = 1.0f;

// Inherited text color (CSS `color` cascade). A node whose style sets a text
// color establishes it for its subtree; a Text with no color of its own uses
// this, falling back to the theme's onSurface. Lets an app set one global
// (theme-aware) default color high in the tree — like `body { color }` on the
// web — instead of hardcoding a color on every Text.
static thread_local std::optional<Color> g_inheritedTextColor;

// Resolve the color for a text draw: the node's own color wins, then the
// inherited cascade, then the theme default (which flips with light/dark).
static Color ResolveTextColor(const std::optional<Color> &own) {
  if (own) return *own;
  if (g_inheritedTextColor) return *g_inheritedTextColor;
  return Theme::GetColorScheme().onSurface;
}

// ─── viewport culling ────────────────────────────────────────────────────────
// Stack of visible-region rectangles in node->layout (DP) space. Seeded with
// the render bounds; a clipping node (overflow hidden/scroll) intersects its own
// rect and pushes it for its subtree. RenderNode skips painting a node whose
// rect falls outside the current region. Paint-only: layout and the animation
// tick still run, so animation timelines advance and geometry stays correct —
// an off-screen animated node returns to view at the right position, it just
// isn't drawn while off-screen.
static thread_local std::vector<Rectangle> g_cullStack;

static bool RectsOverlap(const Rectangle& a, const Rectangle& b) {
  return a.x < b.x + b.width && a.x + a.width > b.x &&
         a.y < b.y + b.height && a.y + a.height > b.y;
}

static Rectangle IntersectRect(const Rectangle& a, const Rectangle& b) {
  float x = std::max(a.x, b.x);
  float y = std::max(a.y, b.y);
  float r = std::min(a.x + a.width, b.x + b.width);
  float btm = std::min(a.y + a.height, b.y + b.height);
  return {x, y, std::max(0.0f, r - x), std::max(0.0f, btm - y)};
}

float CurrentRenderOpacity() { return g_renderOpacity; }

// raylib's ColorAlpha REPLACES the alpha channel (result.a = 255*alpha), which
// silently discards a colour's own alpha. Every user-supplied colour
// (background, border, gradient stops, shadows) must instead have the render
// opacity SCALED into whatever alpha it already carries, or `#RRGGBBAA`,
// `rgb(r g b / 50%)` and packed 0xRRGGBBAA all paint fully opaque.
Color ScaleAlpha(Color color, float factor) {
  const float a = (float)color.a * std::clamp(factor, 0.0f, 1.0f);
  return Color{color.r, color.g, color.b, (unsigned char)std::lround(std::clamp(a, 0.0f, 255.0f))};
}

Color ApplyRenderOpacity(Color color) {
  return ScaleAlpha(color, g_renderOpacity);
}

// All mutable render/input/text-input state lives in RenderContext (see
// raym3/v2/RenderContext.h); FixedNode/StackEntry are defined there too.

static std::string TextInputStateKey(const Node &node) {
  if (!node.id.empty()) {
    return "id:" + node.id;
  }
  if (node.textInput.value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "value:%p",
                  static_cast<const void *>(node.textInput.value));
    return buffer;
  }
  if (node.textInput.buffer) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "buffer:%p",
                  static_cast<const void *>(node.textInput.buffer));
    return buffer;
  }
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "node:%p",
                static_cast<const void *>(&node));
  return buffer;
}

static float DefaultNodeHeight(const Node &node) {
  switch (node.kind) {
  case NodeKind::Button:
    return 40.0f;
  case NodeKind::TextInput:
    return 56.0f;
  case NodeKind::Text: {
    const float fontSize =
        node.style.text.fontSize.value_or(Theme::GetTypographyScale().bodyMedium);
    return node.style.height.value_or(ResolveLineHeight(node.style.text, fontSize));
  }
  default:
    return 0.0f;
  }
}

static float DefaultNodeWidth(const Node &node) {
  switch (node.kind) {
  case NodeKind::Button:
    return std::max(64.0f, 32.0f + static_cast<float>(node.text.size()) * 8.0f);
  case NodeKind::TextInput:
    return 240.0f;
  case NodeKind::Text: {
    float fontSize =
        node.style.text.fontSize.value_or(Theme::GetTypographyScale().bodyMedium);
    FontWeight weight = node.style.text.weight.value_or(FontWeight::Regular);
    return Renderer::MeasureText(node.text.c_str(), fontSize, weight).x;
  }
  default:
    return 0.0f;
  }
}

// True when EffectiveStyle would return `node.style` unchanged: no interaction
// state override applies and the nav-rail adjustment does not kick in. This is
// the overwhelmingly common case (every plain View/Text, every list row).
static inline bool EffectiveStyleIsBase(const Node &node) {
  if (node.inNavigationRail && node.role == NodeRole::NavItem) return false;
  switch (node.state) {
  case ComponentState::Hovered:  return !node.stateStyles.hovered;
  case ComponentState::Pressed:  return !node.stateStyles.pressed;
  case ComponentState::Focused:  return !node.stateStyles.focused;
  case ComponentState::Disabled: return !node.stateStyles.disabled;
  default: return true;
  }
}

static Style EffectiveStyle(const Node &node);

// Copy-free EffectiveStyle for read-only callers. `Style` is a ~60-optional
// struct holding vectors and strings, so returning it by value costs a heap
// allocation per call — and the per-frame layout/paint walks call it several
// times PER NODE (once for the node, once per child for the Fixed test, once
// for stretch resolution). Handing back a reference to node.style on the base
// path removes that entirely; the rare adjusted path fills the caller's
// scratch. Callers that MUTATE the result must keep using EffectiveStyle.
static inline const Style &EffectiveStyleRef(const Node &node, Style &scratch) {
  if (EffectiveStyleIsBase(node)) return node.style;
  scratch = EffectiveStyle(node);
  return scratch;
}

// Fixed-position test without materializing a Style at all.
static inline bool EffectiveIsFixed(const Node &node) {
  if (EffectiveStyleIsBase(node)) return node.style.position == PositionType::Fixed;
  Style scratch;
  return EffectiveStyleRef(node, scratch).position == PositionType::Fixed;
}

static Style EffectiveStyle(const Node &node) {
  Style style = ResolveStateStyle(node.style, node.stateStyles, node.state);
  if (node.inNavigationRail && node.role == NodeRole::NavItem) {
    style.height = 56.0f;
    style.minHeight = 56.0f;
    if (style.flexDirection.value_or(FlexDirection::Column) == FlexDirection::Row) {
      style.padding.top = 0.0f;
      style.padding.bottom = 0.0f;
      style.alignItems = Align::Center;
    } else {
      style.padding.top = 2.0f;
      style.padding.bottom = 2.0f;
      style.alignItems = Align::Center;
    }
  }
  return style;
}

static void MarkNavigationBarContext(const NodePtr &node, bool inBar) {
  if (!node)
    return;
  node->inNavigationBar = inBar || node->role == NodeRole::NavigationBar;
  for (const NodePtr &child : node->children) {
    MarkNavigationBarContext(child, node->inNavigationBar);
  }
}

static void MarkNavigationRailContext(const NodePtr &node, bool inRail,
                                      float railOpenAmount) {
  if (!node)
    return;
  node->inNavigationRail = inRail;
  node->navigationRailOpenAmount = inRail ? railOpenAmount : 0.0f;
  bool childInRail = inRail || node->role == NodeRole::NavigationRail;
  float childRailOpenAmount =
      node->role == NodeRole::NavigationRail ? node->animSelect : railOpenAmount;
  if (node->role == NodeRole::NavigationRail) {
    // Spin the leading expand/collapse button's icon a full turn as the rail
    // opens or closes, so the menu<->menu_open glyph swap reads as a rotation
    // rather than a hard cut. The leading button is the first child that isn't
    // a nav item; rotate its icon (first grandchild) in place.
    float spin = std::clamp(node->animSelect, 0.0f, 1.0f) * 360.0f;
    for (const NodePtr &child : node->children) {
      if (!child || child->role == NodeRole::NavItem)
        continue;
      const NodePtr &target =
          (!child->children.empty() && child->children.front())
              ? child->children.front()
              : child;
      target->style.rotation = spin;
      break;
    }
  }
  for (const NodePtr &child : node->children) {
    MarkNavigationRailContext(child, childInRail, childRailOpenAmount);
  }
}

// Shift a node and its whole subtree horizontally. Layout rects are absolute,
// so recentering the app-bar title means translating the title node and every
// descendant by the same dx.
static void TranslateSubtreeX(const NodePtr &node, float dx) {
  if (!node)
    return;
  node->layout.x += dx;
  for (const NodePtr &child : node->children)
    TranslateSubtreeX(child, dx);
}

static const Node *FindNavigationIconChild(const Node &item) {
  for (const NodePtr &child : item.children) {
    if (child && child->kind != NodeKind::Text)
      return child.get();
  }
  return item.children.empty() || !item.children.front()
             ? nullptr
             : item.children.front().get();
}

static float CubicBezier(float x1, float y1, float x2, float y2, float x) {
  x = std::clamp(x, 0.0f, 1.0f);
  float lo = 0.0f;
  float hi = 1.0f;
  for (int i = 0; i < 12; ++i) {
    float t = (lo + hi) * 0.5f;
    float u = 1.0f - t;
    float bx = 3.0f * u * u * t * x1 + 3.0f * u * t * t * x2 + t * t * t;
    if (bx < x)
      lo = t;
    else
      hi = t;
  }
  float t = (lo + hi) * 0.5f;
  float u = 1.0f - t;
  return 3.0f * u * u * t * y1 + 3.0f * u * t * t * y2 + t * t * t;
}

static float FlutterEaseInOutCubicEmphasized(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  constexpr float mx = 0.166666f;
  constexpr float my = 0.4f;
  if (t <= mx) {
    return CubicBezier(0.05f / mx, 0.0f / my, 0.133333f / mx,
                       0.06f / my, t / mx) *
           my;
  }
  return my + CubicBezier((0.208333f - mx) / (1.0f - mx),
                          (0.82f - my) / (1.0f - my),
                          (0.25f - mx) / (1.0f - mx),
                          (1.0f - my) / (1.0f - my),
                          (t - mx) / (1.0f - mx)) *
                  (1.0f - my);
}

// Walks the parent map from `node` up to the root, returning true if
// `ancestor` is `node` itself or any of its ancestors.
static bool NodeWithinSubtree(const Node *node, const Node *ancestor) {
  const Node *curr = node;
  while (curr) {
    if (curr == ancestor)
      return true;
    auto it = Ctx().parentMap.find(const_cast<Node *>(curr));
    curr = (it != Ctx().parentMap.end()) ? it->second.get() : nullptr;
  }
  return false;
}

// Whether this node participates in the paint-order input stack. CSS default:
// every laid-out box is a hit target unless pointer-events: none. Explicit
// capturesInput / hasScrim / onPress always block. This is what prevents taps
// from falling through transparent flex children inside overlays (pickers,
// bottom sheets) or user z-index layers to content painted underneath.
static bool NodeOccludesInput(const Node &node, const Style &style) {
  if (node.capturesInput || node.hasScrim || node.onPress ||
      node.onDragStart || node.onDragMove || node.onDragEnd ||
      node.kind == NodeKind::TextInput || node.kind == NodeKind::Button ||
      IsControlKind(node.kind))
    return true;
  if (style.pointerEvents == PointerEvents::None)
    return false;
  bool hasBgColor =
      style.backgroundColor.has_value() && (style.backgroundColor->a > 0);
  bool hasBgGradient =
      style.backgroundGradient && (style.backgroundGradient->stops.size() >= 2);
  if (hasBgColor || hasBgGradient)
    return true;
  return node.layout.width > 0.0f && node.layout.height > 0.0f;
}

// pointer-events:none passes hits through for plain layout boxes, but nodes
// that explicitly capture input (native checkbox/slider hosts, modals, etc.)
// must still win hit testing.
static bool NodeReceivesInput(const Node &node, const Style &style) {
  if (node.capturesInput || node.hasScrim || node.onPress ||
      node.kind == NodeKind::TextInput || node.kind == NodeKind::Button ||
      IsControlKind(node.kind))
    return true;
  return style.pointerEvents != PointerEvents::None;
}

// Does this node carry an interaction handler (the climb target for a press)?
static bool NodeIsInteractive(const Node &node) {
  return node.onPress || node.onValueChange || node.onToggle ||
         node.onRequestClose || node.onLongPress || node.onPressIn ||
         node.onPressOut || node.onDragStart || node.onDragMove ||
         node.onDragEnd || node.focusable || IsControlKind(node.kind) ||
         node.kind == NodeKind::Button || node.kind == NodeKind::TextInput;
}

// True if the committed hovered/active node is `node` itself or a descendant
// of it — i.e. `node` sits on the resolved interaction path. Reads the
// previous frame's ResolveInput result (1-frame lag on hover visuals only).
static bool NodeOnInputPath(const Node &node, NodeId targetId) {
  if (targetId == 0)
    return false;
  Node *curr = reinterpret_cast<Node *>(targetId);
  while (curr) {
    if (curr == &node)
      return true;
    auto it = Ctx().committedParentMap.find(curr);
    curr = (it != Ctx().committedParentMap.end()) ? it->second.get() : nullptr;
  }
  return false;
}

static NodeId GetInnermostPressId() {
  NodeId active = GetActiveId();
  if (active != 0)
    return active;
  const PointerInput &p = GetPointer();
  if (p.down)
    return GetPendingPressId();
  return 0;
}

bool IsNodePressed(const Node &node) {
  NodeId pressId = GetInnermostPressId();
  return pressId != 0 && pressId == IdOf(&node);
}

static ComponentState ComputeState(const Node &node) {
  if (node.disabled)
    return ComponentState::Disabled;
  if (node.style.pointerEvents == PointerEvents::None)
    return ComponentState::Default;
  // Containers without a press handler are structural — never interactive.
  if (!node.onPress)
    return ComponentState::Default;

  // Hover/press derive from the unified input pass (z-aware + occlusion-aware),
  // so content behind a higher-z scrim never lights up — no modal special case.
  if (IsNodePressed(node))
    return ComponentState::Pressed;
#if defined(__ANDROID__) || defined(RAYACT_ANDROID) || defined(PLATFORM_ANDROID)
  // Touch devices have no persistent hover state.
  return ComponentState::Default;
#else
  if (NodeOnInputPath(node, GetHoveredId()))
    return ComponentState::Hovered;
  return ComponentState::Default;
#endif
}

// Returns (and caches) the PreparedText for a Text node.
// Pretext pattern: prepare once per text+font change, layout many times.
static const PreparedText& GetOrPrepare(const Node* node) {
  float fontSize = node->style.text.fontSize.value_or(16.0f);
  FontWeight weight = node->style.text.weight.value_or(FontWeight::Regular);
  const std::string& family = node->style.text.fontFamily.value_or(std::string{});
  WhiteSpace whiteSpace = node->style.text.whiteSpace.value_or(
      node->kind == NodeKind::TextInput ? WhiteSpace::PreWrap : WhiteSpace::Normal);
  WordBreak wordBreak = node->style.text.wordBreak.value_or(WordBreak::Normal);
  float letterSpacing = node->style.text.letterSpacing.value_or(0.25f);
  const int maxLines = std::max(0, node->style.text.maxLines.value_or(0));
  const TextOverflow overflow =
      node->style.text.overflow.value_or(TextOverflow::Clip);
  const float lineHeight = ResolveLineHeight(node->style.text, fontSize);
  std::string key = TextCacheKey(node->text, fontSize, weight, family, whiteSpace,
                                 wordBreak, letterSpacing);
  // The prepared layout bakes lineHeight, the clamp and the ellipsis mode, so
  // they have to take part in the cache identity — otherwise a node that gains
  // `numberOfLines` keeps serving its unclamped layout forever.
  key += ':' + std::to_string(static_cast<int>(lineHeight * 100.0f)) + ':' +
         std::to_string(maxLines) + ':' + std::to_string(static_cast<int>(overflow));

  if (!node->preparedTextCache || node->preparedTextKey != key) {
    TextLayoutOptions opts;
    opts.fontSize   = fontSize;
    opts.lineHeight = lineHeight;
    opts.letterSpacing = letterSpacing;
    opts.weight     = weight;
    opts.fontFamily = family;
    opts.whiteSpace = whiteSpace;
    opts.wordBreak  = wordBreak;
    opts.maxLines   = maxLines;
    opts.overflow   = overflow;
    node->preparedTextCache = PrepareText(node->text, opts);
    node->preparedTextKey   = std::move(key);
  }
  return *node->preparedTextCache;
}

static float ClampScrollOffset(float offset, float contentSize, float viewportSize) {
  float maxOffset = std::max(0.0f, contentSize - viewportSize);
  return std::max(0.0f, std::min(offset, maxOffset));
}

// Clamp both axes to this frame's content size, then apply the follow-end pin.
// Every layout path routes through here so the two writes stay attributable:
// the clamp is symmetric, but the follow-end pin only ever pushes toward the
// bottom, which makes it the prime suspect for direction-dependent behaviour.
static void ClampScrollOffsetsForLayout(const NodePtr &node,
                                        bool applyFollowEnd) {
  const float oldX = node->scrollOffsetX;
  const float oldY = node->scrollOffsetY;
  node->scrollOffsetX = ClampScrollOffset(
      node->scrollOffsetX, node->scrollContentWidth, node->layout.width);
  node->scrollOffsetY = ClampScrollOffset(
      node->scrollOffsetY, node->scrollContentHeight, node->layout.height);
  ScrollTraceOffsetWrite(*node, ScrollWriteSource::Clamp, 'x', oldX,
                         node->scrollOffsetX);
  ScrollTraceOffsetWrite(*node, ScrollWriteSource::Clamp, 'y', oldY,
                         node->scrollOffsetY);
  if (applyFollowEnd && node->scrollFollowEnd) {
    const float pinned =
        std::max(0.0f, node->scrollContentHeight - node->layout.height);
    ScrollTraceOffsetWrite(*node, ScrollWriteSource::FollowEnd, 'y',
                           node->scrollOffsetY, pinned);
    node->scrollOffsetY = pinned;
  }
}

static float MeasureNodeHeight(const NodePtr &node, float contentW) {
  Style style = EffectiveStyle(*node);
  if (style.height) {
    float h = *style.height;
    if (style.minHeight) h = std::max(h, *style.minHeight);
    if (style.maxHeight) h = std::min(h, *style.maxHeight);
    return h;
  }

  if (node->kind == NodeKind::Text) {
    const PreparedText& prep = GetOrPrepare(node.get());
    TextLayoutResult res = LayoutText(prep, contentW);
    float textH = res.height;
    if (style.minHeight) textH = std::max(textH, *style.minHeight);
    if (style.maxHeight) textH = std::min(textH, *style.maxHeight);
    return textH;
  }

  if (node->children.empty()) {
    float h = DefaultNodeHeight(*node);
    if (style.minHeight) h = std::max(h, *style.minHeight);
    if (style.maxHeight) h = std::min(h, *style.maxHeight);
    return h;
  }

  float padT = style.padding.Top();
  float padB = style.padding.Bottom();
  float gap = style.gap.value_or(0.0f);
  bool row = style.flexDirection.value_or(FlexDirection::Column) == FlexDirection::Row;

  float measuredH = padT + padB;
  if (row) {
    float maxChildH = 0.0f;
    for (const auto &child : node->children) {
      if (!child) continue;
      Style cs = EffectiveStyle(*child);
      if (cs.display == Display::None) continue;
      float childW = cs.width.value_or(DefaultNodeWidth(*child));
      float childH = MeasureNodeHeight(child, childW);
      maxChildH = std::max(maxChildH, childH);
    }
    measuredH += maxChildH;
  } else {
    float sumH = 0.0f;
    size_t visibleCount = 0;
    for (const auto &child : node->children) {
      if (!child) continue;
      Style cs = EffectiveStyle(*child);
      if (cs.display == Display::None) continue;
      if (cs.position == PositionType::Fixed || cs.position == PositionType::Absolute)
        continue;
      float childW = cs.width.value_or(contentW);
      float childH = MeasureNodeHeight(child, childW);
      sumH += childH;
      visibleCount++;
    }
    measuredH += sumH;
    if (visibleCount > 1) {
      measuredH += gap * (visibleCount - 1);
    }
  }

  if (style.minHeight) measuredH = std::max(measuredH, *style.minHeight);
  if (style.maxHeight) measuredH = std::min(measuredH, *style.maxHeight);
  return measuredH;
}

#if RAYM3_USE_YOGA
static YGFlexDirection ToYogaFlexDirection(FlexDirection direction) {
  switch (direction) {
  case FlexDirection::Row:
    return YGFlexDirectionRow;
  case FlexDirection::RowReverse:
    return YGFlexDirectionRowReverse;
  case FlexDirection::ColumnReverse:
    return YGFlexDirectionColumnReverse;
  case FlexDirection::Column:
  default:
    return YGFlexDirectionColumn;
  }
}

static YGJustify ToYogaJustify(Justify justify) {
  switch (justify) {
  case Justify::FlexEnd:
    return YGJustifyFlexEnd;
  case Justify::Center:
    return YGJustifyCenter;
  case Justify::SpaceBetween:
    return YGJustifySpaceBetween;
  case Justify::SpaceAround:
    return YGJustifySpaceAround;
  case Justify::SpaceEvenly:
    return YGJustifySpaceEvenly;
  case Justify::FlexStart:
  default:
    return YGJustifyFlexStart;
  }
}

static YGAlign ToYogaAlign(Align align) {
  switch (align) {
  case Align::Auto:
    return YGAlignAuto;
  case Align::FlexStart:
    return YGAlignFlexStart;
  case Align::FlexEnd:
    return YGAlignFlexEnd;
  case Align::Center:
    return YGAlignCenter;
  case Align::Baseline:
    return YGAlignBaseline;
  case Align::Stretch:
  default:
    return YGAlignStretch;
  }
}

// Yoga measure function for Text nodes — called by Yoga with the available
// width so LayoutText can wrap correctly and report the real height.
// Uses cached PreparedText — only LayoutText (pure math) runs per Yoga call.
static YGSize MeasureTextNode(YGNodeConstRef ygNode, float width,
                               YGMeasureMode widthMode, float /*height*/,
                               YGMeasureMode heightMode) {
  const Node* node = static_cast<const Node*>(YGNodeGetContext(ygNode));
  if (!node) return {0, 0};

  const PreparedText& prep = GetOrPrepare(node);
  float maxW = (widthMode != YGMeasureModeUndefined) ? width : 0.0f;
  TextLayoutResult res = LayoutText(prep, maxW);

  float outW = res.width;
  float outH = res.height;
  if (widthMode == YGMeasureModeExactly)      outW = width;
  else if (widthMode == YGMeasureModeAtMost)  outW = std::min(outW, width);
  if (heightMode == YGMeasureModeExactly)     outH = outH; // already exact

  return {outW, outH};
}

// True when a child of this node with an auto width will be stretched to the
// parent's content width — i.e. the parent is a column (width is the cross
// axis) and it does not opt out of the default `align-items: stretch`.
static bool StretchesChildWidth(const Style &parentStyle) {
  const FlexDirection dir = parentStyle.flexDirection.value_or(FlexDirection::Column);
  const bool column = dir == FlexDirection::Column || dir == FlexDirection::ColumnReverse;
  if (!column) return false;
  return !parentStyle.alignItems || *parentStyle.alignItems == Align::Stretch;
}

static void ApplyYogaStyle(YGNodeRef ygNode, const Node &node, bool isRoot,
                           bool parentStretchesWidth) {
  Style effScratch;
  const Style &style = EffectiveStyleRef(node, effScratch);

  if (style.display == Display::None) {
    YGNodeStyleSetDisplay(ygNode, YGDisplayNone);
  }

  // Fixed-position nodes are excluded from their parent's flex layout — they
  // are positioned relative to the viewport and rendered in a separate pass.
  if (style.position == PositionType::Fixed) {
    YGNodeStyleSetDisplay(ygNode, YGDisplayNone);
    return;
  }

  if (style.overflow == Overflow::Scroll) {
    YGNodeStyleSetOverflow(ygNode, YGOverflowScroll);
  } else if (style.overflow == Overflow::Hidden) {
    YGNodeStyleSetOverflow(ygNode, YGOverflowHidden);
  }

  YGNodeStyleSetFlexDirection(
      ygNode, ToYogaFlexDirection(style.flexDirection.value_or(FlexDirection::Column)));
  if (style.flexWrap) {
    YGWrap wrap = YGWrapNoWrap;
    if (*style.flexWrap == FlexWrap::Wrap) wrap = YGWrapWrap;
    else if (*style.flexWrap == FlexWrap::WrapReverse) wrap = YGWrapWrapReverse;
    YGNodeStyleSetFlexWrap(ygNode, wrap);
  }
  if (style.justifyContent)
    YGNodeStyleSetJustifyContent(ygNode, ToYogaJustify(*style.justifyContent));
  if (style.alignItems)
    YGNodeStyleSetAlignItems(ygNode, ToYogaAlign(*style.alignItems));
  if (style.alignSelf)
    YGNodeStyleSetAlignSelf(ygNode, ToYogaAlign(*style.alignSelf));

  if (style.position == PositionType::Absolute)
    YGNodeStyleSetPositionType(ygNode, YGPositionTypeAbsolute);
  else
    YGNodeStyleSetPositionType(ygNode, YGPositionTypeRelative);

  // Text nodes use a Yoga measure function — don't hardcode width/height from
  // a full-line measure (that ignores wrapping and overflows the parent).
  bool isText = (node.kind == NodeKind::Text);

  // react-native parity: a TextInput has no intrinsic width — it fills the
  // parent, the way `align-items: stretch` (Yoga's default) sizes any auto-width
  // child. The old fixed 240dp default made every field the same stubby box
  // regardless of its container, so every call site had to restate width: '100%'.
  // The intrinsic width is kept only where stretching cannot happen (a row
  // parent, or a parent that opts out of stretch) and the node is not flex-sized,
  // so a field in a row still shows up instead of collapsing to nothing.
  const bool selfOptsOutOfStretch = style.alignSelf && *style.alignSelf != Align::Stretch;
  const bool flexSized = style.flexGrow.value_or(0.0f) > 0.0f ||
                         style.flexBasis.has_value() || style.flexBasisPercent.has_value();
  const bool widthComesFromParent =
      (parentStretchesWidth && !selfOptsOutOfStretch) || flexSized;
  const bool skipIntrinsicWidth = node.kind == NodeKind::TextInput && widthComesFromParent;

  if (style.widthPercent)
    YGNodeStyleSetWidthPercent(ygNode, *style.widthPercent);
  else if (style.width)
    YGNodeStyleSetWidth(ygNode, *style.width);
  else if (!isText && !isRoot && node.children.empty() && !skipIntrinsicWidth &&
           DefaultNodeWidth(node) > 0.0f)
    YGNodeStyleSetWidth(ygNode, DefaultNodeWidth(node));

  if (style.heightPercent)
    YGNodeStyleSetHeightPercent(ygNode, *style.heightPercent);
  else if (style.height)
    YGNodeStyleSetHeight(ygNode, *style.height);
  else if (!isText && !isRoot && node.children.empty() && DefaultNodeHeight(node) > 0.0f)
    YGNodeStyleSetHeight(ygNode, DefaultNodeHeight(node));

  // Scroll containers must not report their content as their min-size, or the
  // flex parent expands to contain them (classic `min-height: 0` flex fix) and
  // nothing ever overflows. Default the unset min axis to 0 so the parent's
  // flex sizing wins and content overflows internally.
  bool scrolls = style.overflow == Overflow::Scroll;
  if (style.minWidthPercent)
    YGNodeStyleSetMinWidthPercent(ygNode, *style.minWidthPercent);
  else if (style.minWidth)
    YGNodeStyleSetMinWidth(ygNode, *style.minWidth);
  else if (scrolls)
    YGNodeStyleSetMinWidth(ygNode, 0.0f);
  if (style.minHeightPercent)
    YGNodeStyleSetMinHeightPercent(ygNode, *style.minHeightPercent);
  else if (style.minHeight)
    YGNodeStyleSetMinHeight(ygNode, *style.minHeight);
  else if (scrolls)
    YGNodeStyleSetMinHeight(ygNode, 0.0f);
  if (style.maxWidthPercent)
    YGNodeStyleSetMaxWidthPercent(ygNode, *style.maxWidthPercent);
  else if (style.maxWidth)
    YGNodeStyleSetMaxWidth(ygNode, *style.maxWidth);
  if (style.maxHeightPercent)
    YGNodeStyleSetMaxHeightPercent(ygNode, *style.maxHeightPercent);
  else if (style.maxHeight)
    YGNodeStyleSetMaxHeight(ygNode, *style.maxHeight);
  if (style.flexGrow)
    YGNodeStyleSetFlexGrow(ygNode, *style.flexGrow);
  // Web defaults flex-shrink to 1; Yoga defaults it to 0. Without shrink, a
  // flex child with flex-basis:auto (= content size) never shrinks back to a
  // definite parent, so tall content (e.g. a scroll list) inflates the whole
  // ancestor chain past the window instead of overflowing internally. Default
  // to 1 (web-compatible) so the chain stays constrained and scroll works.
  //
  // Text is the exception: Yoga has no `min-height: auto` (content floor), so a
  // shrinkable Text squished by a tall sibling (e.g. a scroll list) clips its
  // glyphs. Keep Text at shrink:0 so siblings with real overflow absorb the
  // shrink and headings stay legible.
  if (style.flexShrink)
    YGNodeStyleSetFlexShrink(ygNode, *style.flexShrink);
  else if (isText || isRoot)
    YGNodeStyleSetFlexShrink(ygNode, 0.0f);
  else
    YGNodeStyleSetFlexShrink(ygNode, 1.0f);
  if (style.flexBasisPercent)
    YGNodeStyleSetFlexBasisPercent(ygNode, *style.flexBasisPercent);
  else if (style.flexBasis)
    YGNodeStyleSetFlexBasis(ygNode, *style.flexBasis);

  if (style.gap)
    YGNodeStyleSetGap(ygNode, YGGutterAll, *style.gap);
  if (style.rowGap)
    YGNodeStyleSetGap(ygNode, YGGutterRow, *style.rowGap);
  if (style.columnGap)
    YGNodeStyleSetGap(ygNode, YGGutterColumn, *style.columnGap);

  if (style.margin.TopIsAuto())
    YGNodeStyleSetMarginAuto(ygNode, YGEdgeTop);
  else
    YGNodeStyleSetMargin(ygNode, YGEdgeTop, style.margin.Top());
  if (style.margin.RightIsAuto())
    YGNodeStyleSetMarginAuto(ygNode, YGEdgeRight);
  else
    YGNodeStyleSetMargin(ygNode, YGEdgeRight, style.margin.Right());
  if (style.margin.BottomIsAuto())
    YGNodeStyleSetMarginAuto(ygNode, YGEdgeBottom);
  else
    YGNodeStyleSetMargin(ygNode, YGEdgeBottom, style.margin.Bottom());
  if (style.margin.LeftIsAuto())
    YGNodeStyleSetMarginAuto(ygNode, YGEdgeLeft);
  else
    YGNodeStyleSetMargin(ygNode, YGEdgeLeft, style.margin.Left());
  YGNodeStyleSetPadding(ygNode, YGEdgeTop, style.padding.Top());
  YGNodeStyleSetPadding(ygNode, YGEdgeRight, style.padding.Right());
  YGNodeStyleSetPadding(ygNode, YGEdgeBottom, style.padding.Bottom());
  YGNodeStyleSetPadding(ygNode, YGEdgeLeft, style.padding.Left());

  if (style.position == PositionType::Absolute || style.position == PositionType::Relative) {
    if (style.inset.top)
      YGNodeStyleSetPosition(ygNode, YGEdgeTop, *style.inset.top);
    if (style.inset.right)
      YGNodeStyleSetPosition(ygNode, YGEdgeRight, *style.inset.right);
    if (style.inset.bottom)
      YGNodeStyleSetPosition(ygNode, YGEdgeBottom, *style.inset.bottom);
    if (style.inset.left)
      YGNodeStyleSetPosition(ygNode, YGEdgeLeft, *style.inset.left);
  }
}

static YGNodeRef BuildYogaTree(const NodePtr &node, bool isRoot,
                               bool parentStretchesWidth = true) {
  YGNodeRef ygNode = YGNodeNew();
  Ctx().lastStats.yogaNodesBuilt++;
  YGNodeSetContext(ygNode, node.get());
  ApplyYogaStyle(ygNode, *node, isRoot, parentStretchesWidth);

  // Text nodes: let Yoga call MeasureTextNode with the real available width
  // so wrapping and multi-line height are computed correctly.
  if (node->kind == NodeKind::Text) {
    YGNodeSetMeasureFunc(ygNode, MeasureTextNode);
  }

  Style stretchScratch;
  const bool stretchesChildren =
      StretchesChildWidth(EffectiveStyleRef(*node, stretchScratch));
  for (const NodePtr &child : node->children) {
    YGNodeRef ygChild = BuildYogaTree(child, false, stretchesChildren);
    YGNodeInsertChild(ygNode, ygChild, YGNodeGetChildCount(ygNode));
  }

  return ygNode;
}

static void StoreYogaLayout(const NodePtr &node, YGNodeRef ygNode, float parentX,
                            float parentY) {
  node->previousLayout = node->layout;
  float x = parentX + YGNodeLayoutGetLeft(ygNode);
  float y = parentY + YGNodeLayoutGetTop(ygNode);
  node->layout = {x, y, YGNodeLayoutGetWidth(ygNode),
                  YGNodeLayoutGetHeight(ygNode)};

  Style effScratch;
  const Style &style = EffectiveStyleRef(*node, effScratch);
  bool scrolls = style.overflow == Overflow::Scroll;

  // Measure this frame's content extent from Yoga's own child geometry BEFORE
  // applying any scroll offset. Yoga has already laid out the whole subtree, so
  // a direct child's Left/Top/Width/Height are valid here and don't depend on
  // this node's scrollOffset (that offset is only a translation applied when
  // committing descendants below). Computing content size up front lets the
  // clamp — and scrollFollowEnd in particular — use THIS frame's size, so
  // appending a child while auto-scrolling to the end lands correctly in the
  // same layout pass instead of clipping the newest row for one frame.
  uint32_t count = YGNodeGetChildCount(ygNode);
  float contentRight = node->layout.width;
  float contentBottom = node->layout.height;
  for (uint32_t i = 0; i < count; ++i) {
    // Fixed-position children are display:none in this Yoga pass (laid out by
    // LayoutFixed against the viewport instead) and don't contribute to
    // scrollable content.
    if (EffectiveIsFixed(*node->children[i]))
      continue;
    YGNodeRef ygChild = YGNodeGetChild(ygNode, i);
    contentRight = std::max(contentRight, YGNodeLayoutGetLeft(ygChild) + YGNodeLayoutGetWidth(ygChild));
    contentBottom = std::max(contentBottom, YGNodeLayoutGetTop(ygChild) + YGNodeLayoutGetHeight(ygChild));
  }
  node->scrollContentWidth = contentRight;
  node->scrollContentHeight = contentBottom;

  if (scrolls) {
    ClampScrollOffsetsForLayout(node, /*applyFollowEnd=*/true);
  } else {
    node->scrollOffsetX = 0.0f;
    node->scrollOffsetY = 0.0f;
  }

  // Now commit children using the corrected (clamped/followed) offset.
  float childParentX = x - node->scrollOffsetX;
  float childParentY = y - node->scrollOffsetY;
  for (uint32_t i = 0; i < count; ++i) {
    // Skip Fixed children here too: writing their 0x0 Yoga result would clobber
    // the fixed-pass layout that hit-testing and ancestor-clip checks read.
    if (EffectiveIsFixed(*node->children[i]))
      continue;
    StoreYogaLayout(node->children[i], YGNodeGetChild(ygNode, i), childParentX, childParentY);
  }
}
#endif

static void LayoutFallback(const NodePtr &node, Rectangle bounds) {
  node->previousLayout = node->layout;
  Style style = EffectiveStyle(*node);

  float marginLeft = style.margin.Left();
  float marginTop = style.margin.Top();
  float marginRight = style.margin.Right();
  float marginBottom = style.margin.Bottom();
  float width = style.width.value_or(bounds.width - marginLeft - marginRight);
  if (style.minWidth) width = std::max(width, *style.minWidth);
  if (style.maxWidth) width = std::min(width, *style.maxWidth);
  if (width <= 0.0f)
    width = DefaultNodeWidth(*node);

  float height = style.height.value_or(
      node->children.empty() ? DefaultNodeHeight(*node)
                             : bounds.height - marginTop - marginBottom);
  if (style.minHeight) height = std::max(height, *style.minHeight);
  if (style.maxHeight) height = std::min(height, *style.maxHeight);
  if (height <= 0.0f)
    height = DefaultNodeHeight(*node);

  node->layout = {bounds.x + marginLeft, bounds.y + marginTop, width, height};

  float padLeft = style.padding.Left();
  float padTop = style.padding.Top();
  float padRight = style.padding.Right();
  float padBottom = style.padding.Bottom();
  float gap = style.gap.value_or(0.0f);
  bool row = style.flexDirection == FlexDirection::Row;
  float cursorX = node->layout.x + padLeft;
  float cursorY = node->layout.y + padTop;
  float contentW = std::max(0.0f, node->layout.width - padLeft - padRight);
  float contentH = std::max(0.0f, node->layout.height - padTop - padBottom);
  bool scrolls = style.overflow == Overflow::Scroll;
  if (scrolls) {
    ClampScrollOffsetsForLayout(node, /*applyFollowEnd=*/false);
    cursorX -= node->scrollOffsetX;
    cursorY -= node->scrollOffsetY;
  } else {
    node->scrollOffsetX = 0.0f;
    node->scrollOffsetY = 0.0f;
  }
  float contentRight = node->layout.width;
  float contentBottom = node->layout.height;

  for (const NodePtr &child : node->children) {
    Style childStyle = EffectiveStyle(*child);
    // Fixed-position and absolute children are excluded from normal flow.
    if (childStyle.position == PositionType::Fixed || childStyle.position == PositionType::Absolute)
      continue;
    float childW = childStyle.width.value_or(row ? DefaultNodeWidth(*child) : contentW);
    if (childStyle.minWidth) childW = std::max(childW, *childStyle.minWidth);
    if (childStyle.maxWidth) childW = std::min(childW, *childStyle.maxWidth);
    if (childW <= 0.0f)
      childW = contentW;

    float childH;
    if (childStyle.height) {
      childH = *childStyle.height;
    } else if (!child->children.empty()) {
      childH = MeasureNodeHeight(child, childW);
    } else {
      childH = row ? contentH : DefaultNodeHeight(*child);
    }
    if (childStyle.minHeight) childH = std::max(childH, *childStyle.minHeight);
    if (childStyle.maxHeight) childH = std::min(childH, *childStyle.maxHeight);
    if (childH <= 0.0f)
      childH = 40.0f;

    float childX = cursorX;
    float childY = cursorY;
    if (childStyle.position == PositionType::Relative) {
      if (childStyle.inset.left) childX += *childStyle.inset.left;
      else if (childStyle.inset.right) childX -= *childStyle.inset.right;

      if (childStyle.inset.top) childY += *childStyle.inset.top;
      else if (childStyle.inset.bottom) childY -= *childStyle.inset.bottom;
    }

    LayoutFallback(child, {childX, childY, childW, childH});
    if (row)
      cursorX += childW + gap;
    else
      cursorY += childH + gap;
    contentRight = std::max(contentRight, child->layout.x - node->layout.x + node->scrollOffsetX + child->layout.width);
    contentBottom = std::max(contentBottom, child->layout.y - node->layout.y + node->scrollOffsetY + child->layout.height);
  }

  // Pass 2: Layout absolute children relative to the parent bounds.
  for (const NodePtr &child : node->children) {
    Style childStyle = EffectiveStyle(*child);
    if (childStyle.position != PositionType::Absolute)
      continue;

    float childW = childStyle.width.value_or(DefaultNodeWidth(*child));
    if (childStyle.minWidth) childW = std::max(childW, *childStyle.minWidth);
    if (childStyle.maxWidth) childW = std::min(childW, *childStyle.maxWidth);
    if (childW <= 0.0f) childW = contentW;

    float childH;
    if (childStyle.height) {
      childH = *childStyle.height;
    } else if (!child->children.empty()) {
      childH = MeasureNodeHeight(child, childW);
    } else {
      childH = DefaultNodeHeight(*child);
    }
    if (childStyle.minHeight) childH = std::max(childH, *childStyle.minHeight);
    if (childStyle.maxHeight) childH = std::min(childH, *childStyle.maxHeight);
    if (childH <= 0.0f) childH = 40.0f;

    float childX = node->layout.x + padLeft;
    float childY = node->layout.y + padTop;

    if (childStyle.inset.left) {
      childX = node->layout.x + *childStyle.inset.left;
    } else if (childStyle.inset.right) {
      childX = node->layout.x + node->layout.width - *childStyle.inset.right - childW;
    }

    if (childStyle.inset.top) {
      childY = node->layout.y + *childStyle.inset.top;
    } else if (childStyle.inset.bottom) {
      childY = node->layout.y + node->layout.height - *childStyle.inset.bottom - childH;
    }

    LayoutFallback(child, {childX, childY, childW, childH});
    contentRight = std::max(contentRight, child->layout.x - node->layout.x + node->scrollOffsetX + child->layout.width);
    contentBottom = std::max(contentBottom, child->layout.y - node->layout.y + node->scrollOffsetY + child->layout.height);
  }

  node->scrollContentWidth = contentRight;
  node->scrollContentHeight = contentBottom;
  if (scrolls) {
    ClampScrollOffsetsForLayout(node, /*applyFollowEnd=*/true);
  }
}

void UpdateLayout(const NodePtr &root, Rectangle bounds) {
  if (!root)
    return;

  Ctx().lastStats.yogaNodesBuilt = 0;
  MarkNavigationRailContext(root, false, 0.0f);
  MarkNavigationBarContext(root, false);

#if RAYM3_USE_YOGA
  YGNodeRef ygRoot = BuildYogaTree(root, true);
  YGNodeStyleSetWidth(ygRoot, bounds.width);
  YGNodeStyleSetHeight(ygRoot, bounds.height);
  YGNodeCalculateLayout(ygRoot, bounds.width, bounds.height, YGDirectionLTR);
  StoreYogaLayout(root, ygRoot, bounds.x, bounds.y);
  YGNodeFreeRecursive(ygRoot);
#else
  LayoutFallback(root, bounds);
#endif
}

// --- Retained layout mirror --------------------------------------------------
// A persistent Yoga tree keyed by Node*, reconciled per call. See Renderer.h.
// Reuses ApplyYogaStyle / MeasureTextNode / StretchesChildWidth above, so the
// style mapping can never drift from the per-frame BuildYogaTree path.
#if RAYM3_USE_YOGA
namespace {

struct RetainedYG {
  YGNodeRef yg = nullptr;
  uint64_t gen = 0;          // mark-and-sweep visit stamp
  bool textHadPrepared = false;
};
std::unordered_map<const Node *, RetainedYG> g_retainedYoga;
uint64_t g_retainedGen = 0;

// Pristine style source: YGNodeCopyStyle from this blank node resets a reused
// yoga node to defaults before ApplyYogaStyle re-applies the current style —
// the retained equivalent of BuildYogaTree starting from a fresh YGNodeNew()
// (removed style keys must fall back to defaults, not linger).
YGNodeRef retainedBlankNode() {
  static YGNodeRef blank = YGNodeNew();
  return blank;
}

void retainedReconcile(const NodePtr &node, bool isRoot,
                       bool parentStretchesWidth,
                       RetainedLayoutStats &stats) {
  RetainedYG &r = g_retainedYoga[node.get()];
  if (!r.yg) {
    r.yg = YGNodeNew();
    YGNodeSetContext(r.yg, node.get());
    if (node->kind == NodeKind::Text)
      YGNodeSetMeasureFunc(r.yg, MeasureTextNode);
    stats.yogaNodesCreated++;
  }
  r.gen = g_retainedGen;
  stats.nodesReconciled++;

  // Reset-to-default + re-apply. Yoga's style setters compare before marking
  // dirty, so an unchanged node stays clean and YGNodeCalculateLayout can
  // short-circuit its subtree.
  YGNodeCopyStyle(r.yg, retainedBlankNode());
  ApplyYogaStyle(r.yg, *node, isRoot, parentStretchesWidth);

  // Text re-measure: the prepared-text cache is invalidated on any text or
  // text-style change; a missing cache means the measurement is stale.
  if (node->kind == NodeKind::Text) {
    const bool prepared = node->preparedTextCache.has_value();
    if (!prepared || !r.textHadPrepared) YGNodeMarkDirty(r.yg);
    r.textHadPrepared = prepared;
  }

  // Children: rebuild the yoga edge list only when it differs from the
  // retained tree's order (pointer compare — cheap in the common case).
  const uint32_t ygCount = YGNodeGetChildCount(r.yg);
  bool same = ygCount == node->children.size();
  if (same) {
    for (uint32_t i = 0; i < ygCount; ++i) {
      auto it = g_retainedYoga.find(node->children[i].get());
      if (it == g_retainedYoga.end() || YGNodeGetChild(r.yg, i) != it->second.yg) {
        same = false;
        break;
      }
    }
  }

  Style stretchScratch;
  const bool stretches =
      StretchesChildWidth(EffectiveStyleRef(*node, stretchScratch));
  for (const NodePtr &child : node->children)
    retainedReconcile(child, false, stretches, stats);

  if (!same) {
    YGNodeRemoveAllChildren(r.yg);
    // re-read: recursion above may have rehashed the map
    YGNodeRef selfYg = g_retainedYoga[node.get()].yg;
    for (const NodePtr &child : node->children) {
      YGNodeRef childYg = g_retainedYoga[child.get()].yg;
      if (YGNodeRef owner = YGNodeGetOwner(childYg))
        YGNodeRemoveChild(owner, childYg);
      YGNodeInsertChild(selfYg, childYg, YGNodeGetChildCount(selfYg));
    }
  }
}

void retainedPrune(RetainedLayoutStats &stats) {
  for (auto it = g_retainedYoga.begin(); it != g_retainedYoga.end();) {
    if (it->second.gen != g_retainedGen) {
      if (it->second.yg) {
        if (YGNodeRef owner = YGNodeGetOwner(it->second.yg))
          YGNodeRemoveChild(owner, it->second.yg);
        YGNodeFree(it->second.yg);
      }
      it = g_retainedYoga.erase(it);
      stats.yogaNodesFreed++;
    } else {
      ++it;
    }
  }
}

// Compare in parent-relative coordinates: StoreYogaLayout accumulates parent
// origins and applies scroll translations to descendants; relative offsets
// sidestep both (child.x - parent.x + parent.scrollOffset == yoga Left).
void retainedCompare(const NodePtr &node, const Node *parent, Rectangle bounds,
                     int &logged, int maxLog, RetainedLayoutStats &stats) {
  const Style style = EffectiveStyle(*node);
  if (style.position == PositionType::Fixed) return;  // separate layout pass
  if (style.display == Display::None) return;

  auto it = g_retainedYoga.find(node.get());
  if (it == g_retainedYoga.end() || !it->second.yg) return;
  YGNodeRef yg = it->second.yg;

  constexpr float kEps = 0.75f;
  const float relX = parent ? node->layout.x - parent->layout.x + parent->scrollOffsetX
                            : node->layout.x - bounds.x;
  const float relY = parent ? node->layout.y - parent->layout.y + parent->scrollOffsetY
                            : node->layout.y - bounds.y;
  const float dx = std::fabs(relX - YGNodeLayoutGetLeft(yg));
  const float dy = std::fabs(relY - YGNodeLayoutGetTop(yg));
  const float dw = std::fabs(node->layout.width - YGNodeLayoutGetWidth(yg));
  const float dh = std::fabs(node->layout.height - YGNodeLayoutGetHeight(yg));
  if (dx > kEps || dy > kEps || dw > kEps || dh > kEps) {
    stats.divergences++;
    if (logged < maxLog) {
      logged++;
      TraceLog(LOG_WARNING,
               "RETAINED-LAYOUT diverge id=%s kind=%d rel=(%.1f,%.1f %.1fx%.1f) "
               "retained=(%.1f,%.1f %.1fx%.1f)",
               node->id.c_str(), (int)node->kind, relX, relY,
               node->layout.width, node->layout.height,
               YGNodeLayoutGetLeft(yg), YGNodeLayoutGetTop(yg),
               YGNodeLayoutGetWidth(yg), YGNodeLayoutGetHeight(yg));
    }
  }
  for (const NodePtr &child : node->children)
    retainedCompare(child, node.get(), bounds, logged, maxLog, stats);
}

}  // namespace

// Reconcile + calculate. Shared by verify and authoritative modes; returns the
// root's retained Yoga node (null when the tree is empty).
static YGNodeRef retainedCalculate(const NodePtr &root, Rectangle bounds,
                                   RetainedLayoutStats &stats) {
  g_retainedGen++;
  retainedReconcile(root, true, true, stats);
  retainedPrune(stats);
  YGNodeRef ygRoot = g_retainedYoga[root.get()].yg;
  if (!ygRoot) return nullptr;
  YGNodeStyleSetWidth(ygRoot, bounds.width);
  YGNodeStyleSetHeight(ygRoot, bounds.height);
  YGNodeCalculateLayout(ygRoot, bounds.width, bounds.height, YGDirectionLTR);
  return ygRoot;
}

RetainedLayoutStats RetainedLayoutVerify(const NodePtr &root, Rectangle bounds,
                                         int maxLogPerCall) {
  RetainedLayoutStats stats;
  if (!root) return stats;
  if (!retainedCalculate(root, bounds, stats)) return stats;
  int logged = 0;
  retainedCompare(root, nullptr, bounds, logged, maxLogPerCall, stats);
  return stats;
}

RetainedLayoutStats RetainedUpdateLayout(const NodePtr &root, Rectangle bounds) {
  RetainedLayoutStats stats;
  if (!root) return stats;
  MarkNavigationRailContext(root, false, 0.0f);
  MarkNavigationBarContext(root, false);
  YGNodeRef ygRoot = retainedCalculate(root, bounds, stats);
  if (!ygRoot) return stats;
  // Same commit walk as the per-frame path: writes node->layout, content
  // extents, scroll clamp + follow-end. Retained yoga children are kept in
  // node->children order by retainedReconcile, so the index pairing holds.
  StoreYogaLayout(root, ygRoot, bounds.x, bounds.y);
  // Report the mirror's allocation count, not the whole tree: with retained
  // layout this should be ~0 in steady state and only spike on mount.
  Ctx().lastStats.yogaNodesBuilt = stats.yogaNodesCreated;
  return stats;
}

void RetainedLayoutReset() {
  for (auto &[node, r] : g_retainedYoga) {
    (void)node;
    if (r.yg) {
      if (YGNodeRef owner = YGNodeGetOwner(r.yg))
        YGNodeRemoveChild(owner, r.yg);
      YGNodeFree(r.yg);
    }
  }
  g_retainedYoga.clear();
}
#else
RetainedLayoutStats RetainedLayoutVerify(const NodePtr &, Rectangle, int) {
  return {};
}
RetainedLayoutStats RetainedUpdateLayout(const NodePtr &root, Rectangle bounds) {
  UpdateLayout(root, bounds);   // no Yoga build: fallback path is already retained-free
  return {};
}
void RetainedLayoutReset() {}
#endif

static void DrawNodeBackground(const Node &node, const Style &style) {
  if (style.display == Display::None)
    return;

  Rectangle backgroundLayout = node.layout;
  if (node.role == NodeRole::NavigationRail) {
    float expandedWidth = tokens::kNavigationRailExpandedWidth;
    float t = std::clamp(node.animSelect, 0.0f, 1.0f);
    backgroundLayout.width =
        node.layout.width + (expandedWidth - node.layout.width) * t;
  }

  float opacity = CurrentRenderOpacity();
  float radius = style.borderRadius.value_or(0.0f);
  // M3 elevation: render a key (umbra) + ambient (penumbra) drop shadow when a
  // component declares an elevation but no explicit boxShadows. dp value maps
  // directly to shadow geometry (FAB/dialog/menu = 3dp, nav/sheets = 2dp).
  float elevation = style.elevation.value_or(0.0f);
  if (elevation > 0.0f && style.boxShadows.empty()) {
    struct ElevShadow { float offsetY; float grow; float alpha; };
    const ElevShadow passes[] = {
        {std::min(4.0f, elevation * 0.75f + 0.5f), 0.0f, 0.14f},
        {std::min(8.0f, elevation * 1.5f + 1.0f),
         std::min(1.5f, elevation * 0.25f), 0.06f},
    };
    for (const ElevShadow &p : passes) {
      Rectangle bounds = {
          backgroundLayout.x - p.grow,
          backgroundLayout.y + p.offsetY,
          backgroundLayout.width + p.grow * 2.0f,
          backgroundLayout.height,
      };
      raym3::Renderer::DrawRoundedRectangle(
          bounds, radius + p.grow,
          ColorAlpha(Color{0, 0, 0, 255}, opacity * p.alpha));
    }
  }
  for (const BoxShadow &shadow : style.boxShadows) {
    if (shadow.inset)
      continue;
    int layers = std::max(1, (int)(shadow.blurRadius / 4.0f));
    for (int i = layers; i >= 1; --i) {
      float t = (float)i / (float)layers;
      float grow = shadow.spreadRadius + shadow.blurRadius * t * 0.5f;
      Rectangle bounds = {
          backgroundLayout.x + shadow.offsetX - grow,
          backgroundLayout.y + shadow.offsetY - grow,
          backgroundLayout.width + grow * 2.0f,
          backgroundLayout.height + grow * 2.0f,
      };
      raym3::Renderer::DrawRoundedRectangle(
          bounds, radius + grow,
          ScaleAlpha(shadow.color, opacity / (float)layers));
    }
  }
  if (style.backdropBlur && *style.backdropBlur > 0.0f) {
    // Cross-platform fallback: the retained renderer exposes the style and
    // preserves draw order. Backends with render-target blur can replace this
    // with an actual sampled backdrop pass without changing the public API.
  }
  if (style.backgroundGradient && style.backgroundGradient->stops.size() >= 2) {
    const auto &stops = style.backgroundGradient->stops;
    Color first = ScaleAlpha(stops.front().color, opacity);
    Color last = ScaleAlpha(stops.back().color, opacity);
    float angle = fmodf(style.backgroundGradient->angleDegrees + 360.0f, 360.0f);
    if (angle >= 45.0f && angle < 135.0f) {
      DrawRectangleGradientEx(backgroundLayout, first, last, last, first);
    } else if (angle >= 225.0f && angle < 315.0f) {
      DrawRectangleGradientEx(backgroundLayout, last, first, first, last);
    } else if (angle >= 135.0f && angle < 225.0f) {
      DrawRectangleGradientEx(backgroundLayout, last, last, first, first);
    } else {
      DrawRectangleGradientEx(backgroundLayout, first, first, last, last);
    }
  }
  if (style.backgroundColor) {
    raym3::Renderer::DrawRoundedRectangle(backgroundLayout, radius,
                                          ScaleAlpha(*style.backgroundColor, opacity));
    if (node.role == NodeRole::BottomSheet && radius > 0.0f) {
      // A modal bottom sheet is attached to the viewport edge. Only its top
      // corners are rounded; fill the lower radius band so the bottom-left and
      // bottom-right corners remain square and visually anchored to screen.
      DrawRectangleRec(
          {backgroundLayout.x,
           backgroundLayout.y + std::max(0.0f, backgroundLayout.height - radius),
           backgroundLayout.width,
           std::min(radius, backgroundLayout.height)},
          ScaleAlpha(*style.backgroundColor, opacity));
    }
  }
  // A plain interactive View (onPress, non-material) gets an implicit hover/press
  // state layer so it dims like a button; its tint comes from CSS
  // state-layer-color when set, else the content colour. Material components
  // supply their own stateLayerColor via state styles.
  //
  // Nodes carrying a material role are excluded: they paint their own indicator
  // (a navigation item's pill, for instance, gets DrawStateLayer below), so an
  // implicit layer on the container would double up and highlight the whole
  // item box on press.
  const bool implicitStateLayer =
      node.kind == NodeKind::View && node.onPress != nullptr &&
      node.role == NodeRole::None;
  if ((style.stateLayerColor || implicitStateLayer) &&
      node.animStateAlpha > 0.001f) {
    // Use the eased alpha rather than the style's baked opacity so the layer
    // fades in/out smoothly. Color carries the hue; animStateAlpha the opacity.
    Color sl = style.stateLayerColor.value_or(
        style.text.color.value_or(Theme::GetColorScheme().onSurface));
    raym3::Renderer::DrawRoundedRectangle(
        backgroundLayout, radius,
        ColorAlpha(Color{sl.r, sl.g, sl.b, 255}, opacity * node.animStateAlpha));
  }
  bool perEdgeBorders = HasPerEdgeBorders(style);
  bool borderDrawn = false;
  if (perEdgeBorders) {
    // Per-edge fields that RESOLVE identical on all four sides are still a
    // uniform border — paint it with the rounded stroke so border-radius is
    // honoured. Only genuinely uneven edges need the square edge-by-edge
    // painter (corners are square where two differently-lit edges meet, which
    // is what a per-side CSS border does).
    const Color fb = style.borderColor.value_or(Color{0, 0, 0, 0});
    const float w0 = ResolveBorderWidth(style, BoxEdge::Top);
    const Color c0 = ResolveBorderColor(style, BoxEdge::Top, fb);
    bool uniform = true;
    for (BoxEdge e : {BoxEdge::Right, BoxEdge::Bottom, BoxEdge::Left}) {
      const Color c = ResolveBorderColor(style, e, fb);
      if (ResolveBorderWidth(style, e) != w0 ||
          c.r != c0.r || c.g != c0.g || c.b != c0.b || c.a != c0.a) {
        uniform = false;
        break;
      }
    }
    if (uniform) {
      perEdgeBorders = false;
      // Draw here (not via the shared-field branch below): translucent border
      // colours must not be stroked twice.
      if (w0 > 0.0f && c0.a > 0) {
        raym3::Renderer::DrawRoundedRectangleEx(backgroundLayout, radius,
                                                ScaleAlpha(c0, opacity), w0);
      }
      borderDrawn = true;
    }
  }
  if (perEdgeBorders) {
    const Color fallback = style.borderColor.value_or(Color{0, 0, 0, 0});
    const float top    = ResolveBorderWidth(style, BoxEdge::Top);
    const float right  = ResolveBorderWidth(style, BoxEdge::Right);
    const float bottom = ResolveBorderWidth(style, BoxEdge::Bottom);
    const float left   = ResolveBorderWidth(style, BoxEdge::Left);
    const Rectangle &r = backgroundLayout;
    if (top > 0.0f) {
      DrawRectangleRec({r.x, r.y, r.width, top},
                       ScaleAlpha(ResolveBorderColor(style, BoxEdge::Top, fallback), opacity));
    }
    if (bottom > 0.0f) {
      DrawRectangleRec({r.x, r.y + r.height - bottom, r.width, bottom},
                       ScaleAlpha(ResolveBorderColor(style, BoxEdge::Bottom, fallback), opacity));
    }
    if (left > 0.0f) {
      DrawRectangleRec({r.x, r.y + top, left, std::max(0.0f, r.height - top - bottom)},
                       ScaleAlpha(ResolveBorderColor(style, BoxEdge::Left, fallback), opacity));
    }
    if (right > 0.0f) {
      DrawRectangleRec({r.x + r.width - right, r.y + top, right,
                        std::max(0.0f, r.height - top - bottom)},
                       ScaleAlpha(ResolveBorderColor(style, BoxEdge::Right, fallback), opacity));
    }
  } else if (!borderDrawn && style.borderColor && style.borderWidth.value_or(0.0f) > 0.0f) {
    raym3::Renderer::DrawRoundedRectangleEx(
        backgroundLayout, radius, ScaleAlpha(*style.borderColor, opacity),
        *style.borderWidth);
  }
  for (const BoxShadow &shadow : style.boxShadows) {
    if (!shadow.inset)
      continue;
    float width = std::max(1.0f, shadow.blurRadius > 0.0f ? shadow.blurRadius : 1.0f);
    raym3::Renderer::DrawRoundedRectangleEx(
        {backgroundLayout.x + shadow.offsetX, backgroundLayout.y + shadow.offsetY,
         backgroundLayout.width, backgroundLayout.height},
        radius, ScaleAlpha(shadow.color, opacity), width);
  }
}

static void RenderTextNode(const Node &node, const Style &style) {
  float fontSize =
      style.text.fontSize.value_or(Theme::GetTypographyScale().bodyMedium);
  float letterSpacing = style.text.letterSpacing.value_or(0.25f);
  FontWeight weight = style.text.weight.value_or(FontWeight::Regular);
  std::string fontFamily = style.text.fontFamily.value_or(std::string{});
  // Reuse the cached PreparedText from Yoga measure phase — no re-measurement.
  const PreparedText& prepared = GetOrPrepare(&node);
  TextLayoutResult layout = LayoutText(prepared, node.layout.width);
  Color color = ApplyRenderOpacity(ResolveTextColor(style.text.color));
  // Layout coords are in dp; the font texture was generated at pixel size
  // (size * GetDpiScale()) and we want to sample it 1:1, not 1:(1/dp).
  // So we render OUTSIDE the host's dp-scaling matrix, at the pixel
  // position. node.layout.x/y is in dp — multiply by dp to get pixels.
  const float dp = Density::GetLayoutDensity();
  // Split the leading (line-height minus the em box) equally above and below the
  // text, so a line's glyphs sit centred in their line box — matching CSS/RN. We
  // previously top-anchored each line (all leading below), which made text sit
  // high in its box and look off-centre inside Yoga-centred containers.
  const float lineHeightDp = prepared.options.lineHeight;
  const float halfLeadingDp = std::max(0.0f, (lineHeightDp - fontSize) * 0.5f);
  float y = Density::DpToPx(node.layout.y + halfLeadingDp);

  // Resolve font once — custom family or Roboto.
  Font resolvedFont = fontFamily.empty()
      ? Theme::GetFont(fontSize, weight)
      : FontManager::LoadFontByFamily(fontFamily, (int)fontSize);

  // Draw text in pixel space: the font texture was generated at size * dp, so
  // we want a 1:1 sample (no extra scale on the draw call). The host applies an
  // ambient dp-scaling matrix (and a parent may add transforms, e.g. a
  // navigation slide). Rather than pop a fixed number of levels — which breaks
  // whenever text is nested under a transform and silently leaks an extra dp
  // onto later siblings — cancel exactly one dp factor with a balanced
  // push/scale(1/dp)/pop. This restores the matrix precisely regardless of
  // nesting, while text coords (layout * dp) still land at the right pixels.
  rlPushMatrix();
  rlScalef(1.0f / dp, 1.0f / dp, 1.0f);

  for (const TextLine &line : layout.lines) {
    float x = Density::DpToPx(node.layout.x);
    TextAlignment alignment = style.text.alignment.value_or(TextAlignment::Left);
    if (alignment == TextAlignment::Center) {
      x += Density::DpToPx((node.layout.width - line.width) * 0.5f);
    } else if (alignment == TextAlignment::Right) {
      x += Density::DpToPx(node.layout.width - line.width);
    }
    DrawTextWithEmoji(resolvedFont, line.text, {x, y},
                      Density::DpToPx(fontSize),
                      Density::DpToPx(letterSpacing), color);
    y += Density::DpToPx(prepared.options.lineHeight);
  }

  rlPopMatrix();
}

static void RenderTextInputNode(Node &node) {
  char *buffer = node.textInput.buffer;
  int bufferSize = node.textInput.bufferSize;
  if (!buffer && node.textInput.value) {
    std::vector<char> &storedBuffer =
        Ctx().textInputBuffers[TextInputStateKey(node)];
    if (storedBuffer.empty()) {
      storedBuffer.assign(1024, '\0');
      std::strncpy(storedBuffer.data(), node.textInput.value->c_str(),
                   storedBuffer.size() - 1);
    }
    buffer = storedBuffer.data();
    bufferSize = static_cast<int>(storedBuffer.size());
    node.textInput.buffer = buffer;
    node.textInput.bufferSize = bufferSize;
  }

  if (!buffer || bufferSize <= 0)
    return;

  bool focused = GetFocusedId() == IdOf(&node);
  if (node.textInput.value) {
    const std::string &external = *node.textInput.value;
    if (external != buffer && !focused) {
      std::strncpy(buffer, external.c_str(),
                   static_cast<size_t>(bufferSize - 1));
      buffer[bufferSize - 1] = '\0';
      node.textEdit.cursor = static_cast<int>(std::strlen(buffer));
    }
  }

  PaintTextInput(node);

  if (node.textInput.value && *node.textInput.value != buffer) {
    *node.textInput.value = std::string(buffer);
    if (node.textInput.onChange)
      node.textInput.onChange(*node.textInput.value);
  }
}

static void RenderNode(const NodePtr &node, int parentMaxZ) {
  if (!node || node->style.display == Display::None)
    return;

  Ctx().lastStats.nodeCount++;
  node->state = ComputeState(*node);
  Style effScratch;
  const Style &style = EffectiveStyleRef(*node, effScratch);
  const float parentOpacity = g_renderOpacity;
  g_renderOpacity *= std::clamp(style.opacity.value_or(1.0f), 0.0f, 1.0f);
  // Establish this node's text color for its subtree (CSS `color` inheritance).
  const std::optional<Color> parentInheritedText = g_inheritedTextColor;
  if (style.text.color) g_inheritedTextColor = style.text.color;

  int effectiveZ = std::max(parentMaxZ, node->zIndex);
  bool occludes = NodeOccludesInput(*node, style);
  Ctx().stackOrder.push_back(
      {node, node->layout, Ctx().paintCounter++, occludes, false, effectiveZ});

  // Report measured layout back to JS once it changes (RN onLayout).
  if (node->onLayout) {
    const Rectangle &r = node->layout;
    if (!node->reportedLayoutValid || r.x != node->reportedLayout.x ||
        r.y != node->reportedLayout.y || r.width != node->reportedLayout.width ||
        r.height != node->reportedLayout.height) {
      node->reportedLayout = r;
      node->reportedLayoutValid = true;
      node->onLayout(r);
    }
  }

  // Viewport culling: skip painting a node whose rect is outside the visible
  // region. Safe to skip the whole subtree when we are inside a clip container
  // (absolute descendants can't escape it) or the node itself clips. At the root
  // (no clip yet) only clipping nodes are subtree-culled, since a non-clipping
  // node's absolutely-positioned descendants may sit elsewhere on screen.
  // Layout + onLayout above already ran, and CSS transitions/animations tick on
  // the whole tree before Render(), so culling never neglects animation state —
  // it only skips draw calls. Fixed-position overlays paint in a separate pass.
  {
    const Rectangle& cull = g_cullStack.back();
    const bool insideClip = g_cullStack.size() > 1;
    const bool clips = style.overflow == Overflow::Hidden ||
                       style.overflow == Overflow::Scroll;
    if ((insideClip || clips) && !RectsOverlap(node->layout, cull)) {
      g_renderOpacity = parentOpacity; // restore; normal path restores at end
      g_inheritedTextColor = parentInheritedText;
      return;
    }
  }

  // Survived culling — this node actually paints. Everything above (state,
  // style resolution, stack-order entry, onLayout) already ran for culled
  // nodes too, so the gap between paintedCount and nodeCount is the work the
  // cull does NOT save.
  Ctx().lastStats.paintedCount++;

  // First-class controls: tick toggle animation + paint directly (no
  // customRender / no host-side polling).
  if (IsControlKind(node->kind)) {
    TickControlAnimation(*node, (float)FrameTimeMs());
    bool active = IsNodePressed(*node);
    bool hovered = GetHoveredId() == IdOf(node.get()) ||
                   NodeOnInputPath(*node, GetHoveredId());
    PaintControl(*node, active, hovered);
  }

  // --- Animation tick (M3 motion) ------------------------------------------
  // Ease the state-layer opacity toward its target so hover/press/focus layers
  // cross-fade instead of snapping. dt-based exponential smoothing approximates
  // M3 short2 (~100ms) at the default frame rate.
  float dt = GetFrameTime();
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;
  float targetAlpha =
      style.stateLayerColor ? (float)style.stateLayerColor->a / 255.0f : 0.0f;
  // Plain interactive Views dim on hover/press. Material components and
  // first-class controls drive their own state layers (via state styles /
  // PaintControl), so this only applies to onPress Views. The peak opacity comes
  // from CSS state-layer-color's alpha when set, else an M3-ish default.
  if (node->kind == NodeKind::View && node->onPress) {
    float peak = style.stateLayerColor
                     ? (float)style.stateLayerColor->a / 255.0f
                     : 0.14f;
    bool pressed = IsNodePressed(*node);
    bool hovered = GetHoveredId() == IdOf(node.get()) ||
                   NodeOnInputPath(*node, GetHoveredId());
    targetAlpha = pressed ? peak : (hovered ? peak * 0.6f : 0.0f);
  }
  if (!node->animInitialized) {
    node->animStateAlpha = targetAlpha;
    node->animSelect = node->selected ? 1.0f : 0.0f;
    node->animIndicatorFade = node->selected ? 1.0f : 0.0f;
    node->animIndicatorLastSelected = node->selected;
    node->animIndicatorSelectionInitialized = true;
    node->animInitialized = true;
  }
  float k = std::min(1.0f, dt * 14.0f);
  node->animStateAlpha += (targetAlpha - node->animStateAlpha) * k;
  float targetSelect = node->selected ? 1.0f : 0.0f;
  if (node->role == NodeRole::NavItem && node->inNavigationBar) {
    constexpr float kNavigationBarAnimationSeconds = 0.5f;
    constexpr float kNavigationIndicatorFadeSeconds = 0.1f;
    float step = dt / kNavigationBarAnimationSeconds;
    if (targetSelect > node->animSelect) {
      node->animSelect = std::min(targetSelect, node->animSelect + step);
    } else {
      node->animSelect = std::max(targetSelect, node->animSelect - step);
    }
    if (!node->animIndicatorSelectionInitialized) {
      node->animIndicatorFade = node->selected ? 1.0f : 0.0f;
      node->animIndicatorLastSelected = node->selected;
      node->animIndicatorSelectionInitialized = true;
    } else if (node->animIndicatorLastSelected != node->selected) {
      node->animIndicatorFade = node->selected ? 0.0f : 1.0f;
      node->animIndicatorLastSelected = node->selected;
    }
    float fadeTarget = node->selected ? 1.0f : 0.0f;
    float fadeStep = dt / kNavigationIndicatorFadeSeconds;
    if (fadeTarget > node->animIndicatorFade) {
      node->animIndicatorFade =
          std::min(fadeTarget, node->animIndicatorFade + fadeStep);
    } else {
      node->animIndicatorFade =
          std::max(fadeTarget, node->animIndicatorFade - fadeStep);
    }
  } else {
    node->animSelect += (targetSelect - node->animSelect) * k;
  }

  // Affine transform (rotation/scale/translate) applied about the node's center
  // so the whole subtree — background, content, children — moves together. Note:
  // hardware scissor/stencil clip in window space, so a clipped *and* rotated
  // node would mis-clip; in practice transforms are used on unclipped nodes
  // (icons, FABs, the nav-rail expand button).
  const bool hasTransform = style.rotation || style.scale ||
                            style.translateX || style.translateY;
  if (hasTransform) {
    float cx = node->layout.x + node->layout.width * 0.5f;
    float cy = node->layout.y + node->layout.height * 0.5f;
    float sc = style.scale.value_or(1.0f);
    rlPushMatrix();
    rlTranslatef(cx + style.translateX.value_or(0.0f),
                 cy + style.translateY.value_or(0.0f), 0.0f);
    rlRotatef(style.rotation.value_or(0.0f), 0.0f, 0.0f, 1.0f);
    rlScalef(sc, sc, 1.0f);
    rlTranslatef(-cx, -cy, 0.0f);
  }

  DrawNodeBackground(*node, style);

  {
    NodeId nodeId = IdOf(node);
    bool paintRipple = node->kind == NodeKind::Button || node->inkRipple ||
                       HasRipplesForNode(nodeId);
    if (paintRipple) {
      float rippleRadius = style.borderRadius.value_or(0.0f);
      // Ripple.cpp tessellates the gradient into the component's rounded
      // outline. It does not depend on backend scissor/stencil behavior.
      PaintRipplesForNode(*node, node->layout, rippleRadius);
    }
  }

  bool clipped = style.overflow == Overflow::Hidden ||
                 style.overflow == Overflow::Scroll;
  float clipRadius = style.borderRadius.value_or(0.0f);
  bool useStencil  = clipped && clipRadius > 0.0f;

  if (useStencil) {
    raym3::PushRoundedStencil(node->layout, clipRadius);
  } else if (clipped) {
    PushScissor(node->layout);
  }
  // Narrow the cull region to this node's rect for its subtree (paint culling,
  // parallel to the scissor/stencil clip above but in DP space).
  if (clipped)
    g_cullStack.push_back(IntersectRect(node->layout, g_cullStack.back()));

  switch (node->kind) {
  case NodeKind::Button: {
    // Background already drawn by DrawNodeBackground with the user's
    // borderRadius. Only add state layer, text, and cursor — no second fill.
    float radius = style.borderRadius.value_or(0.0f);
    Color textColor = ApplyRenderOpacity(ResolveTextColor(style.text.color));
    float fontSize =
        style.text.fontSize.value_or(Theme::GetTypographyScale().labelLarge);

    if (CheckCollisionPointRec(GetMousePosition(), node->layout))
      raym3::RequestCursor(MOUSE_CURSOR_POINTING_HAND);

    if (node->animStateAlpha > 0.001f) {
      raym3::Renderer::DrawRoundedRectangle(
          node->layout, radius,
          ColorAlpha(Color{textColor.r, textColor.g, textColor.b, 255},
                     CurrentRenderOpacity() * node->animStateAlpha));
    }
    raym3::Renderer::DrawTextCentered(node->text.c_str(), node->layout,
                                      fontSize, textColor, FontWeight::Medium);
    // Press handled via HitTest in the host loop — no double-fire.
    break;
  }
  case NodeKind::Text:
    RenderTextNode(*node, style);
    break;
  case NodeKind::TextInput:
    RenderTextInputNode(*node);
    break;
  case NodeKind::Custom:
    if (node->customRender)
      node->customRender(node->layout);
    break;
  case NodeKind::View:
  default:
    // customRender is a universal post-background hook, not Custom-only: host
    // embedders (worker canvases, external surfaces) attach painters to
    // existing view nodes so their content draws inside the tree walk — under
    // the ambient scissor/stencil clip and at the node's z-order.
    if (node->customRender)
      node->customRender(node->layout);
    break;
  }

  std::vector<NodePtr> children = node->children;
  std::stable_sort(children.begin(), children.end(),
                   [](const NodePtr &a, const NodePtr &b) {
                     return a->zIndex < b->zIndex;
                   });

  // Collect fixed-position children into the global overlay queue rather than
  // rendering them in-place — they are painted after all normal content.
  {
    std::vector<NodePtr> flowChildren;
    flowChildren.reserve(children.size());
    for (const NodePtr &child : children) {
      if (!child) continue;
      Style cs = EffectiveStyle(*child);
      if (cs.position == PositionType::Fixed && cs.display != Display::None) {
        // Skip rendering in-place; collected recursively in the pre-pass of Render().
      } else {
        flowChildren.push_back(child);
      }
    }
    children = std::move(flowChildren);
  }

  // Nav item active-indicator pill. Flutter's NavigationBar does not slide one
  // shared indicator between destinations; each destination owns a pill that
  // scales on the x axis from hidden -> 40% -> 100% using
  // easeInOutCubicEmphasized.
  if (node->role == NodeRole::NavItem && !children.empty() &&
      (node->animSelect > 0.001f || node->state != ComponentState::Default)) {
    const Node *iconChild = FindNavigationIconChild(*node);
    const Rectangle &icon =
        iconChild ? iconChild->layout : children.front()->layout;
    float iconCY = icon.y + icon.height * 0.5f;
    float itemCX = node->layout.x + node->layout.width * 0.5f;
    bool rowItem =
        style.flexDirection.value_or(FlexDirection::Column) == FlexDirection::Row;
    {
      // Row items (expanded nav rail) get a wide pill spanning icon+label;
      // column items (collapsed rail, nav bar) get the 56x32 pill behind the
      // icon. The expanded rail uses the SAME selection treatment as the nav
      // bar — no special-case suppression — so the active item always shows an
      // indicator.
      bool pillUsesRowGeometry = rowItem && !node->inNavigationBar;
      float pillH = pillUsesRowGeometry ? 40.0f : tokens::kNavigationIndicatorHeight;
      float pillRadius = pillH * 0.5f;
      float pillCenterX = itemCX;
      float fullPillW = tokens::kNavigationIndicatorWidth;
      if (pillUsesRowGeometry) {
        float contentLeft = icon.x;
        float contentRight = icon.x + icon.width;
        for (const NodePtr &child : children) {
          contentLeft = std::min(contentLeft, child->layout.x);
          contentRight =
              std::max(contentRight, child->layout.x + child->layout.width);
        }
        float contentW = std::max(0.0f, contentRight - contentLeft);
        fullPillW = std::max(40.0f, contentW + 32.0f);
        if (!node->inNavigationRail && style.width && *style.width > fullPillW) {
          fullPillW = std::min(*style.width, node->layout.width);
          pillCenterX = itemCX;
        } else {
          pillCenterX = (contentLeft + contentRight) * 0.5f;
        }
        fullPillW = std::min(fullPillW, node->layout.width);
      }
      float indicatorScale = 0.0f;
      if (node->animSelect > 0.001f) {
        indicatorScale =
            0.4f + 0.6f * FlutterEaseInOutCubicEmphasized(node->animSelect);
      }
      float selectedPillW = fullPillW * indicatorScale;
      float statePillW = node->state == ComponentState::Default
          ? selectedPillW
          : fullPillW;
      Rectangle pill = {pillCenterX - statePillW * 0.5f, iconCY - pillH * 0.5f,
                        statePillW, pillH};
      if (pillUsesRowGeometry) {
        const float minX = node->layout.x;
        const float maxX = node->layout.x + node->layout.width - pill.width;
        pill.x = std::clamp(pill.x, minX, std::max(minX, maxX));
      }
      if (selectedPillW > 0.001f) {
        Rectangle selectedPill = {pillCenterX - selectedPillW * 0.5f,
                                  iconCY - pillH * 0.5f,
                                  selectedPillW, pillH};
        if (pillUsesRowGeometry) {
          const float minX = node->layout.x;
          const float maxX = node->layout.x + node->layout.width - selectedPill.width;
          selectedPill.x = std::clamp(selectedPill.x, minX, std::max(minX, maxX));
        }
        raym3::Renderer::DrawRoundedRectangle(
            selectedPill, pillRadius,
            ColorAlpha(Theme::GetColorScheme().secondaryContainer,
                       node->inNavigationBar ? node->animIndicatorFade
                                             : node->animSelect));
      }
      if (node->state != ComponentState::Default) {
        Color stateBase = node->selected
            ? Theme::GetColorScheme().onSecondaryContainer
            : Theme::GetColorScheme().onSurfaceVariant;
        raym3::Renderer::DrawStateLayer(pill, pillRadius, stateBase, node->state);
      }
    }
  }

  // ButtonGroup/SegmentedButton: draw segment fills + state layers BEFORE
  // children so text/icons render on top. Fills use DrawSegmentShape which
  // rounds only the outer edges — no stencil/clip required (works on Vulkan).
  if (node->role == NodeRole::ButtonGroupContainer) {
    int n = (int)children.size();
    float cr      = 20.0f; // pill corner radius
    float opacity = CurrentRenderOpacity();
    const ColorScheme &scheme = Theme::GetColorScheme();

    auto DrawSegmentFill = [&](int i, Rectangle r, Color color) {
      if (n == 1) {
        raym3::Renderer::DrawRoundedRectangle(r, cr, color);
      } else if (i == 0) {
        Rectangle inner = {r.x + cr, r.y, r.width - cr, r.height};
        if (inner.width > 0) DrawRectangleRec(inner, color);
        DrawCircleSector({r.x + cr, r.y + cr}, cr, 90, 270, 24, color);
      } else if (i == n - 1) {
        Rectangle inner = {r.x, r.y, r.width - cr, r.height};
        if (inner.width > 0) DrawRectangleRec(inner, color);
        DrawCircleSector({r.x + r.width - cr, r.y + cr}, cr, 270, 450, 24, color);
      } else {
        DrawRectangleRec(r, color);
      }
    };

    for (int i = 0; i < n; ++i) {
      const NodePtr &child = children[i];
      if (!child) continue;
      Rectangle r = child->layout;

      // Selection fill (animated)
      if (child->animSelect > 0.001f) {
        DrawSegmentFill(i, r,
            ColorAlpha(scheme.secondaryContainer, opacity * child->animSelect));
      }

      // State layer (hover/press/focus)
      if (child->animStateAlpha > 0.001f) {
        Color contentColor = child->style.text.color.value_or(
            child->selected ? scheme.onSecondaryContainer : scheme.onSurface);
        Color stateColor = Theme::GetStateLayerColor(contentColor, child->state);
        if (stateColor.a > 0) {
          DrawSegmentFill(i, r,
              ColorAlpha(Color{stateColor.r, stateColor.g, stateColor.b, 255},
                         opacity * child->animStateAlpha));
        }
      }
    }
  }

  // SplitButton: two filled segments sharing one pill — leading rounds left,
  // trailing rounds right. Fills drawn before children (text/chevron on top).
  if (node->role == NodeRole::SplitButton && children.size() == 2) {
    const ColorScheme &scheme = Theme::GetColorScheme();
    float opacity = CurrentRenderOpacity();
    float h = node->layout.height;
    float cr = h * 0.5f; // full-pill outer ends
    // M3 Expressive split button: small rounded INNER corners + a gap between
    // the two segments (instead of a divider line).
    float innerR = std::clamp(h * 0.18f, 4.0f, 16.0f);
    float gap = 2.0f;
    Color fill = ColorAlpha(scheme.primary, opacity);
    // Rounded-rect segment: independent left/right corner radii via end caps.
    auto seg = [&](const Rectangle &r, float lr, float rr, Color color) {
      lr = std::clamp(lr, 0.0f, std::min(r.width * 0.5f, r.height * 0.5f));
      rr = std::clamp(rr, 0.0f, std::min(r.width * 0.5f, r.height * 0.5f));
      DrawRectangleRec({r.x + lr, r.y, std::max(0.0f, r.width - lr - rr), r.height}, color);
      if (lr > 0.0f) {
        DrawCircleSector({r.x + lr, r.y + lr}, lr, 180, 270, 16, color);
        DrawCircleSector({r.x + lr, r.y + r.height - lr}, lr, 90, 180, 16, color);
        DrawRectangleRec({r.x, r.y + lr, lr, std::max(0.0f, r.height - lr * 2)}, color);
      }
      if (rr > 0.0f) {
        DrawCircleSector({r.x + r.width - rr, r.y + rr}, rr, 270, 360, 16, color);
        DrawCircleSector({r.x + r.width - rr, r.y + r.height - rr}, rr, 0, 90, 16, color);
        DrawRectangleRec({r.x + r.width - rr, r.y + rr, rr, std::max(0.0f, r.height - rr * 2)}, color);
      }
    };
    // Inset each segment's inner edge by gap/2 to open the gap.
    Rectangle lr = children[0]->layout;
    Rectangle tr = children[1]->layout;
    lr.width = std::max(0.0f, lr.width - gap * 0.5f);
    tr.x += gap * 0.5f;
    tr.width = std::max(0.0f, tr.width - gap * 0.5f);
    seg(lr, cr, innerR, fill);     // leading: full-pill left, small right
    seg(tr, innerR, cr, fill);     // trailing: small left, full-pill right
    // State layers per pressed/hovered segment.
    Rectangle segRects[2] = {lr, tr};
    float lefts[2] = {cr, innerR};
    float rights[2] = {innerR, cr};
    for (int i = 0; i < 2; ++i) {
      const NodePtr &child = children[i];
      if (child->animStateAlpha > 0.001f) {
        Color sc = Theme::GetStateLayerColor(scheme.onPrimary, child->state);
        if (sc.a > 0)
          seg(segRects[i], lefts[i], rights[i],
              ColorAlpha(Color{sc.r, sc.g, sc.b, 255}, opacity * child->animStateAlpha));
      }
    }
  }

  // App bar centerTitle (Flutter NavigationToolbar centerMiddle): the title
  // slot lays out in flow between the leading and trailing clusters; recenter
  // it across the full bar width, clamped so it never overlaps either cluster.
  if (node->role == NodeRole::AppBar && node->appBarCenterTitle) {
    const NodePtr *title = nullptr;
    for (const NodePtr &child : children) {
      if (child && child->isAppBarTitle) { title = &child; break; }
    }
    if (title && (*title)->layout.width > 0.0f) {
      float barL = node->layout.x + style.padding.Left();
      float barR = node->layout.x + node->layout.width - style.padding.Right();
      float leadingRight = barL;
      float trailingLeft = barR;
      bool seenTitle = false;
      for (const NodePtr &child : children) {
        if (!child) continue;
        if (child.get() == title->get()) { seenTitle = true; continue; }
        if (child->layout.width <= 0.0f) continue;
        if (!seenTitle)
          leadingRight = std::max(leadingRight, child->layout.x + child->layout.width);
        else
          trailingLeft = std::min(trailingLeft, child->layout.x);
      }
      const float spacing = style.gap.value_or(0.0f);
      float titleW = (*title)->layout.width;
      float targetX = (barL + barR) * 0.5f - titleW * 0.5f;
      float minX = leadingRight + spacing;
      float maxX = trailingLeft - spacing - titleW;
      if (maxX < minX)
        targetX = minX; // no room to center — sit just past the leading cluster
      else
        targetX = std::clamp(targetX, minX, maxX);
      float dx = targetX - (*title)->layout.x;
      if (std::fabs(dx) > 0.5f)
        TranslateSubtreeX(*title, dx);
    }
  }

  for (const NodePtr &child : children) {
    RenderNode(child, effectiveZ);
  }

  // Dividers between segments (drawn on top of fills, under outer border).
  if (node->role == NodeRole::ButtonGroupContainer) {
    float opacity = CurrentRenderOpacity();
    Color divColor = ColorAlpha(Theme::GetColorScheme().outline, opacity);
    for (size_t i = 0; i + 1 < children.size(); ++i) {
      float x = children[i]->layout.x + children[i]->layout.width;
      DrawLineEx({x, node->layout.y},
                 {x, node->layout.y + node->layout.height},
                 1.0f, divColor);
    }
  }
  // SplitButton: the two segments are separated by a transparent gap (drawn in
  // the fill pass above), so no divider line is needed.

  // Tab active indicator: a 3dp primary bar that slides between the selected
  // child's bounds (M3 secondary-tab width = full tab). Eased per-frame.
  if (node->role == NodeRole::Tabs) {
    const Node *sel = nullptr;
    for (const NodePtr &child : children) {
      if (child && child->selected) {
        sel = child.get();
        break;
      }
    }
    if (sel) {
      node->animIndicatorY = sel->layout.x;
      node->animIndicatorFade = sel->layout.width;
    }
    float targetX = sel ? sel->layout.x : node->animIndicatorY;
    float targetW = sel ? sel->layout.width : node->animIndicatorFade;
    if (targetW > 0.0f || sel) {
      if (node->animIndicatorX < 0.0f) {
        node->animIndicatorX = targetX;
        node->animIndicatorW = targetW;
      }
      float ki = std::min(1.0f, dt * 16.0f);
      node->animIndicatorX += (targetX - node->animIndicatorX) * ki;
      node->animIndicatorW += (targetW - node->animIndicatorW) * ki;
      float h = 3.0f;
      Rectangle ind = {node->animIndicatorX,
                       node->layout.y + node->layout.height - h,
                       node->animIndicatorW, h};
      raym3::Renderer::DrawRoundedRectangle(ind, h * 0.5f,
                                            Theme::GetColorScheme().primary);
    }
  }

  if (useStencil) {
    raym3::PopRoundedStencil();
  } else if (clipped) {
    PopScissor();
  }
  if (clipped && g_cullStack.size() > 1)
    g_cullStack.pop_back();

  // ButtonGroup/SegmentedButton outer border drawn after stencil pop so it
  // sits on top of selection fills and is never clipped by its own container.
  if (node->role == NodeRole::ButtonGroupContainer) {
    float opacity  = CurrentRenderOpacity();
    float radius   = style.borderRadius.value_or(20.0f);
    raym3::Renderer::DrawRoundedRectangleEx(
        node->layout, radius,
        ColorAlpha(Theme::GetColorScheme().outline, opacity), 1.0f);
  }

  if (hasTransform)
    rlPopMatrix();
  g_renderOpacity = parentOpacity;
  g_inheritedTextColor = parentInheritedText;
}

// Compute screen-relative layout for a fixed-position node.
// Two-pass: first measure natural content height, then position at final coords.
static void LayoutFixed(const NodePtr &node, Rectangle screen) {
  Style style = EffectiveStyle(*node);
  float marginL = style.margin.Left();
  float marginR = style.margin.Right();
  float marginT = style.margin.Top();
  float marginB = style.margin.Bottom();

  // Resolve width from inset.left+right pair or explicit width.
  bool fillW = style.inset.left.has_value() && style.inset.right.has_value();
  float w = fillW
      ? (screen.width - style.inset.left.value_or(0) - style.inset.right.value_or(0) - marginL - marginR)
      : style.width.value_or(320.0f);
  if (style.minWidth) w = std::max(w, *style.minWidth);
  if (style.maxWidth) w = std::min(w, *style.maxWidth);
  w = std::max(0.0f, std::min(w, screen.width));

#if RAYM3_USE_YOGA
  float h;
  {
    YGNodeRef ygRootPass1 = BuildYogaTree(node, true);
    YGNodeStyleSetWidth(ygRootPass1, w);
    YGNodeCalculateLayout(ygRootPass1, w, YGUndefined, YGDirectionLTR);
    float contentH = YGNodeLayoutGetHeight(ygRootPass1);
    YGNodeFreeRecursive(ygRootPass1);
    h = style.height.value_or(contentH);
  }
#else
  // Pass 1: measure natural height by laying out at a large unconstrained height.
  LayoutFallback(node, {0, 0, w, screen.height * 2.0f});
  // Sum child heights to approximate true content height.
  float gap  = style.gap.value_or(0.0f);
  float padT = style.padding.Top();
  float padB = style.padding.Bottom();
  float contentH = padT + padB;
  for (size_t i = 0; i < node->children.size(); ++i) {
    if (!node->children[i]) continue;
    Style cs = EffectiveStyle(*node->children[i]);
    if (cs.display == Display::None) continue;
    contentH += node->children[i]->layout.height;
    if (i + 1 < node->children.size()) contentH += gap;
  }
  float h = style.height.value_or(contentH);
#endif
  if (style.minHeight) h = std::max(h, *style.minHeight);
  if (style.maxHeight) h = std::min(h, *style.maxHeight);
  h = std::max(0.0f, std::min(h, screen.height * 0.9f));

  // Resolve X.
  float x;
  if (node->anchorRect) {
    x = node->anchorRect->x;
    x = std::max(screen.x, std::min(x, screen.x + screen.width - w));
  } else if (style.inset.left.has_value()) {
    x = screen.x + *style.inset.left + marginL;
  } else if (style.inset.right.has_value()) {
    x = screen.x + screen.width - *style.inset.right - w - marginR;
  } else {
    x = screen.x + (screen.width - w) * 0.5f;
  }

  // Resolve Y.
  float y;
  if (node->anchorRect) {
    float belowY = node->anchorRect->y + node->anchorRect->height;
    float aboveY = node->anchorRect->y - h;
    if (node->placement == PopoverPlacement::Below) {
      y = belowY;
    } else if (node->placement == PopoverPlacement::Above) {
      y = aboveY;
    } else { // Auto
      if (belowY + h > screen.y + screen.height && node->anchorRect->y - h >= screen.y) {
        y = aboveY;
      } else {
        y = belowY;
      }
    }
    y = std::max(screen.y, std::min(y, screen.y + screen.height - h));
  } else if (style.inset.top.has_value()) {
    y = screen.y + *style.inset.top + marginT;
  } else if (style.inset.bottom.has_value()) {
    y = screen.y + screen.height - *style.inset.bottom - h - marginB +
        node->overlayDragOffsetY;
  } else {
    y = screen.y + (screen.height - h) * 0.5f;
  }

  // Pass 2: final layout at resolved screen position.
#if RAYM3_USE_YOGA
  {
    YGNodeRef ygRootPass2 = BuildYogaTree(node, true);
    YGNodeStyleSetWidth(ygRootPass2, w);
    YGNodeStyleSetHeight(ygRootPass2, h);
    YGNodeCalculateLayout(ygRootPass2, w, h, YGDirectionLTR);
    StoreYogaLayout(node, ygRootPass2, x, y);
    YGNodeFreeRecursive(ygRootPass2);
  }
#else
  LayoutFallback(node, {x, y, w, h});
#endif
}

// Render one fixed-position node: optional scrim + content.
static void RenderFixedNode(const FixedNode &fn, Rectangle screen) {
  if (!fn.node || fn.node->style.display == Display::None)
    return;

  if (fn.node->role == NodeRole::BottomSheet &&
      fn.node->bottomSheetHasPresented && fn.node->bottomSheetAnimating) {
    const float target = fn.node->bottomSheetDismissPending
        ? std::max(1.0f, fn.node->layout.height)
        : 0.0f;
    float dt = GetFrameTime();
    if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;
    // Exponential settling remains stable across 60/90/120 Hz displays and
    // can be interrupted by a new handle drag without discontinuity.
    const float alpha = 1.0f - std::exp(-18.0f * dt);
    fn.node->overlayDragOffsetY +=
        (target - fn.node->overlayDragOffsetY) * alpha;
    if (std::fabs(target - fn.node->overlayDragOffsetY) < 0.75f) {
      fn.node->overlayDragOffsetY = target;
      fn.node->bottomSheetAnimating = false;
      fn.node->alwaysAnimates = false;
      if (fn.node->bottomSheetDismissPending) {
        fn.node->bottomSheetDismissPending = false;
        if (fn.node->onRequestClose) fn.node->onRequestClose();
        else if (fn.node->onPress) fn.node->onPress();
      }
    }
  }

  // Three-layer modal model:
  //   base content      → z 0..N      (everything else)
  //   scrim backdrop     → z = panelZ-1 (full-screen dismiss target)
  //   modal container    → z = panelZ    (the panel itself)
  // The scrim is its OWN layer one step BELOW the panel, so a tap inside the
  // panel always resolves to the higher-z container (never dismiss), and a tap
  // outside lands on the scrim (dismiss) — independent of paint order. Scrim
  // alpha is style-driven (style.scrimOpacity), defaulting to the M3 0.32.
  if (fn.node->hasScrim) {
    ColorScheme &scheme = Theme::GetColorScheme();
    float scrimAlpha =
        EffectiveStyle(*fn.node).scrimOpacity.value_or(kDefaultScrimOpacity);
    scrimAlpha = std::clamp(scrimAlpha, 0.0f, 1.0f);
    // A bottom sheet's scrim tracks how far the sheet has slid off-screen
    // (drag OR the settle animation), so it fades out WITH the sheet
    // instead of staying fully opaque until the sheet fully clears and then
    // vanishing in one frame (perceived as the scrim "sticking" briefly
    // after a swipe-dismiss).
    if (fn.node->role == NodeRole::BottomSheet) {
      const float extent = std::max(1.0f, fn.node->layout.height);
      const float hiddenFraction =
          std::clamp(fn.node->overlayDragOffsetY / extent, 0.0f, 1.0f);
      scrimAlpha *= (1.0f - hiddenFraction);
    }
    int scrimZ = fn.node->zIndex - 1;
    if (scrimAlpha > 0.0f)
      DrawRectangleRec(screen, ColorAlpha(scheme.scrim, scrimAlpha));
    // The scrim entry always captures input (occludes + dismiss target) even
    // when fully transparent, so an invisible scrim still blocks taps from
    // reaching base content and still dismisses on an outside tap.
    Ctx().stackOrder.push_back(
        {fn.node, screen, Ctx().paintCounter++, true, true, scrimZ});
  }

  LayoutFixed(fn.node, screen);
  if (fn.node->role == NodeRole::BottomSheet &&
      !fn.node->bottomSheetHasPresented) {
    // Begin below the viewport, then let the next frames settle to the open
    // detent. The user can grab the handle during this presentation.
    fn.node->bottomSheetHasPresented = true;
    fn.node->overlayDragOffsetY = std::max(1.0f, fn.node->layout.height);
    fn.node->bottomSheetAnimating = true;
    fn.node->alwaysAnimates = true;
    LayoutFixed(fn.node, screen);
  }
  // Fixed roots start a new stacking layer keyed on their own zIndex.
  RenderNode(fn.node, fn.node->zIndex);
}

static void BuildParentMap(const NodePtr &node, std::unordered_map<Node *, NodePtr> &parentMap) {
  if (!node) return;
  for (const NodePtr &child : node->children) {
    if (child) {
      parentMap[child.get()] = node;
      BuildParentMap(child, parentMap);
    }
  }
}

static bool IsDescendant(const NodePtr &child, const NodePtr &parent, const std::unordered_map<Node *, NodePtr> &parentMap) {
  if (!child || !parent) return false;
  Node *curr = child.get();
  while (curr) {
    auto it = parentMap.find(curr);
    curr = (it != parentMap.end()) ? it->second.get() : nullptr;
    if (curr == parent.get()) {
      return true;
    }
  }
  return false;
}

static Node *GetFixedRoot(Node *node, const std::unordered_map<Node *, NodePtr> &parentMap) {
  Node *curr = node;
  Node *lastFixed = node;
  while (curr) {
    Style style = EffectiveStyle(*curr);
    if (style.position == PositionType::Fixed) {
      lastFixed = curr;
    }
    auto it = parentMap.find(curr);
    curr = (it != parentMap.end()) ? it->second.get() : nullptr;
  }
  return lastFixed;
}

static void CollectFixedNodes(const NodePtr &node, std::vector<FixedNode> &fixedNodes) {
  if (!node || node->style.display == Display::None)
    return;

  for (const NodePtr &child : node->children) {
    if (!child) continue;
    Style cs = EffectiveStyle(*child);
    if (cs.position == PositionType::Fixed && cs.display != Display::None) {
      fixedNodes.push_back({child, child->zIndex});
    }
    CollectFixedNodes(child, fixedNodes);
  }
}

void Render(const NodePtr &root, Rectangle bounds, bool layoutAlreadyComputed) {
  float dt = GetFrameTime();
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;
  TickRipples(dt);

  Ctx().fixedNodes.clear();
  Ctx().stackOrder.clear();
  Ctx().parentMap.clear();
  Ctx().paintCounter = 0;
  // UpdateLayout runs before Render in the frame, so the Yoga build count for
  // this frame is already recorded; don't wipe it with the paint counters.
  const int yogaBuilt = Ctx().lastStats.yogaNodesBuilt;
  Ctx().lastStats = {};
  Ctx().lastStats.yogaNodesBuilt = yogaBuilt;
  if (!layoutAlreadyComputed)
    UpdateLayout(root, bounds);
  
  BuildParentMap(root, Ctx().parentMap);

  // Pre-collect all fixed-position overlay nodes recursively before rendering
  CollectFixedNodes(root, Ctx().fixedNodes);

  // Seed viewport culling with the on-screen bounds (DP space).
  g_cullStack.clear();
  g_cullStack.push_back(bounds);

  RenderNode(root, root ? root->zIndex : 0);

  // Fixed-position pass: paint overlay nodes (Dialog, BottomSheet, etc.) on
  // top of all normal content, sorted ascending by zIndex (respecting fixed-root ancestry).
  if (!Ctx().fixedNodes.empty()) {
    std::stable_sort(Ctx().fixedNodes.begin(), Ctx().fixedNodes.end(),
                     [](const FixedNode &a, const FixedNode &b) {
                       Node *ra = GetFixedRoot(a.node.get(), Ctx().parentMap);
                       Node *rb = GetFixedRoot(b.node.get(), Ctx().parentMap);
                       if (ra != rb) {
                         return ra->zIndex < rb->zIndex;
                       }
                       if (IsDescendant(a.node, b.node, Ctx().parentMap)) {
                         return false;
                       }
                       if (IsDescendant(b.node, a.node, Ctx().parentMap)) {
                         return true;
                       }
                       return a.zIndex < b.zIndex;
                     });
    for (const FixedNode &fn : Ctx().fixedNodes) {
      BuildParentMap(fn.node, Ctx().parentMap);
      RenderFixedNode(fn, bounds);
    }
  }

  PaintTextSelectionOverlay(root);

  // Commit the fully-built stack for input queries. Inline OwnsInput calls made
  // during the next frame's tree walk read this complete snapshot; HitTest runs
  // after Render() so it reads the snapshot just committed for the current frame.
  Ctx().committedStackOrder = Ctx().stackOrder;
  Ctx().committedParentMap = Ctx().parentMap;
}

NodePtr CommittedParentOf(const Node *node) {
  auto it = Ctx().committedParentMap.find(const_cast<Node *>(node));
  return it != Ctx().committedParentMap.end() ? it->second : nullptr;
}

void RenderOverlayRepaint(const NodePtr &root, Rectangle bounds) {
  // Save the input state built by the main Render pass. The repaint below
  // runs a full Render (it needs the working maps for its own fixed-node
  // sorting), then we restore both the working and committed snapshots so
  // hit testing keeps seeing the whole frame, not just this subtree.
  auto savedStack = std::move(Ctx().stackOrder);
  auto savedParent = std::move(Ctx().parentMap);
  auto savedFixed = std::move(Ctx().fixedNodes);
  auto savedCommittedStack = std::move(Ctx().committedStackOrder);
  auto savedCommittedParent = std::move(Ctx().committedParentMap);
  auto savedStats = Ctx().lastStats;

  Render(root, bounds, /*layoutAlreadyComputed=*/true);

  Ctx().stackOrder = std::move(savedStack);
  Ctx().parentMap = std::move(savedParent);
  Ctx().fixedNodes = std::move(savedFixed);
  Ctx().committedStackOrder = std::move(savedCommittedStack);
  Ctx().committedParentMap = std::move(savedCommittedParent);
  Ctx().lastStats = savedStats;
}

namespace {

constexpr float kAnimEpsilon = 0.5f;
constexpr float kScrollVelocityActive = 5.0f;

static bool NodeNeedsAnotherFrame(const NodePtr &node) {
  if (!node || node->style.display == Display::None)
    return false;
  if (node->alwaysAnimates)
    return true;
  // In-flight CSS transitions (Transitions.cpp).
  if (!node->activeTransitions.empty())
    return true;
  // Running CSS @keyframes animations (Animations.cpp) — incl. infinite ones,
  // which is what keeps a looping animation ticking with no JS involvement.
  if (!node->activeAnimations.empty())
    return true;
  // Toggle controls (checkbox/switch/radio) mid-transition.
  if (node->control.animTarget >= 0.0f && node->control.anim >= 0.0f &&
      std::abs(node->control.anim - node->control.animTarget) > 0.01f)
    return true;
  if (node->flingActive)
    return true;
  if (std::abs(node->scrollVelocityX) > kScrollVelocityActive ||
      std::abs(node->scrollVelocityY) > kScrollVelocityActive)
    return true;
  if (node->role == NodeRole::Tabs && node->animIndicatorX >= 0.0f &&
      node->animIndicatorFade > 0.0f) {
    if (std::abs(node->animIndicatorX - node->animIndicatorY) > kAnimEpsilon ||
        std::abs(node->animIndicatorW - node->animIndicatorFade) > kAnimEpsilon)
      return true;
  }
  // Nav bar/rail item select + indicator-fade animations tick per frame in
  // DrawNode; without reporting them here, an on-demand frame scheduler
  // (Android) stops pumping and the indicator crawls along on whatever
  // stray frames timers happen to produce.
  if (node->role == NodeRole::NavItem) {
    const float targetSelect = node->selected ? 1.0f : 0.0f;
    if (std::abs(node->animSelect - targetSelect) > kAnimEpsilon)
      return true;
    if (node->inNavigationBar &&
        std::abs(node->animIndicatorFade - targetSelect) > kAnimEpsilon)
      return true;
  }
  if (node->kind == NodeKind::TextInput && GetFocusedId() == IdOf(node))
    return true;
  if (node->kind == NodeKind::TextInput && !node->textInput.label.empty()) {
    char *buf = node->textInput.buffer
                    ? node->textInput.buffer
                    : (node->inputBuffer.empty() ? nullptr : node->inputBuffer.data());
    bool hasContent = buf && buf[0] != '\0';
    bool focused = GetFocusedId() == IdOf(node);
    float target = (focused || hasContent) ? 1.0f : 0.0f;
    if (std::abs(node->textEdit.labelAnim - target) > 0.02f)
      return true;
  }
  if (node->animStateAlpha > 0.02f)
    return true;
  for (const NodePtr &child : node->children) {
    if (NodeNeedsAnotherFrame(child))
      return true;
  }
  return false;
}

} // namespace

bool NeedsAnotherFrame(const NodePtr &root) {
  if (HasActiveRipples())
    return true;
  return NodeNeedsAnotherFrame(root);
}

void SetIdleSkipEnabled(bool enabled) { Ctx().idleSkipEnabled = enabled; }

bool ShouldSkipRender(const NodePtr &root) {
  if (!Ctx().idleSkipEnabled || Ctx().forceRender)
    return false;
  if (NeedsAnotherFrame(root))
    return false;
  if (!GetDirtyRects().empty())
    return false;
  return true;
}

void MarkDirtyRect(Rectangle rect) {
  if (rect.width <= 0 || rect.height <= 0)
    return;
  Ctx().dirtyRects.push_back(rect);
  Ctx().forceRender = true;
}

void ClearDirtyRects() {
  Ctx().dirtyRects.clear();
  Ctx().forceRender = false;
}

const std::vector<Rectangle> &GetDirtyRects() { return Ctx().dirtyRects; }

static bool IsClippedByAncestors(const NodePtr &node, Vector2 point, const std::unordered_map<Node *, NodePtr> &parentMap) {
  Node *curr = node.get();
  while (curr) {
    Style style = EffectiveStyle(*curr);
    if (style.overflow == Overflow::Hidden || style.overflow == Overflow::Scroll) {
      if (!CheckCollisionPointRec(point, curr->layout)) {
        return true; // Clipped!
      }
    }
    // Fixed-position overlays are viewport-positioned: ancestors' overflow
    // rects (scroll containers, cards) don't clip them.
    if (style.position == PositionType::Fixed)
      return false;
    auto it = parentMap.find(curr);
    curr = (it != parentMap.end()) ? it->second.get() : nullptr;
  }
  return false;
}

// Topmost hit-testable committed entry under `point`, ranked by effective z
// then paint order. This is the whole input model: highest-z occluder wins, so
// a high-z scrim naturally blocks everything beneath it — no modal special case.
static const StackEntry *HitEntry(Vector2 point) {
  const StackEntry *best = nullptr;
  for (const StackEntry &e : Ctx().committedStackOrder) {
    if (!e.node || e.node->style.display == Display::None || !e.occludes)
      continue;
    if (!NodeReceivesInput(*e.node, EffectiveStyle(*e.node)))
      continue;
    if (!CheckCollisionPointRec(point, e.bounds))
      continue;
    // Scrim entries span the whole screen on behalf of an overlay node whose
    // own overflow rect is just the panel — don't let the panel's clip swallow
    // backdrop taps.
    if (!e.isScrimBackdrop &&
        IsClippedByAncestors(e.node, point, Ctx().committedParentMap))
      continue;
    if (!best || e.effectiveZ > best->effectiveZ ||
        (e.effectiveZ == best->effectiveZ && e.paintIndex > best->paintIndex))
      best = &e;
  }
  return best;
}

NodePtr InputOwnerAt(Vector2 point) {
  const StackEntry *e = HitEntry(point);
  return e ? e->node : nullptr;
}

bool CapturesPoint(Vector2 point) { return InputOwnerAt(point) != nullptr; }

bool OwnsInput(const NodePtr &node, Vector2 point) {
  if (!node)
    return false;
  NodePtr owner = InputOwnerAt(point);
  Node *curr = owner ? owner.get() : nullptr;
  while (curr) {
    if (curr == node.get())
      return true;
    auto it = Ctx().committedParentMap.find(curr);
    curr = (it != Ctx().committedParentMap.end()) ? it->second.get() : nullptr;
  }
  return false;
}

// Climb from owner to the nearest node carrying an interaction handler. A
// scrim-bearing overlay is dismiss-only (handled via the scrim path), so the
// climb stops there without dismissing — a tap on the panel body is consumed.
static NodePtr InteractiveTargetFrom(const NodePtr &owner) {
  NodePtr curr = owner;
  while (curr) {
    if (curr->hasScrim)
      return nullptr;
    if (NodeIsInteractive(*curr))
      return curr;
    auto it = Ctx().committedParentMap.find(curr.get());
    curr = (it != Ctx().committedParentMap.end()) ? it->second : nullptr;
  }
  return nullptr;
}

NodePtr HitTest(const NodePtr &root, Vector2 point) {
  (void)root;
  Ctx().lastStats.hitTestCount++;
  const StackEntry *e = HitEntry(point);
  if (!e)
    return nullptr;
  if (e->isScrimBackdrop)
    return e->node; // scrim dismiss target
  return InteractiveTargetFrom(e->node);
}

NodePtr InteractiveTargetAt(Vector2 point) {
  const StackEntry *e = HitEntry(point);
  if (!e || e->isScrimBackdrop)
    return nullptr;
  return InteractiveTargetFrom(e->node);
}

// --- Scroll diagnostics (RAYACT_SCROLL_TRACE=1) ----------------------------

namespace {

struct ScrollTraceBucket {
  int writes = 0;
  float netDelta = 0.0f; // signed sum of (to - from)
};

struct ScrollTraceState {
  // Indexed by ScrollWriteSource. Reset per gesture.
  ScrollTraceBucket buckets[7];
  double gestureStartTime = 0.0;
  bool active = false;
};

ScrollTraceState g_scrollTrace;

const char *ScrollSourceName(ScrollWriteSource s) {
  switch (s) {
  case ScrollWriteSource::Drag:      return "drag";
  case ScrollWriteSource::Wheel:     return "wheel";
  case ScrollWriteSource::Fling:     return "fling";
  case ScrollWriteSource::Clamp:     return "clamp";
  case ScrollWriteSource::FollowEnd: return "followEnd";
  case ScrollWriteSource::Js:        return "js";
  case ScrollWriteSource::Mutation:  return "mutation";
  }
  return "?";
}

} // namespace

bool ScrollTraceEnabled() {
  // Read once: this sits on the fling hot path.
  static const bool enabled = [] {
    const char *v = std::getenv("RAYACT_SCROLL_TRACE");
    return v && v[0] == '1';
  }();
  return enabled;
}

void ScrollTraceEvent(const char *fmt, ...) {
  if (!ScrollTraceEnabled())
    return;
  if (!g_scrollTrace.active) {
    g_scrollTrace.active = true;
    g_scrollTrace.gestureStartTime = GetTime();
  }
  fprintf(stderr, "[scroll %+7.3fs] ",
          GetTime() - g_scrollTrace.gestureStartTime);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fputc('\n', stderr);
}

void ScrollTraceOffsetWrite(const Node &node, ScrollWriteSource source,
                            char axis, float from, float to) {
  if (!ScrollTraceEnabled())
    return;
  if (from == to)
    return;
  ScrollTraceBucket &b = g_scrollTrace.buckets[static_cast<int>(source)];
  b.writes++;
  b.netDelta += (to - from);
  // Per-write detail is noisy during a fling, so only the histogram is printed
  // at gesture end; individual writes go out only for the non-fling sources
  // that are supposed to be rare.
  if (source != ScrollWriteSource::Fling && source != ScrollWriteSource::Drag)
    ScrollTraceEvent("  write %-9s %c %8.2f -> %8.2f  (%+.2f) node=%p",
                     ScrollSourceName(source), axis, from, to, to - from,
                     static_cast<const void *>(&node));
}

void ScrollTraceFlushGesture(const char *reason) {
  if (!ScrollTraceEnabled() || !g_scrollTrace.active)
    return;
  fprintf(stderr, "[scroll] --- gesture end (%s) ---\n", reason);
  for (int i = 0; i < 7; ++i) {
    const ScrollTraceBucket &b = g_scrollTrace.buckets[i];
    if (b.writes == 0)
      continue;
    fprintf(stderr, "[scroll]   %-9s writes=%-4d net=%+9.2f\n",
            ScrollSourceName(static_cast<ScrollWriteSource>(i)), b.writes,
            b.netDelta);
  }
  fprintf(stderr, "[scroll] ---------------------------\n");
  g_scrollTrace = ScrollTraceState{};
}

// --- Scroll input (Flutter-like gesture competition) -----------------------

namespace {

constexpr float kWheelScrollScale = 48.0f;
constexpr float kMinFlingVelocity = 50.0f;
constexpr float kMaxFlingVelocity = 8000.0f;
constexpr float kScrollFriction = 0.95f;
constexpr float kVelocityStopThreshold = 5.0f;

// Android ClampingScrollSimulation constants (Flutter scroll_simulation.dart).
const float kFlingDecelerationRate =
    std::log(0.78f) / std::log(0.9f); // ~2.3582
constexpr float kFlingInflexion = 0.35f;
constexpr float kFlingFriction = 0.015f;
constexpr float kFlingPhysicalCoeff = 9.80665f * 39.37f * 160.0f * 0.84f;

// Scroll-gesture mutable state lives in Ctx().scroll (RenderContext.h).
constexpr int kVelocitySampleCapacity = ScrollGestureState::kVelocitySampleCapacity;
constexpr double kVelocityHorizonSeconds = 0.100;
constexpr double kVelocityAssumeStoppedSeconds = 0.040;
constexpr int kVelocityMinSamples = 3;

static void VelocityTrackerReset() {
  ScrollGestureState &sg = Ctx().scroll;
  sg.velocitySampleHead = 0;
  sg.velocitySampleCount = 0;
}

static void VelocityTrackerAddSample(double time, float y) {
  ScrollGestureState &sg = Ctx().scroll;
  sg.velocitySamples[sg.velocitySampleHead] = {time, y};
  sg.velocitySampleHead = (sg.velocitySampleHead + 1) % kVelocitySampleCapacity;
  if (sg.velocitySampleCount < kVelocitySampleCapacity)
    sg.velocitySampleCount++;
}

// Least-squares linear fit y(t) over recent samples; returns velocity in
// pointer px/s, or 0 when the pointer was stopped or data is insufficient.
static float VelocityTrackerEstimate(double releaseTime) {
  double times[kVelocitySampleCapacity];
  float ys[kVelocitySampleCapacity];
  int count = 0;
  double prevTime = releaseTime;
  for (int i = 0; i < Ctx().scroll.velocitySampleCount; ++i) {
    int idx = (Ctx().scroll.velocitySampleHead - 1 - i + 2 * kVelocitySampleCapacity) %
              kVelocitySampleCapacity;
    const ScrollSample &s = Ctx().scroll.velocitySamples[idx];
    double age = releaseTime - s.time;
    double gap = prevTime - s.time;
    if (age > kVelocityHorizonSeconds || gap > kVelocityAssumeStoppedSeconds)
      break;
    times[count] = -age;
    ys[count] = s.y;
    count++;
    prevTime = s.time;
  }
  if (count < kVelocityMinSamples)
    return 0.0f;

  // Linear least squares: slope = (n*sum(ty) - sum(t)sum(y)) / (n*sum(tt) - sum(t)^2)
  double sumT = 0, sumY = 0, sumTY = 0, sumTT = 0;
  for (int i = 0; i < count; ++i) {
    sumT += times[i];
    sumY += ys[i];
    sumTY += times[i] * ys[i];
    sumTT += times[i] * times[i];
  }
  double denom = count * sumTT - sumT * sumT;
  if (std::abs(denom) < 1e-9)
    return 0.0f;
  double slope = (count * sumTY - sumT * sumY) / denom;
  return static_cast<float>(slope);
}

// ClampingScrollSimulation: duration and total travel for a fling velocity.
static float FlingDurationFor(float velocity) {
  float referenceVelocity = kFlingFriction * kFlingPhysicalCoeff / kFlingInflexion;
  float androidDuration = std::pow(std::abs(velocity) / referenceVelocity,
                                   1.0f / (kFlingDecelerationRate - 1.0f));
  return kFlingDecelerationRate * kFlingInflexion * androidDuration;
}

static void StartFling(const NodePtr &node, float velocity) {
  velocity = std::clamp(velocity, -kMaxFlingVelocity, kMaxFlingVelocity);
  float duration = FlingDurationFor(velocity);
  if (duration <= 0.0f)
    return;
  node->flingActive = true;
  node->flingStartTime = GetTime();
  node->flingStartOffsetY = node->scrollOffsetY;
  node->flingDuration = duration;
  node->flingDistance = velocity * duration / kFlingDecelerationRate;
  ScrollTraceEvent(
      "fling start  v=%+8.1f dur=%.3fs dist=%+8.1f from=%8.2f max=%8.2f",
      velocity, duration, node->flingDistance, node->scrollOffsetY,
      std::max(0.0f, node->scrollContentHeight - node->layout.height));
}

static void StopFling(const NodePtr &node) {
  if (node->flingActive) {
    const float achieved = node->scrollOffsetY - node->flingStartOffsetY;
    ScrollTraceEvent(
        "fling stop   requested=%+8.1f achieved=%+8.1f (%.0f%%) at=%8.2f",
        node->flingDistance, achieved,
        node->flingDistance != 0.0f ? 100.0f * achieved / node->flingDistance
                                    : 0.0f,
        node->scrollOffsetY);
  }
  node->flingActive = false;
  node->scrollVelocityY = 0.0f;
}

static NodePtr FixedOverlayRoot(const NodePtr &node) {
  if (!node)
    return nullptr;
  NodePtr result;
  NodePtr curr = node;
  while (curr) {
    Style s = EffectiveStyle(*curr);
    if (s.position == PositionType::Fixed || curr->hasScrim)
      result = curr;
    auto it = Ctx().committedParentMap.find(curr.get());
    curr = (it != Ctx().committedParentMap.end()) ? it->second : nullptr;
  }
  return result;
}

static bool InCommittedSubtree(const NodePtr &node, const NodePtr &root) {
  if (!node || !root)
    return false;
  Node *curr = node.get();
  while (curr) {
    if (curr == root.get())
      return true;
    auto it = Ctx().committedParentMap.find(curr);
    curr = (it != Ctx().committedParentMap.end()) ? it->second.get() : nullptr;
  }
  return false;
}

static void ClearScrollVelocitiesOutside(const NodePtr &node,
                                         const NodePtr &keepSubtree) {
  if (!node)
    return;
  if (!InCommittedSubtree(node, keepSubtree)) {
    node->scrollVelocityX = 0.0f;
    node->scrollVelocityY = 0.0f;
    node->flingActive = false;
  }
  for (const NodePtr &child : node->children)
    ClearScrollVelocitiesOutside(child, keepSubtree);
}

static bool NeedsImmediateCapture(const Node &node, bool isScrim) {
  if (isScrim)
    return true;
  // TextInput uses deferred press so vertical scroll can win the gesture arena
  // without blurring or dismissing the keyboard.
  if (IsControlKind(node.kind))
    return true;
  if (node.onValueChange)
    return true;
  // Pan/drag handlers (e.g. time-picker dial) must capture on press so
  // onDragMove fires while the pointer is down.
  if (node.onDragStart || node.onDragMove || node.onDragEnd)
    return true;
  return false;
}

static NodePtr FindScrollableNodeAt(const NodePtr &node, Vector2 point) {
  if (!node || node->style.display == Display::None ||
      node->style.pointerEvents == PointerEvents::None)
    return nullptr;

  bool inBounds = CheckCollisionPointRec(point, node->layout);
  bool clipped = node->style.overflow == Overflow::Hidden ||
                 node->style.overflow == Overflow::Scroll;
  if (clipped && !inBounds)
    return nullptr;

  std::vector<NodePtr> children = node->children;
  std::stable_sort(children.begin(), children.end(),
                   [](const NodePtr &a, const NodePtr &b) {
                     return a->zIndex > b->zIndex;
                   });
  for (const NodePtr &child : children) {
    if (auto hit = FindScrollableNodeAt(child, point))
      return hit;
  }

  Style style = EffectiveStyle(*node);
  bool canScroll = node->scrollHorizontal
      ? node->scrollContentWidth > node->layout.width + 0.5f
      : node->scrollContentHeight > node->layout.height + 0.5f;
  if (inBounds && style.overflow == Overflow::Scroll && canScroll)
    return node;
  return nullptr;
}

static NodePtr FindScrollableForInput(const NodePtr &root, Vector2 point) {
  const StackEntry *hit = HitEntry(point);
  if (hit && hit->node) {
    if (NodePtr overlay = FixedOverlayRoot(hit->node))
      return FindScrollableNodeAt(overlay, point);
  }
  if (HasModalOverlay()) {
    NodePtr modal = TopmostModalNode();
    return FindScrollableNodeAt(modal, point);
  }
  return FindScrollableNodeAt(root, point);
}

static bool ScrollNodeBy(const NodePtr &node, float deltaX, float deltaY,
                         ScrollWriteSource source = ScrollWriteSource::Drag) {
  if (!node)
    return false;
  if (std::abs(deltaY) > 0.01f)
    node->scrollFollowEnd = false;
  float oldX = node->scrollOffsetX;
  float oldY = node->scrollOffsetY;
  node->scrollOffsetX =
      ClampScrollOffset(node->scrollOffsetX + deltaX, node->scrollContentWidth,
                        node->layout.width);
  node->scrollOffsetY =
      ClampScrollOffset(node->scrollOffsetY + deltaY, node->scrollContentHeight,
                        node->layout.height);
  ScrollTraceOffsetWrite(*node, source, 'x', oldX, node->scrollOffsetX);
  ScrollTraceOffsetWrite(*node, source, 'y', oldY, node->scrollOffsetY);
  bool changed = std::abs(node->scrollOffsetX - oldX) > 0.01f ||
                 std::abs(node->scrollOffsetY - oldY) > 0.01f;
  if (changed && node->onScroll)
    node->onScroll();
  return changed;
}

static void ClearScrollGesture() {
  Ctx().scroll.candidate = nullptr;
  Ctx().scroll.engaged = false;
  Ctx().scroll.frameVelocityY = 0.0f;
}

// A node captured purely because it has pan/drag handlers shares the gesture
// arena with an ancestor scroller instead of owning it outright. Controls,
// scrims and value-drivers still take the gesture exclusively: their drag IS
// the interaction, and there is nothing sensible to hand off.
static bool ActiveIsSharedDragCapture() {
  NodeId id = GetActiveId();
  if (id == 0)
    return false;
  if (Ctx().activeIsScrim || Ctx().activeIsBottomSheetDrag)
    return false;
  auto *n = reinterpret_cast<Node *>(id);
  if (!n)
    return false;
  if (IsControlKind(n->kind) || n->onValueChange)
    return false;
  return n->onDragStart || n->onDragMove || n->onDragEnd;
}

// Called once the scroll gesture engages while such a node holds the pointer.
// The node is told the drag ended so it can spring back, then released so the
// rest of the gesture belongs to the scroller.
static void YieldSharedDragToScroll(Vector2 pt) {
  NodeId id = GetActiveId();
  if (id == 0)
    return;
  auto *n = reinterpret_cast<Node *>(id);
  if (n && n->onDragEnd)
    n->onDragEnd({pt.x - GetDragOrigin().x, pt.y - GetDragOrigin().y});
  StartRippleFadeOut(id);
  SetActiveId(0);
}

static bool NodeSupportsRipple(const Node &node) {
  if (node.disabled || node.hasScrim)
    return false;
  if (node.style.pointerEvents == PointerEvents::None)
    return false;
  if (node.kind == NodeKind::Button)
    return node.onPress != nullptr;
  if (IsControlKind(node.kind))
    return false;
  // A plain interactive View opts into ripples by declaring a CSS ripple-color.
  if (node.kind == NodeKind::View && node.onPress && node.style.rippleColor)
    return true;
  return node.inkRipple && node.onPress;
}

static Color RippleInkForNode(const Node &node) {
  Style style = EffectiveStyle(node);
  // CSS `ripple-color` wins; otherwise derive the M3 pressed state-layer tint
  // from the content color.
  if (style.rippleColor)
    return *style.rippleColor;
  Color content =
      style.text.color.value_or(Theme::GetColorScheme().onSurface);
  return Theme::GetStateLayerColor(content, ComponentState::Pressed);
}

static void TrySpawnRipple(const NodePtr &node, Vector2 pt) {
  if (!node || !NodeSupportsRipple(*node))
    return;
  SpawnRipple(IdOf(node), pt, node->layout, RippleInkForNode(*node));
}

static void ClearPendingPress() {
  if (GetPendingPressId() != 0)
    CancelRipplesForNode(GetPendingPressId());
  Ctx().scroll.pendingPressTarget = nullptr;
  SetPendingPressId(0);
}

static void FinishPendingPress() {
  if (GetPendingPressId() != 0)
    StartRippleFadeOut(GetPendingPressId());
  Ctx().scroll.pendingPressTarget = nullptr;
  SetPendingPressId(0);
}

static float PointerTravel(Vector2 from, Vector2 to) {
  float dx = to.x - from.x;
  float dy = to.y - from.y;
  return std::sqrt(dx * dx + dy * dy);
}

static void TickScrollMomentumRecurse(const NodePtr &node, float dt) {
  if (!node || node->style.display == Display::None)
    return;

  // Android-style spline fling (ClampingScrollSimulation): position follows
  // start + distance * (1 - (1-t)^rate); ends analytically at t=1 or at an edge.
  if (node->flingActive) {
    double elapsed = GetTime() - node->flingStartTime;
    float t = node->flingDuration > 0.0f
                  ? static_cast<float>(elapsed / node->flingDuration)
                  : 1.0f;
    bool done = t >= 1.0f;
    t = std::clamp(t, 0.0f, 1.0f);
    float target = node->flingStartOffsetY +
                   node->flingDistance *
                       (1.0f - std::pow(1.0f - t, kFlingDecelerationRate));
    float maxOffset =
        std::max(0.0f, node->scrollContentHeight - node->layout.height);
    float clamped = std::clamp(target, 0.0f, maxOffset);
    if (std::abs(clamped - node->scrollOffsetY) > 0.01f) {
      ScrollTraceOffsetWrite(*node, ScrollWriteSource::Fling, 'y',
                             node->scrollOffsetY, clamped);
      node->scrollOffsetY = clamped;
      if (node->onScroll)
        node->onScroll();
    }
    // Hit an edge: stop dead (native clamping behavior, no decay-at-wall).
    const bool hitEdge = std::abs(clamped - target) > 0.5f;
    if (done || hitEdge) {
      ScrollTraceEvent("fling end reason=%s t=%.3f target=%8.2f clamped=%8.2f",
                       done ? "duration" : "edge", t, target, clamped);
      StopFling(node);
      ScrollTraceFlushGesture(done ? "fling-complete" : "fling-edge");
    }
  } else {
    node->scrollVelocityY = 0.0f;
  }

  float friction = std::pow(kScrollFriction, dt * 60.0f);
  if (std::abs(node->scrollVelocityX) > kVelocityStopThreshold) {
    float delta = node->scrollVelocityX * dt;
    ScrollNodeBy(node, delta, 0.0f, ScrollWriteSource::Fling);
    node->scrollVelocityX *= friction;
    if (std::abs(node->scrollVelocityX) < kVelocityStopThreshold)
      node->scrollVelocityX = 0.0f;
  } else {
    node->scrollVelocityX = 0.0f;
  }

  for (const NodePtr &child : node->children)
    TickScrollMomentumRecurse(child, dt);
}

} // namespace

void CancelFling(const NodePtr &node) {
  if (node) StopFling(node);
}

void TickScrollMomentum(const NodePtr &root) {
  if (!root)
    return;
  float dt = GetFrameTime();
  if (dt <= 0.0f || dt > 0.1f)
    dt = 0.016f;
  if (HasModalOverlay()) {
    NodePtr modal = TopmostModalNode();
    ClearScrollVelocitiesOutside(root, modal);
    if (modal)
      TickScrollMomentumRecurse(modal, dt);
    return;
  }
  TickScrollMomentumRecurse(root, dt);
}

void ResolveScrollInput(const NodePtr &root) {
  bool modalOpen = HasModalOverlay();
  if (modalOpen && !Ctx().scroll.hadModalOverlay)
    ClearScrollGesture();
  Ctx().scroll.hadModalOverlay = modalOpen;

  const PointerInput &p = GetPointer();
  Vector2 pt = p.pos;

  if (modalOpen && !FindScrollableForInput(root, pt))
    ClearScrollGesture();

  // Wheel / trackpad: no slop, scroll directly.
  if (std::abs(p.wheel) > 0.01f) {
    if (NodePtr target = FindScrollableForInput(root, pt))
      ScrollNodeBy(target, 0.0f, -p.wheel * kWheelScrollScale,
                   ScrollWriteSource::Wheel);
    return;
  }

  // ResolveInput owns the gesture when an interactive node is captured — with
  // one exception. A node captured only because it has drag handlers (a
  // swipe-to-reveal row, say) would otherwise swallow every touch that starts
  // on it, so a vertical drag over a list of such rows could never scroll.
  // Those keep tracking a scroll candidate alongside the drag, and the scroll
  // takes over below if the movement turns out to be along the scroll axis.
  const bool sharedDrag = ActiveIsSharedDragCapture();
  if (GetActiveId() != 0 && !sharedDrag) {
    ClearScrollGesture();
    return;
  }

  if (p.pressed) {
    ClearScrollGesture();
    Ctx().scroll.candidate = FindScrollableForInput(root, pt);
    if (Ctx().scroll.candidate) {
      Ctx().scroll.pressOrigin = pt;
      Ctx().scroll.lastPointer = pt;
      StopFling(Ctx().scroll.candidate);
      Ctx().scroll.candidate->scrollVelocityX = 0.0f;
      VelocityTrackerReset();
      VelocityTrackerAddSample(GetTime(), pt.y);
      ScrollTraceEvent("press        at=(%.1f,%.1f) offset=%8.2f followEnd=%d",
                       pt.x, pt.y, Ctx().scroll.candidate->scrollOffsetY,
                       Ctx().scroll.candidate->scrollFollowEnd ? 1 : 0);
    }
    return;
  }

  if (!Ctx().scroll.candidate)
    return;

  if (p.down) {
    float totalDx = pt.x - Ctx().scroll.pressOrigin.x;
    float totalDy = pt.y - Ctx().scroll.pressOrigin.y;
    if (!Ctx().scroll.engaged) {
      const bool horizontal = Ctx().scroll.candidate->scrollHorizontal;
      const float primary = horizontal ? totalDx : totalDy;
      const float cross = horizontal ? totalDy : totalDx;
      if (std::abs(primary) > kTouchSlop && std::abs(primary) > std::abs(cross)) {
        Ctx().scroll.engaged = true;
        // The drag node, if any, loses the gesture here: the movement is
        // primarily along the scroll axis, so the user is scrolling.
        if (sharedDrag)
          YieldSharedDragToScroll(pt);
        // Samples collected before this point came from pre-slop travel and
        // still enter the least-squares fit below, diluting it.
        ScrollTraceEvent(
            "engage       travel=%+.1fdp slop=%.1fdp presamples=%d",
            primary, kTouchSlop, Ctx().scroll.velocitySampleCount);
        ClearPendingPress();
        NodeId focused = GetFocusedId();
        if (focused) {
          auto *fn = reinterpret_cast<Node *>(focused);
          if (fn && fn->kind == NodeKind::TextInput) {
            fn->textEdit.isSelecting = false;
            fn->textEdit.handlesVisible = false;
            fn->textEdit.toolbarVisible = false;
            fn->textEdit.activeHandle = -1;
            fn->textEdit.activeHandleAnchor = -1;
            fn->textEdit.activeHandleOffset = -1;
            fn->textEdit.activeHandleDragY = 0.0f;
            fn->textEdit.activeHandleDragTargetY = 0.0f;
            fn->textEdit.longPressSelectionActive = false;
          }
        }
      }
    }
    VelocityTrackerAddSample(GetTime(), pt.y);
    if (Ctx().scroll.engaged) {
      float dx = pt.x - Ctx().scroll.lastPointer.x;
      float dy = pt.y - Ctx().scroll.lastPointer.y;
      if (Ctx().scroll.candidate->scrollHorizontal)
        ScrollNodeBy(Ctx().scroll.candidate, -dx, 0.0f);
      else
        ScrollNodeBy(Ctx().scroll.candidate, 0.0f, -dy);
      Ctx().scroll.lastPointer = pt;
    }
    return;
  }

  if (p.released) {
    if (Ctx().scroll.engaged && Ctx().scroll.candidate &&
        Ctx().scroll.candidate->scrollMomentumEnabled) {
      // Pointer velocity → scroll velocity (drag moves content opposite).
      float pointerVy = VelocityTrackerEstimate(GetTime());
      float scrollVy = -pointerVy;
      ScrollTraceEvent("release      samples=%d pointerVy=%+8.1f scrollVy=%+8.1f"
                       " min=%.1f -> %s",
                       Ctx().scroll.velocitySampleCount, pointerVy, scrollVy,
                       kMinFlingVelocity,
                       std::abs(scrollVy) > kMinFlingVelocity ? "fling"
                                                             : "DROPPED");
      if (std::abs(scrollVy) > kMinFlingVelocity)
        StartFling(Ctx().scroll.candidate, scrollVy);
      else
        ScrollTraceFlushGesture("no-fling");
    } else if (Ctx().scroll.engaged) {
      ScrollTraceFlushGesture("release-no-momentum");
    }
    ClearScrollGesture();
  }
}

// --- Unified per-frame interaction (microui hover + ImGui ActiveId) ---------

static constexpr float kBottomSheetDismissThreshold = 80.0f;
static constexpr float kBottomSheetDismissVelocity = 800.0f;
// Include the visible handle and a conventional sheet header in the explicit
// gesture affordance. Non-interactive sheet surface can also acquire the drag
// below (buttons/controls keep priority).
static constexpr float kBottomSheetHandleTouchHeight = 112.0f;

static NodePtr FindBottomSheetRoot(const NodePtr &owner) {
  NodePtr curr = owner;
  while (curr) {
    if (curr->role == NodeRole::BottomSheet)
      return curr;
    auto it = Ctx().committedParentMap.find(curr.get());
    curr = (it != Ctx().committedParentMap.end()) ? it->second : nullptr;
  }
  return nullptr;
}

static bool PointInBottomSheetHandle(const Node &sheet, Vector2 pt) {
  const Rectangle &layout = sheet.layout;
  Rectangle hit = {layout.x, layout.y, layout.width,
                   kBottomSheetHandleTouchHeight};
  return CheckCollisionPointRec(pt, hit);
}

static void DriveControlDrag(Node &n, Vector2 pt) {
  ControlState &st = n.control;
  if (n.layout.width <= 0.0f)
    return;
  float frac = std::clamp((pt.x - n.layout.x) / n.layout.width, 0.0f, 1.0f);
  if (n.kind == NodeKind::Slider) {
    float span = st.maxValue - st.minValue;
    float nv = st.minValue + frac * span;
    if (st.step > 0.0f)
      nv = st.minValue + std::round((nv - st.minValue) / st.step) * st.step;
    nv = std::clamp(nv, st.minValue, st.maxValue);
    if (nv != st.value)
      st.value = nv;
    st.dragging = true;
  } else if (n.kind == NodeKind::RangeSlider) {
    float n01 = frac;
    if (st.step > 0.0f)
      n01 = std::round(n01 / st.step) * st.step;
    n01 = std::clamp(n01, 0.0f, 1.0f);
    float minGap = st.step > 0.0f ? st.step : 0.0f;
    if (st.draggingThumb == 0) {
      float next = std::clamp(std::min(n01, st.endValue - minGap), 0.0f, 1.0f);
      if (next != st.startValue)
        st.startValue = next;
    } else if (st.draggingThumb == 1) {
      float next = std::clamp(std::max(n01, st.startValue + minGap), 0.0f, 1.0f);
      if (next != st.endValue)
        st.endValue = next;
    }
    st.dragging = true;
  }
}

static void PressBegin(const NodePtr &target, Vector2 pt, bool isScrim) {
  Node *n = target.get();
  Ctx().activeIsScrim = isScrim;
  SetDragOrigin(pt);

  if (n->focusable || n->kind == NodeKind::TextInput)
    RequestFocus(target);

  if (n->onPressIn)
    n->onPressIn();
  // Arm long-press timing (fired per-frame in ResolveInput while held).
  n->pressStartTime = GetTime();
  n->pressLongFired = false;
  if (n->onDragStart)
    n->onDragStart(pt);

  if (n->disabled)
    return;
  if (n->kind == NodeKind::Slider) {
    n->control.dragging = true;
    DriveControlDrag(*n, pt); // jump-to-click
  } else if (n->kind == NodeKind::RangeSlider) {
    float leftX = n->layout.x + std::clamp(n->control.startValue, 0.0f, 1.0f) *
                                    n->layout.width;
    float rightX = n->layout.x + std::clamp(n->control.endValue, 0.0f, 1.0f) *
                                     n->layout.width;
    n->control.draggingThumb =
        std::fabs(pt.x - leftX) <= std::fabs(pt.x - rightX) ? 0 : 1;
    n->control.dragging = true;
    DriveControlDrag(*n, pt);
  }
}

static void ReleaseActive(Node *an, Vector2 pt, const Node *releaseTarget) {
  if (an->onPressOut)
    an->onPressOut();
  if (an->onDragEnd)
    an->onDragEnd({pt.x - GetDragOrigin().x, pt.y - GetDragOrigin().y});

  bool over = (releaseTarget == an);

  if (an->kind == NodeKind::Slider || an->kind == NodeKind::RangeSlider) {
    if (an->onValueChange) {
      if (an->kind == NodeKind::Slider)
        an->onValueChange(an->control.value);
      else if (an->control.draggingThumb == 0)
        an->onValueChange(an->control.startValue);
      else if (an->control.draggingThumb == 1)
        an->onValueChange(an->control.endValue);
    }
    an->control.dragging = false;
    an->control.draggingThumb = -1;
    return;
  }

  if (an->disabled)
    return;

  if (an->kind == NodeKind::Checkbox || an->kind == NodeKind::Switch ||
      an->kind == NodeKind::RadioButton) {
    if (over) {
      // Radio selects (never un-selects on tap); checkbox/switch toggle.
      an->control.checked =
          an->kind == NodeKind::RadioButton ? true : !an->control.checked;
      if (an->onToggle)
        an->onToggle(an->control.checked);
    }
    return;
  }

  if (Ctx().activeIsScrim) {
    if (an->onRequestClose)
      an->onRequestClose();
    else if (an->onPress)
      an->onPress();
    return;
  }

  // A long-press that already fired suppresses the trailing onPress (RN parity).
  if (over && an->onPress && !an->pressLongFired) {
    // Immediate-captured nodes get onPress on release even after a long
    // drag (the pointer is usually still over them). A node with drag
    // handlers treats real movement as a drag, not a tap — without this a
    // swipe row's release would re-trigger onPress and undo the drag.
    const bool hasDragHandlers =
        an->onDragStart || an->onDragMove || an->onDragEnd;
    if (!hasDragHandlers || PointerTravel(GetDragOrigin(), pt) <= kTouchSlop)
      an->onPress();
  }
}

void ResolveInput(const NodePtr &root) {
  const PointerInput &p = GetPointer();
  Vector2 pt = p.pos;
  Ctx().lastStats.hitTestCount++;

  const Node *rootNode = root.get();
  if (NodeId fid = GetFocusedId()) {
    auto *fn = reinterpret_cast<Node *>(fid);
    if (!NodeWithinSubtree(fn, rootNode))
      SetFocusedId(0);
  }

  if (HandleTextSelectionOverlayInput(root))
    return;

  const StackEntry *hitE = HitEntry(pt);
  NodePtr owner = hitE ? hitE->node : nullptr;
  bool ownerIsScrim = hitE ? hitE->isScrimBackdrop : false;

  NodePtr target = ownerIsScrim ? owner : InteractiveTargetFrom(owner);
  NodeId hoveredId = target ? IdOf(target) : 0;

  NodeId activeId = GetActiveId();
  if (activeId != 0) {
    Node *an = reinterpret_cast<Node *>(activeId);
    if (!NodeWithinSubtree(an, rootNode)) {
      SetActiveId(0);
      Ctx().activeIsScrim = false;
      Ctx().activeIsBottomSheetDrag = false;
    } else {
      if (Ctx().activeIsBottomSheetDrag) {
        if (p.down) {
          float dy = pt.y - GetDragOrigin().y;
          an->overlayDragOffsetY = std::clamp(
              an->bottomSheetDragStartOffsetY + dy,
              0.0f, std::max(1.0f, an->layout.height));
          ClearScrollGesture();
        } else {
          const float velocityY = VelocityTrackerEstimate(GetTime());
          if (an->overlayDragOffsetY > kBottomSheetDismissThreshold ||
              velocityY > kBottomSheetDismissVelocity) {
            an->bottomSheetDismissPending = true;
          } else {
            an->bottomSheetDismissPending = false;
          }
          an->bottomSheetAnimating = true;
          an->alwaysAnimates = true;
          SetActiveId(0);
          Ctx().activeIsBottomSheetDrag = false;
        }
        SetHoveredId(hoveredId);
        return;
      }
      if (p.down) {
        if (IsControlKind(an->kind) && an->control.dragging)
          DriveControlDrag(*an, pt);
        if (an->onDragMove)
          an->onDragMove({pt.x - GetDragOrigin().x, pt.y - GetDragOrigin().y});
        // Long-press: fire once when held past the delay without moving beyond
        // the touch slop. Movement past slop is treated as a drag/scroll, not a
        // long-press, matching react-native.
        if (an->onLongPress && !an->pressLongFired &&
            GetTime() - an->pressStartTime >= kLongPressDelay &&
            PointerTravel(GetDragOrigin(), pt) <= kTouchSlop) {
          an->pressLongFired = true;
          an->onLongPress();
        }
      } else {
        if (!Ctx().activeIsScrim)
          StartRippleFadeOut(activeId);
        const float travel = PointerTravel(GetDragOrigin(), pt);
        if (an->kind != NodeKind::TextInput)
          DismissTextInputIfNeeded(target, Ctx().scroll.engaged, travel);
        ReleaseActive(an, pt, target.get());
        SetActiveId(0);
        Ctx().activeIsScrim = false;
      }
      SetHoveredId(hoveredId);
      return;
    }
  }

  // Hover enter/leave edges.
  NodeId prevHover = GetHoveredId();
  if (hoveredId != prevHover) {
    if (prevHover) {
      Node *ph = reinterpret_cast<Node *>(prevHover);
      if (NodeWithinSubtree(ph, rootNode) && ph->onHoverOut)
        ph->onHoverOut();
    }
    if (hoveredId && target && target->onHoverIn)
      target->onHoverIn();
  }
  SetHoveredId(hoveredId);
  if (target && target->onHoverMove)
    target->onHoverMove(pt);

  if (p.pressed) {
    ClearPendingPress();
    Ctx().input.dismissTapActive = false;
    if (NodeId fid = GetFocusedId()) {
      auto *fn = reinterpret_cast<Node *>(fid);
      if (fn && fn->kind == NodeKind::TextInput) {
        const bool onTextInput =
            target && target->kind == NodeKind::TextInput;
        if (!onTextInput) {
          Ctx().input.dismissTapActive = true;
          Ctx().input.dismissTapOrigin = pt;
        }
      }
    }
    NodePtr sheet = FindBottomSheetRoot(owner);
    // Both the handle grip AND the plain surface-drag fallback are gated on
    // "no real interactive target here" — the 112dp handle band is measured
    // from the sheet's top edge for an easy-to-grab drag target, but sheet
    // CONTENT (e.g. a per-screen title bar's Back button) can legitimately
    // render inside that same band. A real onPress target under the finger
    // must win, or every screen whose header falls within the handle band
    // has its controls silently eaten by the drag gesture.
    const bool noTargetHere = !ownerIsScrim && !target;
    const bool sheetSurfaceDrag = sheet && noTargetHere;
    if (sheet && !sheet->disabled && noTargetHere &&
        (PointInBottomSheetHandle(*sheet, pt) || sheetSurfaceDrag)) {
      SetActiveId(IdOf(sheet));
      Ctx().activeIsScrim = false;
      Ctx().activeIsBottomSheetDrag = true;
      sheet->bottomSheetAnimating = false;
      sheet->alwaysAnimates = false;
      sheet->bottomSheetDismissPending = false;
      sheet->bottomSheetDragStartOffsetY = sheet->overlayDragOffsetY;
      SetDragOrigin(pt);
      return;
    }
    if (target) {
      if (NeedsImmediateCapture(*target, ownerIsScrim)) {
        SetActiveId(IdOf(target));
        PressBegin(target, pt, ownerIsScrim);
        TrySpawnRipple(target, pt);
      } else {
        Ctx().scroll.pendingPressTarget = target;
        Ctx().scroll.pendingPressOrigin = pt;
        SetPendingPressId(IdOf(target));
        TrySpawnRipple(target, pt);
      }
    }
    return;
  }

  if (p.released && Ctx().scroll.pendingPressTarget && !Ctx().scroll.engaged) {
    NodePtr pending = Ctx().scroll.pendingPressTarget;
    Vector2 pressOrigin = Ctx().scroll.pendingPressOrigin;
    FinishPendingPress();
    if (PointerTravel(pressOrigin, pt) <= kTouchSlop &&
        InteractiveTargetFrom(InputOwnerAt(pt)) == pending) {
      if (pending->kind == NodeKind::TextInput) {
        if (GetFocusedId() == IdOf(pending)) {
          if (pending->textInput.onFocus)
            pending->textInput.onFocus();
        } else {
          RequestFocus(pending);
        }
      } else {
        if (pending->onPress)
          pending->onPress();
        DismissTextInputIfNeeded(pending, false,
                                 PointerTravel(pressOrigin, pt));
      }
    }
    return;
  }

  if (p.released) {
    if (Ctx().input.dismissTapActive && !Ctx().scroll.engaged) {
      const float travel =
          PointerTravel(Ctx().input.dismissTapOrigin, pt);
      DismissTextInputIfNeeded(nullptr, false, travel);
    }
    Ctx().input.dismissTapActive = false;
    FinishPendingPress();
  }
}

RenderStats GetLastRenderStats() { return Ctx().lastStats; }

bool HasModalOverlay() {
  for (const FixedNode &fn : Ctx().fixedNodes) {
    if (fn.node && fn.node->hasScrim) {
      return true;
    }
  }
  return false;
}

NodePtr TopmostModalNode() {
  for (auto it = Ctx().fixedNodes.rbegin(); it != Ctx().fixedNodes.rend(); ++it) {
    if (it->node && it->node->hasScrim) {
      return it->node;
    }
  }
  return nullptr;
}

Rectangle Measure(const NodePtr &node) {
  return node ? node->layout : Rectangle{0, 0, 0, 0};
}

std::vector<NodePtr> GetFixedOverlayNodes() {
  std::vector<NodePtr> nodes;
  nodes.reserve(Ctx().fixedNodes.size());
  for (const FixedNode &fn : Ctx().fixedNodes) {
    if (fn.node) {
      nodes.push_back(fn.node);
    }
  }
  return nodes;
}

} // namespace raym3::v2
