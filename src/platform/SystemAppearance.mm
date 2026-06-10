#include "raym3/platform/SystemAppearance.h"

#import <Foundation/Foundation.h>

#include <functional>

namespace raym3 {

static id g_appearanceObserver = nil;

bool platformReadSystemDarkMode() {
    NSString* style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
    return style != nil && [style isEqualToString:@"Dark"];
}

void platformStartWatcher(std::function<void()> signalChange) {
    g_appearanceObserver = [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName:@"AppleInterfaceThemeChangedNotification"
                    object:nil
                     queue:nil
                usingBlock:^(__unused NSNotification* note) {
                    if (signalChange) signalChange();
                }];
}

void platformStopWatcher() {
    if (g_appearanceObserver) {
        [[NSDistributedNotificationCenter defaultCenter] removeObserver:g_appearanceObserver];
        g_appearanceObserver = nil;
    }
}

} // namespace raym3
