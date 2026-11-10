// ================================================================
// AQUARIUM - layered window implementation
// See transparent_window.h for the recipe and where it came from.
// ================================================================

#include "transparent_window.h"

#include "composite.h"

#include <windowsx.h>

#include <cstring>

namespace
{

constexpr const wchar_t* kPropName = L"AquariumTransparentWindow";

} // namespace

bool TransparentWindow::Create(int width, int height, int x, int y,
                               const wchar_t* className, const wchar_t* title)
{
    w_ = width;
    h_ = height;
    x_ = x;
    y_ = y;

    HINSTANCE inst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.lpszClassName = className;
    if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return false;

    // No WS_EX_TRANSPARENT: the window is click-through per pixel, not wholesale.
    hwnd_ = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
        className, title,
        WS_POPUP,
        x, y, width, height,
        nullptr, nullptr, inst, this);
    if (hwnd_ == nullptr)
        return false;

    HDC screen = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;   // negative = top-down, matching the canvas
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmp_ = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, (void**)&bits_, nullptr, 0);
    if (bmp_ == nullptr)
    {
        ReleaseDC(nullptr, screen);
        Destroy();
        return false;
    }
    dc_ = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);
    if (dc_ == nullptr)
    {
        Destroy();
        return false;
    }
    SelectObject(dc_, bmp_);
    Clear();
    return true;
}

void TransparentWindow::Destroy()
{
    if (dc_ != nullptr && bmp_ != nullptr)
        SelectObject(dc_, nullptr);
    if (bmp_ != nullptr) DeleteObject(bmp_);
    if (dc_ != nullptr) DeleteDC(dc_);
    bmp_ = nullptr;
    dc_ = nullptr;
    bits_ = nullptr;
    if (hwnd_ != nullptr && IsWindow(hwnd_))
        DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

void TransparentWindow::Show()
{
    if (hwnd_ == nullptr) return;
    SetWindowPos(hwnd_, HWND_TOPMOST, x_, y_, w_, h_, SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void TransparentWindow::MoveTo(int x, int y)
{
    x_ = x;
    y_ = y;
    if (hwnd_ != nullptr)
        SetWindowPos(hwnd_, HWND_TOPMOST, x_, y_, 0, 0,
                     SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
    Push();
}

void TransparentWindow::Clear()
{
    if (bits_ != nullptr)
        std::memset(bits_, 0, size_t(w_) * size_t(h_) * 4);
}

void TransparentWindow::DrawSprite(const uint8_t* rgba, int srcW, int srcH,
                                   int srcX, int srcY, int srcCellW, int srcCellH,
                                   int dstX, int dstY, int scale, bool /*flipX*/, int alpha)
{
    BlendSprite(bits_, w_, h_, rgba, srcW, srcH, srcX, srcY, srcCellW, srcCellH,
                dstX, dstY, scale, alpha);
}

void TransparentWindow::Push()
{
    if (hwnd_ == nullptr || dc_ == nullptr) return;
    POINT dstPt = { x_, y_ };
    POINT srcPt = { 0, 0 };
    SIZE  size  = { w_, h_ };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, nullptr, &dstPt, &size, dc_, &srcPt, 0, &bf, ULW_ALPHA);
}

bool TransparentWindow::OpaqueAt(int clientX, int clientY, int threshold) const
{
    if (bits_ == nullptr) return false;
    if (clientX < 0 || clientY < 0 || clientX >= w_ || clientY >= h_) return false;
    const int a = bits_[(size_t(clientY) * size_t(w_) + size_t(clientX)) * 4 + 3];
    return a >= threshold;
}

TransparentWindow* TransparentWindow::From(HWND hwnd)
{
    return reinterpret_cast<TransparentWindow*>(GetPropW(hwnd, kPropName));
}

LRESULT CALLBACK TransparentWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    TransparentWindow* self = From(hwnd);
    if (self == nullptr && msg == WM_CREATE)
    {
        self = reinterpret_cast<TransparentWindow*>(
            reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetPropW(hwnd, kPropName, reinterpret_cast<HANDLE>(self));
    }

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        if (self == nullptr) break;
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &p);
        // A fish pixel belongs to the aquarium; everything else is desktop.
        return self->OpaqueAt(p.x, p.y) ? HTCLIENT : HTTRANSPARENT;
    }
    case WM_RBUTTONUP:
    case WM_NCRBUTTONUP:
    {
        if (self == nullptr || !self->onRightClick) break;
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (msg == WM_NCRBUTTONUP) ScreenToClient(hwnd, &p);
        POINT screen = p;
        ClientToScreen(hwnd, &screen);
        self->onRightClick(screen.x, screen.y);
        return 0;
    }
    case WM_HOTKEY:
        if (self != nullptr && self->onHotkey)
            self->onHotkey(int(wParam));
        return 0;
    case WM_DESTROY:
        RemovePropW(hwnd, kPropName);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
