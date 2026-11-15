// ================================================================
// AQUARIUM - layered window implementation
// See transparent_window.h for the recipe and where it came from.
// ================================================================

#include "transparent_window.h"

#include "composite.h"

#include <windowsx.h>

#include <algorithm>
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

bool TransparentWindow::RebuildCanvas(int width, int height)
{
    HDC screen = GetDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;   // top-down, like Create()
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void*   bits = nullptr;
    HBITMAP bmp  = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC     dc   = CreateCompatibleDC(screen);
    ReleaseDC(nullptr, screen);

    // Build the new canvas before dropping the old one: if this fails the
    // window keeps rendering into what it already has.
    if (bmp == nullptr || dc == nullptr)
    {
        if (bmp != nullptr) DeleteObject(bmp);
        if (dc != nullptr) DeleteDC(dc);
        return false;
    }
    SelectObject(dc, bmp);

    if (dc_ != nullptr && bmp_ != nullptr)
        SelectObject(dc_, nullptr);
    if (bmp_ != nullptr) DeleteObject(bmp_);
    if (dc_ != nullptr) DeleteDC(dc_);

    dc_   = dc;
    bmp_  = bmp;
    bits_ = static_cast<uint8_t*>(bits);
    w_    = width;
    h_    = height;
    Clear();
    return true;
}

bool TransparentWindow::Resize(int width, int height, int x, int y)
{
    if (hwnd_ == nullptr) return false;
    width  = std::max(width, 32);
    height = std::max(height, 32);

    const bool changed = (width != w_ || height != h_);
    if (!RebuildCanvas(width, height)) return false;
    x_ = x;
    y_ = y;

    // SWP_SHOWWINDOW: a layered window moved while hidden would otherwise keep
    // its old content on screen until the next Push().
    SetWindowPos(hwnd_, HWND_TOPMOST, x_, y_, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // The canvas is a new size, so whatever was drawn in it has to be rebuilt.
    // One path for a menu action and an edge drag alike: the app rebuilds the
    // tank and the water here, not at each call site.
    if (changed && onClientResize) onClientResize(width, height);

    Push();
    return true;
}

bool TransparentWindow::SetWindowed(bool on)
{
    if (hwnd_ == nullptr || on == windowed_) return false;

    // Deliberately no style change. A per-pixel-alpha layered window has no
    // system non-client area - UpdateLayeredWindow owns the whole surface - so
    // WS_CAPTION/WS_THICKFRAME would change the geometry and paint nothing.
    // Windowed is therefore a behaviour, not a frame: the hit test, the drag
    // and the border below are all ours.
    windowed_ = on;
    dragging_ = false;
    resizing_ = false;
    edge_     = 0;
    SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(on ? IDC_SIZEALL : IDC_ARROW)));
    Push();   // the outline appears or goes immediately
    return true;
}

int TransparentWindow::EdgeAt(int clientX, int clientY) const
{
    int edge = 0;
    if (clientX < kEdge)        edge |= 1;   // left
    if (clientX >= w_ - kEdge)  edge |= 2;   // right
    if (clientY < kEdge)        edge |= 4;   // top
    if (clientY >= h_ - kEdge)  edge |= 8;   // bottom
    return edge;
}

void TransparentWindow::DrawBorder()
{
    if (bits_ == nullptr || w_ < 2 || h_ < 2) return;
    // 40% white, premultiplied (UpdateLayeredWindow expects colour already
    // multiplied by alpha), so it reads on sand, water and bare desktop alike.
    const uint8_t v = 102;
    auto put = [&](int x, int y)
    {
        uint8_t* p = bits_ + (size_t(y) * size_t(w_) + size_t(x)) * 4;
        p[0] = v; p[1] = v; p[2] = v; p[3] = v;
    };
    for (int x = 0; x < w_; ++x) { put(x, 0); put(x, h_ - 1); }
    for (int y = 1; y < h_ - 1; ++y) { put(0, y); put(w_ - 1, y); }
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
    // The outline is drawn here because this is the last thing before the
    // canvas goes to the screen, so it lands on top of the tank rather than
    // being painted over by the next frame's content.
    if (windowed_) DrawBorder();
    POINT dstPt = { x_, y_ };
    POINT srcPt = { 0, 0 };
    SIZE  size  = { w_, h_ };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hwnd_, nullptr, &dstPt, &size, dc_, &srcPt, 0, &bf, ULW_ALPHA);
}

void TransparentWindow::SetHitMask(const uint8_t* mask, int width, int height, int scale)
{
    hitMask_   = mask;
    hitW_      = width;
    hitH_      = height;
    hitScale_  = std::max(scale, 1);
    waterBody_ = mask != nullptr && width > 0 && height > 0;
}

bool TransparentWindow::OpaqueAt(int clientX, int clientY, int threshold) const
{
    if (clientX < 0 || clientY < 0 || clientX >= w_ || clientY >= h_) return false;

    if (hitMask_ != nullptr && hitW_ > 0 && hitH_ > 0)
    {
        const int mx = clientX / hitScale_;
        const int my = clientY / hitScale_;
        if (mx < hitW_ && my < hitH_)
            return hitMask_[size_t(my) * size_t(hitW_) + size_t(mx)] != 0;
        return false;
    }

    if (bits_ == nullptr) return false;
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
        // Windowed means the tank is grabbable, so every pixel belongs to it -
        // that is the only way a press on open water can start a drag or a
        // resize. The overlay keeps per-pixel click-through.
        if (self->windowed_) return HTCLIENT;
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &p);
        // A fish pixel belongs to the aquarium. The water body does not, so the
        // desktop keeps its left clicks - except while the right button is
        // down, which is the one press the water has to take, or the menu would
        // be unreachable from anywhere but a fish. The button is already down by
        // the time the system asks where a press landed, so the real key state
        // is what tells the two apart here.
        const bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        return TankOwnsHit(self->OpaqueAt(p.x, p.y), self->waterBody_, rightDown)
                   ? HTCLIENT
                   : HTTRANSPARENT;
    }
    case WM_SETCURSOR:
        if (self != nullptr && self->windowed_)
        {
            // The edge under the pointer decides the cursor, which is the only
            // way to see where a chrome-less window can be pulled from.
            POINT p = {};
            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            switch (self->EdgeAt(p.x, p.y))
            {
            case 1: case 2:  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEWE)));   break;
            case 4: case 8:  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZENS)));   break;
            case 5: case 10: SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZENWSE))); break;
            case 6: case 9:  SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZENESW))); break;
            default:         SetCursor(LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_SIZEALL)));  break;
            }
            return TRUE;
        }
        break;
    case WM_LBUTTONDOWN:
    {
        if (self == nullptr || !self->windowed_) break;
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        POINT screen = p;
        ClientToScreen(hwnd, &screen);
        self->edge_       = self->EdgeAt(p.x, p.y);
        self->resizing_   = self->edge_ != 0;
        self->grab_.x     = screen.x - self->x_;
        self->grab_.y     = screen.y - self->y_;
        self->dragOrigin_ = screen;
        GetWindowRect(hwnd, &self->dragRect_);
        self->dragging_   = true;
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        if (self == nullptr || !self->dragging_) break;
        POINT screen = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hwnd, &screen);
        if (self->resizing_)
        {
            // Always from where the drag began, never accumulated: a resize
            // that adds up its own deltas drifts as soon as a step is clamped.
            RECT r = self->dragRect_;
            const int dx = screen.x - self->dragOrigin_.x;
            const int dy = screen.y - self->dragOrigin_.y;
            if (self->edge_ & 1) r.left   += dx;
            if (self->edge_ & 2) r.right  += dx;
            if (self->edge_ & 4) r.top    += dy;
            if (self->edge_ & 8) r.bottom += dy;
            if (r.right - r.left < kMinW)
            {
                if (self->edge_ & 1) r.left  = r.right - kMinW;
                else                 r.right = r.left + kMinW;
            }
            if (r.bottom - r.top < kMinH)
            {
                if (self->edge_ & 4) r.top    = r.bottom - kMinH;
                else                 r.bottom = r.top + kMinH;
            }
            self->Resize(int(r.right - r.left), int(r.bottom - r.top), int(r.left), int(r.top));
        }
        else
        {
            self->MoveTo(screen.x - self->grab_.x, screen.y - self->grab_.y);
        }
        return 0;
    }
    case WM_CAPTURECHANGED:
        // Capture taken away mid-drag: drop the drag rather than leave the tank
        // following a pointer it no longer owns.
        if (self != nullptr && self->dragging_)
        {
            self->dragging_ = false;
            self->resizing_ = false;
        }
        break;
    case WM_GETMINMAXINFO:
        if (self != nullptr && self->windowed_)
        {
            // Nothing should be able to squeeze the tank below a reef and a
            // couple of fish.
            MINMAXINFO* mm = reinterpret_cast<MINMAXINFO*>(lParam);
            mm->ptMinTrackSize.x = kMinW;
            mm->ptMinTrackSize.y = kMinH;
            return 0;
        }
        break;
    case WM_SIZE:
        if (self != nullptr)
        {
            // Only reachable from a resize we did not do ourselves (ours sets
            // w_/h_ first, so it lands in the early return). The canvas has to
            // follow, and the app has to rebuild what is drawn in it.
            const int cw = GET_X_LPARAM(lParam);
            const int ch = GET_Y_LPARAM(lParam);
            if (cw > 0 && ch > 0 && (cw != self->w_ || ch != self->h_)
                && self->RebuildCanvas(cw, ch))
            {
                if (self->onClientResize) self->onClientResize(cw, ch);
                self->Push();
            }
            return 0;
        }
        break;
    case WM_MOVE:
        // The frame can be dragged anywhere, so the window's own idea of where
        // it is has to follow - the app asks for it when it needs to know which
        // monitor the tank is on now.
        if (self != nullptr)
        {
            self->x_ = GET_X_LPARAM(lParam);
            self->y_ = GET_Y_LPARAM(lParam);
        }
        break;
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
    case WM_LBUTTONUP:
    {
        // The button that ends a drag is not a poke at a fish: a windowed press
        // starts a move or a resize, and releasing it must not also startle
        // whatever happened to be under the pointer.
        if (self != nullptr && self->dragging_)
        {
            self->dragging_ = false;
            self->resizing_ = false;
            if (GetCapture() == hwnd) ReleaseCapture();
            return 0;
        }
        // Only delivered for HTCLIENT pixels, i.e. on something solid - a
        // fish or a rock. Clicking open water passes straight through.
        if (self == nullptr || !self->onLeftClick) break;
        POINT p = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        self->onLeftClick(p.x, p.y);
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
