// Windows: rasterize a UTF-8 emoji cluster into an RGBA8 bitmap using
// DirectWrite color glyph images (PNG/BGRA) when available, else D2D
// DrawTextLayout. Returns malloc'd w*h*4 straight-alpha bytes.
//
// Do not include raym3/v2/EmojiFont.h (pulls raylib.h): Windows SDK declares
// Rectangle / CloseWindow / ShowCursor and clashes with raylib's names.
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif

#include <windows.h>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dwrite.h>
#include <dwrite_3.h>
#include <wincodec.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace raym3::v2 {
namespace {

template <typename T>
struct ComRelease {
  void operator()(T *p) const {
    if (p) p->Release();
  }
};

template <typename T>
using ComPtr = std::unique_ptr<T, ComRelease<T>>;

struct ComScope {
  bool owned = false;
  ComScope() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    owned = (hr == S_OK);
  }
  ~ComScope() {
    if (owned) CoUninitialize();
  }
};

unsigned char *PackCenteredRgba(const std::uint8_t *bgra, int srcW, int srcH,
                                int srcStride, int px) {
  int minX = srcW, minY = srcH, maxX = -1, maxY = -1;
  for (int y = 0; y < srcH; ++y) {
    const std::uint8_t *row =
        bgra + static_cast<std::size_t>(y) * static_cast<std::size_t>(srcStride);
    for (int x = 0; x < srcW; ++x) {
      if (row[static_cast<std::size_t>(x) * 4u + 3u] == 0) continue;
      minX = std::min(minX, x);
      minY = std::min(minY, y);
      maxX = std::max(maxX, x);
      maxY = std::max(maxY, y);
    }
  }
  if (maxX < minX || maxY < minY) return nullptr;

  const int inkW = maxX - minX + 1;
  const int inkH = maxY - minY + 1;
  const int margin = std::max(2, px / 32);
  const int fit = std::max(1, px - margin * 2);
  const float scale = std::min(
      1.0f, std::min(static_cast<float>(fit) / static_cast<float>(inkW),
                     static_cast<float>(fit) / static_cast<float>(inkH)));
  const int outInkW = std::max(1, static_cast<int>(std::lround(inkW * scale)));
  const int outInkH = std::max(1, static_cast<int>(std::lround(inkH * scale)));
  const int dstX = (px - outInkW) / 2;
  const int dstY = (px - outInkH) / 2;

  const UINT outBytes = static_cast<UINT>(px) * static_cast<UINT>(px) * 4u;
  unsigned char *buf = static_cast<unsigned char *>(std::malloc(outBytes));
  if (!buf) return nullptr;
  std::memset(buf, 0, outBytes);

  for (int y = 0; y < outInkH; ++y) {
    const int dy = dstY + y;
    if (dy < 0 || dy >= px) continue;
    const int sy =
        minY + std::min(inkH - 1,
                        static_cast<int>(std::lround((y + 0.5f) / scale - 0.5f)));
    const std::uint8_t *srcRow =
        bgra + static_cast<std::size_t>(sy) * static_cast<std::size_t>(srcStride);
    unsigned char *dstRow =
        buf + static_cast<std::size_t>(dy) * static_cast<std::size_t>(px) * 4u;
    for (int x = 0; x < outInkW; ++x) {
      const int dx = dstX + x;
      if (dx < 0 || dx >= px) continue;
      const int sx =
          minX + std::min(inkW - 1, static_cast<int>(std::lround(
                                         (x + 0.5f) / scale - 0.5f)));
      const std::uint8_t *s = srcRow + static_cast<std::size_t>(sx) * 4u;
      unsigned char *d = dstRow + static_cast<std::size_t>(dx) * 4u;
      const unsigned char b = s[0], g = s[1], r = s[2], a = s[3];
      if (a > 0 && a < 255) {
        d[0] = static_cast<unsigned char>(
            std::min(255, static_cast<int>(r) * 255 / a));
        d[1] = static_cast<unsigned char>(
            std::min(255, static_cast<int>(g) * 255 / a));
        d[2] = static_cast<unsigned char>(
            std::min(255, static_cast<int>(b) * 255 / a));
      } else {
        d[0] = r;
        d[1] = g;
        d[2] = b;
      }
      d[3] = a;
    }
  }
  return buf;
}

// Decode a color-glyph PNG/BGRA blob into tightly-packed BGRA8.
bool DecodeGlyphImageToBgra(IWICImagingFactory *wic, const void *data,
                            UINT32 size, DWRITE_GLYPH_IMAGE_FORMATS format,
                            std::vector<std::uint8_t> &outBgra, int &outW,
                            int &outH, int &outStride) {
  outBgra.clear();
  outW = outH = outStride = 0;
  if (!data || size == 0) return false;

  if (format == DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8) {
    // Square pixel buffer; size must be 4*w*h with w==h.
    const UINT32 pixels = size / 4u;
    const int side = static_cast<int>(std::lround(std::sqrt(pixels)));
    if (side <= 0 || static_cast<UINT32>(side * side * 4) != size) return false;
    outW = outH = side;
    outStride = side * 4;
    outBgra.assign(static_cast<const std::uint8_t *>(data),
                   static_cast<const std::uint8_t *>(data) + size);
    return true;
  }

  if (format != DWRITE_GLYPH_IMAGE_FORMATS_PNG &&
      format != DWRITE_GLYPH_IMAGE_FORMATS_JPEG &&
      format != DWRITE_GLYPH_IMAGE_FORMATS_TIFF)
    return false;

  IWICStream *streamRaw = nullptr;
  if (FAILED(wic->CreateStream(&streamRaw)) || !streamRaw) return false;
  ComPtr<IWICStream> stream(streamRaw);
  if (FAILED(stream->InitializeFromMemory(
          reinterpret_cast<BYTE *>(const_cast<void *>(data)), size)))
    return false;

  IWICBitmapDecoder *decoderRaw = nullptr;
  const GUID *vendor = nullptr;
  if (FAILED(wic->CreateDecoderFromStream(stream.get(), vendor,
                                          WICDecodeMetadataCacheOnLoad,
                                          &decoderRaw)) ||
      !decoderRaw)
    return false;
  ComPtr<IWICBitmapDecoder> decoder(decoderRaw);

  IWICBitmapFrameDecode *frameRaw = nullptr;
  if (FAILED(decoder->GetFrame(0, &frameRaw)) || !frameRaw) return false;
  ComPtr<IWICBitmapFrameDecode> frame(frameRaw);

  IWICFormatConverter *convRaw = nullptr;
  if (FAILED(wic->CreateFormatConverter(&convRaw)) || !convRaw) return false;
  ComPtr<IWICFormatConverter> conv(convRaw);
  if (FAILED(conv->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA,
                              WICBitmapDitherTypeNone, nullptr, 0.0,
                              WICBitmapPaletteTypeCustom)))
    return false;

  UINT w = 0, h = 0;
  if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0) return false;
  outW = static_cast<int>(w);
  outH = static_cast<int>(h);
  outStride = static_cast<int>(w * 4u);
  outBgra.resize(static_cast<std::size_t>(outStride) * h);
  if (FAILED(conv->CopyPixels(nullptr, static_cast<UINT>(outStride),
                              static_cast<UINT>(outBgra.size()), outBgra.data())))
    return false;
  return true;
}

// Shape the cluster and pull the first available color bitmap for the emoji
// glyph. Avoids DrawTextLayout's glyph-box clipping of Fluent COLR/PNG ink.
unsigned char *TryRasterizeViaGlyphImage(IWICImagingFactory *wic,
                                         IDWriteFactory *dwriteBase,
                                         const std::wstring &wide, int px) {
  ComPtr<IDWriteFactory4> factory4;
  {
    IDWriteFactory4 *raw = nullptr;
    if (FAILED(dwriteBase->QueryInterface(__uuidof(IDWriteFactory4),
                                          reinterpret_cast<void **>(&raw))) ||
        !raw)
      return nullptr;
    factory4.reset(raw);
  }

  IDWriteFontCollection *fontsRaw = nullptr;
  if (FAILED(dwriteBase->GetSystemFontCollection(&fontsRaw)) || !fontsRaw)
    return nullptr;
  ComPtr<IDWriteFontCollection> fonts(fontsRaw);

  UINT32 index = 0;
  BOOL exists = FALSE;
  if (FAILED(fonts->FindFamilyName(L"Segoe UI Emoji", &index, &exists)) ||
      !exists)
    return nullptr;

  IDWriteFontFamily *familyRaw = nullptr;
  if (FAILED(fonts->GetFontFamily(index, &familyRaw)) || !familyRaw)
    return nullptr;
  ComPtr<IDWriteFontFamily> family(familyRaw);

  IDWriteFont *fontRaw = nullptr;
  if (FAILED(family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_REGULAR,
                                          DWRITE_FONT_STRETCH_NORMAL,
                                          DWRITE_FONT_STYLE_NORMAL,
                                          &fontRaw)) ||
      !fontRaw)
    return nullptr;
  ComPtr<IDWriteFont> font(fontRaw);

  IDWriteFontFace *faceRaw = nullptr;
  if (FAILED(font->CreateFontFace(&faceRaw)) || !faceRaw) return nullptr;
  ComPtr<IDWriteFontFace> face(faceRaw);

  ComPtr<IDWriteFontFace3> face3;
  {
    IDWriteFontFace3 *raw = nullptr;
    if (FAILED(face->QueryInterface(__uuidof(IDWriteFontFace3),
                                    reinterpret_cast<void **>(&raw))) ||
        !raw)
      return nullptr;
    face3.reset(raw);
  }

  // Shape via a 1-line layout so ZWJ/flag clusters resolve to the ligature
  // glyph id(s) Segoe UI Emoji expects.
  const FLOAT fontSize = static_cast<FLOAT>(px);
  IDWriteTextFormat *formatRaw = nullptr;
  if (FAILED(dwriteBase->CreateTextFormat(
          L"Segoe UI Emoji", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize,
          L"en-us", &formatRaw)) ||
      !formatRaw)
    return nullptr;
  ComPtr<IDWriteTextFormat> format(formatRaw);
  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  IDWriteTextLayout *layoutRaw = nullptr;
  if (FAILED(dwriteBase->CreateTextLayout(
          wide.c_str(), static_cast<UINT32>(wide.size()), format.get(),
          fontSize * 4.0f, fontSize * 4.0f, &layoutRaw)) ||
      !layoutRaw)
    return nullptr;
  ComPtr<IDWriteTextLayout> layout(layoutRaw);

  // Enumerate shaped glyphs through a tiny IDWriteTextRenderer.
  struct GlyphCatcher : public IDWriteTextRenderer {
    ULONG refs = 1;
    std::vector<UINT16> glyphs;
    IDWriteFontFace *face = nullptr;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
      if (riid == __uuidof(IUnknown) || riid == __uuidof(IDWritePixelSnapping) ||
          riid == __uuidof(IDWriteTextRenderer)) {
        *ppv = this;
        AddRef();
        return S_OK;
      }
      *ppv = nullptr;
      return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
      const ULONG n = --refs;
      if (n == 0) delete this;
      return n;
    }
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) override {
      *disabled = TRUE;
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *m) override {
      *m = {1, 0, 0, 1, 0, 0};
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *ppd) override {
      *ppd = 1.0f;
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(
        void *, FLOAT, FLOAT, DWRITE_MEASURING_MODE,
        const DWRITE_GLYPH_RUN *run, const DWRITE_GLYPH_RUN_DESCRIPTION *,
        IUnknown *) override {
      if (!run || !run->glyphIndices || run->glyphCount == 0) return S_OK;
      if (!face) face = run->fontFace;
      for (UINT32 i = 0; i < run->glyphCount; ++i)
        glyphs.push_back(run->glyphIndices[i]);
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT,
                                            const DWRITE_UNDERLINE *,
                                            IUnknown *) override {
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT,
                                                const DWRITE_STRIKETHROUGH *,
                                                IUnknown *) override {
      return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT,
                                               IDWriteInlineObject *, BOOL,
                                               BOOL, IUnknown *) override {
      return S_OK;
    }
  };

  GlyphCatcher *catcher = new GlyphCatcher();
  layout->Draw(nullptr, catcher, 0.0f, 0.0f);
  if (catcher->glyphs.empty()) {
    catcher->Release();
    return nullptr;
  }

  ComPtr<IDWriteFontFace4> face4;
  {
    IDWriteFontFace4 *raw = nullptr;
    IDWriteFontFace *useFace =
        catcher->face ? catcher->face : static_cast<IDWriteFontFace *>(face.get());
    if (FAILED(useFace->QueryInterface(__uuidof(IDWriteFontFace4),
                                       reinterpret_cast<void **>(&raw))) ||
        !raw) {
      catcher->Release();
      return nullptr;
    }
    face4.reset(raw);
  }

  const DWRITE_GLYPH_IMAGE_FORMATS formatsToTry[] = {
      DWRITE_GLYPH_IMAGE_FORMATS_PNG,
      DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8,
      DWRITE_GLYPH_IMAGE_FORMATS_JPEG,
      DWRITE_GLYPH_IMAGE_FORMATS_TIFF,
  };

  // Prefer the last glyph in the run — for ZWJ ligatures the replacement glyph
  // is typically last after mark/component glyphs.
  for (int gi = static_cast<int>(catcher->glyphs.size()) - 1; gi >= 0; --gi) {
    const UINT16 glyphId = catcher->glyphs[static_cast<std::size_t>(gi)];
    for (DWRITE_GLYPH_IMAGE_FORMATS fmt : formatsToTry) {
      DWRITE_GLYPH_IMAGE_DATA image{};
      void *context = nullptr;
      const HRESULT hr = face4->GetGlyphImageData(
          glyphId, static_cast<UINT32>(std::max(px, 128)), fmt, &image, &context);
      if (FAILED(hr) || !context || !image.imageData || image.imageDataSize == 0)
        continue;

      std::vector<std::uint8_t> bgra;
      int w = 0, h = 0, stride = 0;
      const bool ok = DecodeGlyphImageToBgra(wic, image.imageData,
                                             image.imageDataSize, fmt, bgra, w,
                                             h, stride);
      face4->ReleaseGlyphImageData(context);
      if (!ok) continue;

      catcher->Release();
      return PackCenteredRgba(bgra.data(), w, h, stride, px);
    }
  }

  catcher->Release();
  return nullptr;
}

unsigned char *RasterizeViaWicDrawTextLayout(IWICImagingFactory *wic,
                                             IDWriteFactory *dwrite,
                                             const std::wstring &wide, int px) {
  const int big = std::max(px * 3, px + 64);
  const FLOAT layoutSize = static_cast<FLOAT>(big);

  IWICBitmap *bitmapRaw = nullptr;
  if (FAILED(wic->CreateBitmap(big, big, GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapCacheOnLoad, &bitmapRaw)) ||
      !bitmapRaw)
    return nullptr;
  ComPtr<IWICBitmap> bitmap(bitmapRaw);

  ID2D1Factory *d2dRaw = nullptr;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dRaw)) ||
      !d2dRaw)
    return nullptr;
  ComPtr<ID2D1Factory> d2d(d2dRaw);

  const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
      D2D1_RENDER_TARGET_TYPE_DEFAULT,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f, 96.0f, D2D1_RENDER_TARGET_USAGE_NONE,
      D2D1_FEATURE_LEVEL_DEFAULT);

  ID2D1RenderTarget *rtRaw = nullptr;
  if (FAILED(d2d->CreateWicBitmapRenderTarget(bitmap.get(), rtProps, &rtRaw)) ||
      !rtRaw)
    return nullptr;
  ComPtr<ID2D1RenderTarget> rt(rtRaw);

  const FLOAT fontSize = static_cast<FLOAT>(px) * 0.88f;
  IDWriteTextFormat *formatRaw = nullptr;
  if (FAILED(dwrite->CreateTextFormat(
          L"Segoe UI Emoji", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize,
          L"en-us", &formatRaw)) ||
      !formatRaw)
    return nullptr;
  ComPtr<IDWriteTextFormat> format(formatRaw);
  format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  IDWriteTextLayout *layoutRaw = nullptr;
  if (FAILED(dwrite->CreateTextLayout(
          wide.c_str(), static_cast<UINT32>(wide.size()), format.get(),
          layoutSize, layoutSize, &layoutRaw)) ||
      !layoutRaw)
    return nullptr;
  ComPtr<IDWriteTextLayout> layout(layoutRaw);

  ID2D1SolidColorBrush *brushRaw = nullptr;
  if (FAILED(rt->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                       &brushRaw)) ||
      !brushRaw)
    return nullptr;
  ComPtr<ID2D1SolidColorBrush> brush(brushRaw);

  rt->BeginDraw();
  rt->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
  rt->DrawTextLayout(D2D1::Point2F(0.0f, 0.0f), layout.get(), brush.get(),
                     D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT |
                         D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
  if (FAILED(rt->EndDraw())) return nullptr;

  const UINT bigStride = static_cast<UINT>(big) * 4u;
  std::vector<std::uint8_t> bgra(static_cast<std::size_t>(bigStride) * big);
  WICRect rect{0, 0, big, big};
  IWICBitmapLock *lockRaw = nullptr;
  if (FAILED(bitmap->Lock(&rect, WICBitmapLockRead, &lockRaw)) || !lockRaw)
    return nullptr;
  ComPtr<IWICBitmapLock> lock(lockRaw);
  UINT lockSize = 0, lockStride = 0;
  BYTE *lockData = nullptr;
  if (FAILED(lock->GetStride(&lockStride)) || lockStride < bigStride)
    return nullptr;
  if (FAILED(lock->GetDataPointer(&lockSize, &lockData)) || !lockData)
    return nullptr;
  for (int y = 0; y < big; ++y) {
    std::memcpy(bgra.data() + static_cast<std::size_t>(y) * bigStride,
                lockData + static_cast<std::size_t>(y) * lockStride, bigStride);
  }
  lock.reset();
  return PackCenteredRgba(bgra.data(), big, big, static_cast<int>(bigStride), px);
}

unsigned char *RasterizeViaDrawTextLayout(IWICImagingFactory * /*wic*/,
                                          IDWriteFactory *dwrite,
                                          const std::wstring &wide, int px) {
  // Use a D3D11 WARP + ID2D1DeviceContext target. The older WIC render-target
  // path clips Fluent color-glyph ink to the glyph box (flat left edge); the
  // device-context DrawTextLayout path keeps the full COLR/bitmap artwork.
  const int big = std::max(px * 3, px + 64);
  const FLOAT layoutSize = static_cast<FLOAT>(big);
  const FLOAT fontSize = static_cast<FLOAT>(px) * 0.88f;

  ID3D11Device *d3dRaw = nullptr;
  ID3D11DeviceContext *d3dCtxRaw = nullptr;
  D3D_FEATURE_LEVEL featureLevel{};
  const D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
  HRESULT hr = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, &d3dRaw, &featureLevel,
      &d3dCtxRaw);
  if (FAILED(hr) || !d3dRaw) {
    // Retry with hardware if WARP is unavailable.
    hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, ARRAYSIZE(levels),
        D3D11_SDK_VERSION, &d3dRaw, &featureLevel, &d3dCtxRaw);
  }
  if (FAILED(hr) || !d3dRaw) return nullptr;
  ComPtr<ID3D11Device> d3d(d3dRaw);
  ComPtr<ID3D11DeviceContext> d3dCtx(d3dCtxRaw);

  IDXGIDevice *dxgiRaw = nullptr;
  if (FAILED(d3d->QueryInterface(__uuidof(IDXGIDevice),
                                 reinterpret_cast<void **>(&dxgiRaw))) ||
      !dxgiRaw)
    return nullptr;
  ComPtr<IDXGIDevice> dxgi(dxgiRaw);

  ID2D1Factory1 *d2dFactoryRaw = nullptr;
  if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                               __uuidof(ID2D1Factory1), nullptr,
                               reinterpret_cast<void **>(&d2dFactoryRaw))) ||
      !d2dFactoryRaw)
    return nullptr;
  ComPtr<ID2D1Factory1> d2dFactory(d2dFactoryRaw);

  ID2D1Device *d2dDeviceRaw = nullptr;
  if (FAILED(d2dFactory->CreateDevice(dxgi.get(), &d2dDeviceRaw)) ||
      !d2dDeviceRaw)
    return nullptr;
  ComPtr<ID2D1Device> d2dDevice(d2dDeviceRaw);

  ID2D1DeviceContext *dcRaw = nullptr;
  if (FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                            &dcRaw)) ||
      !dcRaw)
    return nullptr;
  ComPtr<ID2D1DeviceContext> dc(dcRaw);

  const D2D1_BITMAP_PROPERTIES1 cpuProps = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_CPU_READ,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f, 96.0f);

  // GPU target, then copy into a CPU-readable bitmap (TARGET+CPU_READ is
  // illegal as a single options combo).
  const D2D1_BITMAP_PROPERTIES1 targetProps = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      96.0f, 96.0f);
  ID2D1Bitmap1 *targetRaw = nullptr;
  if (FAILED(dc->CreateBitmap(D2D1::SizeU(big, big), nullptr, 0, &targetProps,
                              &targetRaw)) ||
      !targetRaw)
    return nullptr;
  ComPtr<ID2D1Bitmap1> target(targetRaw);

  ID2D1Bitmap1 *readRaw = nullptr;
  if (FAILED(dc->CreateBitmap(D2D1::SizeU(big, big), nullptr, 0, &cpuProps,
                              &readRaw)) ||
      !readRaw)
    return nullptr;
  ComPtr<ID2D1Bitmap1> readable(readRaw);

  IDWriteTextFormat *formatRaw = nullptr;
  if (FAILED(dwrite->CreateTextFormat(
          L"Segoe UI Emoji", nullptr, DWRITE_FONT_WEIGHT_REGULAR,
          DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, fontSize,
          L"en-us", &formatRaw)) ||
      !formatRaw)
    return nullptr;
  ComPtr<IDWriteTextFormat> format(formatRaw);
  format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
  format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
  format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  IDWriteTextLayout *layoutRaw = nullptr;
  if (FAILED(dwrite->CreateTextLayout(
          wide.c_str(), static_cast<UINT32>(wide.size()), format.get(),
          layoutSize, layoutSize, &layoutRaw)) ||
      !layoutRaw)
    return nullptr;
  ComPtr<IDWriteTextLayout> layout(layoutRaw);

  ID2D1SolidColorBrush *brushRaw = nullptr;
  if (FAILED(dc->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                       &brushRaw)) ||
      !brushRaw)
    return nullptr;
  ComPtr<ID2D1SolidColorBrush> brush(brushRaw);

  dc->SetTarget(target.get());
  dc->BeginDraw();
  dc->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
  dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
  dc->DrawTextLayout(D2D1::Point2F(0.0f, 0.0f), layout.get(), brush.get(),
                     D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT |
                         D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
  if (FAILED(dc->EndDraw())) return nullptr;

  if (FAILED(readable->CopyFromBitmap(nullptr, target.get(), nullptr)))
    return nullptr;

  D2D1_MAPPED_RECT mapped{};
  if (FAILED(readable->Map(D2D1_MAP_OPTIONS_READ, &mapped)) || !mapped.bits)
    return nullptr;

  unsigned char *packed = PackCenteredRgba(
      mapped.bits, big, big, static_cast<int>(mapped.pitch), px);
  readable->Unmap();
  return packed;
}

} // namespace

unsigned char *PlatformRasterizeEmoji(const char *utf8, int px, int *outW,
                                      int *outH) {
  if (!utf8 || px <= 0 || !outW || !outH) return nullptr;

  int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
  if (wideLen <= 1) return nullptr;
  std::wstring wide(static_cast<std::size_t>(wideLen - 1), L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), wideLen) <= 0)
    return nullptr;

  ComScope com;

  IWICImagingFactory *wicRaw = nullptr;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                              CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicRaw))) ||
      !wicRaw)
    return nullptr;
  ComPtr<IWICImagingFactory> wic(wicRaw);

  IDWriteFactory *dwRaw = nullptr;
  if (FAILED(DWriteCreateFactory(
          DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown **>(&dwRaw))) ||
      !dwRaw)
    return nullptr;
  ComPtr<IDWriteFactory> dwrite(dwRaw);

  unsigned char *buf = TryRasterizeViaGlyphImage(wic.get(), dwrite.get(), wide, px);
  if (!buf)
    buf = RasterizeViaDrawTextLayout(wic.get(), dwrite.get(), wide, px);
  if (!buf)
    // Last resort: older WIC render-target path (may clip Fluent overhangs).
    buf = RasterizeViaWicDrawTextLayout(wic.get(), dwrite.get(), wide, px);
  if (!buf) return nullptr;

  *outW = px;
  *outH = px;
  return buf;
}

} // namespace raym3::v2

#endif // _WIN32
