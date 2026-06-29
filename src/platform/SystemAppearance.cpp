#include "raym3/platform/SystemAppearance.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>

namespace raym3 {

static std::function<void(bool)> g_onChange;
static std::atomic<bool> g_watcherRunning{false};
static std::thread g_watcherThread;

#if defined(__APPLE__)
bool platformReadSystemDarkMode();
void platformStartWatcher(std::function<void()> signalChange);
void platformStopWatcher();
#elif defined(_WIN32)
#include <windows.h>
#else
#include <cstdio>
#include <cstdlib>
#include <string>
#endif

bool SystemAppearance::IsDarkMode() {
#if defined(__APPLE__)
    return platformReadSystemDarkMode();
#elif defined(_WIN32)
    DWORD value = 1;
    DWORD size = sizeof(value);
    if (RegGetValueA(HKEY_CURRENT_USER,
            "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            "AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return false;
    }
    return value == 0;
#else
    FILE* pipe = popen("gsettings get org.gnome.desktop.interface color-scheme 2>/dev/null", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            std::string val(buf);
            pclose(pipe);
            return val.find("dark") != std::string::npos;
        }
        pclose(pipe);
    }
    pipe = popen("gsettings get org.gnome.desktop.interface gtk-theme 2>/dev/null", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe)) {
            std::string val(buf);
            pclose(pipe);
            return val.find("dark") != std::string::npos;
        }
        pclose(pipe);
    }
    return true;
#endif
}

void SystemAppearance::StartWatching(std::function<void(bool isDark)> onChange) {
    g_onChange = std::move(onChange);
    g_watcherRunning = true;
#if defined(__EMSCRIPTEN__)
    // Web: no background watcher thread (pthreads requires cross-origin isolation).
    // Report the current value once; a future media-query listener can update it.
    if (g_onChange) g_onChange(SystemAppearance::IsDarkMode());
#elif defined(__APPLE__)
    platformStartWatcher([]() {
        if (g_onChange) g_onChange(SystemAppearance::IsDarkMode());
    });
#elif defined(_WIN32)
    g_watcherThread = std::thread([]() {
        bool lastDark = SystemAppearance::IsDarkMode();
        while (g_watcherRunning) {
            bool isDark = SystemAppearance::IsDarkMode();
            if (isDark != lastDark) {
                lastDark = isDark;
                if (g_onChange) g_onChange(isDark);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
#else
    g_watcherThread = std::thread([]() {
        bool lastDark = SystemAppearance::IsDarkMode();
        while (g_watcherRunning) {
            bool isDark = SystemAppearance::IsDarkMode();
            if (isDark != lastDark) {
                lastDark = isDark;
                if (g_onChange) g_onChange(isDark);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
#endif
}

void SystemAppearance::StopWatching() {
    g_watcherRunning = false;
#if defined(__APPLE__)
    platformStopWatcher();
#elif defined(_WIN32)
    if (g_watcherThread.joinable()) g_watcherThread.join();
#else
    if (g_watcherThread.joinable()) g_watcherThread.join();
#endif
    g_onChange = nullptr;
}

} // namespace raym3
