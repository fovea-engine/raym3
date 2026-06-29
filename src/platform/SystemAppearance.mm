#include "raym3/platform/SystemAppearance.h"

#import <Foundation/Foundation.h>
#include <TargetConditionals.h>

#if TARGET_OS_IOS || TARGET_OS_SIMULATOR
#import <UIKit/UIKit.h>
#endif

#include <functional>

namespace raym3 {

static id g_appearanceObserver = nil;

bool platformReadSystemDarkMode() {
#if TARGET_OS_OSX
    NSString* style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    return style != nil && [style isEqualToString:@"Dark"];
#elif TARGET_OS_IOS || TARGET_OS_SIMULATOR
    // UITraitCollection.current reflects the app's effective appearance while
    // UIKit is rendering. The engine reads this on the main thread (render +
    // initSystemAppearance), so it is valid here. Live toggles are pushed from
    // the Swift host via RayactIOSSessionRefreshAppearance (the macOS
    // distributed-notification watcher below does not fire on iOS).
    return UITraitCollection.currentTraitCollection.userInterfaceStyle ==
           UIUserInterfaceStyleDark;
#else
    return false;
#endif
}

void platformStartWatcher(std::function<void()> signalChange) {
#if TARGET_OS_OSX
    g_appearanceObserver = [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName:@"AppleInterfaceThemeChangedNotification"
                    object:nil
                     queue:nil
                usingBlock:^(__unused NSNotification* note) {
                    if (signalChange) signalChange();
                }];
#else
    (void)signalChange;
#endif
}

void platformStopWatcher() {
#if TARGET_OS_OSX
    if (g_appearanceObserver) {
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:g_appearanceObserver];
        g_appearanceObserver = nil;
    }
#endif
}

} // namespace raym3
