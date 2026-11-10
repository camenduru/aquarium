#pragma once
// ================================================================
// AQUARIUM - the transparent window system
// ----------------------------------------------------------------
// Learned from C:/Users/PC/Documents/VRMPet (src/main.cpp, LayeredOverlay).
// A hardware swap chain cannot carry per-pixel alpha: DXGI's
// CreateSwapChainForHwnd wants DXGI_ALPHA_MODE_IGNORE, so a D3D window is
// always opaque. Windows' own answer to "pixels, not a rectangle" is a
// LAYERED window fed by UpdateLayeredWindow, and that is what this is:
//
//   WS_POPUP             no caption, no border, no chrome at all
//   WS_EX_LAYERED        required by UpdateLayeredWindow (per-pixel alpha)
//   WS_EX_TOOLWINDOW     keeps it out of the taskbar and Alt+Tab
//   WS_EX_NOACTIVATE     never steals focus from the app you are using
//   WS_EX_TOPMOST        floats above other windows
//
// Unlike VRMPet this window does NOT set WS_EX_TRANSPARENT: that would make
// the whole window click-through, including the fish. The click-through is
// per pixel instead, in WM_NCHITTEST - a transparent pixel answers
// HTTRANSPARENT so the click goes to the desktop, and a fish pixel answers
// HTCLIENT so the fish can be right-clicked for the menu.
//
// AC_SRC_ALPHA requires PREMULTIPLIED colour. BlendSprite() does that once,
// on the way into the DIB.
// ================================================================

#include <windows.h>

#include <cstdint>
#include <functional>

class TransparentWindow
{
public:
    bool Create(int width, int height, int x, int y,
                const wchar_t* className, const wchar_t* title);
    void Destroy();
    void Show();
    void MoveTo(int x, int y);

    /// Move and resize. The DIB is recreated at the new size, so the previous
    /// frame is dropped - harmless, every frame redraws the whole canvas.
    bool Resize(int width, int height, int x, int y);

    HWND Hwnd() const { return hwnd_; }
    int Width() const { return w_; }
    int Height() const { return h_; }

    /// Premultiplied BGRA canvas, top-down rows, cleared to fully transparent.
    uint8_t* Pixels() { return bits_; }
    const uint8_t* Pixels() const { return bits_; }
    void Clear();

    /// Straight-alpha RGBA sprite -> premultiplied canvas (see composite.h).
    void DrawSprite(const uint8_t* rgba, int srcW, int srcH,
                    int srcX, int srcY, int srcCellW, int srcCellH,
                    int dstX, int dstY, int scale, bool /*flipX (unused: sheets
                    already carry both facings)*/, int alpha = 255);

    /// Push the canvas to the screen. One call per frame.
    void Push();

    /// True when the canvas pixel is solid enough to belong to the aquarium.
    bool OpaqueAt(int clientX, int clientY, int threshold = 8) const;

    // Filled in by the app.
    std::function<void(int screenX, int screenY)> onRightClick;
    std::function<void(int id)> onHotkey;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static TransparentWindow* From(HWND hwnd);

    HWND     hwnd_ = nullptr;
    HDC      dc_   = nullptr;
    HBITMAP  bmp_  = nullptr;
    uint8_t* bits_ = nullptr;
    int      w_ = 0;
    int      h_ = 0;
    int      x_ = 0;
    int      y_ = 0;
};
