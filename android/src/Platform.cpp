#include "Platform.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <mutex>
#include <thread>
#include "3ds.h"
#include "citro3d.h"
#include "Font.h"
#include "Gfx.h"
#include "Screens.h"
#include "game/Inventory.h"
#include "game/ItemTable.h"
#include "game/Options.h"
#include "render/Renderer.h"
#include "world/BlockTable.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "axeolon", __VA_ARGS__)

int axeolon_main(); // source/main.cpp

namespace platform {
namespace {

// ---------------------------------------------------------------- shared with the activity thread
std::mutex gMx;
std::condition_variable gCv;
ANativeWindow* gWindow = nullptr;     // what the activity gave us
bool gWindowGoing = false;            // the activity is waiting for us to let go of it
struct TouchEv { TouchType type; int id; float x, y; };
std::vector<TouchEv> gEventsIn;       // since the game thread last looked
std::vector<Finger> gFingersIn;       // the fingers down now (for the sign keyboard's Done)
int gBackPresses = 0;
std::atomic<bool> gQuit{false};
std::atomic<bool> gRunning{false};
AAssetManager* gAssetMgr = nullptr;
std::string gFilesDir;
float gDpi = 320.0f;

// ---------------------------------------------------------------- the game thread's GL
EGLDisplay gDisplay = EGL_NO_DISPLAY;
EGLContext gContext = EGL_NO_CONTEXT;
EGLSurface gSurface = EGL_NO_SURFACE;
EGLConfig gConfig = nullptr;
ANativeWindow* gSurfaceWindow = nullptr;
int gW = 1280, gH = 720;

using Clock = std::chrono::steady_clock;
const Clock::time_point gStart = Clock::now();

// ---------------------------------------------------------------- controls (game thread)
struct Owned {
    int id;
    enum Kind { PAD, WORLD, PANEL, BUTTON, NONE } kind;
    float x0, y0, x, y, lastX, lastY;
    Clock::time_point t0;
    bool looking = false, breaking = false;
};
std::vector<Owned> gOwned;
u32 gHeld = 0, gDown = 0, gUp = 0;
u32 gTapKeys = 0;          // pressed for exactly one frame (a tap)
touchPosition gTouch = {0, 0};
circlePosition gPad = {0, 0};
float gLookDX = 0, gLookDY = 0;
int gHotbarTap = -1;
bool gInvToggle = false;
bool gBottom = false;      // the bottom screen panel is open
bool gSneak = false;       // the sneak toggle
int gPadDir = -1;          // 0 up 1 left 2 right 3 back 4 jump 5 up-left 6 up-right
bool gPanelTapUp = false;  // a panel touch ended this frame
std::vector<std::pair<float, float>> gMenuTaps; // taps in top-screen units (menus)

// ---- layout, in window pixels ----
float cellPx() { return std::min(gDpi * 0.42f, gH / 5.5f); }
float padX() { return gH * 0.03f + 8; }
float padY() { return gH - 8 - gH * 0.03f - cellPx() * 3; }
float guiS() { return float(guiScale()); }
// Pocket Edition's hotbar: 9 slots of 20 + the "..." button, centred at the bottom
float hotbarW() { return (182.0f + 20.0f) * guiS(); }
float hotbarX() { return (gW - hotbarW()) / 2.0f; }
float hotbarY() { return gH - 22.0f * guiS(); }
float pauseSize() { return std::min(18.0f * guiS(), gDpi * 0.3f); }
// the top screen's 400x240 in the window
float topScale() { return float(gH) / 240.0f; }
float topX() { return (gW - 400.0f * topScale()) / 2.0f; }
// the bottom panel
float panelScale() { return std::min(gH * 0.94f / 240.0f, gW * 0.62f / 320.0f); }
float panelX() { return (gW - 320.0f * panelScale()) / 2.0f; }
float panelY() { return (gH - 240.0f * panelScale()) / 2.0f; }
bool inPanel(float x, float y) {
    float s = panelScale();
    return x >= panelX() && y >= panelY() && x < panelX() + 320 * s && y < panelY() + 240 * s;
}
bool inRect(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

int padButton(float x, float y) {
    float c = cellPx();
    int col = int(std::floor((x - padX()) / c)), row = int(std::floor((y - padY()) / c));
    if (col < 0 || col > 2 || row < 0 || row > 2) return -1;
    if (row == 0) return col == 1 ? 0 : (gPadDir == 0 || gPadDir == 5 || gPadDir == 6) ? (col == 0 ? 5 : 6) : -1;
    if (row == 1) return col == 0 ? 1 : col == 2 ? 2 : 4;
    return col == 1 ? 3 : -1;
}
bool onSneak(float x, float y) { // above the pad, on its right
    float c = cellPx();
    return inRect(x, y, padX() + c * 2, padY() - c * 0.75f, c, c * 0.7f);
}
// the pause and chat buttons sit side by side at the top middle (Pocket Edition 0.9+)
float pauseX() { return gW / 2.0f + 3; }
float chatX() { return gW / 2.0f - 3 - pauseSize(); }
bool onPause(float x, float y) { return inRect(x, y, pauseX(), 6, pauseSize(), pauseSize()); }
bool onChat(float x, float y) { return inRect(x, y, chatX(), 6, pauseSize(), pauseSize()); }
bool gChatToggle = false;
std::vector<std::string> gInteract; // the interact buttons' labels
int gInteractTap = -1;
// button i's rectangle, window pixels: side by side, centred over the hotbar
bool interactRect(size_t i, float& x, float& y, float& w, float& h) {
    if (i >= gInteract.size()) return false;
    float s = guiS(), gap = 4 * s, total = -gap;
    h = 20 * s;
    for (const std::string& l : gInteract) total += (font::width(l) + 24) * s + gap;
    x = (gW - total) / 2.0f;
    for (size_t k = 0; k < i; ++k) x += (font::width(gInteract[k]) + 24) * s + gap;
    w = (font::width(gInteract[i]) + 24) * s;
    y = hotbarY() - h - 26 * s; // clear of the held item's name
    return true;
}
int onInteract(float x, float y) {
    float bx, by, bw, bh;
    for (size_t i = 0; interactRect(i, bx, by, bw, bh); ++i)
        if (inRect(x, y, bx, by, bw, bh)) return int(i);
    return -1;
}

int onHotbar(float x, float y) {
    if (!inRect(x, y, hotbarX(), hotbarY(), hotbarW(), 22 * guiS())) return -2;
    int slot = int((x - hotbarX() - guiS()) / (20 * guiS()));
    return slot > 9 ? 9 : slot; // 9 = the "..." button
}

bool gInGame = false;

std::mutex gAimMx;
float gAimX = 0, gAimY = 0; // the last world finger, window pixels
void aimAt(float x, float y) {
    std::lock_guard<std::mutex> l(gAimMx);
    gAimX = x;
    gAimY = y;
}

void beginTouch(int id, float x, float y, u32& taps) {
    Owned o{id, Owned::NONE, x, y, x, y, x, y, Clock::now()};
    int hb;
    if (gBottom) {
        if (inPanel(x, y)) {
            o.kind = Owned::PANEL;
            float s = panelScale();
            gTouch.px = u16(std::max(0.0f, std::min(319.0f, (x - panelX()) / s)));
            gTouch.py = u16(std::max(0.0f, std::min(239.0f, (y - panelY()) / s)));
        }
        else { gBottom = false; o.kind = Owned::NONE; } // a tap outside closes it
    } else if (!gInGame) {
        o.kind = Owned::WORLD; // menus: taps only
    } else if (onPause(x, y)) {
        o.kind = Owned::BUTTON;
        taps |= KEY_START;
    } else if (onChat(x, y)) {
        o.kind = Owned::BUTTON;
        gChatToggle = true;
    } else if ((hb = onInteract(x, y)) >= 0) {
        o.kind = Owned::BUTTON;
        gInteractTap = hb;
    } else if ((hb = onHotbar(x, y)) >= -1) {
        o.kind = Owned::BUTTON;
        if (hb == 9) gInvToggle = true;
        else if (hb >= 0) gHotbarTap = hb;
    } else if (onSneak(x, y)) {
        o.kind = Owned::BUTTON;
        gSneak = !gSneak;
    } else if (padButton(x, y) >= 0 && std::none_of(gOwned.begin(), gOwned.end(), [](const Owned& q) { return q.kind == Owned::PAD; })) {
        o.kind = Owned::PAD;
    } else {
        o.kind = Owned::WORLD;
        aimAt(x, y); // taps and holds aim where the finger is
    }
    gOwned.push_back(o);
}

void endTouch(size_t i, u32& taps, bool cancelled) {
    Owned& o = gOwned[i];
    if (!cancelled && o.kind == Owned::WORLD && !o.looking && !o.breaking) {
        taps |= KEY_ZL; // a tap: use / place (the menus take it as a press)
        gMenuTaps.push_back({(o.x - topX()) / topScale(), o.y / topScale()});
    }
    if (o.kind == Owned::PANEL) gPanelTapUp = true;
    gOwned.erase(gOwned.begin() + long(i));
}

void scanControls() {
    std::vector<TouchEv> evs;
    int backs;
    {
        std::lock_guard<std::mutex> l(gMx);
        evs.swap(gEventsIn);
        backs = gBackPresses;
        gBackPresses = 0;
    }
    u32 held = 0, taps = gTapKeys;
    gTapKeys = 0;
    gPanelTapUp = false;
    if (backs > 0) {
        if (gBottom) gBottom = false;
        else taps |= KEY_START;
    }
    for (const TouchEv& e : evs) {
        auto it = std::find_if(gOwned.begin(), gOwned.end(), [&](const Owned& o) { return o.id == e.id; });
        switch (e.type) {
        case TOUCH_DOWN:
            if (it == gOwned.end()) beginTouch(e.id, e.x, e.y, taps);
            break;
        case TOUCH_MOVE:
            if (it != gOwned.end()) {
                it->x = e.x;
                it->y = e.y;
                if (it->kind == Owned::WORLD) { // the drag so far
                    float moved = std::hypot(it->x - it->x0, it->y - it->y0);
                    if (!it->breaking && moved > gDpi * 0.06f) it->looking = true;
                    if (!it->looking) aimAt(it->x, it->y);
                    if (it->looking) {
                        gLookDX += it->x - it->lastX;
                        gLookDY += it->y - it->lastY;
                    }
                    it->lastX = it->x;
                    it->lastY = it->y;
                }
                if (it->kind == Owned::PANEL) { // where the stylus is
                    float s = panelScale();
                    gTouch.px = u16(std::max(0.0f, std::min(319.0f, (it->x - panelX()) / s)));
                    gTouch.py = u16(std::max(0.0f, std::min(239.0f, (it->y - panelY()) / s)));
                }
            }
            break;
        case TOUCH_UP:
            if (it != gOwned.end()) endTouch(size_t(it - gOwned.begin()), taps, false);
            break;
        case TOUCH_CANCEL:
            while (!gOwned.empty()) endTouch(0, taps, true);
            break;
        }
    }
    // what each finger does this frame
    gPadDir = -1;
    gPad = {0, 0};
    bool panelDown = false;
    for (Owned& o : gOwned) {
        switch (o.kind) {
        case Owned::PAD: {
            int b = padButton(o.x, o.y);
            gPadDir = b;
            if (b == 0 || b == 5 || b == 6) gPad.dy = 156;
            if (b == 3) gPad.dy = -156;
            if (b == 1 || b == 5) gPad.dx = -156;
            if (b == 2 || b == 6) gPad.dx = 156;
            if (b == 4) held |= KEY_A;
            break;
        }
        case Owned::WORLD: {
            if (!o.looking && gInGame && std::chrono::duration<float>(Clock::now() - o.t0).count() > 0.3f) {
                o.breaking = true; // held still: mine / attack
            }
            if (o.breaking) held |= KEY_ZR;
            break;
        }
        case Owned::PANEL: {
            float s = panelScale();
            gTouch.px = u16(std::max(0.0f, std::min(319.0f, (o.x - panelX()) / s)));
            gTouch.py = u16(std::max(0.0f, std::min(239.0f, (o.y - panelY()) / s)));
            panelDown = true;
            break;
        }
        default: break;
        }
    }
    // a stylus tap that went down and up between two frames still touches for one frame
    if (panelDown || gPanelTapUp) held |= KEY_TOUCH;
    if (gSneak && gInGame) held |= KEY_B;
    held |= taps;
    gDown = (held & ~gHeld) | taps; // a tap is a press even if the key was held a frame ago
    gUp = gHeld & ~held;
    gHeld = held;
}

} // namespace

// ---------------------------------------------------------------- the activity thread's side

void setWindow(ANativeWindow* w) {
    std::unique_lock<std::mutex> l(gMx);
    if (w) {
        gWindow = w;
        gWindowGoing = false;
        gCv.notify_all();
        return;
    }
    // the window is going: wait until the game thread has dropped its surface
    gWindowGoing = true;
    gCv.notify_all();
    gCv.wait_for(l, std::chrono::seconds(3), [] { return gSurfaceWindow == nullptr || !gRunning; });
    gWindow = nullptr;
    gWindowGoing = false;
}

void setAssets(AAssetManager* a, const std::string& filesDir, float dpi) {
    gAssetMgr = a;
    gFilesDir = filesDir;
    gDpi = dpi > 50 ? dpi : 320.0f;
}

void touchEvent(TouchType type, int id, float x, float y) {
    std::lock_guard<std::mutex> l(gMx);
    gEventsIn.push_back({type, id, x, y});
    if (gEventsIn.size() > 512) gEventsIn.erase(gEventsIn.begin()); // nobody's reading (no game thread yet)
    auto it = std::find_if(gFingersIn.begin(), gFingersIn.end(), [&](const Finger& f) { return f.id == id; });
    if (type == TOUCH_DOWN || type == TOUCH_MOVE) {
        if (it == gFingersIn.end()) gFingersIn.push_back({id, x, y});
        else { it->x = x; it->y = y; }
    } else if (type == TOUCH_UP) {
        if (it != gFingersIn.end()) gFingersIn.erase(it);
    } else {
        gFingersIn.clear();
    }
}

// ---- typing ----
std::atomic<bool> gTyping{false};
std::string gTyped;
bool gTypedDone = false, gTypedCancel = false;
bool typingWanted() { return gTyping; }
void typeChar(unsigned c) {
    std::lock_guard<std::mutex> l(gMx);
    if (!gTyping) return;
    if (c == 8) { if (!gTyped.empty()) gTyped.pop_back(); }
    else if (c == '\n') gTyped += '\n';
    else if (c >= 32 && c < 127) gTyped += char(c);
}

void backPressed() {
    std::lock_guard<std::mutex> l(gMx);
    ++gBackPresses;
}

std::atomic<bool> gSaveReq{false};
void requestSave() { gSaveReq = true; }
bool takeSaveRequest() { return gSaveReq.exchange(false); }

void requestQuit() {
    gQuit = true;
    gCv.notify_all();
}

bool gameRunning() { return gRunning; }

// ---------------------------------------------------------------- EGL on the game thread

void ensureSurface() {
    std::unique_lock<std::mutex> l(gMx);
    if (gSurface != EGL_NO_SURFACE && (gWindowGoing || gWindow != gSurfaceWindow)) {
        eglMakeCurrent(gDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, gContext);
        eglDestroySurface(gDisplay, gSurface);
        gSurface = EGL_NO_SURFACE;
        gSurfaceWindow = nullptr;
        gCv.notify_all();
    }
    gCv.wait(l, [] { return (gWindow && !gWindowGoing) || gQuit; });
    if (gQuit && !gWindow) return;
    if (gDisplay == EGL_NO_DISPLAY) {
        gDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(gDisplay, nullptr, nullptr);
        const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8,
                                  EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE};
        EGLint n = 0;
        eglChooseConfig(gDisplay, attribs, &gConfig, 1, &n);
        if (n == 0) { // no 24-bit depth: 16
            const EGLint a16[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_DEPTH_SIZE, 16, EGL_NONE};
            eglChooseConfig(gDisplay, a16, &gConfig, 1, &n);
        }
        const EGLint ctx[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
        gContext = eglCreateContext(gDisplay, gConfig, EGL_NO_CONTEXT, ctx);
    }
    if (gSurface == EGL_NO_SURFACE) {
        gSurface = eglCreateWindowSurface(gDisplay, gConfig, gWindow, nullptr);
        gSurfaceWindow = gWindow;
        eglMakeCurrent(gDisplay, gSurface, gSurface, gContext);
        eglSwapInterval(gDisplay, 1);
    }
    EGLint w = 0, h = 0;
    eglQuerySurface(gDisplay, gSurface, EGL_WIDTH, &w);
    eglQuerySurface(gDisplay, gSurface, EGL_HEIGHT, &h);
    if (w > 0 && h > 0 && (w != gW || h != gH)) {
        gW = w;
        gH = h;
        gfx::resize(w, h);
    }
}

bool presentFrame() {
    gfx::flush();
    if (gSurface == EGL_NO_SURFACE) return false;
    eglSwapBuffers(gDisplay, gSurface);
    ensureSurface(); // a new window, a rotation, or none at all
    return true;
}

int windowWidth() { return gW; }
int windowHeight() { return gH; }
float topAspect() { return float(gW) / float(gH); }
float topLeft() { return -topX() / topScale(); }
float topRight() { return (gW - topX()) / topScale(); }
void aimPoint(float& nx, float& ny) {
    std::lock_guard<std::mutex> l(gAimMx);
    nx = gW > 0 ? gAimX / gW * 2.0f - 1.0f : 0.0f;
    ny = gH > 0 ? 1.0f - gAimY / gH * 2.0f : 0.0f;
}
int guiScale() { return gfx::guiScale(); }

void beginScreen(int screen) {
    float m[16] = {0};
    // virtual units -> clip space
    float s, ox, oy, vw, vh;
    if (screen == 1) { s = panelScale(); ox = panelX(); oy = panelY(); vw = 320; vh = 240; }
    else { s = topScale(); ox = topX(); oy = 0; vw = 400; vh = 240; }
    (void)vw;
    (void)vh;
    m[0] = 2.0f * s / gW;
    m[5] = -2.0f * s / gH;
    m[10] = -1.0f / 1000.0f;
    m[12] = 2.0f * ox / gW - 1.0f;
    m[13] = 1.0f - 2.0f * oy / gH;
    m[15] = 1.0f;
    gfx::setMatrix(m);
}

bool bottomVisible() { return gBottom; }
void showBottom(bool on) { gBottom = on; }

// ---------------------------------------------------------------- hooks used by the game loop

void applyLook(float& yaw, float& pitch) {
    float degPerPx = 110.0f / (gDpi * 1.5f) * gOptions.lookScale();
    yaw += gLookDX * degPerPx;
    pitch += (gOptions.invertY ? -gLookDY : gLookDY) * degPerPx;
    if (pitch < -90.0f) pitch = -90.0f;
    if (pitch > 90.0f) pitch = 90.0f;
    gLookDX = gLookDY = 0;
}

void setInteractButtons(const std::vector<std::string>& labels) { gInteract = labels; }
int takeInteractTap() {
    int t = gInteractTap;
    gInteractTap = -1;
    return t < int(gInteract.size()) ? t : -1;
}

bool takeChatToggle() {
    bool t = gChatToggle;
    gChatToggle = false;
    return t;
}

int takeHotbarTap() {
    int s = gHotbarTap;
    gHotbarTap = -1;
    return s;
}

bool takeInventoryToggle() {
    bool t = gInvToggle;
    gInvToggle = false;
    return t;
}

int takeMenuTap(int buttons, float firstY, float stepY) {
    int hit = -1;
    for (auto& t : gMenuTaps)
        for (int i = 0; i < buttons; ++i)
            if (t.first >= 100 && t.first < 300 && t.second >= firstY + i * stepY && t.second < firstY + i * stepY + 20) hit = i;
    gMenuTaps.clear();
    return hit;
}

// Pocket Edition's pad (gui.png), the sneak toggle, the pause button and the hotbar
void drawOverlay(const Inventory& inv, Renderer& r, bool inGame) {
    gInGame = inGame;
    gfx::restore();
    float m[16] = {2.0f / gW, 0, 0, 0, 0, -2.0f / gH, 0, 0, 0, 0, -0.001f, 0, -1, 1, 0, 1};
    gfx::setMatrix(m); // window pixels
    if (!inGame) return;
    gfx::Texture* g = gfx::texture("images/gui/gui.png");
    float c = cellPx(), k = c / 26.0f;
    auto btn = [&](int col, int row, int u, int v, int w, int h, bool held) {
        float x = padX() + col * c + (c - w * k) / 2, y = padY() + row * c + (c - h * k) / 2;
        gfx::blit(g, x, y, w * k, h * k, u, v, w, h, held ? gfx::rgb(0xC0C0C0, 150) : gfx::rgb(0xFFFFFF, 120));
    };
    if (!gBottom) {
        btn(1, 0, 2, 109, 22, 22, gPadDir == 0);
        btn(0, 1, 28, 109, 22, 22, gPadDir == 1);
        btn(1, 2, 54, 109, 22, 22, gPadDir == 3);
        btn(2, 1, 80, 109, 22, 22, gPadDir == 2);
        btn(1, 1, 108, 111, 18, 18, gPadDir == 4);
        if (gPadDir == 0 || gPadDir == 5 || gPadDir == 6) {
            btn(0, 0, 6, 139, 18, 18, gPadDir == 5);
            btn(2, 0, 28, 139, 18, 18, gPadDir == 6);
        }
        // sneak toggle
        float sx = padX() + c * 2, sy = padY() - c * 0.75f;
        gfx::ninePatch(gfx::texture("images/gui/spritesheet.png"), sx, sy, c, c * 0.7f, gSneak ? 0 : 8, 32, 8, 8, 2);
        float fs = std::max(1.0f, c * 0.7f / 20.0f);
        (void)fs;
        // pause
        float ps = pauseSize();
        gfx::blit(g, pauseX(), 6, ps, ps, 200, 64, 18, 18, gfx::rgb(0xFFFFFF, 160));
        { // chat: the same square, a speech bubble on it
            float u = ps / 18.0f, cx = chatX();
            gfx::fill(cx + u, 6 + u, ps - 2 * u, ps - 2 * u, gfx::rgb(0x000000, 90));
            gfx::fill(cx, 6, ps, u, gfx::rgb(0xFFFFFF, 110));
            gfx::fill(cx, 6 + ps - u, ps, u, gfx::rgb(0xFFFFFF, 110));
            gfx::fill(cx, 6 + u, u, ps - 2 * u, gfx::rgb(0xFFFFFF, 110));
            gfx::fill(cx + ps - u, 6 + u, u, ps - 2 * u, gfx::rgb(0xFFFFFF, 110));
            uint32_t w = gfx::rgb(0xFFFFFF, 200);
            gfx::fill(cx + 4 * u, 6 + 4 * u, 10 * u, 7 * u, w);      // the bubble
            gfx::fill(cx + 5 * u, 6 + 11 * u, 2 * u, 2 * u, w);      // its tail
            gfx::fill(cx + 4 * u, 6 + 13 * u, 1 * u, 1 * u, w);
            uint32_t d = gfx::rgb(0x404040, 220);                     // the dots
            for (int i = 0; i < 3; ++i) gfx::fill(cx + (6 + i * 2.5f) * u, 6 + 7 * u, 1.5f * u, 1.5f * u, d);
        }
    }
    // the hotbar: PE's gui.png bar (182x22), the selection frame, the items, then "..."
    float s = guiS(), hx = hotbarX(), hy = hotbarY();
    gfx::blit(g, hx, hy, 182 * s, 22 * s, 0, 0, 182, 22);
    gfx::blit(g, hx + 182 * s, hy, 20 * s, 22 * s, 162, 0, 20, 22); // one more slot's frame for "..."
    int sel = inv.currentItem < 9 ? inv.currentItem : 0;
    gfx::blit(g, hx - s + sel * 20 * s, hy - s, 24 * s, 24 * s, 0, 22, 24, 24);
    C3D_Tex* atlas = r.terrainTexture();
    C3D_Tex* items = r.texture("romfs:/textures/gui/items.rgba");
    for (int i = 0; i < 9; ++i) {
        const ItemStack& st = inv.main[i];
        if (st.empty()) continue;
        float x = hx + (3 + i * 20) * s, y = hy + 3 * s, w = 16 * s;
        const ItemInfo* info = itemInfo(st.id, st.damage);
        auto put = [&](C3D_Tex* t, int tile, int texW, int texH, uint32_t rgb) {
            if (!t) return;
            float u0 = float(tile % 16 * 16) / texW, v0 = float(tile / 16 * 16) / texH;
            gfx::Vertex q[6];
            uint32_t col = gfx::rgb(rgb);
            float u1 = u0 + 16.0f / texW, v1 = v0 + 16.0f / texH;
            q[0] = {x, y, 0, u0, v0, col}; q[1] = {x + w, y, 0, u1, v0, col}; q[2] = {x + w, y + w, 0, u1, v1, col};
            q[3] = q[0]; q[4] = q[2]; q[5] = {x, y + w, 0, u0, v1, col};
            gfx::trianglesGL(t->id, q, 6);
        };
        if (st.id < 256) {
            int tile = info && info->icon >= 0 ? info->icon : 0;
            if (info && (info->flags & IF_BLOCK3D)) tile = BLOCKS[st.id].tex[st.damage & 15][2];
            else if (info && (info->flags & IF_TERRAIN2)) tile = 256 + (tile & 255);
            put(atlas, tile & 511, 256, 1024, 0xFFFFFF);
        } else if (info && info->icon >= 0) {
            put(items, info->icon & 255, 256, 256, info->colour);
            if (info->overlay >= 0) put(items, info->overlay & 255, 256, 256, info->overlayColour);
        }
        if (st.count > 1) {
            gfx::flush();
            float fm[16] = {2.0f * s / gW, 0, 0, 0, 0, -2.0f * s / gH, 0, 0, 0, 0, -0.001f, 0, -1, 1, 0, 1};
            gfx::setMatrix(fm);
            std::string n = std::to_string(st.count);
            font::draw(n, (x + w) / s - font::width(n) + 1, (y + w) / s - 7, 0xFFFFFF);
            gfx::setMatrix(m);
        }
    }
    {
        float fm[16] = {2.0f * s / gW, 0, 0, 0, 0, -2.0f * s / gH, 0, 0, 0, 0, -0.001f, 0, -1, 1, 0, 1};
        gfx::setMatrix(fm);
        font::drawCentered("...", (hx + 192 * s) / s, (hy + 6 * s) / s, 0xFFFFFF);
        if (!gBottom) { // the interact buttons (Pocket Edition 0.9+: Ride, Sit...)
            gfx::Texture* sheet = gfx::texture("images/gui/spritesheet.png");
            float bx, by, bw, bh;
            for (size_t i = 0; interactRect(i, bx, by, bw, bh); ++i) {
                gfx::setMatrix(m);
                gfx::ninePatch(sheet, bx, by, bw, bh, 8, 32, 8, 8, 2);
                gfx::setMatrix(fm);
                font::drawCentered(gInteract[i], (bx + bw / 2) / s, (by + bh / 2) / s - 4, 0xE0E0E0);
            }
        }
        if (!gBottom) {
            float c2 = c * 0.7f / s;
            font::drawCentered(gSneak ? "Sneaking" : "Sneak", (padX() + c * 2.5f) / s, (padY() - c * 0.75f) / s + c2 / 2 - 4,
                               gSneak ? 0xFFFFA0 : 0xE0E0E0);
        }
        gfx::setMatrix(m);
    }
    gfx::flush();
}

// ---------------------------------------------------------------- the title

namespace {
// first start (or a new APK): the game's own files out of the APK into files/romfs:/, where the
// shared code opens them with "romfs:/..." (a relative path: the game runs in the files folder)
bool extractRomfs(const std::function<void(float)>& progress) {
    std::vector<uint8_t> index;
    if (!gfx::readAsset("romfs_index.txt", index)) return false;
    std::string idx(index.begin(), index.end());
    std::string stamp = gFilesDir + "/romfs_index.txt";
    if (FILE* f = std::fopen(stamp.c_str(), "rb")) {
        std::string had;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) had.append(buf, n);
        std::fclose(f);
        if (had == idx) return true; // already out
    }
    std::vector<std::string> files;
    size_t p = 0;
    while (p < idx.size()) {
        size_t e = idx.find('\n', p);
        if (e == std::string::npos) e = idx.size();
        if (e > p) files.push_back(idx.substr(p, e - p));
        p = e + 1;
    }
    for (size_t i = 0; i < files.size(); ++i) {
        const std::string& rel = files[i];
        bool isWorld = rel.compare(0, 7, "worlds/") == 0;
        std::string dst = "romfs:/" + rel;
        for (size_t k = 1; k < dst.size(); ++k)
            if (dst[k] == '/') mkdir(dst.substr(0, k).c_str(), 0777);
        if (isWorld && access(dst.c_str(), F_OK) == 0) continue;
        std::vector<uint8_t> data;
        if (!gfx::readAsset(rel, data)) continue;
        if (FILE* f = std::fopen(dst.c_str(), "wb")) {
            std::fwrite(data.data(), 1, data.size(), f);
            std::fclose(f);
        }
        if (i % 40 == 0) progress(float(i) / float(files.size()));
    }
    if (FILE* f = std::fopen(stamp.c_str(), "wb")) {
        std::fwrite(idx.data(), 1, idx.size(), f);
        std::fclose(f);
    }
    return true;
}
} // namespace

void startGame() {
    if (gRunning) return;
    gRunning = true;
    std::thread([] {
        ensureSurface();
        if (gQuit) { gRunning = false; return; }
        chdir(gFilesDir.c_str());
        mkdir("sdmc:", 0777); // the 3DS paths ("sdmc:/...", "romfs:/...") are folders in here
        mkdir("romfs:", 0777);
        gfx::init(gAssetMgr);
        gfx::resize(gW, gH);
        font::load();
        extractRomfs([](float f) {
            gfx::beginFrame(0x000000);
            gfx::begin2D();
            float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
            gfx::Texture* bg = gfx::texture("images/gui/background.png");
            if (bg) gfx::quad(bg, 0, 0, w, h, 0, 0, w / 32.0f, h / 32.0f, gfx::rgb(0x404040));
            font::drawCentered("Preparing Minecraft Axeolon", w / 2, h / 2 - 16, 0xFFFFFF);
            gfx::fill(w / 2 - 50, h / 2, 100, 2, gfx::rgb(0x808080));
            gfx::fill(w / 2 - 50, h / 2, 100 * f, 2, gfx::rgb(0x80FF80));
            presentFrame();
        });
        axeolon_main();
        gRunning = false;
        LOG("game thread done");
    }).detach();
}

bool runTitle(TitleChoice& out) {
    gInGame = false;
    gBottom = false;
    ui::setChoice(nullptr);
    ui::setScreen(ui::makeTitle());
    auto last = Clock::now();
    ui::Touch t;
    int menuFinger = -1;
    while (aptMainLoop()) {
        std::vector<TouchEv> evs;
        int backs;
        {
            std::lock_guard<std::mutex> l(gMx);
            evs.swap(gEventsIn);
            backs = gBackPresses;
            gBackPresses = 0;
        }
        auto n = Clock::now();
        float dt = std::chrono::duration<float>(n - last).count();
        last = n;
        ui::tick(dt > 0.25f ? 0.25f : dt); // a new screen takes over here
        if (backs && ui::current()) ui::current()->back();
        // the menus follow one finger; every press and release reaches them, even a quick tap
        float s = float(gfx::guiScale());
        for (const TouchEv& e : evs) {
            if (e.type == TOUCH_DOWN && menuFinger < 0) {
                menuFinger = e.id;
                t.x = e.x / s; t.y = e.y / s; t.down = true; t.pressed = true; t.released = false;
            } else if (e.type == TOUCH_MOVE && e.id == menuFinger) {
                t.x = e.x / s; t.y = e.y / s; t.pressed = t.released = false;
            } else if ((e.type == TOUCH_UP && e.id == menuFinger) || e.type == TOUCH_CANCEL) {
                if (e.type == TOUCH_UP) { t.x = e.x / s; t.y = e.y / s; }
                menuFinger = -1;
                t.down = false; t.pressed = false; t.released = e.type == TOUCH_UP;
            } else {
                continue;
            }
            if (ui::current()) ui::current()->touch(t);
        }
        t.pressed = t.released = false;
        if (ui::choiceMade()) {
            const ui::Choice& c = ui::choice();
            if (c.quit) return false;
            out.world = c.world;
            out.created = c.created;
            out.gameType = c.gameType;
            gInGame = true;
            return true;
        }
        gfx::beginFrame(0x000000);
        gfx::begin2D();
        if (ui::current()) ui::current()->render(t);
        gfx::flush();
        presentFrame();
    }
    return false;
}

} // namespace platform

// ---------------------------------------------------------------- libctru stand-ins

u64 osGetTime() {
    return u64(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}
u64 svcGetSystemTick() {
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - platform::gStart).count();
    return u64(double(ns) * (double(SYSCLOCK_ARM11) / 1e9));
}
void svcSleepThread(s64 ns) { std::this_thread::sleep_for(std::chrono::nanoseconds(ns)); }

struct AxThread {
    std::thread t;
};
Thread threadCreate(ThreadFunc entry, void* arg, size_t, int, int, bool detached) {
    AxThread* t = new AxThread;
    t->t = std::thread(entry, arg);
    if (detached) t->t.detach();
    return t;
}
Result threadJoin(Thread t, u64) {
    if (t && t->t.joinable()) t->t.join();
    return 0;
}
void threadFree(Thread t) {
    if (t && t->t.joinable()) t->t.detach();
    delete t;
}
Result svcGetThreadId(u32* out, Handle) {
    *out = u32(std::hash<std::thread::id>()(std::this_thread::get_id()));
    return 0;
}
Result svcGetThreadPriority(s32* out, Handle) {
    *out = 0x30;
    return 0;
}

void LightLock_Init(LightLock* l) { l->impl = new std::mutex; }
void LightLock_Lock(LightLock* l) {
    if (!l->impl) l->impl = new std::mutex;
    static_cast<std::mutex*>(l->impl)->lock();
}
void LightLock_Unlock(LightLock* l) { static_cast<std::mutex*>(l->impl)->unlock(); }

namespace {
struct Ev {
    std::mutex m;
    std::condition_variable cv;
    bool set = false;
    ResetType type = RESET_ONESHOT;
};
} // namespace
void LightEvent_Init(LightEvent* e, ResetType t) {
    Ev* ev = new Ev;
    ev->type = t;
    e->impl = ev;
}
void LightEvent_Signal(LightEvent* e) {
    Ev* ev = static_cast<Ev*>(e->impl);
    std::lock_guard<std::mutex> l(ev->m);
    ev->set = true;
    ev->cv.notify_all();
}
void LightEvent_Wait(LightEvent* e) {
    Ev* ev = static_cast<Ev*>(e->impl);
    std::unique_lock<std::mutex> l(ev->m);
    ev->cv.wait(l, [&] { return ev->set; });
    if (ev->type != RESET_STICKY) ev->set = false;
}

void* linearAlloc(size_t size) {
    size_t* p = static_cast<size_t*>(std::malloc(size + 16));
    if (!p) return nullptr;
    p[0] = size;
    return reinterpret_cast<uint8_t*>(p) + 16;
}
void linearFree(void* p) {
    if (p) std::free(static_cast<uint8_t*>(p) - 16);
}
u32 linearGetSize(void* p) { return p ? u32(*reinterpret_cast<size_t*>(static_cast<uint8_t*>(p) - 16)) : 0; }

bool aptMainLoop() {
    if (platform::gQuit) return false;
    platform::ensureSurface();
    return !platform::gQuit;
}
void gspWaitForVBlank() { std::this_thread::sleep_for(std::chrono::milliseconds(16)); }
float osGet3DSliderState() { return 0.0f; }

void hidScanInput() {
    platform::scanControls();
}
u32 hidKeysDown() { return platform::gDown; }
u32 hidKeysHeld() { return platform::gHeld; }
u32 hidKeysUp() { return platform::gUp; }
void hidTouchRead(touchPosition* t) { *t = platform::gTouch; }
void hidCircleRead(circlePosition* p) { *p = platform::gPad; }
void hidCstickRead(circlePosition* p) { p->dx = p->dy = 0; } // looking is the drag (applyLook)

// GuiEditSign on Android: the system keyboard types into a box over the game; Done (or the 5th
// Enter) finishes, Back cancels
SwkbdButton swkbdInputText(SwkbdState* st, char* buf, size_t size) {
    using namespace platform;
    {
        std::lock_guard<std::mutex> l(gMx);
        gTyped.clear();
        gBackPresses = 0;
    }
    gTyping = true;
    SwkbdButton result = SWKBD_BUTTON_NONE;
    bool wasDown = false;
    while (aptMainLoop()) {
        std::string text;
        int backs;
        std::vector<Finger> fingers;
        bool tapped = false;
        {
            std::lock_guard<std::mutex> l(gMx);
            text = gTyped;
            backs = gBackPresses;
            gBackPresses = 0;
            fingers = gFingersIn;
            for (const TouchEv& e : gEventsIn) tapped |= e.type == TOUCH_UP;
            gEventsIn.clear();
        }
        int lines = 1;
        for (char c : text) lines += c == '\n';
        if (backs) break;
        float s = float(gfx::guiScale());
        float w = float(gfx::guiWidth());
        bool down = !fingers.empty();
        // Done: under the box
        float bx = w / 2 - 50, by = 112;
        if (tapped) result = SWKBD_BUTTON_CONFIRM; // a tap on the screen (the Done button) ends it
        wasDown = down;
        if (result == SWKBD_BUTTON_CONFIRM || lines > 4) {
            result = SWKBD_BUTTON_CONFIRM;
            break;
        }
        gfx::beginFrame(0x000000);
        gfx::begin2D();
        float h = float(gfx::guiHeight());
        gfx::fill(0, 0, w, h, gfx::rgb(0x101010));
        font::drawCentered(st->hint && *st->hint ? st->hint : "Edit sign message", w / 2, 16, 0xFFFFFF);
        gfx::fill(w / 2 - 70, 34, 140, 70, gfx::rgb(0x6B5433));
        int line = 0;
        std::string cur;
        for (size_t i = 0; i <= text.size(); ++i) {
            if (i == text.size() || text[i] == '\n') {
                font::drawCentered(cur + (i == text.size() ? "_" : ""), w / 2, 40 + line * 14.0f, 0x000000, false);
                cur.clear();
                ++line;
            } else {
                cur += text[i];
            }
        }
        gfx::ninePatch(gfx::texture("images/gui/spritesheet.png"), bx, by, 100, 24, 8, 32, 8, 8, 2);
        font::drawCentered("Done", w / 2, by + 8, 0xE0E0E0);
        (void)s;
        presentFrame();
    }
    gTyping = false;
    std::string t;
    {
        std::lock_guard<std::mutex> l(gMx);
        t = gTyped;
    }
    std::snprintf(buf, size, "%s", t.c_str());
    return result;
}
