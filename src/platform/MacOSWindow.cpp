#include "raym3/platform/MacOSWindow.h"

#if !defined(__APPLE__)
namespace raym3::platform {
void ConfigureMacOSWindow(const MacOSWindowOptions&) {}
void SetMacOSWindowMaterialRegions(
    const std::vector<MacOSWindowMaterialRegion>&) {}
void SetMacOSWindowDragRegions(
    const std::vector<MacOSWindowDragRegion>&) {}
void SetMacOSWindowControlsPosition(const MacOSWindowControlsPosition&) {}
}  // namespace raym3::platform
#endif
