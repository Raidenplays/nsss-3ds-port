#pragma once
// Bottom (touch) screen, Pocket Edition style (PE 0.5.0's own textures, romfs:/textures/pe):
//   Map (home) — the New 3DS Edition layout: PE hotbar along the top, the coordinates, the map on
//                the left, and Inventory / Crafting / Pause buttons on the right
//   Items      — inventory, chests, furnaces, crafting (MATTIS), under a PE header with Craft and X
//   Menu       — the pause menu (options, save and quit), under a PE header with X
//   (in creative, Items opens on GuiBuild's block picker)
//
// Old 3DS budget: only redrawn when something changes (markDirty), never
// every frame. The bottom screen is single-buffered so the last image stays up.
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include "game/Inventory.h"
#include "game/Minimap.h"
#include "game/RecipeBook.h"
#include <string>
#include <vector>

class Renderer;
struct Camera;

struct FrameStats {
    int fps = 0;
    int sections = 0;
    int triangles = 0;
    unsigned gpuKB = 0;
    float lightMs = 0;
};

class BottomScreen {
public:
    enum class Tab { Map, Items, Menu };

    bool init(Renderer& renderer, Camera& camera, Inventory& inventory, Minimap& map);
    void shutdown();

    // Touch + buttons. Call every frame before drawing.
    void handleInput(u32 down, u32 held, const touchPosition& touch);

    // Per-frame bookkeeping (map refresh throttling, stats). Call outside a frame.
    void update(float dt, const FrameStats& stats);

    // Draws only if dirty. Call inside a frame after Renderer::prepare2D().
    void draw();

    void markDirty() { dirty = true; }
    void setTab(Tab t);
    Tab tab() const { return current; }
    bool quitRequested() const { return quit; }

private:
    Renderer* renderer = nullptr;
    Camera* camera = nullptr;
    Inventory* inv = nullptr;
    Minimap* map = nullptr;

    C3D_RenderTarget* target = nullptr;
    C2D_TextBuf textBuf = nullptr;
    C2D_TextBuf measureBuf = nullptr; // scratch for measuring words while wrapping
    Tex3DS_SubTexture tileSub[256];  // terrain.png's tiles
    Tex3DS_SubTexture itemSub[256];  // gui/items.png
    C3D_Tex* itemsTex = nullptr;
    C3D_Tex* peSheet = nullptr; // PE spritesheet.png (128x128)
    C3D_Tex* peGui = nullptr;   // PE gui.png (256x256)
    // a PE nine-patch: the (sx,sy,sw,sh) sprite with a 'border'-texel frame, at 2x
    void ninePatch(C3D_Tex* tex, float tw, float th, int sx, int sy, int sw, int sh, int border, float x, float y,
                   float w, float h, float scale = 2.0f);
    void sprite(C3D_Tex* tex, float tw, float th, int sx, int sy, int sw, int sh, float x, float y, float scale);
    void peButton(float x, float y, float w, float h, const char* label);
    void drawPeHeader(const char* title, bool craftButton);
    void drawHome();
    void touchHome(int x, int y);
    C3D_Tex mapTex;
    Tex3DS_SubTexture mapSub;
    u32* mapStaging = nullptr;

    Tab current = Tab::Map;
    int mapZoom = 1;
    bool dirty = true;
    bool quit = false;
    float mapTimer = 0, statsTimer = 0;
    FrameStats stats;
    int cursorX = 160, cursorY = 120;

    // MATTIS, Pocket Edition's crafting list (Options craftStyle 1)
    bool mattisOpen = false;
    int mattisCat = 0;
    int mattisScroll = 0;              // pixels down the list
    int mattisSelId = -1, mattisSelDamage = 0; // the picked result (kept across list rebuilds)
    bool isPicked(const recipes::Entry& e) const;
    void pick(const recipes::Entry& e);
    std::vector<recipes::Entry> mattisList;
    bool dragging = false, dragMoved = false;
    int dragY0 = 0, dragScroll0 = 0;
    int* dragTarget = nullptr; // what the drag scrolls (the MATTIS list)
    int dragMax = 0;
    bool mattisShowing() const;
    void openMattis(bool on);
    void returnCraftGrid();            // the 2x2 grid's items go back to the inventory
    void drawMattis();
    // GuiBuild (creative): the block picker, page = buildMenuTier
    bool buildOwnPage = false;   // the normal Items page instead (armour, crafting)
    int buildTier = 0;
    bool buildShowing() const;
    void drawBuild();
    void touchBuild(int x, int y, bool alt);
    void touchMattis(int x, int y, bool alt);
    const recipes::Entry* mattisSelected() const;
    std::vector<std::string> wrap(const std::string& s, float width, float scale);

    void uploadMap();
    // drawing helpers
    void text(const char* s, float x, float y, float scale, u32 colour, u32 flags = 0);
    void bevel(float x, float y, float w, float h, bool raised, u32 face);
    void slotFrame(float x, float y);
    void itemIcon(const ItemStack& st, float x, float y, float scale = 1.5f);
    void button(float x, float y, float w, float h, const char* label);

    void drawHotbar();
    void drawMapTab();
    void drawItemsTab();
    void drawMenuTab();

    void touchItemsTab(int x, int y, bool alt);
    void touchMenuTab(int x, int y);
    bool touchHotbar(int x, int y, bool alt);
};
