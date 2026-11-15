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

/// Does a click at this pixel belong to the tank?
///
/// A solid pixel always does. The water body deliberately does NOT - that is
/// what lets the desktop keep its left clicks - but a right-press must still
/// reach the menu, so the water takes the click while the right button is down.
/// Split out as a pure rule because the interesting cases are the two booleans,
/// not the window: a hit test that silently answers HTTRANSPARENT for the whole
/// water body is a menu you cannot open.
inline bool TankOwnsHit(bool solidPixel, bool waterBody, bool rightButtonDown)
{
    return solidPixel || (waterBody && rightButtonDown);
}

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
    int X() const { return x_; }
    int Y() const { return y_; }

    /// Windowed mode: the tank becomes grabbable. There is no system frame -
    /// per-pixel alpha means UpdateLayeredWindow owns the whole window surface,
    /// so a WS_CAPTION would be applied to the geometry and never painted - so
    /// windowed means "every pixel belongs to the tank": a press inside drags
    /// it, a press within kEdge of an edge resizes it, the cursor says which,
    /// and a thin border shows where the edges are. The window stays
    /// WS_EX_NOACTIVATE and topmost throughout, so grabbing it never steals
    /// focus from the app you are working in.
    bool SetWindowed(bool on);
    bool Windowed() const { return windowed_; }

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

    /// A separate, binary hit mask at `scale` times reduced resolution.
    /// When set, the click test uses it instead of the canvas alpha, so a
    /// translucent water body never captures clicks meant for the desktop.
    /// The pointer is borrowed and must outlive the window.
    void SetHitMask(const uint8_t* mask, int width, int height, int scale);

    /// True when the canvas pixel is solid enough to belong to the aquarium.
    bool OpaqueAt(int clientX, int clientY, int threshold = 8) const;

    // Filled in by the app.
    std::function<void(int screenX, int screenY)> onRightClick;
    std::function<void(int clientX, int clientY)> onLeftClick;
    std::function<void(int id)> onHotkey;
    /// The user dragged the frame to a new client size. The canvas is already
    /// that size when this fires; the app has to rebuild what is drawn in it.
    std::function<void(int clientW, int clientH)> onClientResize;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static TransparentWindow* From(HWND hwnd);

    /// New DIB + DC at this size, old ones dropped. Keeps the window and the
    /// canvas the same size in one place, since both Resize() and an edge drag
    /// end up here.
    bool RebuildCanvas(int width, int height);

    /// Which edges a client point is within kEdge of: 1 left, 2 right, 4 top,
    /// 8 bottom, 0 for the interior. Corners are two bits.
    int EdgeAt(int clientX, int clientY) const;

    /// The 1px outline drawn while windowed, so the edges are visible.
    void DrawBorder();

    static constexpr int kEdge = 8;     // grab margin, client pixels
    static constexpr int kMinW = 320;
    static constexpr int kMinH = 240;

    HWND     hwnd_ = nullptr;
    HDC      dc_   = nullptr;
    HBITMAP  bmp_  = nullptr;
    uint8_t* bits_ = nullptr;
    int      w_ = 0;
    int      h_ = 0;
    int      x_ = 0;
    int      y_ = 0;

    // Borrowed hit mask: one byte per logical pixel, `hitScale_` window
    // pixels per entry. Null means "fall back to the canvas alpha", which is
    // what the pre-water build did.
    const uint8_t* hitMask_ = nullptr;
    int            hitW_    = 0;
    int            hitH_    = 0;
    int            hitScale_ = 1;

    /// True when a hit mask is installed, i.e. the ghost water body is on. The
    /// water takes right-clicks (see TankOwnsHit) but not left ones.
    bool waterBody_ = false;

    // Windowed mode: the tank is grabbable. `edge_` is the grab the press
    // started on, `dragRect_` the window rect at that moment, so a resize is
    // always computed from where the drag began rather than accumulated.
    bool  windowed_ = false;
    bool  dragging_ = false;
    bool  resizing_ = false;
    int   edge_     = 0;
    RECT  dragRect_ = {};
    POINT dragOrigin_ = { 0, 0 };   // pointer at the press, screen pixels
    POINT grab_       = { 0, 0 };   // pointer -> window offset, screen pixels
};
