#pragma once

#include <d2d1.h>
#include <windows.h>

namespace launcher::ui {

HRESULT LoadPngResourceBitmap(
    HINSTANCE instance,
    UINT resource_id,
    ID2D1RenderTarget* render_target,
    ID2D1Bitmap** bitmap);

}  // namespace launcher::ui
