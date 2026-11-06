#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <raylib.h>
#include <objc/runtime.h>
#include <algorithm>
#include <cmath>

#include "raym3/platform/MacOSWindow.h"

@class Raym3BorderView;

@interface Raym3MaterialHostView : NSView {
 @public
  std::vector<raym3::platform::MacOSWindowMaterialRegion> regionValues;
  std::vector<raym3::platform::MacOSWindowDragRegion> dragValues;
}
@property(nonatomic, strong) NSView* raylibView;
@property(nonatomic, strong) NSView* borderView;
@property(nonatomic, strong) NSMutableArray<NSVisualEffectView*>* effects;
@property(nonatomic, strong) NSMutableArray<NSView*>* dragViews;
@end

@implementation Raym3MaterialHostView
- (void)layout {
  [super layout];
  self.raylibView.frame = self.bounds;
  self.borderView.frame = self.bounds;
  [(Raym3BorderView*)self.borderView updatePath];
  self.layer.cornerRadius = 12.0;
  self.layer.masksToBounds = YES;
  const NSRect bounds = self.bounds;
  for (NSUInteger i = 0; i < self.effects.count && i < regionValues.size(); ++i) {
    const Rectangle r = regionValues[i].bounds;
    self.effects[i].frame = NSMakeRect(r.x, NSHeight(bounds) - r.y - r.height,
                                      r.width, r.height);
    self.effects[i].wantsLayer = YES;
    self.effects[i].layer.cornerRadius = regionValues[i].cornerRadius;
    self.effects[i].layer.masksToBounds = regionValues[i].cornerRadius > 0;
  }
  for (NSUInteger i = 0; i < self.dragViews.count && i < dragValues.size(); ++i) {
    const Rectangle r = dragValues[i].bounds;
    self.dragViews[i].frame = NSMakeRect(r.x, NSHeight(bounds) - r.y - r.height,
                                        r.width, r.height);
  }
}
@end

static const NSWindowButton kRaym3WindowButtons[3] = {
    NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton};

static NSButton* Raym3WindowButtonAtPoint(NSWindow* window, NSPoint windowPoint) {
  if (window == nil) return nil;
  for (NSWindowButton kind : kRaym3WindowButtons) {
    NSButton* button = [window standardWindowButton:kind];
    if (button == nil || button.isHidden || button.alphaValue < 0.01) continue;
    NSRect rect = [button convertRect:button.bounds toView:nil];
    if (NSPointInRect(windowPoint, rect)) return button;
  }
  return nil;
}

using Raym3HitTestFn = NSView* (*)(id, SEL, NSPoint);
static Raym3HitTestFn Raym3OriginalThemeHitTest = nullptr;

static NSView* Raym3ThemeHitTest(id self, SEL cmd, NSPoint point) {
  NSWindow* window = [self window];
  if (window != nil && Raym3OriginalThemeHitTest != nullptr) {
    // hitTest: receives the point in the superview's coordinate system.
    NSPoint windowPoint = [self superview] == nil
                              ? point
                              : [[self superview] convertPoint:point toView:nil];
    NSButton* button = Raym3WindowButtonAtPoint(window, windowPoint);
    if (button != nil && button.superview != nil) {
      NSPoint inSuperview =
          [button.superview convertPoint:windowPoint fromView:nil];
      NSView* hit = [button hitTest:inSuperview];
      if (hit != nil) return hit;
    }
  }
  return Raym3OriginalThemeHitTest(self, cmd, point);
}

static void InstallControlHitTest(NSWindow* window) {
  static bool installed = false;
  if (!installed && window.contentView.superview != nil) {
    Class cls = window.contentView.superview.class;
    Method viewMethod = class_getInstanceMethod([NSView class], @selector(hitTest:));
    const char* encoding = method_getTypeEncoding(viewMethod);
    if (class_addMethod(cls, @selector(hitTest:), (IMP)Raym3ThemeHitTest, encoding)) {
      Raym3OriginalThemeHitTest =
          (Raym3HitTestFn)method_getImplementation(viewMethod);
    } else {
      Method method = class_getInstanceMethod(cls, @selector(hitTest:));
      Raym3OriginalThemeHitTest = (Raym3HitTestFn)method_setImplementation(
          method, (IMP)Raym3ThemeHitTest);
    }
    installed = Raym3OriginalThemeHitTest != nullptr;
  }
}

@interface Raym3WindowDragView : NSView
@end
@implementation Raym3WindowDragView
// mouseDownCanMoveWindow only applies when the whole window background is
// movable, so start the drag explicitly.
- (void)mouseDown:(NSEvent*)event {
  [self.window performWindowDragWithEvent:event];
}
- (BOOL)acceptsFirstMouse:(NSEvent*)event { return YES; }
- (BOOL)isOpaque { return NO; }
@end

// Hosts the native window buttons above the titlebar and the drag layer.
// Their original superview is only as tall as the system titlebar, so a
// moved button is drawn but never hit-tested or hovered. This overlay is
// transparent everywhere except the buttons themselves.
@interface Raym3ControlsOverlay : NSView {
 @public
  bool hasPositions;
  NSPoint topLeft[3];
  bool syncScheduled;
  bool applying;
}
- (void)applyButtonPositions;
@end

@implementation Raym3ControlsOverlay
- (BOOL)isOpaque { return NO; }
- (BOOL)acceptsFirstResponder { return NO; }
- (NSView*)hitTest:(NSPoint)point {
  // The point is in the superview's coordinate system, same as self.frame.
  if (self.hidden || !NSMouseInRect(point, self.frame, self.isFlipped))
    return nil;
  NSPoint local = [self convertPoint:point fromView:self.superview];
  for (NSView* subview in [self.subviews reverseObjectEnumerator]) {
    NSView* hit = [subview hitTest:local];
    if (hit != nil) return hit;
  }
  return nil;
}
- (void)applyButtonPositions {
  if (applying || !hasPositions || self.window == nil) return;
  NSView* parent = self.window.contentView;
  if (parent == nil) return;
  InstallControlHitTest(self.window);
  applying = true;
  // Keep the buttons inside the content view. The theme frame draws the
  // titlebar above its own subviews, so an overlay placed there never
  // receives the clicks that land on a lowered button.
  if (!NSEqualRects(self.frame, parent.bounds)) self.frame = parent.bounds;
  if (self.superview != parent) [parent addSubview:self];
  else if (parent.subviews.lastObject != self) [parent addSubview:self];

  NSView* content = self.window.contentView;
  for (int i = 0; i < 3; ++i) {
    NSButton* button = [self.window standardWindowButton:kRaym3WindowButtons[i]];
    if (button == nil || NSHeight(button.frame) < 1) continue;
    bool moved = button.superview != self;
    if (moved) {
      button.translatesAutoresizingMaskIntoConstraints = YES;
      button.autoresizingMask = NSViewNotSizable;
      [self addSubview:button];
    }
    NSPoint origin = NSMakePoint(
        topLeft[i].x, NSHeight(content.bounds) - topLeft[i].y);
    origin = [content convertPoint:origin toView:self];
    origin.y -= NSHeight(button.frame);
    if (std::fabs(button.frame.origin.x - origin.x) > 0.5 ||
        std::fabs(button.frame.origin.y - origin.y) > 0.5) {
      [button setFrameOrigin:origin];
      moved = true;
    }
    if (moved) [button updateTrackingAreas];
    button.hidden = NO;
    if (button.alphaValue != 1) button.alphaValue = 1;
  }
  applying = false;
}
- (void)layout {
  [super layout];
  if (!hasPositions || syncScheduled) {
    [self applyButtonPositions];
    return;
  }
  [self applyButtonPositions];
  syncScheduled = true;
  Raym3ControlsOverlay* overlay = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    overlay->syncScheduled = false;
    [overlay applyButtonPositions];
  });
}
- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
  [center removeObserver:self name:NSWindowDidUpdateNotification object:nil];
  if (self.window != nil) {
    [center addObserver:self
               selector:@selector(windowDidUpdate:)
                   name:NSWindowDidUpdateNotification
                 object:self.window];
  }
  [self applyButtonPositions];
}
- (void)windowDidUpdate:(NSNotification*)note {
  if (!hasPositions || self.window == nil || applying) return;
  NSButton* close = [self.window standardWindowButton:NSWindowCloseButton];
  bool stolen = close != nil && close.superview != self;
  bool buried = self.superview != nil && self.superview.subviews.lastObject != self;
  if (stolen || buried) [self applyButtonPositions];
}
- (void)dealloc {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  [super dealloc];
}
@end

static Raym3ControlsOverlay* ControlsOverlay(NSWindow* window, bool create) {
  if (window == nil || window.contentView == nil) return nil;
  NSView* parent = window.contentView;
  for (NSView* subview in parent.subviews) {
    if ([subview isKindOfClass:Raym3ControlsOverlay.class])
      return (Raym3ControlsOverlay*)subview;
  }
  if (!create) return nil;
  Raym3ControlsOverlay* overlay =
      [[Raym3ControlsOverlay alloc] initWithFrame:parent.bounds];
  overlay.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  if (@available(macOS 14.0, *)) overlay.clipsToBounds = NO;
  [parent addSubview:overlay];
  return overlay;
}

// Draws the window border above all other views and never takes input.
// The border is a filled ring: outer rounded rect minus an inner one. A larger
// inner radius than a concentric ring would use makes the corners heavier.
@interface Raym3BorderView : NSView
@property(nonatomic) CGFloat width;
@property(nonatomic) CGFloat cornerWidth;
- (void)updatePath;
@end
@implementation Raym3BorderView
- (NSView*)hitTest:(NSPoint)point { return nil; }
- (CALayer*)makeBackingLayer { return [CAShapeLayer layer]; }
- (void)updatePath {
  const CGFloat radius = 12.0;
  const CGFloat w = self.width;
  const CGFloat k = sqrt(2.0) - 1.0;
  // Diagonal thickness is w*sqrt2 + (innerRadius - radius) * k.
  CGFloat inner = radius + (std::max(self.cornerWidth, w) - w * sqrt(2.0)) / k;
  const NSRect box = NSInsetRect(self.bounds, w, w);
  inner = std::min(inner, std::min(NSWidth(box), NSHeight(box)) / 2);
  CAShapeLayer* layer = (CAShapeLayer*)self.layer;
  // No border: an empty path. The outer rect alone would fill the whole window.
  if (w <= 0) {
    layer.path = nil;
    return;
  }
  CGMutablePathRef path = CGPathCreateMutable();
  CGPathAddRoundedRect(path, NULL, self.bounds, radius, radius);
  CGPathAddRoundedRect(path, NULL, box, std::max<CGFloat>(inner, 0), std::max<CGFloat>(inner, 0));
  layer.path = path;
  layer.fillRule = kCAFillRuleEvenOdd;
  CGPathRelease(path);
}
@end

namespace raym3::platform {

static NSVisualEffectMaterial NativeMaterial(MacOSWindowMaterial material) {
  switch (material) {
    case MacOSWindowMaterial::Sidebar: return NSVisualEffectMaterialSidebar;
    case MacOSWindowMaterial::Hud: return NSVisualEffectMaterialHUDWindow;
    case MacOSWindowMaterial::Menu: return NSVisualEffectMaterialMenu;
    case MacOSWindowMaterial::Popover: return NSVisualEffectMaterialPopover;
    case MacOSWindowMaterial::UnderWindowBackground:
      return NSVisualEffectMaterialUnderWindowBackground;
    case MacOSWindowMaterial::WindowBackground:
      return NSVisualEffectMaterialWindowBackground;
  }
  return NSVisualEffectMaterialSidebar;
}

static Raym3MaterialHostView* MaterialHost(NSWindow* window, bool create) {
  if (window == nil) return nil;
  NSView* content = window.contentView;
  if ([content isKindOfClass:Raym3MaterialHostView.class])
    return (Raym3MaterialHostView*)content;
  if (!create || content == nil) return nil;

  Raym3MaterialHostView* host = [[Raym3MaterialHostView alloc] initWithFrame:content.frame];
  host.wantsLayer = YES;
  host.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  host.effects = [NSMutableArray array];
  host.dragViews = [NSMutableArray array];
  host.raylibView = content;
  content.frame = host.bounds;
  content.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [host addSubview:content];
  host.borderView = [[Raym3BorderView alloc] initWithFrame:host.bounds];
  host.borderView.wantsLayer = YES;
  [host addSubview:host.borderView];
  window.contentView = host;
  return host;
}

void ConfigureMacOSWindow(const MacOSWindowOptions& options) {
  // raylib 6's Cocoa backend exposes its NSWindow through this public API.
  NSWindow* window = (__bridge NSWindow*)GetWindowHandle();
  if (window == nil) return;

  if (options.transparentBackground) {
    [window setOpaque:NO];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setHasShadow:YES];
    Raym3MaterialHostView* host = MaterialHost(window, true);
    host.wantsLayer = YES;
    host.layer.cornerRadius = 12.0;
    host.layer.masksToBounds = YES;
    Raym3BorderView* border = (Raym3BorderView*)host.borderView;
    border.width = options.showWindowBorder ? std::max(0.0f, options.windowBorderWidth) : 0.0;
    border.cornerWidth = options.windowBorderCornerWidth;
    ((CAShapeLayer*)border.layer).fillColor =
        [NSColor colorWithCalibratedRed:options.windowBorderColor.r / 255.0
                                  green:options.windowBorderColor.g / 255.0
                                   blue:options.windowBorderColor.b / 255.0
                                  alpha:options.windowBorderColor.a / 255.0].CGColor;
    [border updatePath];
  }

  NSWindowStyleMask style = [window styleMask];
  if (options.hideTitlebar) {
    style &= ~NSWindowStyleMaskTitled;
    [window setStyleMask:style];
  } else if (options.transparentTitlebar || options.fullSizeContentView) {
    style |= NSWindowStyleMaskTitled;
    [window setStyleMask:style];
  }

  if (options.fullSizeContentView) {
    style |= NSWindowStyleMaskFullSizeContentView;
    [window setStyleMask:style];
  } else {
    style &= ~NSWindowStyleMaskFullSizeContentView;
    [window setStyleMask:style];
  }

  [window setTitlebarAppearsTransparent:options.transparentTitlebar ||
                                                options.fullSizeContentView];
  [window setTitleVisibility:(options.hideTitlebar || options.transparentTitlebar ||
                              options.fullSizeContentView)
                                 ? NSWindowTitleHidden
                                 : NSWindowTitleVisible];
  [window.contentView setNeedsLayout:YES];
  [window.contentView layoutSubtreeIfNeeded];
  [window.contentView setNeedsDisplay:YES];
  [window displayIfNeeded];
}

void SetMacOSWindowMaterialRegions(
    const std::vector<MacOSWindowMaterialRegion>& regions) {
  NSWindow* window = (__bridge NSWindow*)GetWindowHandle();
  Raym3MaterialHostView* host = MaterialHost(window, !regions.empty());
  if (host == nil) return;

  for (NSVisualEffectView* effect in host.effects) [effect removeFromSuperview];
  [host.effects removeAllObjects];
  host->regionValues = regions;
  for (const auto& region : regions) {
    NSVisualEffectView* effect = [[NSVisualEffectView alloc] initWithFrame:NSZeroRect];
    effect.material = NativeMaterial(region.material);
    effect.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    effect.state = NSVisualEffectStateActive;
    [host addSubview:effect positioned:NSWindowBelow relativeTo:host.raylibView];
    [host.effects addObject:effect];
  }
  [host setNeedsLayout:YES];
  [host layoutSubtreeIfNeeded];
}

void SetMacOSWindowDragRegions(
    const std::vector<MacOSWindowDragRegion>& regions) {
  NSWindow* window = (__bridge NSWindow*)GetWindowHandle();
  Raym3MaterialHostView* host = MaterialHost(window, !regions.empty());
  if (host == nil) return;

  for (NSView* view in host.dragViews) [view removeFromSuperview];
  [host.dragViews removeAllObjects];
  host->dragValues = regions;
  for (const auto& region : regions) {
    Raym3WindowDragView* dragView = [[Raym3WindowDragView alloc] initWithFrame:NSZeroRect];
    dragView.autoresizingMask = NSViewNotSizable;
    [host addSubview:dragView positioned:NSWindowBelow relativeTo:host.borderView];
    [host.dragViews addObject:dragView];
  }
  [host setNeedsLayout:YES];
  [host layoutSubtreeIfNeeded];
}

void SetMacOSWindowControlsPosition(
    const MacOSWindowControlsPosition& positions) {
  NSWindow* window = (__bridge NSWindow*)GetWindowHandle();
  Raym3ControlsOverlay* overlay = ControlsOverlay(window, true);
  if (overlay == nil) return;
  const Vector2 points[3] = {positions.close, positions.minimize, positions.zoom};
  overlay->hasPositions = true;
  for (int i = 0; i < 3; ++i)
    overlay->topLeft[i] = NSMakePoint(points[i].x, points[i].y);
  [overlay applyButtonPositions];
  dispatch_async(dispatch_get_main_queue(), ^{
    [overlay applyButtonPositions];
  });
}

}  // namespace raym3::platform
