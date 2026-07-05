#pragma once

#include "raym3/v2/Input.h"
#include "raym3/v2/Renderer.h"
#include "raym3/v2/View.h"

#include <string>
#include <unordered_map>
#include <vector>

// raym3 v2 per-engine render/input state.
//
// All mutable state that was previously file-static in Renderer.cpp,
// Input.cpp and TextInput.cpp lives here, so one process can host multiple
// independent engines and so the render thread can own this state exclusively
// once the JS/render thread split lands. GPU-backed caches (icon atlas,
// fonts, emoji textures) intentionally stay process-wide singletons: they are
// only ever touched during paint, which is render-thread-only by construction,
// and sharing them across windows avoids duplicate atlases.
namespace raym3::v2 {

// Fixed-position node captured during the Render() tree walk. Mirrors CSS
// `position: fixed`: excluded from parent flex layout, painted above all
// normal content.
struct FixedNode {
  NodePtr node;
  int zIndex = 0;
};

struct StackEntry {
  NodePtr node;
  Rectangle bounds;
  int paintIndex;
  bool occludes;
  bool isScrimBackdrop = false;
  // Effective z-order key = max zIndex along the ancestor chain (incl. self).
  // Higher wins input regardless of tree nesting / paint order; paintIndex
  // breaks ties (later paint = on top).
  int effectiveZ = 0;
};

enum class RipplePhase { Growing, Fading };

struct RippleInstance {
  NodeId ownerId = 0;
  Vector2 origin = {0, 0};
  Vector2 boundsCenter = {0, 0};
  float targetRadius = 0.0f;
  float growT = 0.0f;
  float fadeT = 0.0f;
  float displayAlpha = 0.0f;
  RipplePhase phase = RipplePhase::Growing;
  Color inkColor = {0, 0, 0, 0};
};

struct TextInputUndoState {
  std::vector<std::string> history;
  int index = -1;
  bool isUndoRedo = false;
};

struct TextInputMirrorState {
  std::string text;
  int selectionStart = -1;
  int selectionEnd = -1;
  int composingStart = -1;
  int composingEnd = -1;
};

// Flutter VelocityTracker-style pointer history (least-squares fit over the
// last 100ms of samples; pointer considered stopped after a 40ms gap).
struct ScrollSample {
  double time = 0.0;
  float y = 0.0f;
};

struct ScrollGestureState {
  static constexpr int kVelocitySampleCapacity = 20;

  NodePtr candidate;
  bool engaged = false;
  bool hadModalOverlay = false;
  Vector2 pressOrigin = {0, 0};
  Vector2 lastPointer = {0, 0};
  float frameVelocityY = 0.0f;
  NodePtr pendingPressTarget;
  Vector2 pendingPressOrigin = {0, 0};

  ScrollSample velocitySamples[kVelocitySampleCapacity];
  int velocitySampleHead = 0;
  int velocitySampleCount = 0;
};

struct InputState {
  PointerInput pointer;
  NodeId hovered = 0;
  NodeId active = 0;
  NodeId focused = 0;
  NodeId pendingPress = 0;
  Vector2 dragOrigin = {0, 0};
  bool textSelectionOverlayConsumedPointer = false;
  // Release-time outside tap: press outside TextField, dismiss on release if
  // the finger didn't scroll and stayed within touch slop.
  bool dismissTapActive = false;
  Vector2 dismissTapOrigin = {0, 0};
};

struct RenderContext {
  // --- Render() tree-walk state ---------------------------------------------
  RenderStats lastStats;
  std::vector<FixedNode> fixedNodes;
  std::vector<StackEntry> stackOrder;
  std::unordered_map<Node *, NodePtr> parentMap;
  int paintCounter = 0;

  // Committed (double-buffered) input state. Input ownership queries read
  // these instead of the live stackOrder/parentMap, which are only partially
  // built while Render() is walking the tree. Committed at the end of
  // Render() once every node — including deferred fixed-position overlays —
  // has been pushed.
  std::vector<StackEntry> committedStackOrder;
  std::unordered_map<Node *, NodePtr> committedParentMap;

  // Active-capture qualifiers resolved during input handling.
  bool activeIsScrim = false;
  bool activeIsBottomSheetDrag = false;

  // --- Input state -----------------------------------------------------------
  InputState input;
  ScrollGestureState scroll;
  std::vector<RippleInstance> ripples;

  // --- Density (dp scaling) ----------------------------------------------------
  float platformDensity = 1.0f;
  float layoutDensity = 1.0f;

  // --- TextInput state ---------------------------------------------------------
  std::unordered_map<std::string, std::vector<char>> textInputBuffers;
  std::unordered_map<NodeId, TextInputUndoState> textInputUndo;
  std::unordered_map<NodeId, TextInputMirrorState> lastNotifiedTextInputState;
  NodeId lastFocusedTextInput = 0;
  bool ibeamCursorActive = false;

  // Idle-skip + damage tracking (Phase 2 render perf).
  bool forceRender = true;
  bool idleSkipEnabled = true;
  std::vector<Rectangle> dirtyRects;
};

// Current context. Defaults to a process-wide instance so existing
// single-engine callers keep working unchanged; an engine hosting multiple
// roots (or the threaded engine) installs its own.
RenderContext &Ctx();
// Pass nullptr to restore the default process-wide context.
void SetCurrentRenderContext(RenderContext *ctx);

} // namespace raym3::v2
