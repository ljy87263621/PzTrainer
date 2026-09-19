#include "ui/brand_icon.hpp"

#include <Windows.h>
#include <GL/gl.h>
#include <wincodec.h>

#include <cstdint>
#include <vector>

#include "runtime/resource.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace pztrainer::ui {
namespace {

GLuint g_texture = 0;
bool g_load_attempted = false;

template <typename Interface>
void Release(Interface*& value) {
    if (value == nullptr) return;
    value->Release();
    value = nullptr;
}

bool DecodeBrandIcon(std::vector<std::uint8_t>& pixels,
                     UINT& width, UINT& height) {
    const HMODULE module = reinterpret_cast<HMODULE>(&__ImageBase);
    HRSRC resource = FindResourceW(
        module, MAKEINTRESOURCEW(IDR_PZSA_BRAND_ICON), MAKEINTRESOURCEW(10));
    if (resource == nullptr) return false;
    HGLOBAL loaded = LoadResource(module, resource);
    const DWORD resource_size = SizeofResource(module, resource);
    BYTE* resource_data = static_cast<BYTE*>(LockResource(loaded));
    if (loaded == nullptr || resource_data == nullptr || resource_size == 0) {
        return false;
    }

    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(com_result);
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT result = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result)) result = factory->CreateStream(&stream);
    if (SUCCEEDED(result)) {
        result = stream->InitializeFromMemory(resource_data, resource_size);
    }
    if (SUCCEEDED(result)) {
        result = factory->CreateDecoderFromStream(
            stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder);
    }
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) {
        result = converter->Initialize(
            frame, GUID_WICPixelFormat32bppRGBA,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeCustom);
    }
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    if (SUCCEEDED(result) && width > 0 && height > 0 &&
        width <= 4096 && height <= 4096) {
        pixels.resize(static_cast<std::size_t>(width) * height * 4);
        result = converter->CopyPixels(
            nullptr, width * 4, static_cast<UINT>(pixels.size()),
            pixels.data());
    } else if (SUCCEEDED(result)) {
        result = E_FAIL;
    }

    Release(converter);
    Release(frame);
    Release(decoder);
    Release(stream);
    Release(factory);
    if (uninitialize) CoUninitialize();
    return SUCCEEDED(result);
}

bool LoadBrandIcon() {
    std::vector<std::uint8_t> pixels;
    UINT width = 0;
    UINT height = 0;
    if (!DecodeBrandIcon(pixels, width, height)) return false;

    GLint previous_texture = 0;
    GLint previous_unpack_alignment = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previous_unpack_alignment);
    glGenTextures(1, &g_texture);
    if (g_texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width),
        static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE,
        pixels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, previous_unpack_alignment);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
    return true;
}

}  // namespace

ImTextureID BrandIconTexture() {
    if (!g_load_attempted) {
        g_load_attempted = true;
        LoadBrandIcon();
    }
    return static_cast<ImTextureID>(g_texture);
}

}  // namespace pztrainer::ui
