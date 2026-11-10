#pragma once
// The title screen and world menus.
//
// Main menu: Pocket Edition 0.5.0's, from PE's own textures (romfs:/textures/pe). Top screen: the
// dirt background, the Pocket Edition logo and the version line. Bottom screen: the three square
// touchgui.png buttons (Join Game, Start Game, Options) that light up while held and act on release,
// like PE. D-pad left/right + A works too; START quits.
// The other screens (world select, create, delete, options) are still Axeolon's Java-style ones.
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
        int icon = -1; // a PE square button: row of touchgui.png (0 Options, 1 Start Game, 2 Join Game)
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
    C3D_Tex* peTouch = nullptr; // PE touchgui.png
    C3D_Tex* peLogo = nullptr;  // PE title.png
    C3D_Tex* peDirt = nullptr;  // PE bg32.png
    int held = -1;              // the PE button under a held touch
    bool padUsed = false;       // the D-pad picked a button: show it lit
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
    void drawPeButton(const Button& b, bool lit);
    void drawPeDirt(float w, float h);
    void drawPeTop();
    bool deleteWorld(int slot);
};
