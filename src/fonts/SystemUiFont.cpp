#include "raym3/fonts/SystemUiFont.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#if defined(__linux__) && !defined(__ANDROID__) && \
    __has_include(<fontconfig/fontconfig.h>)
#define RAYM3_HAS_FONTCONFIG 1
#include <fontconfig/fontconfig.h>
#endif

namespace raym3 {
namespace {

bool FileExists(const std::string &path) {
  return !path.empty() && std::filesystem::exists(path);
}

bool IsBold(FontWeight weight) {
  return weight == FontWeight::Bold || weight == FontWeight::Black;
}

bool IsItalic(FontStyle style) { return style == FontStyle::Italic; }

#if defined(RAYM3_HAS_FONTCONFIG)
bool ResolveViaFontconfig(FontWeight weight, FontStyle style,
                          std::string &outPath) {
  if (!FcInit()) return false;
  FcPattern *pat = FcPatternCreate();
  if (!pat) return false;
  FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)"sans-serif");
  FcPatternAddInteger(pat, FC_WEIGHT,
                      IsBold(weight) ? FC_WEIGHT_BOLD : FC_WEIGHT_REGULAR);
  FcPatternAddInteger(pat, FC_SLANT,
                      IsItalic(style) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
  FcConfigSubstitute(nullptr, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);
  FcResult result = FcResultNoMatch;
  FcPattern *match = FcFontMatch(nullptr, pat, &result);
  FcPatternDestroy(pat);
  if (!match) return false;
  FcChar8 *file = nullptr;
  const bool ok =
      FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file;
  if (ok) outPath = reinterpret_cast<const char *>(file);
  FcPatternDestroy(match);
  return ok && FileExists(outPath);
}
#endif

} // namespace

bool ResolveSystemUiFontPath(FontWeight weight, FontStyle style,
                             std::string &outPath) {
  outPath.clear();

#if defined(__EMSCRIPTEN__)
  (void)weight;
  (void)style;
  return false;

#elif defined(_WIN32)
  // Avoid windows.h — it clashes with raylib (Rectangle / CloseWindow / …).
  const char *windir = std::getenv("WINDIR");
  if (!windir || !windir[0]) windir = "C:\\Windows";
  const std::string dir = std::string(windir) + "\\Fonts\\";
  const char *file = "segoeui.ttf";
  if (IsBold(weight) && IsItalic(style))
    file = "segoeuiz.ttf";
  else if (IsBold(weight))
    file = "segoeuib.ttf";
  else if (IsItalic(style))
    file = "segoeuii.ttf";
  outPath = dir + file;
  if (FileExists(outPath)) return true;
  if (IsItalic(style)) {
    outPath = dir + (IsBold(weight) ? "segoeuib.ttf" : "segoeui.ttf");
    if (FileExists(outPath)) return true;
  }
  outPath = dir + "segoeui.ttf";
  return FileExists(outPath);

#elif defined(__APPLE__)
  if (IsItalic(style)) {
    outPath = "/System/Library/Fonts/SFNSItalic.ttf";
    if (FileExists(outPath)) return true;
  }
  outPath = "/System/Library/Fonts/SFNS.ttf";
  if (FileExists(outPath)) return true;
  outPath = "/System/Library/Fonts/Geneva.ttf";
  return FileExists(outPath);

#elif defined(__ANDROID__)
  if (IsBold(weight) && IsItalic(style)) {
    outPath = "/system/fonts/Roboto-BoldItalic.ttf";
    if (FileExists(outPath)) return true;
  }
  if (IsItalic(style)) {
    outPath = "/system/fonts/Roboto-Italic.ttf";
    if (FileExists(outPath)) return true;
  }
  if (IsBold(weight)) {
    outPath = "/system/fonts/Roboto-Bold.ttf";
    if (FileExists(outPath)) return true;
  }
  outPath = "/system/fonts/Roboto-Regular.ttf";
  return FileExists(outPath);

#else
#if defined(RAYM3_HAS_FONTCONFIG)
  if (ResolveViaFontconfig(weight, style, outPath)) return true;
#endif
  const char *candidates[8] = {};
  int n = 0;
  if (IsBold(weight) && IsItalic(style)) {
    candidates[n++] =
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-BoldOblique.ttf";
    candidates[n++] =
        "/usr/share/fonts/truetype/liberation/LiberationSans-BoldItalic.ttf";
  } else if (IsBold(weight)) {
    candidates[n++] = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf";
    candidates[n++] =
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf";
    candidates[n++] = "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf";
  } else if (IsItalic(style)) {
    candidates[n++] =
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf";
    candidates[n++] =
        "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf";
  }
  candidates[n++] = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
  candidates[n++] =
      "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
  candidates[n++] = "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf";
  for (int i = 0; i < n; ++i) {
    if (candidates[i] && FileExists(candidates[i])) {
      outPath = candidates[i];
      return true;
    }
  }
  return false;
#endif
}

} // namespace raym3
