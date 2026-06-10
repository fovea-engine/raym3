// macOS / iOS: rasterize a UTF-8 emoji cluster into an RGBA8 bitmap using
// CoreText. Returns a malloc'd buffer of w*h*4 bytes (straight alpha).
// On iOS this would work identically — left as a TODO stub comment for now.
#include "raym3/v2/EmojiFont.h"

#import <CoreGraphics/CoreGraphics.h>
#import <CoreText/CoreText.h>
#import <Foundation/Foundation.h>

namespace raym3::v2 {

unsigned char *PlatformRasterizeEmoji(const char *utf8, int px,
                                      int *outW, int *outH) {
  if (!utf8 || px <= 0) return nullptr;

  NSString *str = [NSString stringWithUTF8String:utf8];
  if (!str || str.length == 0) return nullptr;

  int size = px;
  *outW = size;
  *outH = size;

  std::size_t bytes = (std::size_t)(size * size * 4);
  unsigned char *buf = (unsigned char *)std::malloc(bytes);
  if (!buf) return nullptr;
  std::memset(buf, 0, bytes);

  CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
  CGContextRef ctx = CGBitmapContextCreate(
      buf, size, size, 8, (std::size_t)(size * 4), cs,
      (CGBitmapInfo)((uint32_t)kCGImageAlphaPremultipliedLast | (uint32_t)kCGBitmapByteOrder32Big));
  CGColorSpaceRelease(cs);
  if (!ctx) { std::free(buf); return nullptr; }

  // CoreGraphics uses bottom-left origin, which matches OpenGL/raylib's texture
  // convention — no coordinate flip needed. DrawTexturePro will display correctly.

  // Use CTLine for correct emoji rendering (incl. ZWJ sequences & flags).
  CFStringRef cfStr = CFStringCreateWithCString(kCFAllocatorDefault, utf8,
                                                kCFStringEncodingUTF8);
  if (!cfStr) { CGContextRelease(ctx); std::free(buf); return nullptr; }

  CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CGFloat fontSize = (CGFloat)size * 0.88;  // slight inset so glyphs don't clip
  CTFontRef font = CTFontCreateWithName(CFSTR("Apple Color Emoji"), fontSize, nullptr);
  CFDictionarySetValue(attrs, kCTFontAttributeName, font);

  CFAttributedStringRef attrStr =
      CFAttributedStringCreate(kCFAllocatorDefault, cfStr, attrs);
  CFRelease(cfStr); CFRelease(attrs); CFRelease(font);
  if (!attrStr) { CGContextRelease(ctx); std::free(buf); return nullptr; }

  CTLineRef line = CTLineCreateWithAttributedString(attrStr);
  CFRelease(attrStr);
  if (!line) { CGContextRelease(ctx); std::free(buf); return nullptr; }

  // Center the glyph in the square (CG bottom-left coords).
  CGRect bounds = CTLineGetImageBounds(line, ctx);
  CGFloat dx = ((CGFloat)size - bounds.size.width) * 0.5f - bounds.origin.x;
  CGFloat dy = ((CGFloat)size - bounds.size.height) * 0.5f - bounds.origin.y;
  CGContextSetTextPosition(ctx, dx, dy);
  CTLineDraw(line, ctx);
  CFRelease(line);
  CGContextRelease(ctx);

  // CoreGraphics produces premultiplied alpha; un-premultiply for raylib.
  for (std::size_t i = 0; i < bytes; i += 4) {
    unsigned char a = buf[i + 3];
    if (a > 0 && a < 255) {
      buf[i + 0] = (unsigned char)std::min(255, (int)buf[i + 0] * 255 / a);
      buf[i + 1] = (unsigned char)std::min(255, (int)buf[i + 1] * 255 / a);
      buf[i + 2] = (unsigned char)std::min(255, (int)buf[i + 2] * 255 / a);
    }
  }

  return buf;
}

} // namespace raym3::v2
