#pragma once

#include <functional>

namespace raym3 {

class SystemAppearance {
public:
    static bool IsDarkMode();
    static void StartWatching(std::function<void(bool isDark)> onChange);
    static void StopWatching();
};

} // namespace raym3
