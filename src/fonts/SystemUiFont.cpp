#include "raym3/fonts/SystemUiFont.h"

#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <string>

#if defined(__linux__) && !defined(__ANDROID__) &&                             \
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

bool IsCssBold(FontWeight weight) {
  return weight == FontWeight::Medium || IsBold(weight);
}

bool IsItalic(FontStyle style) { return style == FontStyle::Italic; }

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return (char)std::tolower(c); });
  return value;
}

#if defined(RAYM3_HAS_FONTCONFIG)
bool ResolveViaFontconfig(FontWeight weight, FontStyle style,
                          std::string &outPath) {
  if (!FcInit())
    return false;
  FcPattern *pat = FcPatternCreate();
  if (!pat)
    return false;
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
  if (!match)
    return false;
  FcChar8 *file = nullptr;
  const bool ok =
      FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file;
  if (ok)
    outPath = reinterpret_cast<const char *>(file);
  FcPatternDestroy(match);
  return ok && FileExists(outPath);
}

bool ResolveViaFontconfigFamily(const std::string &family, FontWeight weight,
                                FontStyle style, std::string &outPath) {
  if (!FcInit())
    return false;
  FcPattern *pat = FcPatternCreate();
  if (!pat)
    return false;
  FcPatternAddString(pat, FC_FAMILY,
                     reinterpret_cast<const FcChar8 *>(family.c_str()));
  const int fcWeight = weight == FontWeight::Black ? FC_WEIGHT_BLACK
      : weight == FontWeight::Bold ? FC_WEIGHT_BOLD
      : weight == FontWeight::Medium ? FC_WEIGHT_MEDIUM
      : weight == FontWeight::Light ? FC_WEIGHT_LIGHT : FC_WEIGHT_REGULAR;
  FcPatternAddInteger(pat, FC_WEIGHT, fcWeight);
  FcPatternAddInteger(pat, FC_SLANT,
                      IsItalic(style) ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
  FcConfigSubstitute(nullptr, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);
  FcResult result = FcResultNoMatch;
  FcPattern *match = FcFontMatch(nullptr, pat, &result);
  FcPatternDestroy(pat);
  if (!match)
    return false;
  FcChar8 *file = nullptr;
  const bool ok =
      FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file;
  if (ok)
    outPath = reinterpret_cast<const char *>(file);
  FcPatternDestroy(match);
  return ok && FileExists(outPath);
}

bool ResolveFallbackViaFontconfig(const std::vector<int> &codepoints,
                                  std::string &outPath) {
  if (codepoints.empty() || !FcInit())
    return false;
  FcPattern *pat = FcPatternCreate();
  FcCharSet *charset = FcCharSetCreate();
  if (!pat || !charset) {
    if (pat)
      FcPatternDestroy(pat);
    if (charset)
      FcCharSetDestroy(charset);
    return false;
  }
  for (int codepoint : codepoints)
    if (codepoint >= 0 && codepoint <= 0x10FFFF)
      FcCharSetAddChar(charset, static_cast<FcChar32>(codepoint));
  FcPatternAddString(pat, FC_FAMILY, (const FcChar8 *)"sans-serif");
  FcPatternAddCharSet(pat, FC_CHARSET, charset);
  FcCharSetDestroy(charset);
  FcConfigSubstitute(nullptr, pat, FcMatchPattern);
  FcDefaultSubstitute(pat);
  FcResult result = FcResultNoMatch;
  FcPattern *match = FcFontMatch(nullptr, pat, &result);
  FcPatternDestroy(pat);
  if (!match)
    return false;
  FcChar8 *file = nullptr;
  const bool ok =
      FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch && file;
  if (ok)
    outPath = reinterpret_cast<const char *>(file);
  FcPatternDestroy(match);
  return ok && FileExists(outPath);
}
#endif

} // namespace

bool ResolveCssFontPath(const std::string &family, FontWeight weight,
                        FontStyle style, std::string &outPath) {
  const std::string name = Lower(family);
  outPath.clear();

#if defined(__APPLE__)
  const std::string dir = "/System/Library/Fonts/Supplemental/";
  std::string file;
  if (name == "tahoma")
    file = IsCssBold(weight) ? "Tahoma Bold.ttf" : "Tahoma.ttf";
  else if (name == "verdana")
    file = IsCssBold(weight) ? "Verdana Bold.ttf" : "Verdana.ttf";
  else if (name == "arial" || name == "sans-serif")
    file = IsCssBold(weight) ? "Arial Bold.ttf" : "Arial.ttf";
  else if (name == "geneva" && !IsCssBold(weight)) {
    outPath = "/System/Library/Fonts/Geneva.ttf";
    return FileExists(outPath);
  }
  if (!file.empty()) {
    outPath = dir + file;
    if (FileExists(outPath))
      return true;
  }
#elif defined(_WIN32)
  const char *windir = std::getenv("WINDIR");
  if (!windir || !windir[0])
    windir = "C:\\Windows";
  const std::string dir = std::string(windir) + "\\Fonts\\";
  std::string stem;
  if (name == "segoe ui") {
    const std::string file = dir + (IsCssBold(weight) ? "segoeuib.ttf" : "segoeui.ttf");
    if (FileExists(file)) { outPath = file; return true; }
  } else if (name == "tahoma") stem = "tahoma";
  else if (name == "arial" || name == "sans-serif") stem = "arial";
  if (!stem.empty()) {
    const std::string file = dir + stem + (IsCssBold(weight) ? "bd.ttf" : ".ttf");
    if (FileExists(file)) {
      outPath = file;
      return true;
    }
  }
#elif defined(__linux__) && !defined(__ANDROID__)
#if defined(RAYM3_HAS_FONTCONFIG)
  if (ResolveViaFontconfigFamily(family, weight, style, outPath))
    return true;
#endif
#elif defined(__ANDROID__)
  if (name == "roboto" || name == "sans-serif") {
    const std::string file = IsCssBold(weight)
        ? "/system/fonts/Roboto-Bold.ttf" : "/system/fonts/Roboto-Regular.ttf";
    if (FileExists(file)) {
      outPath = file;
      return true;
    }
  }
#else
  (void)style;
#endif
  return false;
}

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
  if (!windir || !windir[0])
    windir = "C:\\Windows";
  const std::string dir = std::string(windir) + "\\Fonts\\";
  const char *file = "segoeui.ttf";
  if (IsBold(weight) && IsItalic(style))
    file = "segoeuiz.ttf";
  else if (IsBold(weight))
    file = "segoeuib.ttf";
  else if (IsItalic(style))
    file = "segoeuii.ttf";
  outPath = dir + file;
  if (FileExists(outPath))
    return true;
  if (IsItalic(style)) {
    outPath = dir + (IsBold(weight) ? "segoeuib.ttf" : "segoeui.ttf");
    if (FileExists(outPath))
      return true;
  }
  outPath = dir + "segoeui.ttf";
  return FileExists(outPath);

#elif defined(__APPLE__)
  if (IsItalic(style)) {
    outPath = "/System/Library/Fonts/SFNSItalic.ttf";
    if (FileExists(outPath))
      return true;
  }
  outPath = "/System/Library/Fonts/SFNS.ttf";
  if (FileExists(outPath))
    return true;
  outPath = "/System/Library/Fonts/Geneva.ttf";
  return FileExists(outPath);

#elif defined(__ANDROID__)
  if (IsBold(weight) && IsItalic(style)) {
    outPath = "/system/fonts/Roboto-BoldItalic.ttf";
    if (FileExists(outPath))
      return true;
  }
  if (IsItalic(style)) {
    outPath = "/system/fonts/Roboto-Italic.ttf";
    if (FileExists(outPath))
      return true;
  }
  if (IsBold(weight)) {
    outPath = "/system/fonts/Roboto-Bold.ttf";
    if (FileExists(outPath))
      return true;
  }
  outPath = "/system/fonts/Roboto-Regular.ttf";
  return FileExists(outPath);

#else
#if defined(RAYM3_HAS_FONTCONFIG)
  if (ResolveViaFontconfig(weight, style, outPath))
    return true;
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
    candidates[n++] = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Oblique.ttf";
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

bool ResolveSystemFallbackFontPath(const std::vector<int> &codepoints,
                                   std::string &outPath) {
  outPath.clear();
#if defined(__EMSCRIPTEN__)
  (void)codepoints;
  return false;
#elif defined(__APPLE__)
  // Arial Unicode is a single-face TTF, so raylib's stb_truetype loader can
  // use it directly (the built-in macOS CJK fallback faces are TTC bundles).
  outPath = "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
  return FileExists(outPath);
#elif defined(_WIN32)
  const char *windir = std::getenv("WINDIR");
  if (!windir || !windir[0])
    windir = "C:\\Windows";
  const std::string dir = std::string(windir) + "\\Fonts\\";
  const char *candidates[] = {"arialuni.ttf", "meiryo.ttc", "msgothic.ttc",
                              "msyh.ttc"};
  for (const char *candidate : candidates) {
    outPath = dir + candidate;
    if (FileExists(outPath))
      return true;
  }
  return false;
#elif defined(__ANDROID__)
  const char *candidates[] = {"/system/fonts/NotoSansCJK-Regular.ttc",
                              "/system/fonts/NotoSansJP-Regular.otf",
                              "/system/fonts/DroidSansFallback.ttf"};
  for (const char *candidate : candidates) {
    outPath = candidate;
    if (FileExists(outPath))
      return true;
  }
  return false;
#else
#if defined(RAYM3_HAS_FONTCONFIG)
  if (ResolveFallbackViaFontconfig(codepoints, outPath))
    return true;
#endif
  const char *candidates[] = {
      "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
      "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"};
  for (const char *candidate : candidates) {
    outPath = candidate;
    if (FileExists(outPath))
      return true;
  }
  return false;
#endif
}

} // namespace raym3
