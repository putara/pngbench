#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <conio.h>
#include <intrin.h>
#include <windows.h>
#include <mmsystem.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <wincodec.h>
#include "libpng/png.h"
#include "zlib/zconf.h"
#include "zlib/zlib.h"
#include "lodepng/lodepng.h"
#include "lodepng/picopng.h"

extern "C" {
#include "upng/upng.h"
}

#pragma optimize("ts", on)

#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#pragma optimize("", on)

#include "bin/testimgs.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "libpng\\libpng.lib")
#pragma comment(lib, "zlib\\zlib.lib")


class CMemStream : public IStream
{
private:
    LONG volatile   cRef;
    const BYTE*     base;
    ULONG           cb;
    ULONG           curPtr;

public:
    CMemStream(const BYTE* p, DWORD cb)
        : cRef(1)
        , base(p)
        , cb(cb)
        , curPtr()
    {
    }

    IFACEMETHOD(QueryInterface)(REFIID iid, void** ppv)
    {
        if (IsEqualIID(iid, __uuidof(IStream))) {
            *ppv = static_cast<IStream*>(this);
            return S_OK;
        } else if (IsEqualIID(iid, __uuidof(ISequentialStream))) {
            *ppv = static_cast<ISequentialStream*>(this);
            return S_OK;
        } else if (IsEqualIID(iid, __uuidof(IUnknown))) {
            *ppv = static_cast<IUnknown*>(this);
            return S_OK;
        } else {
            *ppv = NULL;
            return E_NOINTERFACE;
        }
    }

    IFACEMETHOD_(ULONG, AddRef)()
    {
        return _InterlockedIncrement(&this->cRef);
    }

    IFACEMETHOD_(ULONG, Release)() override
    {
        LONG ref = _InterlockedDecrement(&this->cRef);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    IFACEMETHOD(Read)(void* pv, ULONG cb, ULONG* pcbRead)
    {
        if (pcbRead != NULL) {
            *pcbRead = 0;
        }
        if (pv == NULL) {
            return E_INVALIDARG;
        }
        const ULONG remains = this->cb - this->curPtr;
        const DWORD copySize = __min(remains, cb);
        if (copySize > 0) {
            MoveMemory(pv, this->base + this->curPtr, copySize);
            this->curPtr += copySize;
        }
        if (pcbRead != NULL) {
            *pcbRead = copySize;
        }
        return copySize > 0 ? S_OK : S_FALSE;
    }

    IFACEMETHOD(Write)(const void* pv, ULONG cb, ULONG* pcbWritten)
    {
        if (pcbWritten != NULL) {
            *pcbWritten = 0;
        }
        UNREFERENCED_PARAMETER(pv);
        UNREFERENCED_PARAMETER(cb);
        return STG_E_ACCESSDENIED;
    }

    IFACEMETHOD(Seek)(LARGE_INTEGER offset, DWORD origin, ULARGE_INTEGER* poffNew)
    {
        if (poffNew != NULL) {
            poffNew->QuadPart = 0;
        }
        LONGLONG offNew = 0;
        switch (origin) {
        case STREAM_SEEK_SET:
            offNew = __max(0, offset.QuadPart);
            break;
        case STREAM_SEEK_CUR:
            offNew = static_cast<LONGLONG>(this->curPtr) + offset.QuadPart;
            break;
        case STREAM_SEEK_END:
            offNew = static_cast<LONGLONG>(this->cb) + offset.QuadPart;
            break;
        default:
            return STG_E_INVALIDFUNCTION;
        }
        this->curPtr = offNew < 0 ? 0 : offNew > this->cb ? this->cb : static_cast<ULONG>(offNew);
        if (poffNew != NULL) {
            poffNew->QuadPart = this->curPtr;
        }
        return S_OK;
    }

    IFACEMETHOD(SetSize)(ULARGE_INTEGER cb)
    {
        UNREFERENCED_PARAMETER(cb);
        return E_NOTIMPL;
    }

    IFACEMETHOD(CopyTo)(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
    {
        if (pcbRead != NULL) {
            pcbRead->QuadPart = 0;
        }
        if (pcbWritten != NULL) {
            pcbWritten->QuadPart = 0;
        }
        UNREFERENCED_PARAMETER(pstm);
        UNREFERENCED_PARAMETER(cb);
        return E_NOTIMPL;
    }

    IFACEMETHOD(Commit)(DWORD flags)
    {
        UNREFERENCED_PARAMETER(flags);
        return E_NOTIMPL;
    }

    IFACEMETHOD(Revert)()
    {
        return E_NOTIMPL;
    }

    IFACEMETHOD(LockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb, DWORD type)
    {
        UNREFERENCED_PARAMETER(offset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(type);
        return E_NOTIMPL;
    }

    IFACEMETHOD(UnlockRegion)(ULARGE_INTEGER offset, ULARGE_INTEGER cb, DWORD type)
    {
        UNREFERENCED_PARAMETER(offset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(type);
        return E_NOTIMPL;
    }

    IFACEMETHOD(Stat)(STATSTG* pstatstg, DWORD flags)
    {
        if (pstatstg == NULL) {
            return E_INVALIDARG;
        }
        ZeroMemory(pstatstg, sizeof(*pstatstg));
        pstatstg->type              = STGTY_STREAM;
        pstatstg->cbSize.LowPart    = this->cb;
        UNREFERENCED_PARAMETER(flags);
        return S_OK;
    }

    IFACEMETHOD(Clone)(IStream** ppstm)
    {
        if (ppstm == NULL) {
            return E_POINTER;
        }
        *ppstm = NULL;
        return E_NOTIMPL;
    }
};

extern "C" IMAGE_DOS_HEADER __ImageBase;

void lockMem(void* p, const DWORD cb)
{
    VirtualLock(p, cb);
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const DWORD page = si.dwPageSize;
    const void* pEnd = reinterpret_cast<char*>(p) + cb;
    while (p < pEnd) {
        *reinterpret_cast<volatile intptr_t*>(p);
        p = reinterpret_cast<char*>(p) + page;
    }
}

void locksection(const BYTE* data)
{
    IMAGE_NT_HEADERS* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<PBYTE>(&__ImageBase) + __ImageBase.e_lfanew);
    PIMAGE_SECTION_HEADER scn = IMAGE_FIRST_SECTION(nt);
    UINT cScns = nt->FileHeader.NumberOfSections;
    for (UINT i = 0; i < cScns; i++, scn++) {
        PBYTE p = reinterpret_cast<PBYTE>(&__ImageBase) + scn->VirtualAddress;
        ULONG cb = scn->Misc.VirtualSize;
        if (p <= data && data < p + cb) {
            lockMem(p, cb);
            break;
        }
    }
}

const int LODEPNG_ZLIB_BUILTIN = 0;
const int LODEPNG_ZLIB_OFFICIAL = 1;

struct GpCtx
{
    Gdiplus::GpImage* image;
    Gdiplus::BitmapData data;
};

IWICImagingFactory* wicFactory;

void* zalloc(void*, unsigned n, unsigned m)
{
    return calloc(n, m);
}

void zfree(void*, void* p)
{
    free(p);
}

unsigned zlib_decompress(unsigned char* out, size_t* outsize,
                                const unsigned char* in, size_t insize)
{
    z_stream zs = {};
    zs.zalloc = zalloc;
    zs.zfree = zfree;
    zs.next_in  = const_cast<unsigned char*>(in);
    zs.avail_in = insize;
    zs.next_out = out;
    zs.avail_out = *outsize;
    int err = inflateInit(&zs);
    if (err == Z_OK) {
        err = inflate(&zs, Z_NO_FLUSH);
        if (err == Z_OK || err == Z_STREAM_END) {
            *outsize = *outsize - zs.avail_out;
            err = Z_OK;
        }
        inflateEnd(&zs);
    }
    return err;
}

unsigned picopng_zlib_decompress(unsigned char* out, size_t outsize,
                                const unsigned char* in, size_t insize)
{
    return zlib_decompress(out, &outsize, in, insize);
}

unsigned custom_decompress_zlib(unsigned char** out, size_t* outsize, const unsigned char* in,
                                size_t insize, const LodePNGDecompressSettings* settings)
{
    if (*out == NULL) {
        return 2;
    }
    // HACK! lodepng preallocates the enough size of buffer, so use it.
    *outsize = _msize(*out);
    int err = zlib_decompress(*out, outsize, in, insize);
    return err == Z_OK ? 0 : 3;
}

template <typename Loader, typename Disposer>
void test(const char* name, Loader loader, Disposer disposer)
{
    locksection(images[0].data);
    LARGE_INTEGER freq, begin, end;
    LONGLONG accum = 0;
    QueryPerformanceFrequency(&freq);
    for (UINT i = 0; i < _countof(images); i++) {
        QueryPerformanceCounter(&begin);
        void* context = loader(images[i].data, images[i].size);
        QueryPerformanceCounter(&end);
        accum += end.QuadPart - begin.QuadPart;
        if (context == NULL) {
            printf("%-12s: failed at image %u\n", name, i);
            return;
        }
        disposer(context);
    }
    printf("%-12s: %5.f ms\n", name, (1000.0 / freq.QuadPart) * accum);
}

inline void* libpng_loader(const unsigned char* src, unsigned int size)
{
    png_image image = {};
    image.version = PNG_IMAGE_VERSION;
    if (png_image_begin_read_from_memory(&image, src, size)) {
#ifdef SUPPORT_COLOUR_CONVERSION
        image.format = PNG_FORMAT_ARGB;
#endif
        png_bytep buffer = static_cast<png_bytep>(malloc(PNG_IMAGE_SIZE(image)));
        if (png_image_finish_read(&image, NULL, buffer, 0, NULL)) {
            return buffer;
        }
        free(buffer);
    }
    return NULL;
}

inline void libpng_disposer(void* ptr)
{
    ::free(ptr);
}

template <int zlib>
inline void* lodepng_loader(const unsigned char* src, unsigned int size)
{
    LodePNGState state = {};
    if (zlib != LODEPNG_ZLIB_BUILTIN) {
        state.decoder.zlibsettings.custom_zlib = custom_decompress_zlib;
    }
    state.decoder.ignore_end = true;
#ifdef SUPPORT_COLOUR_CONVERSION
    state.decoder.color_convert = true;
    state.info_raw.colortype = LCT_RGBA;
    state.info_raw.bitdepth = 8;
#endif
    unsigned char* outPtr = NULL;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int ret = lodepng_decode(&outPtr, &width, &height, &state, src, size);
    return ret == 0 ? outPtr : NULL;
}

inline void lodepng_disposer(void* ptr)
{
    ::free(ptr);
}

inline void* upng_loader(const unsigned char* src, unsigned int size)
{
    upng_t* upng = upng_new_from_bytes(src, size);
    upng_error err = upng_decode(upng);
    return err == UPNG_EOK ? upng : NULL;
}

inline void upng_disposer(void* ptr)
{
    upng_free(static_cast<upng_t*>(ptr));
}

inline void* picopng_loader(const unsigned char* src, unsigned int size)
{
    int err;
#ifndef SUPPORT_COLOUR_CONVERSION
    bool conv = true;
#else
    bool conv = false;
#endif
    buffer<unsigned char> out;
    unsigned long cx, cy;
    err = pico_decodePNG(out, cx, cy, src, size, conv);
    return err == 0 ? out.detach() : NULL;
}

inline void picopng_disposer(void* ptr)
{
    ::free(ptr);
}

inline void* gdiplus_loader(const unsigned char* src, unsigned int size)
{
    IStream* stream = new CMemStream(src, size);
    // IStream* stream = ::SHCreateMemStream(src, size);
    GpCtx* ctx = new GpCtx();
    Gdiplus::Status stat = Gdiplus::DllExports::GdipLoadImageFromStream(stream, &ctx->image);
    stream->Release();
    stream = NULL;
    if (stat == Gdiplus::Ok) {
        UINT cx, cy;
        Gdiplus::DllExports::GdipGetImageWidth(ctx->image, &cx);
        Gdiplus::DllExports::GdipGetImageHeight(ctx->image, &cy);
        Gdiplus::GpRect rect;
        rect.X = 0;
        rect.Y = 0;
        rect.Width = cx;
        rect.Height = cy;
        stat = Gdiplus::DllExports::GdipBitmapLockBits(static_cast<Gdiplus::GpBitmap*>(ctx->image), &rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &ctx->data);
        if (stat == Gdiplus::Ok) {
            return ctx;
        }
        Gdiplus::DllExports::GdipDisposeImage(ctx->image);
    }
    return NULL;
}

inline void gdiplus_disposer(void* ptr)
{
    GpCtx* ctx = static_cast<GpCtx*>(ptr);
    Gdiplus::DllExports::GdipBitmapUnlockBits(static_cast<Gdiplus::GpBitmap*>(ctx->image), &ctx->data);
    Gdiplus::DllExports::GdipDisposeImage(ctx->image);
}

inline void* wic_loader(const unsigned char* src, unsigned int size)
{
    IStream* stream = new CMemStream(src, size);
    // IStream* stream = ::SHCreateMemStream(src, size);
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICBitmapSource* converted = NULL;
    HRESULT hr = wicFactory->CreateDecoder(GUID_ContainerFormatPng, NULL, &decoder);
    if (SUCCEEDED(hr)) {
        hr = decoder->Initialize(stream, WICDecodeMetadataCacheOnDemand);
    }
    stream->Release();
    stream = NULL;
    if (SUCCEEDED(hr)) {
        hr = decoder->GetFrame(0, &frame);
        decoder->Release();
        decoder = NULL;
    }
    if (SUCCEEDED(hr)) {
#ifdef SUPPORT_COLOUR_CONVERSION
        hr = ::WICConvertBitmapSource(GUID_WICPixelFormat32bppBGRA, frame, &converted);
        frame->Release();
        frame = NULL;
#else
        converted = frame;
        frame = NULL;
#endif
    }
    if (SUCCEEDED(hr)) {
        UINT cx = 0, cy = 0;
        converted->GetSize(&cx, &cy);
        UINT stride = cx * 4;
        UINT size = stride * cy;
        BYTE* bits = static_cast<BYTE*>(::malloc(size));
        hr = converted->CopyPixels(NULL, stride, size, bits);
        converted->Release();
        converted = NULL;
        if (FAILED(hr)) {
            free(bits);
            return NULL;
        }
        return bits;
    }
    return NULL;
}

inline void wic_disposer(void* ptr)
{
    ::free(ptr);
}

inline void* stb_loader(const unsigned char* src, unsigned int size)
{
    int x, y, c;
#ifdef SUPPORT_COLOUR_CONVERSION
    const int ic = 4;
#else
    const int ic = 0;
#endif
    return stbi_load_from_memory(src, size, &x, &y, &c, ic);
}

inline void stb_disposer(void* ptr)
{
    ::free(ptr);
}


int main()
{
    timeBeginPeriod(1);
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr)) {
        hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
        if (SUCCEEDED(hr)) {
            ULONG_PTR token = 0;
            Gdiplus::GdiplusStartupInput gpsi;
            Gdiplus::Status stat = Gdiplus::GdiplusStartup(&token, &gpsi, NULL);
            if (stat == Gdiplus::Ok) {
                test("WIC", wic_loader, wic_disposer);
                test("GDI+", gdiplus_loader, gdiplus_disposer);
                test("libpng+zlib", libpng_loader, libpng_disposer);
                test("stb_image", stb_loader, stb_disposer);
                puts("-- LodePNG family --");
                test("LodePNG+zlib", lodepng_loader<LODEPNG_ZLIB_OFFICIAL>, lodepng_disposer);
#ifndef SUPPORT_COLOUR_CONVERSION
                test("uPNG", upng_loader, upng_disposer);
#else
                test("(uPNG)", upng_loader, upng_disposer);
#endif
                test("LodePNG", lodepng_loader<LODEPNG_ZLIB_BUILTIN>, lodepng_disposer);
                test("picoPNG", picopng_loader, picopng_disposer);
                Gdiplus::GdiplusShutdown(token);
            } else {
                printf("GdiplusStartup failed\n");
            }
            wicFactory->Release();
        } else {
            printf("CoCreateInstance failed\n");
        }
        CoUninitialize();
    } else {
        printf("CoInitialize failed\n");
    }
    timeEndPeriod(1);
    return 0;
}
