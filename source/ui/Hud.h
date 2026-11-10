#pragma once
// GuiIngame's status icons on the top screen (hearts and air bubbles top left, armour top right)
// and GuiGameOver. The hotbar lives on the bottom screen.
#include <citro2d.h>
#include <string>
#include <vector>
#include <cstdint>
#include "Font.h"
#include "world/JavaRandom.h"

class Renderer;
class Player;

class Hud {
public:
    void load(Renderer& r);
    // updateCounter: GuiIngame's tick counter (seeds the low-health shake)
    void draw(const Player& p, bool inWater, int updateCounter, bool survival);
    // GuiGameOver: selected 0 = Respawn, 1 = Title menu; buttons wake up after 20 ticks
    void drawGameOver(int selected, bool enabled, int score);
    // GuiIngame's vignette (fancy graphics: the edges darken where it's dark; eases in slowly) and
    // ItemRenderer.renderOverlays (under water, on fire)
    void drawOverlays(float brightness, bool underwater, bool burning, float yaw, float pitch, Renderer& r);
    // GuiIngameMenu: 0 back to game, 1 save game, 2 save and quit to title
    void drawPause(int selected);
    // The corner "saving" indicator consoles show while they write the save
    void drawSaving(int frame);
    // A line of text in the middle of the screen that fades (autosave, tips)
    void drawToast(const std::string& s, float alpha);
    const Font& textFont() const { return font; }
    // GuiIngame.addChatMessage: lines stack at the bottom left and fade after 10 seconds
    void addChat(const std::string& s);
    void setRecordPlaying(const std::string& s); // GuiIngame.func_553_b: "Now playing: ..." for 3 seconds
    void drawRecordPlaying(float partial);
    void tickChat();
    void drawChat();
    bool chatOpen = false; // GuiChat: every line, not fading (the chat button on Android)

    std::string recordText;
    int recordTicks = 0;
private:
    C3D_Tex* icons = nullptr;   // /gui/icons.png
    C3D_Tex* gui = nullptr;     // /gui/gui.png (buttons)
    C3D_Tex* vignette = nullptr, *water = nullptr;
    C3D_Tex* items = nullptr;   // /gui/items.png
    Renderer* renderer = nullptr;
    void itemIcon(int id, int damage, float x, float y);
    struct ChatLine { std::string text; int age; };
    std::vector<ChatLine> chat;
    float vignetteLevel = 1.0f; // GuiIngame.field_931_c
    Font font;
    JavaRandom rand{0};
    void icon(C3D_Tex* t, float x, float y, int u, int v, int w = 9, int h = 9, bool flip = false);
    void button(float x, float y, float w, const char* label, bool enabled, bool hover);
};
