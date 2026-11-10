#pragma once
// The title screen and world menus, ported from Axeolon's GuiMainMenu, GuiSelectWorld,
// GuiCreateWorld, GuiDeleteWorld and GuiYesNo.
//
// Top screen: the rotating Axeolon panorama, the 3D stone "MINECRAFT" logo with the AXEOLON
// subtitle falling into place, the splash text. Bottom screen: the menu buttons (touch, or
// D-pad + A, B to go back).
#include <3ds.h>
#include <citro2d.h>
#include <string>
#include <vector>
#include "model/AxmModel.h"
#include "ui/Font.h"

class Renderer;

class TitleScreen {
public:
    enum Result { NONE, PLAY, QUIT };

    bool init(Renderer& r, const std::string& savesDir);
    void shutdown();
    // 20 times a second
    void tick();
    // Reads input; PLAY means world() is chosen (creating it first if createdNew())
    Result input(u32 down, const touchPosition& touch);
    void draw(float partial); // inside a frame: top (3D + 2D) and bottom
    // LoadingScreenRenderer: dirt, a title, a status line and the green progress bar (0..1, <0 = none)
    void drawLoading(const std::string& title, const std::string& status, float progress);

    std::string world() const { return chosen; }
    bool createdNew() const { return newWorld; }
    int gameType() const { return chosenGameType; }

    // World creation needs the terrain generator; until it is ported the button is disabled
    static bool canCreateWorlds;

private:
    enum Screen { MAIN, SELECT, CREATE, DELETE, CONFIRM, OPTIONS, CONTROLS };
    struct Button {
        int id;
        float x, y, w, h;
        std::string text;
        bool enabled = true;
    };
    struct Slot {
        bool exists = false;
        bool unreadable = false;
        float mb = 0;
        int gameType = 0;
    };

    Renderer* renderer = nullptr;
    std::string savesDir;
    C3D_RenderTarget* bottom = nullptr;
    Font font;
    C3D_Tex* panorama[6] = {};
    C3D_Tex* black = nullptr;
    C3D_Tex* gui = nullptr;
    C3D_Tex* background = nullptr;
    Tex3DS_SubTexture panoSub, bgSub;

    Screen screen = MAIN;
    std::vector<Button> buttons;
    int selected = 0;
    Slot slots[5];
    int pendingSlot = 0;
    int createGameType = 0;
    std::string chosen;
    bool newWorld = false;
    int chosenGameType = 0;

    std::string splash = "missingno";
    int panoramaTimer = 0;
    struct Fall { double now, prev, speed; };
    std::vector<Fall> logoFx, subFx;
    uint32_t rng = 0x2545F491;

    std::vector<axm::ModelVertex> tris;

    float randf();
    void open(Screen s);
    void nudge(const Button& b, int dir);
    void scanSlots();
    void press(const Button& b);
    void drawPanorama(float partial);
    void drawLogo(float partial);
    void drawTop2D();
    void drawBottom();
    void drawButton(const Button& b, bool hover);
    bool deleteWorld(int slot);
};
