#pragma once
// The Android side of the 3DS shims: one game thread runs the same main loop as the 3DS (source/
// main.cpp) while the activity thread feeds it the window and the fingers.
//
// Screen layout: the 3D view fills the window. The 3DS top screen's 2D (HUD, menus) is laid out in
// its 400x240 units, scaled to the window's height and centred. The bottom screen (inventory,
// Almanac, map, menu) is a 320x240 panel shown over the game when it's open. Pocket Edition's touch
// controls, hotbar and buttons are drawn over everything in GUI units.
#include <string>
#include <vector>

struct ANativeWindow;
struct AAssetManager;
class Inventory;
class Renderer;

namespace platform {

// ---- from the activity thread ----
void setWindow(ANativeWindow* w);          // null: the window is going away (waits for the game thread)
void setAssets(AAssetManager* a, const std::string& filesDir, float dpi);
struct Finger { int id; float x, y; };     // window pixels
enum TouchType { TOUCH_DOWN, TOUCH_MOVE, TOUCH_UP, TOUCH_CANCEL };
// every touch event, in order (a quick tap's down and up can come between two frames)
void touchEvent(TouchType type, int id, float x, float y);
void backPressed();
// typing (sign text): the activity thread passes the keyboard's characters while it's up
bool typingWanted();          // the game is waiting for text: show the keyboard
void typeChar(unsigned c);    // a character ('\n' for Enter, 8 for backspace)
void requestQuit();                        // the activity is being destroyed: save and stop
void requestSave();                        // the app is going to the background
bool takeSaveRequest();
void startGame();                          // the game thread
bool gameRunning();

// ---- for the game thread ----
int windowWidth();
int windowHeight();
float topAspect();     // the 3D view's aspect (the window's)
int guiScale();        // Pocket Edition's GUI scale for this window
bool presentFrame();   // swaps; false if the window is gone (the frame is dropped)
void ensureSurface();  // waits for a window and (re)makes the EGL surface

// where 2D goes: 0 = the top screen space (400x240 fitted to the window), 1 = the bottom panel
void beginScreen(int screen);
bool bottomVisible();
void showBottom(bool on);

// the game loop's hooks (source/main.cpp, #ifdef __ANDROID__)
void applyLook(float& yawDeg, float& pitchDeg);     // the look finger's drag since last frame
// where taps and held fingers aim (Pocket Edition has no crosshair): -1..1 across the 3D view, y up
void aimPoint(float& nx, float& ny);
float topLeft();       // the window's left and right edges in the top screen's 400x240 units
float topRight();
int takeHotbarTap();                                 // a tapped hotbar slot, or -1
bool takeInventoryToggle();                          // the "..." button
bool takeChatToggle();                               // the chat button (top middle)
int takeMenuTap(int buttons, float firstY, float stepY); // a tapped 3DS menu button (pause, game over), or -1
void drawOverlay(const Inventory& inv, Renderer& r, bool inGame); // Pocket Edition controls + hotbar
// the interact buttons over the hotbar (Ride, Sit, Feed...): set each frame, empty for none
void setInteractButtons(const std::vector<std::string>& labels);
int takeInteractTap();                               // the tapped one, or -1

// the menus before a world (android/src/Screens.cpp): fills the choice, false = quit
struct TitleChoice { std::string world; bool created = false; int gameType = 0; };
bool runTitle(TitleChoice& out);

} // namespace platform
