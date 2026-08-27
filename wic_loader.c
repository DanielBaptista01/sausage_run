#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")
#endif

#include "jogo.h"

ALLEGRO_BITMAP* carregarBitmapWICSeguro(const char* caminho)
{
#ifdef _WIN32
    if (!caminho) return NULL;

    wchar_t wide[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, caminho, -1, wide,
                            (int)(sizeof(wide) / sizeof(wide[0]))) <= 0)
        return NULL;

    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    bool finalizarCom = SUCCEEDED(hrCom);
    IWICImagingFactory* factory = NULL;
    IWICBitmapDecoder* decoder = NULL;
    IWICBitmapFrameDecode* frame = NULL;
    IWICFormatConverter* converter = NULL;
    unsigned char* pixels = NULL;
    ALLEGRO_BITMAP* bitmap = NULL;
    ALLEGRO_LOCKED_REGION* lock = NULL;

    HRESULT hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                                  CLSCTX_INPROC_SERVER,
                                  &IID_IWICImagingFactory,
                                  (LPVOID*)&factory);
    if (FAILED(hr) || !factory) goto fim;

    hr = IWICImagingFactory_CreateDecoderFromFilename(
        factory, wide, NULL, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr) || !decoder) goto fim;

    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr) || !frame) goto fim;

    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr) || !converter) goto fim;

    hr = IWICFormatConverter_Initialize(
        converter, (IWICBitmapSource*)frame,
        &GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, NULL, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto fim;

    UINT w = 0, h = 0;
    hr = IWICFormatConverter_GetSize(converter, &w, &h);
    if (FAILED(hr) || w == 0 || h == 0 || w > 10000 || h > 10000)
        goto fim;

    UINT stride = w * 4;
    UINT bytes = stride * h;
    pixels = (unsigned char*)malloc(bytes);
    if (!pixels) goto fim;

    hr = IWICFormatConverter_CopyPixels(converter, NULL, stride, bytes, pixels);
    if (FAILED(hr)) goto fim;

    int flags = al_get_new_bitmap_flags();
    al_set_new_bitmap_flags(ALLEGRO_MEMORY_BITMAP);
    bitmap = al_create_bitmap((int)w, (int)h);
    al_set_new_bitmap_flags(flags);
    if (!bitmap) goto fim;

    lock = al_lock_bitmap(bitmap,
                          ALLEGRO_PIXEL_FORMAT_ABGR_8888_LE,
                          ALLEGRO_LOCK_WRITEONLY);
    if (!lock)
    {
        al_destroy_bitmap(bitmap);
        bitmap = NULL;
        goto fim;
    }

    for (UINT y = 0; y < h; y++)
    {
        unsigned char* src = pixels + y * stride;
        unsigned char* dst = (unsigned char*)lock->data + y * lock->pitch;
        for (UINT x = 0; x < w; x++)
        {
            unsigned int a = src[x * 4 + 3];
            dst[x * 4 + 0] = (unsigned char)((src[x * 4 + 0] * a + 127) / 255);
            dst[x * 4 + 1] = (unsigned char)((src[x * 4 + 1] * a + 127) / 255);
            dst[x * 4 + 2] = (unsigned char)((src[x * 4 + 2] * a + 127) / 255);
            dst[x * 4 + 3] = (unsigned char)a;
        }
    }

    al_unlock_bitmap(bitmap);
    lock = NULL;

fim:
    if (lock && bitmap) al_unlock_bitmap(bitmap);
    free(pixels);
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (factory) IWICImagingFactory_Release(factory);
    if (finalizarCom) CoUninitialize();
    return bitmap;
#else
    (void)caminho;
    return NULL;
#endif
}
