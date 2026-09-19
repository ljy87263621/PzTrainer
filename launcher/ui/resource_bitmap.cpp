#include "resource_bitmap.hpp"

#include <wincodec.h>
#include <wrl/client.h>

namespace launcher::ui {

using Microsoft::WRL::ComPtr;

HRESULT LoadPngResourceBitmap(
    HINSTANCE instance,
    UINT resource_id,
    ID2D1RenderTarget* render_target,
    ID2D1Bitmap** bitmap) {
    if (instance == nullptr || render_target == nullptr || bitmap == nullptr) {
        return E_INVALIDARG;
    }
    *bitmap = nullptr;

    const HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(resource_id), RT_RCDATA);
    if (resource == nullptr) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    const HGLOBAL loaded_resource = LoadResource(instance, resource);
    const DWORD resource_size = SizeofResource(instance, resource);
    BYTE* resource_data = static_cast<BYTE*>(LockResource(loaded_resource));
    if (loaded_resource == nullptr || resource_data == nullptr || resource_size == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    ComPtr<IWICImagingFactory> imaging_factory;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(imaging_factory.GetAddressOf()));
    if (FAILED(result)) {
        return result;
    }

    ComPtr<IWICStream> stream;
    result = imaging_factory->CreateStream(stream.GetAddressOf());
    if (FAILED(result)) {
        return result;
    }
    result = stream->InitializeFromMemory(resource_data, resource_size);
    if (FAILED(result)) {
        return result;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    result = imaging_factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnLoad,
        decoder.GetAddressOf());
    if (FAILED(result)) {
        return result;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    result = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(result)) {
        return result;
    }

    ComPtr<IWICFormatConverter> converter;
    result = imaging_factory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(result)) {
        return result;
    }
    result = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(result)) {
        return result;
    }

    return render_target->CreateBitmapFromWicBitmap(converter.Get(), nullptr, bitmap);
}

}  // namespace launcher::ui
