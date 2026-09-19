#include "launcher_window.hpp"

#include "animation.hpp"
#include "localization.hpp"
#include "resource_bitmap.hpp"
#include "theme.hpp"
#include "../injector.hpp"
#include "../resource.h"
#include "vmprotect.hpp"

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <wrl/client.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace launcher::ui {
namespace {

using Microsoft::WRL::ComPtr;

std::wstring_view ProtectedGroupNumber() {
    static const wchar_t* value = PZ_VMP_DECRYPT_STRING_W(L"1074183906");
    return value;
}

constexpr wchar_t kWindowClass[] = L"PzTrainerAnimatedLauncher";
constexpr UINT_PTR kAnimationTimer = 1;
constexpr float kDesignWidth = 625.0f;
constexpr float kDesignHeight = 500.0f;
constexpr float kWindowScale = 0.70f;
constexpr float kSplashWidth = 260.0f;
constexpr float kSplashHeight = 260.0f;
constexpr float kStartupSpinnerFadeBegin = 1.80f;
constexpr float kStartupExpandBegin = 2.05f;
constexpr float kStartupMainBegin = 2.50f;
constexpr float kStartupEnd = 2.82f;

enum class HitTarget {
    None,
    CloseDetails,
    About,
    AboutClose,
    Group,
    GroupClose,
    GroupCopy,
    LanguageEnglish,
    LanguageChinese,
    LanguageRussian,
    LanguageGerman,
    Load,
    LoadingAction,
};

enum class WorkerState {
    Idle,
    Running,
    Success,
    Error,
};

D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha) {
    color.a *= Clamp01(alpha);
    return color;
}

D2D1_COLOR_F Mix(D2D1_COLOR_F from, D2D1_COLOR_F to, float amount) {
    amount = Clamp01(amount);
    return D2D1::ColorF(
        from.r + (to.r - from.r) * amount,
        from.g + (to.g - from.g) * amount,
        from.b + (to.b - from.b) * amount,
        from.a + (to.a - from.a) * amount);
}

bool Contains(const D2D1_RECT_F& rectangle, float x, float y) {
    return x >= rectangle.left && x <= rectangle.right &&
           y >= rectangle.top && y <= rectangle.bottom;
}

class LauncherWindow {
public:
    LauncherWindow(
        HINSTANCE instance,
        std::filesystem::path game_directory,
        std::filesystem::path dll_path)
        : instance_(instance),
          game_directory_(std::move(game_directory)),
          dll_path_(std::move(dll_path)),
          language_(DetectSystemLanguage()) {}

    ~LauncherWindow() {
        if (worker_.joinable()) {
            worker_.join();
        }
    }

    int Run(int show_command) {
        if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
            return 1;
        }
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = WindowProc;
        window_class.hInstance = instance_;
        window_class.hIcon = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_PZTRAINER_APP), IMAGE_ICON,
            0, 0, LR_DEFAULTSIZE | LR_SHARED));
        window_class.hIconSm = static_cast<HICON>(LoadImageW(
            instance_, MAKEINTRESOURCEW(IDI_PZTRAINER_APP), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            CoUninitialize();
            return 1;
        }

        const UINT dpi = GetDpiForSystem();
        const int width = MulDiv(static_cast<int>(std::round(kSplashWidth * kWindowScale)), dpi, 96);
        const int height = MulDiv(static_cast<int>(std::round(kSplashHeight * kWindowScale)), dpi, 96);
        const int screen_width = GetSystemMetrics(SM_CXSCREEN);
        const int screen_height = GetSystemMetrics(SM_CYSCREEN);
        window_center_x_ = screen_width / 2;
        window_center_y_ = screen_height / 2;

        hwnd_ = CreateWindowExW(
            WS_EX_APPWINDOW,
            kWindowClass,
            L"PZ Launcher",
            WS_POPUP | WS_MINIMIZEBOX,
            window_center_x_ - width / 2,
            window_center_y_ - height / 2,
            width,
            height,
            nullptr,
            nullptr,
            instance_,
            this);
        if (hwnd_ == nullptr) {
            CoUninitialize();
            return 1;
        }

        const BOOL dark_mode = TRUE;
        DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark_mode, sizeof(dark_mode));
        const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
        const COLORREF border_color = static_cast<COLORREF>(0xFFFFFFFE);
        DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &border_color, sizeof(border_color));

        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        start_counter_ = counter.QuadPart;
        last_counter_ = counter.QuadPart;
        QueryPerformanceFrequency(&counter);
        counter_frequency_ = counter.QuadPart;

        ShowWindow(hwnd_, show_command == SW_HIDE ? SW_SHOWNORMAL : show_command);
        UpdateWindow(hwnd_);
        SetTimer(hwnd_, kAnimationTimer, 16, nullptr);

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        KillTimer(hwnd_, kAnimationTimer);
        DiscardGraphicsResources();
        CoUninitialize();
        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        LauncherWindow* window = nullptr;
        if (message == WM_NCCREATE) {
            const auto create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            window = static_cast<LauncherWindow*>(create->lpCreateParams);
            window->hwnd_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        } else {
            window = reinterpret_cast<LauncherWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        return window != nullptr
            ? window->HandleMessage(message, wparam, lparam)
            : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_PAINT:
            Paint();
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (render_target_ != nullptr) {
                render_target_->Resize(D2D1::SizeU(LOWORD(lparam), HIWORD(lparam)));
            }
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(lparam);
            window_center_x_ = (suggested->left + suggested->right) / 2;
            window_center_y_ = (suggested->top + suggested->bottom) / 2;
            SetWindowPos(
                hwnd_, nullptr,
                suggested->left, suggested->top,
                suggested->right - suggested->left,
                suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            if (render_target_ != nullptr) {
                const float dpi = static_cast<float>(HIWORD(wparam));
                render_target_->SetDpi(dpi, dpi);
            }
            return 0;
        }
        case WM_NCHITTEST: {
            const LRESULT hit = DefWindowProcW(hwnd_, message, wparam, lparam);
            if (hit != HTCLIENT) {
                return hit;
            }
            POINT cursor{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd_, &cursor);
            const D2D1_POINT_2F point = ToDesignPoint(cursor.x, cursor.y);
            if (HitTest(point.x, point.y) != HitTarget::None) {
                return HTCLIENT;
            }
            if (point.y < 92.0f) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }
        case WM_MOUSEMOVE:
            OnMouseMove(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_MOUSELEAVE:
            tracking_mouse_ = false;
            mouse_x_ = -1000.0f;
            mouse_y_ = -1000.0f;
            UpdateCursor();
            return 0;
        case WM_LBUTTONDOWN:
            OnMouseDown(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_LBUTTONUP:
            OnMouseUp(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_KEYDOWN:
            if (ElapsedSeconds() < kStartupEnd && wparam != VK_ESCAPE) {
                return 0;
            }
            if (wparam == VK_ESCAPE) {
                if (loading_visible_) {
                    return 0;
                }
                if (about_visible_) {
                    about_visible_ = false;
                } else if (group_visible_) {
                    group_visible_ = false;
                } else {
                    DestroyWindow(hwnd_);
                }
            } else if (wparam == VK_RETURN && !loading_visible_ && !about_visible_ && !group_visible_) {
                StartLoading();
            }
            return 0;
        case WM_TIMER:
            Tick();
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, message, wparam, lparam);
        }
    }

    void OnMouseMove(int x, int y) {
        if (!tracking_mouse_) {
            TRACKMOUSEEVENT tracking{};
            tracking.cbSize = sizeof(tracking);
            tracking.dwFlags = TME_LEAVE;
            tracking.hwndTrack = hwnd_;
            TrackMouseEvent(&tracking);
            tracking_mouse_ = true;
        }
        const D2D1_POINT_2F point = ToDesignPoint(x, y);
        mouse_x_ = point.x;
        mouse_y_ = point.y;
        UpdateCursor();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void OnMouseDown(int x, int y) {
        const D2D1_POINT_2F point = ToDesignPoint(x, y);
        pressed_ = HitTest(point.x, point.y);
        if (pressed_ != HitTarget::None) {
            SetCapture(hwnd_);
            click_bounce_ = 1.0f;
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }

    void OnMouseUp(int x, int y) {
        const D2D1_POINT_2F point = ToDesignPoint(x, y);
        const HitTarget released = HitTest(point.x, point.y);
        const HitTarget action = released == pressed_ ? pressed_ : HitTarget::None;
        pressed_ = HitTarget::None;
        ReleaseCapture();

        switch (action) {
        case HitTarget::CloseDetails:
            DestroyWindow(hwnd_);
            break;
        case HitTarget::About:
            about_visible_ = true;
            break;
        case HitTarget::AboutClose:
            about_visible_ = false;
            break;
        case HitTarget::Group:
            group_visible_ = true;
            break;
        case HitTarget::GroupClose:
            group_visible_ = false;
            break;
        case HitTarget::GroupCopy:
            if (CopyTextToClipboard(ProtectedGroupNumber())) {
                group_copy_notice_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            }
            break;
        case HitTarget::LanguageEnglish:
            language_ = Language::English;
            break;
        case HitTarget::LanguageChinese:
            language_ = Language::Chinese;
            break;
        case HitTarget::LanguageRussian:
            language_ = Language::Russian;
            break;
        case HitTarget::LanguageGerman:
            language_ = Language::German;
            break;
        case HitTarget::Load:
            StartLoading();
            break;
        case HitTarget::LoadingAction:
            if (worker_state_.load() == WorkerState::Error) {
                loading_visible_ = false;
                worker_state_.store(WorkerState::Idle);
            }
            break;
        default:
            break;
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    HitTarget HitTest(float x, float y) const {
        if (ElapsedSeconds() < kStartupEnd) {
            return HitTarget::None;
        }
        if (loading_visible_) {
            const WorkerState state = worker_state_.load();
            if (state == WorkerState::Error && Contains(LoadingActionRect(), x, y)) {
                return HitTarget::LoadingAction;
            }
            return HitTarget::None;
        }
        if (about_visible_) {
            if (Contains(AboutCloseRect(), x, y)) {
                return HitTarget::AboutClose;
            }
            if (Contains(LanguageEnglishRect(), x, y)) {
                return HitTarget::LanguageEnglish;
            }
            if (Contains(LanguageChineseRect(), x, y)) {
                return HitTarget::LanguageChinese;
            }
            if (Contains(LanguageRussianRect(), x, y)) {
                return HitTarget::LanguageRussian;
            }
            if (Contains(LanguageGermanRect(), x, y)) {
                return HitTarget::LanguageGerman;
            }
            return HitTarget::None;
        }
        if (group_visible_) {
            if (Contains(GroupCloseRect(), x, y)) {
                return HitTarget::GroupClose;
            }
            if (Contains(GroupCopyRect(), x, y)) {
                return HitTarget::GroupCopy;
            }
            return HitTarget::None;
        }
        if (details_visible_) {
            if (Contains(AboutRect(), x, y)) {
                return HitTarget::About;
            }
            if (Contains(CloseDetailsRect(), x, y)) {
                return HitTarget::CloseDetails;
            }
            if (Contains(LoadRect(), x, y)) {
                return HitTarget::Load;
            }
            if (Contains(GroupRect(), x, y)) {
                return HitTarget::Group;
            }
            return HitTarget::None;
        }
        return HitTarget::None;
    }

    void UpdateCursor() {
        const HitTarget target = HitTest(mouse_x_, mouse_y_);
        SetCursor(LoadCursorW(
            nullptr,
            target == HitTarget::None ? IDC_ARROW : IDC_HAND));
    }

    bool CopyTextToClipboard(std::wstring_view text) const {
        if (!OpenClipboard(hwnd_)) {
            return false;
        }
        EmptyClipboard();

        const SIZE_T bytes = (text.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory == nullptr) {
            CloseClipboard();
            return false;
        }
        void* destination = GlobalLock(memory);
        if (destination == nullptr) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        CopyMemory(destination, text.data(), text.size() * sizeof(wchar_t));
        static_cast<wchar_t*>(destination)[text.size()] = L'\0';
        GlobalUnlock(memory);

        if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    D2D1_POINT_2F ToDesignPoint(int x, int y) const {
        RECT client{};
        GetClientRect(hwnd_, &client);
        const float scale_x = static_cast<float>(client.right) / kDesignWidth;
        const float scale_y = static_cast<float>(client.bottom) / kDesignHeight;
        return D2D1::Point2F(x / std::max(scale_x, 0.001f), y / std::max(scale_y, 0.001f));
    }

    void Tick() {
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        const float delta = std::clamp(
            static_cast<float>(counter.QuadPart - last_counter_) / static_cast<float>(counter_frequency_),
            0.0f,
            0.05f);
        last_counter_ = counter.QuadPart;
        UpdateStartupWindow(ElapsedSeconds());

        details_progress_ = Approach(details_progress_, details_visible_ ? 1.0f : 0.0f, 14.0f, delta);
        about_progress_ = Approach(about_progress_, about_visible_ ? 1.0f : 0.0f, 16.0f, delta);
        group_progress_ = Approach(group_progress_, group_visible_ ? 1.0f : 0.0f, 16.0f, delta);
        loading_progress_ = Approach(loading_progress_, loading_visible_ ? 1.0f : 0.0f, 13.0f, delta);
        click_bounce_ = Approach(click_bounce_, 0.0f, 16.0f, delta);

        if (loading_visible_) {
            const WorkerState state = worker_state_.load();
            if (state == WorkerState::Running) {
                displayed_load_progress_ = std::min(
                    0.82f,
                    displayed_load_progress_ + delta * (0.16f + displayed_load_progress_ * 0.12f));
            } else if (state == WorkerState::Success) {
                displayed_load_progress_ = Approach(displayed_load_progress_, 1.0f, 7.0f, delta);
                if (!success_display_started_) {
                    success_display_started_ = true;
                    success_display_started_at_ = std::chrono::steady_clock::now();
                    success_fade_opacity_ = 1.0f;
                } else {
                    const float elapsed = std::chrono::duration<float>(
                        std::chrono::steady_clock::now() - success_display_started_at_).count();
                    success_fade_opacity_ = Clamp01(1.0f - elapsed);
                    if (elapsed >= 1.0f) {
                        DestroyWindow(hwnd_);
                        return;
                    }
                }
            } else {
                success_display_started_ = false;
                success_fade_opacity_ = 1.0f;
            }
        }

        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void UpdateStartupWindow(float elapsed) {
        if (startup_full_size_ || elapsed < kStartupExpandBegin) {
            return;
        }

        const float progress = EaseOutCubic(
            (elapsed - kStartupExpandBegin) / (kStartupMainBegin - kStartupExpandBegin));
        const float logical_width = (kSplashWidth + (kDesignWidth - kSplashWidth) * progress) * kWindowScale;
        const float logical_height = (kSplashHeight + (kDesignHeight - kSplashHeight) * progress) * kWindowScale;
        const UINT dpi = GetDpiForWindow(hwnd_);
        int width = MulDiv(static_cast<int>(std::round(logical_width)), dpi, 96);
        int height = MulDiv(static_cast<int>(std::round(logical_height)), dpi, 96);
        width += width & 1;
        height += height & 1;
        SetWindowPos(
            hwnd_, nullptr,
            window_center_x_ - width / 2,
            window_center_y_ - height / 2,
            width,
            height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
        startup_full_size_ = progress >= 0.999f;
    }

    float ElapsedSeconds() const {
        LARGE_INTEGER counter{};
        QueryPerformanceCounter(&counter);
        return static_cast<float>(counter.QuadPart - start_counter_) /
               static_cast<float>(counter_frequency_);
    }

    void StartLoading() {
        if (worker_state_.load() == WorkerState::Running) {
            return;
        }
        if (worker_.joinable()) {
            worker_.join();
        }
        loading_visible_ = true;
        success_display_started_ = false;
        success_fade_opacity_ = 1.0f;
        displayed_load_progress_ = 0.03f;
        load_started_ = std::chrono::steady_clock::now();
        worker_state_.store(WorkerState::Running);
        worker_ = std::thread([this] {
            LaunchResult result = LaunchAndInject(game_directory_, dll_path_);
            const auto elapsed = std::chrono::steady_clock::now() - load_started_;
            const auto minimum = std::chrono::milliseconds(1350);
            if (elapsed < minimum) {
                std::this_thread::sleep_for(minimum - elapsed);
            }
            {
                std::lock_guard<std::mutex> lock(result_mutex_);
                result_message_ = result.message;
            }
            worker_state_.store(result.success ? WorkerState::Success : WorkerState::Error);
        });
    }

    HRESULT CreateGraphicsResources() {
        if (render_target_ != nullptr) {
            return S_OK;
        }

        HRESULT result = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf());
        if (FAILED(result)) {
            return result;
        }
        result = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(write_factory_.GetAddressOf()));
        if (FAILED(result)) {
            return result;
        }

        RECT client{};
        GetClientRect(hwnd_, &client);
        const UINT dpi = GetDpiForWindow(hwnd_);
        result = factory_->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_UNKNOWN),
                static_cast<float>(dpi),
                static_cast<float>(dpi)),
            D2D1::HwndRenderTargetProperties(
                hwnd_,
                D2D1::SizeU(client.right, client.bottom),
                D2D1_PRESENT_OPTIONS_NONE),
            render_target_.GetAddressOf());
        if (FAILED(result)) {
            return result;
        }
        result = render_target_->CreateSolidColorBrush(theme::Text, brush_.GetAddressOf());
        if (FAILED(result)) {
            return result;
        }
        LoadPngResourceBitmap(
            instance_, IDR_PZTRAINER_LOGO_PNG, render_target_.Get(), game_logo_bitmap_.GetAddressOf());

        return CreateTextFormats();
    }

    HRESULT CreateTextFormats() {
        struct FormatSpec {
            ComPtr<IDWriteTextFormat>* destination;
            float size;
            DWRITE_FONT_WEIGHT weight;
        };
        FormatSpec formats[] = {
            {&text_small_, 13.0f, DWRITE_FONT_WEIGHT_NORMAL},
            {&text_body_, 16.0f, DWRITE_FONT_WEIGHT_NORMAL},
            {&text_body_bold_, 16.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
            {&text_title_, 22.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD},
            {&text_brand_, 40.0f, DWRITE_FONT_WEIGHT_BOLD},
            {&text_logo_, 15.0f, DWRITE_FONT_WEIGHT_BOLD},
        };

        for (FormatSpec& format : formats) {
            const HRESULT result = write_factory_->CreateTextFormat(
                L"Microsoft YaHei UI",
                nullptr,
                format.weight,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                format.size,
                L"zh-CN",
                format.destination->GetAddressOf());
            if (FAILED(result)) {
                return result;
            }
            (*format.destination)->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            (*format.destination)->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }
        return S_OK;
    }

    void DiscardGraphicsResources() {
        text_small_.Reset();
        text_body_.Reset();
        text_body_bold_.Reset();
        text_title_.Reset();
        text_brand_.Reset();
        text_logo_.Reset();
        game_logo_bitmap_.Reset();
        brush_.Reset();
        render_target_.Reset();
        write_factory_.Reset();
        factory_.Reset();
    }

    void Paint() {
        PAINTSTRUCT paint{};
        BeginPaint(hwnd_, &paint);
        if (FAILED(CreateGraphicsResources())) {
            EndPaint(hwnd_, &paint);
            return;
        }

        const D2D1_SIZE_F actual_size = render_target_->GetSize();
        const float scale_x = actual_size.width / kDesignWidth;
        const float scale_y = actual_size.height / kDesignHeight;
        render_scale_x_ = scale_x;
        render_scale_y_ = scale_y;
        render_target_->BeginDraw();
        render_target_->SetTransform(D2D1::Matrix3x2F::Scale(scale_x, scale_y));
        render_target_->Clear(theme::Background);

        const float elapsed = ElapsedSeconds();
        if (elapsed >= kStartupMainBegin) {
            DrawDetails();
        }
        if (loading_progress_ > 0.01f) {
            DrawLoading();
        }
        if (about_progress_ > 0.01f) {
            DrawAboutOverlay();
        }
        if (group_progress_ > 0.01f) {
            DrawGroupOverlay();
        }
        if (elapsed < kStartupEnd) {
            DrawStartup(elapsed, actual_size);
        }

        const HRESULT result = render_target_->EndDraw();
        if (result == D2DERR_RECREATE_TARGET) {
            DiscardGraphicsResources();
        }
        EndPaint(hwnd_, &paint);
    }

#if 0
    float MeasureTextWidth(std::wstring_view text, IDWriteTextFormat* format) const {
        ComPtr<IDWriteTextLayout> layout;
        if (FAILED(write_factory_->CreateTextLayout(
                text.data(),
                static_cast<UINT32>(text.size()),
                format,
                1000.0f,
                100.0f,
                layout.GetAddressOf()))) {
            return 0.0f;
        }
        DWRITE_TEXT_METRICS metrics{};
        return SUCCEEDED(layout->GetMetrics(&metrics)) ? metrics.widthIncludingTrailingWhitespace : 0.0f;
    }

    void DrawInputBox(
        const D2D1_RECT_F& rectangle,
        std::wstring_view label,
        std::wstring_view value,
        InputField field,
        bool password,
        float opacity) {
        const bool focused = active_input_ == field;
        const bool hovered = Contains(rectangle, mouse_x_, mouse_y_);
        DrawText(
            label,
            D2D1::RectF(rectangle.left, rectangle.top - 25.0f, rectangle.right, rectangle.top - 2.0f),
            text_small_.Get(), theme::Muted, opacity, DWRITE_TEXT_ALIGNMENT_LEADING);
        FillRoundedRect(rectangle, 7.0f, WithAlpha(theme::Background, opacity * 0.92f));
        StrokeRoundedRect(
            rectangle,
            7.0f,
            WithAlpha(focused ? theme::Accent : hovered ? theme::BorderBright : theme::Border, opacity));
        std::wstring displayed = password ? std::wstring(value.size(), L'•') : std::wstring(value);
        DrawText(
            displayed.empty() ? (password ? L"请输入密码" : L"请输入内容") : displayed,
            D2D1::RectF(rectangle.left + 12.0f, rectangle.top, rectangle.right - 12.0f, rectangle.bottom),
            text_body_.Get(), displayed.empty() ? theme::Dimmed : theme::Text,
            opacity, DWRITE_TEXT_ALIGNMENT_LEADING);
        if (focused && request_state_.load() != RequestState::Running &&
            std::fmod(ElapsedSeconds(), 1.0f) < 0.55f) {
            const float width = displayed.empty() ? 0.0f : MeasureTextWidth(displayed, text_body_.Get());
            const float caret_x = std::min(rectangle.right - 10.0f, rectangle.left + 12.0f + width + 1.0f);
            SetBrush(WithAlpha(theme::Accent, opacity));
            render_target_->DrawLine(
                D2D1::Point2F(caret_x, rectangle.top + 10.0f),
                D2D1::Point2F(caret_x, rectangle.bottom - 10.0f),
                brush_.Get(), 1.4f);
        }
    }

    void DrawAuthPage(float elapsed) {
        DrawAmbientGlow(D2D1::Point2F(313.0f, 182.0f), 210.0f, 0.06f);
        const float intro = Stagger(elapsed, 0.02f, 0.55f);
        DrawText(
            L"PZ",
            D2D1::RectF(24.0f, 24.0f, 95.0f, 76.0f),
            text_brand_.Get(), theme::Text, intro, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawWindowControls(intro);

        const D2D1_RECT_F panel = D2D1::RectF(145.0f, 78.0f, 510.0f, 458.0f);
        const float eased = EaseOutBack(intro);
        const float scale = 0.95f + eased * 0.05f;
        const D2D1_POINT_2F center = D2D1::Point2F(327.5f, 268.0f);
        render_target_->SetTransform(
            D2D1::Matrix3x2F::Scale(scale, scale, center) *
            D2D1::Matrix3x2F::Translation(0.0f, (1.0f - eased) * 16.0f));
        DrawShadow(panel, 12.0f, intro * 0.65f);
        FillRoundedRect(panel, 12.0f, WithAlpha(theme::Panel, intro));
        StrokeRoundedRect(panel, 12.0f, WithAlpha(theme::BorderBright, intro * 0.82f));

        DrawText(
            auth_register_mode_ ? L"创建账号" : L"账号登录",
            D2D1::RectF(184.0f, 98.0f, 470.0f, 132.0f),
            text_title_.Get(), theme::Text, intro, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawText(
            auth_register_mode_ ? L"使用服务器生成的邀请码注册" : L"登录后同步订阅、头像与在线授权",
            D2D1::RectF(184.0f, 129.0f, 480.0f, 154.0f),
            text_small_.Get(), theme::Muted, intro, DWRITE_TEXT_ALIGNMENT_LEADING);

        DrawInputBox(AuthUsernameRect(), L"用户名", username_input_, InputField::Username, false, intro);
        DrawInputBox(AuthPasswordRect(), L"密码", password_input_, InputField::Password, true, intro);
        if (auth_register_mode_) {
            DrawInputBox(AuthInvitationRect(), L"邀请码", invitation_input_, InputField::Invitation, false, intro);
        }

        const D2D1_RECT_F submit = ScaleRect(
            AuthSubmitRect(), pressed_ == HitTarget::AuthSubmit ? 0.965f : 1.0f);
        const bool submit_hover = Contains(AuthSubmitRect(), mouse_x_, mouse_y_);
        const bool enabled = authorization_ready_ && request_state_.load() != RequestState::Running;
        FillRoundedRect(
            submit,
            7.0f,
            WithAlpha(
                enabled ? Mix(theme::AccentPressed, theme::Accent, submit_hover ? 1.0f : 0.74f) : theme::Dimmed,
                intro));
        if (request_state_.load() == RequestState::Running) {
            DrawSpinner(D2D1::Point2F((submit.left + submit.right) * 0.5f, (submit.top + submit.bottom) * 0.5f), ElapsedSeconds(), intro);
        } else {
            DrawText(
                auth_register_mode_ ? L"注册" : L"登录",
                submit, text_body_bold_.Get(), theme::Text, intro, DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        std::wstring message = authorization_ready_ ? RequestMessage() : authorization_error_;
        if (!message.empty()) {
            const RequestState state = request_state_.load();
            DrawText(
                message,
                AuthMessageRect(),
                text_small_.Get(),
                state == RequestState::Success ? theme::Success : theme::Danger,
                intro,
                DWRITE_TEXT_ALIGNMENT_CENTER,
                DWRITE_WORD_WRAPPING_WRAP);
        }
        const bool toggle_hover = Contains(AuthToggleRect(), mouse_x_, mouse_y_);
        DrawText(
            auth_register_mode_ ? L"已有账号？返回登录" : L"没有账号？使用邀请码注册",
            AuthToggleRect(),
            text_small_.Get(), toggle_hover ? theme::Accent : theme::Muted,
            intro, DWRITE_TEXT_ALIGNMENT_CENTER);
        render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
    }

    void DrawRedeemOverlay() {
        const float opacity = 1.0f;
        FillRect(
            D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight),
            D2D1::ColorF(0.0f, 0.004f, 0.008f, 0.88f));
        const D2D1_RECT_F panel = D2D1::RectF(118.0f, 112.0f, 507.0f, 402.0f);
        DrawShadow(panel, 12.0f, 0.8f);
        FillRoundedRect(panel, 11.0f, theme::Panel);
        StrokeRoundedRect(panel, 11.0f, theme::BorderBright);
        DrawText(
            L"激活订阅",
            D2D1::RectF(154.0f, 137.0f, 430.0f, 172.0f),
            text_title_.Get(), theme::Text, opacity, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawText(
            L"输入管理员生成的长激活码；激活后会立即刷新在线授权。",
            D2D1::RectF(154.0f, 174.0f, 472.0f, 214.0f),
            text_small_.Get(), theme::Muted, opacity, DWRITE_TEXT_ALIGNMENT_LEADING,
            DWRITE_WORD_WRAPPING_WRAP);

        const bool close_hover = Contains(RedeemCloseRect(), mouse_x_, mouse_y_);
        SetBrush(close_hover ? theme::Text : theme::Muted);
        render_target_->DrawLine(D2D1::Point2F(462.0f, 137.0f), D2D1::Point2F(474.0f, 149.0f), brush_.Get(), 1.7f);
        render_target_->DrawLine(D2D1::Point2F(474.0f, 137.0f), D2D1::Point2F(462.0f, 149.0f), brush_.Get(), 1.7f);

        DrawInputBox(RedeemFieldRect(), L"激活码", redeem_input_, InputField::RedeemCode, false, opacity);
        const D2D1_RECT_F button = ScaleRect(
            RedeemSubmitRect(), pressed_ == HitTarget::RedeemSubmit ? 0.965f : 1.0f);
        const bool hover = Contains(RedeemSubmitRect(), mouse_x_, mouse_y_);
        FillRoundedRect(
            button,
            7.0f,
            request_state_.load() == RequestState::Running
                ? theme::Dimmed
                : Mix(theme::AccentPressed, theme::Accent, hover ? 1.0f : 0.72f));
        if (request_state_.load() == RequestState::Running) {
            DrawSpinner(D2D1::Point2F(312.5f, 312.0f), ElapsedSeconds(), 1.0f);
        } else {
            DrawText(L"兑换并激活", button, text_body_bold_.Get(), theme::Text, 1.0f, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        const std::wstring message = RequestMessage();
        if (!message.empty()) {
            DrawText(
                message,
                D2D1::RectF(145.0f, 341.0f, 480.0f, 380.0f),
                text_small_.Get(),
                request_state_.load() == RequestState::Success ? theme::Success : theme::Danger,
                1.0f,
                DWRITE_TEXT_ALIGNMENT_CENTER,
                DWRITE_WORD_WRAPPING_WRAP);
        }
    }

    void DrawProfileAvatar(float opacity) {
        const D2D1_RECT_F rectangle = AvatarRect();
        if (avatar_bitmap_ != nullptr) {
            ComPtr<ID2D1EllipseGeometry> clip;
            ComPtr<ID2D1Layer> layer;
            if (SUCCEEDED(factory_->CreateEllipseGeometry(
                    D2D1::Ellipse(
                        D2D1::Point2F((rectangle.left + rectangle.right) * 0.5f, (rectangle.top + rectangle.bottom) * 0.5f),
                        (rectangle.right - rectangle.left) * 0.5f,
                        (rectangle.bottom - rectangle.top) * 0.5f),
                    clip.GetAddressOf())) &&
                SUCCEEDED(render_target_->CreateLayer(nullptr, layer.GetAddressOf()))) {
                render_target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clip.Get()), layer.Get());
                render_target_->DrawBitmap(
                    avatar_bitmap_.Get(), rectangle, opacity,
                    D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
                render_target_->PopLayer();
            }
        } else {
            FillCircle(D2D1::Point2F(49.0f, 450.0f), 25.0f, WithAlpha(theme::PanelRaised, opacity));
            DrawText(L"PZ", rectangle, text_logo_.Get(), theme::Accent, opacity, DWRITE_TEXT_ALIGNMENT_CENTER);
        }
        const bool hover = Contains(rectangle, mouse_x_, mouse_y_);
        StrokeCircle(
            D2D1::Point2F(49.0f, 450.0f),
            24.5f,
            WithAlpha(hover ? theme::Accent : theme::BorderBright, opacity));
        if (request_operation_.load() == RequestOperation::Avatar &&
            request_state_.load() == RequestState::Running) {
            FillCircle(
                D2D1::Point2F(49.0f, 450.0f),
                24.0f,
                D2D1::ColorF(0.0f, 0.01f, 0.02f, opacity * 0.72f));
            DrawSpinner(D2D1::Point2F(49.0f, 450.0f), ElapsedSeconds(), opacity);
        }
    }

#endif

    void DrawStartup(float elapsed, D2D1_SIZE_F size) {
        float opacity = 1.0f;
        if (elapsed > kStartupMainBegin) {
            opacity = 1.0f - EaseOutCubic(
                (elapsed - kStartupMainBegin) / (kStartupEnd - kStartupMainBegin));
        }

        render_target_->SetTransform(D2D1::Matrix3x2F::Identity());
        FillRect(D2D1::RectF(0.0f, 0.0f, size.width, size.height), WithAlpha(theme::Panel, opacity));
        POINT client_center{window_center_x_, window_center_y_};
        ScreenToClient(hwnd_, &client_center);
        float dpi_x = 96.0f;
        float dpi_y = 96.0f;
        render_target_->GetDpi(&dpi_x, &dpi_y);
        const D2D1_POINT_2F center = D2D1::Point2F(
            static_cast<float>(client_center.x) * 96.0f / dpi_x,
            static_cast<float>(client_center.y) * 96.0f / dpi_y);
        const float spinner_exit = EaseOutCubic(
            (elapsed - kStartupSpinnerFadeBegin) /
            (kStartupExpandBegin - kStartupSpinnerFadeBegin));
        const float spinner_opacity = opacity * (1.0f - spinner_exit);
        if (spinner_opacity > 0.01f) {
            DrawSpinner(center, elapsed, spinner_opacity);
        }
    }

    void DrawDetails() {
        const float progress = details_progress_;
        const LocalizedText& text = TextFor(language_);
        FillRect(D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight), D2D1::ColorF(0.0f, 0.008f, 0.014f, progress * 0.93f));

        const float eased = EaseOutBack(progress);
        const float scale = 0.94f + eased * 0.06f;
        const D2D1_POINT_2F center = D2D1::Point2F(312.0f, 250.0f);
        render_target_->SetTransform(
            D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_) *
            D2D1::Matrix3x2F::Scale(scale, scale, center) *
            D2D1::Matrix3x2F::Translation(0.0f, (1.0f - eased) * 18.0f));

        const D2D1_RECT_F panel = D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight);
        FillRect(panel, WithAlpha(theme::Panel, progress));
        DrawGameLogo(D2D1::RectF(39.0f, 31.0f, 103.0f, 95.0f), progress);
        DrawText(
            L"Project Zomboid",
            D2D1::RectF(117.0f, 46.0f, 420.0f, 94.0f),
            text_title_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_LEADING);

        const D2D1_RECT_F about = AboutRect();
        const bool about_hover = Contains(about, mouse_x_, mouse_y_);
        if (about_hover) {
            FillCircle(D2D1::Point2F(about.left + 16.0f, about.top + 16.0f), 13.0f, WithAlpha(theme::PanelRaised, progress));
        }
        StrokeCircle(
            D2D1::Point2F(about.left + 16.0f, about.top + 16.0f),
            9.0f,
            WithAlpha(about_hover ? theme::Accent : theme::Muted, progress));
        DrawText(L"!", about, text_body_bold_.Get(), about_hover ? theme::Accent : theme::Muted, progress, DWRITE_TEXT_ALIGNMENT_CENTER);

        const bool close_hover = Contains(CloseDetailsRect(), mouse_x_, mouse_y_);
        if (close_hover) {
            FillRoundedRect(CloseDetailsRect(), 7.0f, WithAlpha(theme::PanelRaised, progress));
        }
        SetBrush(WithAlpha(close_hover ? theme::Text : theme::Muted, progress));
        render_target_->DrawLine(D2D1::Point2F(564.0f, 55.5f), D2D1::Point2F(575.0f, 66.5f), brush_.Get(), 1.8f);
        render_target_->DrawLine(D2D1::Point2F(575.0f, 55.5f), D2D1::Point2F(564.0f, 66.5f), brush_.Get(), 1.8f);
        FillRect(D2D1::RectF(0.0f, 114.0f, kDesignWidth, 115.0f), WithAlpha(theme::Border, progress));

        DrawLabelValue(text.branch, text.branch_value, 50.0f, 142.0f, progress);
        DrawLabelValue(text.target, L"Build 42.20", 50.0f, 174.0f, progress);
        DrawLabelValue(text.menu, L"Insert", 50.0f, 206.0f, progress);

        DrawText(
            text.current_version,
            D2D1::RectF(317.0f, 138.0f, 545.0f, 168.0f),
            text_body_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_LEADING);
        FillRect(D2D1::RectF(317.0f, 171.0f, 568.0f, 172.0f), WithAlpha(theme::Border, progress));
        const std::wstring_view notes[] = {
            text.note_launcher,
        };
        for (int index = 0; index < 1; ++index) {
            DrawText(
                notes[index],
                D2D1::RectF(317.0f, 181.0f + index * 27.0f, 572.0f, 208.0f + index * 27.0f),
                text_body_.Get(), theme::Muted, progress, DWRITE_TEXT_ALIGNMENT_LEADING);
        }

        FillRect(D2D1::RectF(0.0f, 388.0f, kDesignWidth, 389.0f), WithAlpha(theme::Border, progress));
        const bool load_hover = Contains(LoadRect(), mouse_x_, mouse_y_);
        const bool load_pressed = pressed_ == HitTarget::Load;
        const float press_scale = load_pressed ? 0.965f : 1.0f - click_bounce_ * 0.025f;
        const D2D1_RECT_F load = ScaleRect(LoadRect(), press_scale);
        FillRoundedRect(
            load,
            7.0f,
            WithAlpha(
                load_pressed ? theme::AccentPressed : Mix(theme::AccentPressed, theme::Accent, load_hover ? 1.0f : 0.72f),
                progress));
        DrawPlayIcon(D2D1::Point2F(load.left + 34.0f, 431.5f), progress);
        DrawText(
            text.load,
            D2D1::RectF(load.left + 56.0f, load.top, load.right - 9.0f, load.bottom),
            text_body_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);

        const bool group_hover = Contains(GroupRect(), mouse_x_, mouse_y_);
        const bool group_pressed = pressed_ == HitTarget::Group;
        const D2D1_RECT_F group = ScaleRect(GroupRect(), group_pressed ? 0.94f : 1.0f);
        const D2D1_POINT_2F group_center = D2D1::Point2F(
            (group.left + group.right) * 0.5f,
            (group.top + group.bottom) * 0.5f);
        FillCircle(group_center, 17.0f, WithAlpha(D2D1::ColorF(0.01f, 0.014f, 0.020f), progress));
        StrokeCircle(
            group_center,
            16.5f,
            WithAlpha(group_hover ? theme::Text : theme::Muted, progress));
        DrawText(
            L"Q",
            D2D1::RectF(group.left, group.top - 1.0f, group.right, group.bottom + 1.0f),
            text_body_bold_.Get(),
            group_hover ? theme::Text : theme::Muted,
            progress,
            DWRITE_TEXT_ALIGNMENT_CENTER);

        render_target_->SetTransform(D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_));
    }

    void DrawGroupOverlay() {
        const float progress = group_progress_;
        FillRect(
            D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight),
            D2D1::ColorF(0.0f, 0.004f, 0.008f, progress * 0.78f));

        const D2D1_POINT_2F center = D2D1::Point2F(312.5f, 250.0f);
        const float eased = EaseOutBack(progress);
        render_target_->SetTransform(
            D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_) *
            D2D1::Matrix3x2F::Scale(0.92f + eased * 0.08f, 0.92f + eased * 0.08f, center) *
            D2D1::Matrix3x2F::Translation(0.0f, (1.0f - eased) * 22.0f));

        const D2D1_RECT_F panel = D2D1::RectF(172.0f, 174.0f, 453.0f, 333.0f);
        DrawShadow(panel, 12.0f, progress * 0.82f);
        FillRoundedRect(panel, 12.0f, WithAlpha(theme::Panel, progress));
        StrokeRoundedRect(panel, 12.0f, WithAlpha(theme::Text, progress * 0.78f));

        FillCircle(D2D1::Point2F(312.5f, 225.0f), 19.0f, WithAlpha(D2D1::ColorF(0.01f, 0.014f, 0.020f), progress));
        StrokeCircle(D2D1::Point2F(312.5f, 225.0f), 18.5f, WithAlpha(theme::Text, progress));
        DrawText(L"Q", D2D1::RectF(293.5f, 204.0f, 331.5f, 246.0f), text_body_bold_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        DrawText(L"QQ交流群", D2D1::RectF(212.0f, 252.0f, 413.0f, 278.0f), text_body_bold_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        DrawText(ProtectedGroupNumber(), D2D1::RectF(212.0f, 277.0f, 386.0f, 302.0f), text_body_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);

        const bool copy_hover = Contains(GroupCopyRect(), mouse_x_, mouse_y_);
        const D2D1_COLOR_F copy_color = WithAlpha(copy_hover ? theme::Text : theme::Muted, progress);
        StrokeRoundedRect(D2D1::RectF(395.0f, 277.0f, 406.0f, 288.0f), 2.0f, copy_color);
        StrokeRoundedRect(D2D1::RectF(399.0f, 281.0f, 412.0f, 294.0f), 2.0f, copy_color);

        if (std::chrono::steady_clock::now() < group_copy_notice_until_) {
            DrawText(L"已复制", D2D1::RectF(212.0f, 304.0f, 413.0f, 324.0f), text_small_.Get(), theme::Success, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        const bool close_hover = Contains(GroupCloseRect(), mouse_x_, mouse_y_);
        if (close_hover) {
            FillRoundedRect(GroupCloseRect(), 7.0f, WithAlpha(theme::PanelRaised, progress));
        }
        SetBrush(WithAlpha(close_hover ? theme::Text : theme::Muted, progress));
        render_target_->DrawLine(D2D1::Point2F(421.0f, 199.0f), D2D1::Point2F(432.0f, 210.0f), brush_.Get(), 1.6f);
        render_target_->DrawLine(D2D1::Point2F(432.0f, 199.0f), D2D1::Point2F(421.0f, 210.0f), brush_.Get(), 1.6f);

        render_target_->SetTransform(D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_));
    }

    void DrawAboutOverlay() {
        const float progress = about_progress_;
        const LocalizedText& text = TextFor(language_);
        FillRect(
            D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight),
            D2D1::ColorF(0.0f, 0.004f, 0.008f, progress * 0.78f));

        const D2D1_POINT_2F center = D2D1::Point2F(312.5f, 250.0f);
        const float eased = EaseOutBack(progress);
        render_target_->SetTransform(
            D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_) *
            D2D1::Matrix3x2F::Scale(0.92f + eased * 0.08f, 0.92f + eased * 0.08f, center) *
            D2D1::Matrix3x2F::Translation(0.0f, (1.0f - eased) * 22.0f));

        const D2D1_RECT_F panel = D2D1::RectF(135.0f, 96.0f, 490.0f, 398.0f);
        DrawShadow(panel, 12.0f, progress * 0.82f);
        FillRoundedRect(panel, 12.0f, WithAlpha(theme::Panel, progress));
        StrokeRoundedRect(panel, 12.0f, WithAlpha(theme::BorderBright, progress * 0.88f));

        FillCircle(D2D1::Point2F(312.5f, 150.0f), 20.0f, WithAlpha(theme::PanelRaised, progress));
        StrokeCircle(D2D1::Point2F(312.5f, 150.0f), 19.5f, WithAlpha(theme::Accent, progress));
        DrawText(L"!", D2D1::RectF(292.5f, 128.0f, 332.5f, 172.0f), text_title_.Get(), theme::Accent, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        DrawText(text.about, D2D1::RectF(170.0f, 174.0f, 455.0f, 202.0f), text_body_bold_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        DrawText(
            text.free_notice,
            D2D1::RectF(166.0f, 210.0f, 459.0f, 282.0f),
            text_small_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_WORD_WRAPPING_WRAP);
        DrawText(AuthorText(language_), D2D1::RectF(166.0f, 286.0f, 459.0f, 310.0f), text_small_.Get(), theme::Muted, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        FillRect(D2D1::RectF(160.0f, 315.0f, 465.0f, 316.0f), WithAlpha(theme::Border, progress));
        DrawText(text.language, D2D1::RectF(166.0f, 323.0f, 230.0f, 349.0f), text_small_.Get(), theme::Muted, progress, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawLanguageButton(LanguageEnglishRect(), Language::English, progress);
        DrawLanguageButton(LanguageChineseRect(), Language::Chinese, progress);
        DrawLanguageButton(LanguageRussianRect(), Language::Russian, progress);
        DrawLanguageButton(LanguageGermanRect(), Language::German, progress);

        const bool close_hover = Contains(AboutCloseRect(), mouse_x_, mouse_y_);
        if (close_hover) {
            FillRoundedRect(AboutCloseRect(), 7.0f, WithAlpha(theme::PanelRaised, progress));
        }
        SetBrush(WithAlpha(close_hover ? theme::Text : theme::Muted, progress));
        render_target_->DrawLine(D2D1::Point2F(458.0f, 115.0f), D2D1::Point2F(469.0f, 126.0f), brush_.Get(), 1.6f);
        render_target_->DrawLine(D2D1::Point2F(469.0f, 115.0f), D2D1::Point2F(458.0f, 126.0f), brush_.Get(), 1.6f);

        render_target_->SetTransform(D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_));
    }

    void DrawLanguageButton(const D2D1_RECT_F& rectangle, Language language, float opacity) {
        const bool selected = language_ == language;
        const bool hover = Contains(rectangle, mouse_x_, mouse_y_);
        FillRoundedRect(
            rectangle,
            6.0f,
            WithAlpha(
                selected ? Mix(theme::AccentPressed, theme::Accent, hover ? 1.0f : 0.74f) : theme::PanelRaised,
                opacity));
        if (!selected) {
            StrokeRoundedRect(rectangle, 6.0f, WithAlpha(theme::Border, opacity));
        }
        DrawText(LanguageName(language), rectangle, text_small_.Get(), selected ? theme::Text : theme::Muted, opacity, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    void DrawLoading() {
        const LocalizedText& text = TextFor(language_);
        const WorkerState state = worker_state_.load();
        const float entrance_progress = loading_progress_;
        const float progress = entrance_progress * (state == WorkerState::Success ? success_fade_opacity_ : 1.0f);
        FillRect(D2D1::RectF(0.0f, 0.0f, kDesignWidth, kDesignHeight), D2D1::ColorF(0.0f, 0.004f, 0.008f, progress * 0.84f));

        const float eased = EaseOutBack(entrance_progress);
        const float scale = 0.91f + eased * 0.09f;
        const D2D1_POINT_2F center = D2D1::Point2F(312.5f, 250.0f);
        render_target_->SetTransform(
            D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_) *
            D2D1::Matrix3x2F::Scale(scale, scale, center) *
            D2D1::Matrix3x2F::Translation(0.0f, (1.0f - eased) * 22.0f));

        const D2D1_RECT_F panel = D2D1::RectF(95.0f, 97.0f, 530.0f, 403.0f);
        DrawShadow(panel, 10.0f, progress * 0.85f);
        FillRoundedRect(panel, 8.0f, WithAlpha(theme::Panel, progress));
        StrokeRoundedRect(panel, 8.0f, WithAlpha(theme::BorderBright, progress * 0.8f));

        const float time = ElapsedSeconds();
        const float bounce = state == WorkerState::Running
            ? std::abs(std::sin(time * 3.4f)) * 9.0f
            : 0.0f;
        DrawGameLogo(D2D1::RectF(290.0f, 198.0f - bounce, 335.0f, 243.0f - bounce), progress);

        std::wstring status;
        if (state == WorkerState::Running) {
            status = std::wstring(text.loading);
        } else if (state == WorkerState::Success) {
            status = std::wstring(text.injection_successful);
        } else {
            std::lock_guard<std::mutex> lock(result_mutex_);
            status = result_message_;
        }
        const D2D1_RECT_F status_rectangle = state == WorkerState::Error
            ? D2D1::RectF(115.0f, 234.0f, 510.0f, 322.0f)
            : D2D1::RectF(120.0f, 263.0f, 505.0f, 301.0f);
        DrawText(
            status,
            status_rectangle,
            state == WorkerState::Error ? text_small_.Get() : text_body_.Get(),
            state == WorkerState::Error ? theme::Danger :
                state == WorkerState::Success ? theme::Success : theme::Muted,
            progress,
            DWRITE_TEXT_ALIGNMENT_CENTER,
            DWRITE_WORD_WRAPPING_WRAP);

        if (state == WorkerState::Running || state == WorkerState::Success) {
            const D2D1_RECT_F track = D2D1::RectF(172.0f, 314.0f, 453.0f, 320.0f);
            FillRoundedRect(track, 3.0f, WithAlpha(Mix(theme::PanelRaised, theme::Border, 0.58f), progress));
            D2D1_RECT_F fill = track;
            fill.right = fill.left + (fill.right - fill.left) * displayed_load_progress_;
            FillRoundedRect(fill, 3.0f, WithAlpha(state == WorkerState::Success ? theme::Success : theme::Accent, progress));
        }

        if (state == WorkerState::Error) {
            const bool hover = Contains(LoadingActionRect(), mouse_x_, mouse_y_);
            const D2D1_RECT_F action = ScaleRect(
                LoadingActionRect(),
                pressed_ == HitTarget::LoadingAction ? 0.965f : 1.0f);
            FillRoundedRect(
                action,
                7.0f,
                WithAlpha(Mix(theme::PanelRaised, theme::BorderBright, hover ? 0.55f : 0.15f), progress));
            StrokeRoundedRect(action, 7.0f, WithAlpha(theme::BorderBright, progress));
            DrawText(
                text.back,
                action,
                text_body_.Get(), theme::Text, progress, DWRITE_TEXT_ALIGNMENT_CENTER);
        }

        render_target_->SetTransform(D2D1::Matrix3x2F::Scale(render_scale_x_, render_scale_y_));
    }

    void DrawLabelValue(
        std::wstring_view label,
        std::wstring_view value,
        float x,
        float y,
        float opacity) {
        DrawText(label, D2D1::RectF(x, y, x + 82.0f, y + 28.0f), text_body_.Get(), theme::Text, opacity, DWRITE_TEXT_ALIGNMENT_LEADING);
        DrawText(value, D2D1::RectF(x + 72.0f, y, x + 240.0f, y + 28.0f), text_body_.Get(), theme::Accent, opacity, DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    void DrawGameLogo(const D2D1_RECT_F& rectangle, float opacity) {
        if (game_logo_bitmap_ != nullptr) {
            render_target_->DrawBitmap(
                game_logo_bitmap_.Get(), rectangle, opacity,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
            return;
        }
        FillRoundedRect(rectangle, 7.0f, WithAlpha(D2D1::ColorF(0.035f, 0.105f, 0.215f), opacity));
        D2D1_RECT_F accent = rectangle;
        accent.right = accent.left + (accent.right - accent.left) * 0.30f;
        FillRoundedRect(accent, 7.0f, WithAlpha(theme::Accent, opacity));
        DrawText(L"PZ", rectangle, text_logo_.Get(), theme::Text, opacity, DWRITE_TEXT_ALIGNMENT_CENTER);
    }

    void DrawPlayIcon(D2D1_POINT_2F center, float opacity) {
        ComPtr<ID2D1PathGeometry> geometry;
        if (FAILED(factory_->CreatePathGeometry(geometry.GetAddressOf()))) {
            return;
        }
        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(geometry->Open(sink.GetAddressOf()))) {
            return;
        }
        sink->BeginFigure(D2D1::Point2F(center.x - 5.0f, center.y - 6.5f), D2D1_FIGURE_BEGIN_FILLED);
        sink->AddLine(D2D1::Point2F(center.x + 4.1f, center.y - 1.4f));
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(center.x + 5.6f, center.y - 0.55f),
            D2D1::Point2F(center.x + 5.6f, center.y + 0.55f),
            D2D1::Point2F(center.x + 4.1f, center.y + 1.4f)));
        sink->AddLine(D2D1::Point2F(center.x - 5.0f, center.y + 6.5f));
        sink->AddBezier(D2D1::BezierSegment(
            D2D1::Point2F(center.x - 6.2f, center.y + 5.8f),
            D2D1::Point2F(center.x - 6.2f, center.y - 5.8f),
            D2D1::Point2F(center.x - 5.0f, center.y - 6.5f)));
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        sink->Close();
        SetBrush(WithAlpha(theme::Text, opacity));
        render_target_->FillGeometry(geometry.Get(), brush_.Get());
    }

    void DrawSpinner(D2D1_POINT_2F center, float time, float opacity, float radius = 14.0f) {
        constexpr float pi = 3.14159265358979323846f;
        constexpr float sweep = 118.0f;
        const float rotation = std::fmod(time * 250.0f, 360.0f);
        const auto point_on_circle = [&](float degrees) {
            const float radians = degrees * pi / 180.0f;
            return D2D1::Point2F(
                center.x + std::cos(radians) * radius,
                center.y + std::sin(radians) * radius);
        };

        SetBrush(WithAlpha(theme::Border, opacity * 0.55f));
        const float stroke_width = radius * (3.6f / 14.0f);
        render_target_->DrawEllipse(D2D1::Ellipse(center, radius, radius), brush_.Get(), stroke_width);

        ComPtr<ID2D1PathGeometry> geometry;
        if (FAILED(factory_->CreatePathGeometry(geometry.GetAddressOf()))) {
            return;
        }
        ComPtr<ID2D1GeometrySink> sink;
        if (FAILED(geometry->Open(sink.GetAddressOf()))) {
            return;
        }
        sink->BeginFigure(point_on_circle(rotation), D2D1_FIGURE_BEGIN_HOLLOW);
        sink->AddArc(D2D1::ArcSegment(
            point_on_circle(rotation + sweep),
            D2D1::SizeF(radius, radius),
            0.0f,
            D2D1_SWEEP_DIRECTION_CLOCKWISE,
            sweep > 180.0f ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
        sink->Close();

        SetBrush(WithAlpha(theme::Accent, opacity));
        render_target_->DrawGeometry(geometry.Get(), brush_.Get(), radius * (3.8f / 14.0f));
    }

    void DrawChevron(D2D1_POINT_2F center, float opacity) {
        SetBrush(WithAlpha(theme::Text, opacity));
        render_target_->DrawLine(D2D1::Point2F(center.x - 4.0f, center.y - 5.0f), center, brush_.Get(), 1.6f);
        render_target_->DrawLine(center, D2D1::Point2F(center.x - 4.0f, center.y + 5.0f), brush_.Get(), 1.6f);
    }

    void DrawAmbientGlow(D2D1_POINT_2F center, float radius, float opacity) {
        for (int layer = 6; layer >= 1; --layer) {
            const float amount = static_cast<float>(layer) / 6.0f;
            FillCircle(center, radius * amount, D2D1::ColorF(theme::Accent.r, theme::Accent.g, theme::Accent.b, opacity * (1.0f - amount) * 0.22f));
        }
    }

    void DrawShadow(const D2D1_RECT_F& rectangle, float radius, float opacity) {
        for (int layer = 5; layer >= 1; --layer) {
            const float spread = layer * 2.0f;
            FillRoundedRect(
                D2D1::RectF(
                    rectangle.left - spread,
                    rectangle.top + spread * 0.45f,
                    rectangle.right + spread,
                    rectangle.bottom + spread),
                radius + spread,
                D2D1::ColorF(0.0f, 0.0f, 0.0f, opacity * (0.018f + layer * 0.009f)));
        }
    }

    void DrawText(
        std::wstring_view text,
        const D2D1_RECT_F& rectangle,
        IDWriteTextFormat* format,
        D2D1_COLOR_F color,
        float opacity,
        DWRITE_TEXT_ALIGNMENT alignment,
        DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_NO_WRAP) {
        format->SetTextAlignment(alignment);
        format->SetWordWrapping(wrapping);
        SetBrush(WithAlpha(color, opacity));
        render_target_->DrawTextW(
            text.data(),
            static_cast<UINT32>(text.size()),
            format,
            rectangle,
            brush_.Get(),
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    void FillRect(const D2D1_RECT_F& rectangle, D2D1_COLOR_F color) {
        SetBrush(color);
        render_target_->FillRectangle(rectangle, brush_.Get());
    }

    void FillRoundedRect(const D2D1_RECT_F& rectangle, float radius, D2D1_COLOR_F color) {
        SetBrush(color);
        render_target_->FillRoundedRectangle(D2D1::RoundedRect(rectangle, radius, radius), brush_.Get());
    }

    void StrokeRoundedRect(const D2D1_RECT_F& rectangle, float radius, D2D1_COLOR_F color) {
        SetBrush(color);
        render_target_->DrawRoundedRectangle(D2D1::RoundedRect(rectangle, radius, radius), brush_.Get(), 1.0f);
    }

    void FillCircle(D2D1_POINT_2F center, float radius, D2D1_COLOR_F color) {
        SetBrush(color);
        render_target_->FillEllipse(D2D1::Ellipse(center, radius, radius), brush_.Get());
    }

    void StrokeCircle(D2D1_POINT_2F center, float radius, D2D1_COLOR_F color) {
        SetBrush(color);
        render_target_->DrawEllipse(D2D1::Ellipse(center, radius, radius), brush_.Get(), 1.0f);
    }

    void SetBrush(D2D1_COLOR_F color) {
        brush_->SetColor(color);
    }

    static D2D1_RECT_F ScaleRect(D2D1_RECT_F rectangle, float scale) {
        const float center_x = (rectangle.left + rectangle.right) * 0.5f;
        const float center_y = (rectangle.top + rectangle.bottom) * 0.5f;
        const float half_width = (rectangle.right - rectangle.left) * 0.5f * scale;
        const float half_height = (rectangle.bottom - rectangle.top) * 0.5f * scale;
        return D2D1::RectF(center_x - half_width, center_y - half_height, center_x + half_width, center_y + half_height);
    }

    static D2D1_RECT_F CloseDetailsRect() { return D2D1::RectF(553.0f, 45.0f, 586.0f, 77.0f); }
    static D2D1_RECT_F AboutRect() { return D2D1::RectF(510.0f, 45.0f, 542.0f, 77.0f); }
    static D2D1_RECT_F AboutCloseRect() { return D2D1::RectF(447.0f, 104.0f, 480.0f, 137.0f); }
    static D2D1_RECT_F GroupRect() { return D2D1::RectF(18.0f, 412.0f, 52.0f, 446.0f); }
    static D2D1_RECT_F GroupCloseRect() { return D2D1::RectF(410.0f, 188.0f, 443.0f, 221.0f); }
    static D2D1_RECT_F GroupCopyRect() { return D2D1::RectF(390.0f, 272.0f, 420.0f, 302.0f); }
    static D2D1_RECT_F LanguageEnglishRect() { return D2D1::RectF(235.0f, 322.0f, 345.0f, 350.0f); }
    static D2D1_RECT_F LanguageChineseRect() { return D2D1::RectF(350.0f, 322.0f, 465.0f, 350.0f); }
    static D2D1_RECT_F LanguageRussianRect() { return D2D1::RectF(235.0f, 356.0f, 345.0f, 384.0f); }
    static D2D1_RECT_F LanguageGermanRect() { return D2D1::RectF(350.0f, 356.0f, 465.0f, 384.0f); }
    static D2D1_RECT_F LoadRect() { return D2D1::RectF(450.0f, 412.0f, 575.0f, 451.0f); }
    static D2D1_RECT_F LoadingActionRect() { return D2D1::RectF(250.0f, 337.0f, 375.0f, 376.0f); }

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    std::filesystem::path game_directory_;
    std::filesystem::path dll_path_;

    ComPtr<ID2D1Factory> factory_;
    ComPtr<ID2D1HwndRenderTarget> render_target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1Bitmap> game_logo_bitmap_;
    ComPtr<IDWriteFactory> write_factory_;
    ComPtr<IDWriteTextFormat> text_small_;
    ComPtr<IDWriteTextFormat> text_body_;
    ComPtr<IDWriteTextFormat> text_body_bold_;
    ComPtr<IDWriteTextFormat> text_title_;
    ComPtr<IDWriteTextFormat> text_brand_;
    ComPtr<IDWriteTextFormat> text_logo_;
    bool tracking_mouse_ = false;
    bool details_visible_ = true;
    bool about_visible_ = false;
    bool group_visible_ = false;
    bool loading_visible_ = false;
    Language language_ = Language::English;
    float mouse_x_ = -1000.0f;
    float mouse_y_ = -1000.0f;
    float details_progress_ = 0.0f;
    float about_progress_ = 0.0f;
    float group_progress_ = 0.0f;
    std::chrono::steady_clock::time_point group_copy_notice_until_{};
    float loading_progress_ = 0.0f;
    float displayed_load_progress_ = 0.0f;
    bool success_display_started_ = false;
    float success_fade_opacity_ = 1.0f;
    float click_bounce_ = 0.0f;
    float render_scale_x_ = 1.0f;
    float render_scale_y_ = 1.0f;
    HitTarget pressed_ = HitTarget::None;

    long long start_counter_ = 0;
    long long last_counter_ = 0;
    long long counter_frequency_ = 1;
    int window_center_x_ = 0;
    int window_center_y_ = 0;
    bool startup_full_size_ = false;

    std::thread worker_;
    std::atomic<WorkerState> worker_state_{WorkerState::Idle};
    std::mutex result_mutex_;
    std::wstring result_message_;
    std::chrono::steady_clock::time_point load_started_{};
    std::chrono::steady_clock::time_point success_display_started_at_{};
};

}  // namespace

int RunLauncherWindow(
    HINSTANCE instance,
    int show_command,
    const std::filesystem::path& game_directory,
    const std::filesystem::path& dll_path) {
    LauncherWindow window(instance, game_directory, dll_path);
    return window.Run(show_command);
}

}  // namespace launcher::ui
