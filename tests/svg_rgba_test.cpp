#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NANOSVG_IMPLEMENTATION
#include "external/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "external/nanosvgrast.h"

namespace {

bool nearlyEqual(unsigned char actual, int expected, int tolerance = 2) {
  return std::abs((int)actual - expected) <= tolerance;
}

bool checkPixel(const char *name, const char *source, int x, int y,
                int red, int green, int blue, int alpha, int tolerance = 2) {
  NSVGimage *image = nsvgParse(strdup(source), "px", 96.0f);
  if (!image) {
    std::fprintf(stderr, "FAIL %s: parse failed\n", name);
    return false;
  }
  NSVGrasterizer *rasterizer = nsvgCreateRasterizer();
  const int width = (int)image->width;
  const int height = (int)image->height;
  unsigned char *pixels = (unsigned char *)std::calloc((size_t)width * height, 4);
  nsvgRasterize(rasterizer, image, 0, 0, 1, pixels, width, height, width * 4);
  const unsigned char *pixel = pixels + ((size_t)y * width + x) * 4;
  const bool passed = nearlyEqual(pixel[0], red, tolerance)
    && nearlyEqual(pixel[1], green, tolerance)
    && nearlyEqual(pixel[2], blue, tolerance)
    && nearlyEqual(pixel[3], alpha, tolerance);
  if (!passed)
    std::fprintf(stderr, "FAIL %s: got %u,%u,%u,%u expected %d,%d,%d,%d\n",
      name, pixel[0], pixel[1], pixel[2], pixel[3], red, green, blue, alpha);
  nsvgDeleteRasterizer(rasterizer);
  nsvgDelete(image);
  std::free(pixels);
  return passed;
}

} // namespace

int main() {
  bool passed = true;
  passed &= checkPixel("rgba fill color and alpha",
    "<svg width='4' height='4' viewBox='0 0 4 4'>"
    "<rect width='4' height='4' fill='rgba(10,20,30,0.5)'/>"
    "</svg>", 2, 2, 8, 18, 28, 127);
  passed &= checkPixel("rgba in style with fill opacity composition",
    "<svg width='4' height='4' viewBox='0 0 4 4'>"
    "<rect width='4' height='4' style='fill:rgba(100,120,140,0.5);fill-opacity:0.5'/>"
    "</svg>", 2, 2, 97, 117, 137, 63);
  return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
