#include "BottomScreen.h"
#include "game/Options.h"
#include "audio/Sound.h"
#include <algorithm>
#include "game/ItemTable.h"
#include "game/BuildMenu.h"
#include "game/Survival.h"
#include "world/BlockTable.h"
#include "render/Renderer.h"
#include "client/Camera.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// ---- layout (320 x 240) ----
constexpr int TAB_H = 22;
constexpr int SLOT = 32;           // slot pitch
constexpr int SLOT_IN = 30;        // slot drawn size
constexpr int HOTBAR_X = 16, HOTBAR_Y = 208;

// Items tab
constexpr int ARMOUR_X = 16, CRAFT_X = 184, RESULT_X = 280, TOP_ROW_Y = 36;
constexpr int GRID_X = 16, GRID_Y = 108;
constexpr int TSLOT = 24, TABLE_X = 120, TABLE_Y = 28; // crafting table grid (smaller slots)
constexpr int CSLOT = 20, CONT_X = 70; // chests: 9 columns of 20 px (a large chest needs 6 rows + yours)

// Map tab
constexpr int MAP_X = 8, MAP_Y = 68, MAP_PX = 168; // the home view's map (New 3DS Edition layout)
constexpr float HOME_SLOT = 32, HOME_SLOT_PITCH = 34.5f, HOME_SLOT_Y = 5; // nine slots: 2..316 px
constexpr float HOME_SLOT_X = (320 - 8 * HOME_SLOT_PITCH - HOME_SLOT) / 2;
constexpr int HOME_BTN_X = 188, HOME_BTN_W = 124, HOME_BTN_H = 52, HOME_BTN_Y0 = 68, HOME_BTN_STEP = 58;
constexpr int PE_X_BTN = 286; // the header's close button (spritesheet's 18x18 X at 1.75x)
constexpr int MENU_Y0 = 25, MENU_ROW = 22, MENU_H = 20; // the Menu tab's rows
// MATTIS: category buttons down the left, the recipe list, then the picked recipe
constexpr int MT_CAT_X = 2, MT_CAT_W = 44, MT_CAT_H = 43, MT_CAT_STEP = 45;
constexpr int MT_LIST_X = 48, MT_LIST_W = 148, MT_LIST_Y = TAB_H + 2, MT_ROW = 18, MT_LIST_H = 180;
constexpr int MT_PANEL_X = 202;
constexpr int MT_CRAFT_X = 238, MT_CRAFT_Y = 42, MT_CRAFT_W = 78, MT_CRAFT_H = 26;
constexpr float MAP_SCALE = MAP_PX / float(Minimap::SIZE);

// Beta inventory GUI colours
inline u32 rgb(u32 c, u8 a = 255) { return C2D_Color32((c >> 16) & 255, (c >> 8) & 255, c & 255, a); }
const u32 GUI_BG = 0xC6C6C6, SLOT_BG = 0x8B8B8B, DARK = 0x373737, LIGHT = 0xFFFFFF;
const u32 PARCH = 0xD8C8A0, INK = 0x3F2A14;

#define MAP_TRANSFER_FLAGS \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

inline bool in(int x, int y, int rx, int ry, int rw, int rh) { return x >= rx && y >= ry && x < rx + rw && y < ry + rh; }

} // namespace

bool BottomScreen::init(Renderer& r, Camera& c, Inventory& i, Minimap& m) {
    renderer = &r; camera = &c; inv = &i; map = &m;

    gfxSetDoubleBuffering(GFX_BOTTOM, false); // keep last image when we skip redraws
    target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    textBuf = C2D_TextBufNew(3072);
    measureBuf = C2D_TextBufNew(256);
    if (!target || !textBuf || !measureBuf) return false;

    // One sub-texture per atlas tile (terrain.png then terrain2.png), and per gui/items.png icon
    for (int t = 0; t < 512; t++) {
        int tx = t & 15, ty = t >> 4;
        tileSub[t] = {16, 16, tx / 16.0f, 1.0f - ty / 64.0f, (tx + 1) / 16.0f, 1.0f - (ty + 1) / 64.0f};
    }
    for (int t = 0; t < 256; t++) {
        int tx = t & 15, ty = t >> 4;
        itemSub[t] = {16, 16, tx / 16.0f, 1.0f - ty / 16.0f, (tx + 1) / 16.0f, 1.0f - (ty + 1) / 16.0f};
    }
    itemsTex = r.texture("romfs:/textures/gui/items.rgba");
    peSheet = r.texture("romfs:/textures/pe/spritesheet.rgba");
    peGui = r.texture("romfs:/textures/pe/gui.rgba");

    const int n = Minimap::SIZE;
    if (!C3D_TexInit(&mapTex, n, n, GPU_RGBA8)) return false;
    C3D_TexSetFilter(&mapTex, GPU_NEAREST, GPU_NEAREST);
    // parchment until the first paint (linear memory comes back with whatever was there before)
    for (u32 i = 0; i < u32(n) * n; i++) ((u32*)mapTex.data)[i] = 0xD9C79AFF;
    C3D_TexFlush(&mapTex);
    mapSub = {(u16)n, (u16)n, 0.0f, 1.0f, 1.0f, 0.0f};
    mapStaging = (u32*)linearAlloc(n * n * 4);
    return mapStaging != nullptr;
}

void BottomScreen::shutdown() {
    if (mapStaging) linearFree(mapStaging);
    C3D_TexDelete(&mapTex);
    if (textBuf) C2D_TextBufDelete(textBuf);
    if (measureBuf) C2D_TextBufDelete(measureBuf);
}

void BottomScreen::setTab(Tab t) {
    if (t != current && current == Tab::Items) { // like closing GuiInventory / GuiCrafting
        inv->returnCursor();
        if (inv->tableOpen) inv->closeTable();
        mattisOpen = false;
        dragging = false;
    }
    current = t;
    dirty = true;
}

// --------------------------------------------------------------------------
// update / input
// --------------------------------------------------------------------------

void BottomScreen::uploadMap() {
    const int n = Minimap::SIZE;
    memcpy(mapStaging, map->pixels(), n * n * 4);
    GSPGPU_FlushDataCache(mapStaging, n * n * 4);
    C3D_SyncDisplayTransfer(mapStaging, GX_BUFFER_DIM(n, n), (u32*)mapTex.data, GX_BUFFER_DIM(n, n),
                            MAP_TRANSFER_FLAGS);
}

void BottomScreen::update(float dt, const FrameStats& s) {
    // Map: track exploration every frame (cheap); repaint+upload at most
    // 4x/sec and only while the Map tab is showing
    map->track((int)std::floor(camera->x), (int)std::floor(camera->z));
    mapTimer += dt;
    if (current == Tab::Map && mapTimer >= 0.25f) {
        mapTimer = 0;
        if (map->refresh(mapZoom)) {
            uploadMap();
            dirty = true;
        } else {
            static float lastYaw = 0;
            if (std::fabs(camera->yaw - lastYaw) > 0.05f) { lastYaw = camera->yaw; dirty = true; } // arrow
        }
    }
    // Menu stats refresh twice a second
    statsTimer += dt;
    if (current == Tab::Menu && statsTimer >= 0.5f) {
        statsTimer = 0;
        stats = s;
        dirty = true;
    }
}

void BottomScreen::handleInput(u32 down, u32 held, const touchPosition& touch) {
    if (down & KEY_START) setTab(current == Tab::Menu ? Tab::Map : Tab::Menu);
    // dragging the MATTIS list up and down
    if (dragging) {
        if (!(held & KEY_TOUCH)) {
            dragging = false;
            dirty = true;
        } else if (!(down & KEY_TOUCH)) {
            int dy = dragY0 - int(touch.py);
            if (dy > 4 || dy < -4) dragMoved = true;
            if (dragMoved && dragTarget) {
                int sc = std::min(dragMax, std::max(0, dragScroll0 + dy));
                if (sc != *dragTarget) { *dragTarget = sc; dirty = true; }
            }
            return;
        }
    }
    if (!(down & KEY_TOUCH)) return;

    int x = touch.px, y = touch.py;
    cursorX = x; cursorY = y;
    // Holding L or R while tapping = Beta's right-click (split / place one)
    bool alt = (held & (KEY_L | KEY_R)) != 0;
    dirty = true;

    if (current == Tab::Map) { touchHome(x, y); return; }
    if (y < TAB_H) { // the PE header: X closes back to the home view, Craft opens PE crafting
        if (x >= PE_X_BTN) setTab(Tab::Map);
        else if (current == Tab::Items && x < 74 && buildShowing()) buildOwnPage = true;
        else if (current == Tab::Items && x < 74 && inv->creative && buildOwnPage && !mattisShowing() &&
                 inv->containerKind == Inventory::NONE && !inv->tableOpen) buildOwnPage = false;
        else if (current == Tab::Items && x < 74 && gOptions.craftStyle == 1 && inv->containerKind == Inventory::NONE)
            openMattis(!mattisShowing());
        sound::playUi("random.click", 1.0f, 1.0f);
        return;
    }
    if (touchHotbar(x, y, alt)) return;

    switch (current) {
    case Tab::Map: break;
    case Tab::Items:
        if (mattisShowing()) touchMattis(x, y, alt);
        else if (buildShowing()) touchBuild(x, y, alt);
        else touchItemsTab(x, y, alt);
        break;
    case Tab::Menu: touchMenuTab(x, y); break;
    }
}

bool BottomScreen::touchHotbar(int x, int y, bool alt) {
    if (!in(x, y, HOTBAR_X, HOTBAR_Y, 9 * SLOT, SLOT_IN)) return false;
    int i = (x - HOTBAR_X) / SLOT;
    if (current == Tab::Items && !mattisShowing()) { // move items, like the inventory's bottom row
        if (alt) inv->rightClickSlot(SlotGroup::Main, i);
        else inv->clickSlot(SlotGroup::Main, i);
    } else {
        inv->currentItem = i;
    }
    return true;
}

void BottomScreen::touchItemsTab(int x, int y, bool alt) {
    auto click = [&](SlotGroup g, int i) {
        alt ? inv->rightClickSlot(g, i) : inv->clickSlot(g, i);
        inv->updateCraftResult();
    };
    if (inv->containerKind == Inventory::CHEST) {
        int rows = int(inv->container.size()) / 9;
        int top = rows > 3 ? 24 : 34;
        int py = top + rows * CSLOT + (rows > 3 ? 2 : 10);
        for (int i = 0; i < int(inv->container.size()); i++)
            if (in(x, y, CONT_X + (i % 9) * CSLOT, top + (i / 9) * CSLOT, CSLOT - 2, CSLOT - 2)) return click(SlotGroup::Container, i);
        for (int i = 0; i < 27; i++)
            if (in(x, y, CONT_X + (i % 9) * CSLOT, py + (i / 9) * CSLOT, CSLOT - 2, CSLOT - 2)) return click(SlotGroup::Main, 9 + i);
        return;
    }
    if (inv->containerKind == Inventory::FURNACE) {
        if (in(x, y, 110, 30, SLOT_IN, SLOT_IN)) return click(SlotGroup::Container, 0);
        if (in(x, y, 110, 74, SLOT_IN, SLOT_IN)) return click(SlotGroup::Container, 1);
        if (in(x, y, 200, 52, SLOT_IN, SLOT_IN)) return click(SlotGroup::Container, 2);
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 9; col++)
                if (in(x, y, GRID_X + col * SLOT, GRID_Y + row * SLOT, SLOT_IN, SLOT_IN))
                    return click(SlotGroup::Main, 9 + row * 9 + col);
        return;
    }
    if (inv->tableOpen) { // crafting table: 3x3 grid on the left, the result on the right
        for (int i = 0; i < 9; i++)
            if (in(x, y, TABLE_X + (i % 3) * TSLOT, TABLE_Y + (i / 3) * TSLOT, TSLOT - 2, TSLOT - 2)) return click(SlotGroup::Table, i);
        if (in(x, y, RESULT_X, TOP_ROW_Y + SLOT / 2, SLOT_IN, SLOT_IN)) return click(SlotGroup::TableResult, 0);
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 9; col++)
                if (in(x, y, GRID_X + col * SLOT, GRID_Y + row * SLOT, SLOT_IN, SLOT_IN))
                    return click(SlotGroup::Main, 9 + row * 9 + col);
        return;
    }
    if (gOptions.craftStyle == 1) { // Pocket: one button instead of the 2x2 grid
        if (in(x, y, CRAFT_X, TOP_ROW_Y, RESULT_X + SLOT_IN - CRAFT_X, SLOT + SLOT_IN)) {
            if (!inv->cursor.empty()) return;
            openMattis(true);
            sound::playUi("random.click", 1.0f, 1.0f);
            return;
        }
    }
    for (int i = 0; i < 4; i++) {
        int cx = (i & 1) * SLOT, cy = (i >> 1) * SLOT;
        if (in(x, y, ARMOUR_X + cx, TOP_ROW_Y + cy, SLOT_IN, SLOT_IN)) return click(SlotGroup::Armour, 3 - i);
        if (gOptions.craftStyle == 0 && in(x, y, CRAFT_X + cx, TOP_ROW_Y + cy, SLOT_IN, SLOT_IN)) return click(SlotGroup::Craft, i);
    }
    if (gOptions.craftStyle == 0 && in(x, y, RESULT_X, TOP_ROW_Y + SLOT / 2, SLOT_IN, SLOT_IN)) return click(SlotGroup::CraftResult, 0);
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 9; col++)
            if (in(x, y, GRID_X + col * SLOT, GRID_Y + row * SLOT, SLOT_IN, SLOT_IN))
                return click(SlotGroup::Main, 9 + row * 9 + col);
}

void BottomScreen::touchMenuTab(int x, int y) {
    // GuiOptions on the touch screen: [-] value [+] rows, then switches
    Options& o = gOptions;
    auto step = [](float v, float d) { v += d; return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; };
    bool changed = true;
    if (in(x, y, 40, MENU_Y0, 118, MENU_H)) { setTab(Tab::Map); return; }
    if (in(x, y, 162, MENU_Y0, 118, MENU_H)) {
        o.craftStyle ^= 1;
        if (o.craftStyle == 1) returnCraftGrid();
        o.save();
        sound::playUi("random.click", 1.0f, 1.0f);
        return;
    }
    int row = (y - MENU_Y0) / MENU_ROW; // rows 1..4: sliders
    bool minus = in(x, y, 40, MENU_Y0 + row * MENU_ROW, 34, MENU_H), plus = in(x, y, 246, MENU_Y0 + row * MENU_ROW, 34, MENU_H);
    if ((minus || plus) && row >= 1 && row <= 4) {
        float d = plus ? 0.1f : -0.1f;
        if (row == 1) o.music = step(o.music, d);
        if (row == 2) o.sound = step(o.sound, d);
        if (row == 3) o.sensitivity = step(o.sensitivity, d);
        if (row == 4) {
            if (plus && renderer->renderDistance < 128) renderer->renderDistance += 8;
            if (minus && renderer->renderDistance > 24) renderer->renderDistance -= 8;
            o.renderDistance = int(renderer->renderDistance);
        }
        sound::setVolumes(o.sound, o.music);
    } else if (in(x, y, 40, MENU_Y0 + 5 * MENU_ROW, 118, MENU_H)) {
        o.difficulty = (o.difficulty + 1) & 3;
    } else if (in(x, y, 162, MENU_Y0 + 5 * MENU_ROW, 118, MENU_H)) {
        o.bobView = !o.bobView;
    } else if (in(x, y, 40, MENU_Y0 + 6 * MENU_ROW, 118, MENU_H)) {
        o.invertY = !o.invertY;
    } else if (in(x, y, 40, MENU_Y0 + 7 * MENU_ROW, 240, MENU_H)) {
        quit = true;
        changed = false;
    } else if (in(x, y, 0, 0, 320, 0) || in(x, y, 290, MENU_Y0 + 7 * MENU_ROW, 30, MENU_H)) {
        o.showStats = !o.showStats;
    } else {
        changed = false;
    }
    if (changed) {
        o.save();
        sound::playUi("random.click", 1.0f, 1.0f);
    }
}

// --------------------------------------------------------------------------
// drawing
// --------------------------------------------------------------------------

void BottomScreen::text(const char* s, float x, float y, float scale, u32 colour, u32 flags) {
    C2D_Text t;
    C2D_TextParse(&t, textBuf, s);
    C2D_TextOptimize(&t);
    C2D_DrawText(&t, C2D_WithColor | flags, x, y, 0.5f, scale, scale, colour);
}

void BottomScreen::bevel(float x, float y, float w, float h, bool raised, u32 face) {
    C2D_DrawRectSolid(x, y, 0.5f, w, h, rgb(raised ? LIGHT : DARK));
    C2D_DrawRectSolid(x + 1, y + 1, 0.5f, w - 1, h - 1, rgb(raised ? DARK : LIGHT));
    C2D_DrawRectSolid(x + 1, y + 1, 0.5f, w - 2, h - 2, rgb(face));
}

void BottomScreen::slotFrame(float x, float y) { bevel(x, y, SLOT_IN, SLOT_IN, false, SLOT_BG); }

void BottomScreen::itemIcon(const ItemStack& st, float x, float y, float scale) {
    if (st.empty()) return;
    // RenderItem: cube blocks as their side tile (flat for now), everything else its icon + tint
    const ItemInfo* info = itemInfo(st.id, st.damage);
    auto draw = [&](C3D_Tex* tex, const Tex3DS_SubTexture* sub, uint32_t rgb) {
        if (!tex) return;
        C2D_Image img = {tex, sub};
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255), 0.5f);
        C2D_DrawImageAt(img, x + 3, y + 3, 0.5f, rgb == 0xFFFFFF ? nullptr : &tint, scale, scale);
    };
    if (st.id < 256) {
        int tile = info && info->icon >= 0 ? info->icon : 0;
        if (info && (info->flags & IF_BLOCK3D)) tile = BLOCKS[st.id].tex[st.damage & 15][2];
        else if (info && (info->flags & IF_TERRAIN2)) tile = 256 + (tile & 255);
        draw(renderer->terrainTexture(), &tileSub[tile & 511], 0xFFFFFF); // atlas is pre-tinted
    } else if (info && info->icon >= 0) {
        draw(itemsTex, &itemSub[info->icon & 255], info->colour);
        if (info->overlay >= 0) draw(itemsTex, &itemSub[info->overlay & 255], info->overlayColour);
    }
    if (st.count > 1) {
        char buf[4];
        snprintf(buf, sizeof buf, "%d", st.count);
        float sz = 16.0f * scale + 6.0f; // SLOT_IN at the normal size
        text(buf, x + sz, y + sz - 13, 0.45f, rgb(0x3F3F3F), C2D_AlignRight); // shadow
        text(buf, x + sz - 1, y + sz - 14, 0.45f, rgb(LIGHT), C2D_AlignRight);
    }
}

void BottomScreen::button(float x, float y, float w, float h, const char* label) {
    bevel(x, y, w, h, true, 0x6F6F6F);
    text(label, x + w / 2 + 1, y + h / 2 - 7, 0.5f, rgb(0x3F3F3F), C2D_AlignCenter);
    text(label, x + w / 2, y + h / 2 - 8, 0.5f, rgb(0xE0E0E0), C2D_AlignCenter);
}

void BottomScreen::drawHotbar() {
    C2D_DrawRectSolid(0, HOTBAR_Y - 4, 0.5f, 320, 240 - (HOTBAR_Y - 4), rgb(0x2A2A2A));
    for (int i = 0; i < Inventory::HOTBAR; i++) {
        float x = HOTBAR_X + i * SLOT;
        slotFrame(x, HOTBAR_Y);
        itemIcon(inv->main[i], x, HOTBAR_Y);
    }
    if (current != Tab::Items && inv->currentItem < Inventory::HOTBAR) { // selection frame, like Beta's gui.png highlight
        float x = HOTBAR_X + inv->currentItem * SLOT - 2;
        u32 w = rgb(LIGHT);
        C2D_DrawRectSolid(x, HOTBAR_Y - 2, 0.5f, SLOT_IN + 4, 2, w);
        C2D_DrawRectSolid(x, HOTBAR_Y + SLOT_IN, 0.5f, SLOT_IN + 4, 2, w);
        C2D_DrawRectSolid(x, HOTBAR_Y - 2, 0.5f, 2, SLOT_IN + 4, w);
        C2D_DrawRectSolid(x + SLOT_IN + 2, HOTBAR_Y - 2, 0.5f, 2, SLOT_IN + 4, w);
    }
}

void BottomScreen::drawMapTab() {
    C2D_DrawRectSolid(MAP_X - 3, MAP_Y - 3, 0.5f, MAP_PX + 6, MAP_PX + 6, rgb(INK));
    C2D_Image img = {&mapTex, &mapSub};
    C2D_DrawImageAt(img, MAP_X, MAP_Y, 0.5f, nullptr, MAP_SCALE, MAP_SCALE);

    // Player arrow (north up)
    float cx = MAP_X + MAP_PX / 2.0f, cy = MAP_Y + MAP_PX / 2.0f;
    float s = std::sin(camera->yaw), c = -std::cos(camera->yaw);
    auto pt = [&](float ang, float r, float& ox, float& oy) {
        float ca = std::cos(ang), sa = std::sin(ang);
        ox = cx + (s * ca - c * sa) * r;
        oy = cy + (c * ca + s * sa) * r;
    };
    float x0, y0, x1, y1, x2, y2;
    pt(0, 8, x0, y0);
    pt(2.5f, 6, x1, y1);
    pt(-2.5f, 6, x2, y2);
    u32 white = rgb(LIGHT), black = rgb(0);
    C2D_DrawTriangle(x0, y0, black, x1, y1, black, x2, y2, black, 0.5f);
    pt(0, 6, x0, y0); pt(2.5f, 4, x1, y1); pt(-2.5f, 4, x2, y2);
    C2D_DrawTriangle(x0, y0, white, x1, y1, white, x2, y2, white, 0.5f);

}

void BottomScreen::drawItemsTab() {
    C2D_DrawRectSolid(0, TAB_H, 0.5f, 320, HOTBAR_Y - 4 - TAB_H, rgb(GUI_BG));
    if (inv->containerKind == Inventory::CHEST) {
        int rows = int(inv->container.size()) / 9;
        int top = rows > 3 ? 24 : 34;
        text(rows > 3 ? "Large Chest" : "Chest", 6, top - 1, 0.38f, rgb(0x404040));
        auto grid = [&](const ItemStack* st, int n, int y0) {
            for (int i = 0; i < n; i++) {
                float sx = CONT_X + (i % 9) * CSLOT, sy = y0 + (i / 9) * CSLOT;
                bevel(sx, sy, CSLOT - 2, CSLOT - 2, false, SLOT_BG);
                itemIcon(st[i], sx - 2, sy - 2, 1.0f);
            }
        };
        grid(inv->container.data(), int(inv->container.size()), top);
        int py = top + rows * CSLOT + (rows > 3 ? 2 : 10);
        text("Inventory", 6, py - 1, 0.38f, rgb(0x404040));
        grid(&inv->main[9], 27, py);
        return;
    }
    if (inv->containerKind == Inventory::FURNACE) {
        text("Furnace", 16, 23, 0.42f, rgb(0x404040));
        const float IX = 110, IY = 30, FY = 30 + 44, OX = 200, OY = 52;
        slotFrame(IX, IY); itemIcon(inv->container[0], IX, IY);
        slotFrame(IX, FY); itemIcon(inv->container[1], IX, FY);
        slotFrame(OX, OY); itemIcon(inv->container[2], OX, OY);
        // the flame (fuel left) and the arrow (cooking)
        float fh = 12 * std::min(1.0f, inv->furnaceBurn);
        C2D_DrawRectSolid(IX + 9, IY + 31, 0.5f, 12, 12, rgb(0x8B8B8B));
        if (fh > 0) C2D_DrawRectSolid(IX + 9, IY + 31 + 12 - fh, 0.5f, 12, fh, rgb(0xF08020));
        C2D_DrawRectSolid(IX + 40, OY + 11, 0.5f, 44, 8, rgb(0x8B8B8B));
        C2D_DrawRectSolid(IX + 40, OY + 11, 0.5f, 44 * std::min(1.0f, inv->furnaceCook), 8, rgb(0xFFFFFF));
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 9; col++) {
                float x = GRID_X + col * SLOT, y = GRID_Y + row * SLOT;
                slotFrame(x, y);
                itemIcon(inv->main[9 + row * 9 + col], x, y);
            }
        return;
    }
    if (inv->tableOpen) {
        text("Crafting", 16, 23, 0.42f, rgb(0x404040));
        for (int i = 0; i < 9; i++) {
            float sx = TABLE_X + (i % 3) * TSLOT, sy = TABLE_Y + (i / 3) * TSLOT;
            bevel(sx, sy, TSLOT - 2, TSLOT - 2, false, SLOT_BG);
            itemIcon(inv->table[i], sx - 3, sy - 3, 1.1f);
        }
        text(">", RESULT_X - 14, TOP_ROW_Y + SLOT / 2 + 6, 0.6f, rgb(0x404040));
        slotFrame(RESULT_X, TOP_ROW_Y + SLOT / 2);
        itemIcon(inv->tableResult, RESULT_X, TOP_ROW_Y + SLOT / 2);
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 9; col++) {
                float x = GRID_X + col * SLOT, y = GRID_Y + row * SLOT;
                slotFrame(x, y);
                itemIcon(inv->main[9 + row * 9 + col], x, y);
            }
        return;
    }
    text("Armour", ARMOUR_X, 23, 0.42f, rgb(0x404040));
    text("Crafting", CRAFT_X, 23, 0.42f, rgb(0x404040));
    for (int i = 0; i < 4; i++) {
        int cx = (i & 1) * SLOT, cy = (i >> 1) * SLOT;
        slotFrame(ARMOUR_X + cx, TOP_ROW_Y + cy); itemIcon(inv->armour[3 - i], ARMOUR_X + cx, TOP_ROW_Y + cy);
        if (gOptions.craftStyle == 0) {
            slotFrame(CRAFT_X + cx, TOP_ROW_Y + cy);
            itemIcon(inv->craft[i], CRAFT_X + cx, TOP_ROW_Y + cy);
        }
    }
    if (gOptions.craftStyle == 1) { // Pocket: the craft button    } else if (gOptions.craftStyle == 1) { // Pocket: the craft button
        float bw = RESULT_X + SLOT_IN - CRAFT_X, bh = SLOT + SLOT_IN;
        bevel(CRAFT_X, TOP_ROW_Y, bw, bh, true, 0x8B8B8B);
        ItemStack bench;
        bench.id = 58;
        bench.count = 1;
        itemIcon(bench, CRAFT_X + bw / 2 - 18, TOP_ROW_Y + 4, 2.0f);
        text("Craft", CRAFT_X + bw / 2 + 1, TOP_ROW_Y + bh - 17, 0.5f, rgb(0x3F3F3F), C2D_AlignCenter);
        text("Craft", CRAFT_X + bw / 2, TOP_ROW_Y + bh - 18, 0.5f, rgb(0xFFFFFF), C2D_AlignCenter);
    } else {
        text(">", RESULT_X - 14, TOP_ROW_Y + SLOT / 2 + 6, 0.6f, rgb(0x404040));
        slotFrame(RESULT_X, TOP_ROW_Y + SLOT / 2);
        itemIcon(inv->craftResult, RESULT_X, TOP_ROW_Y + SLOT / 2);
    }

    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 9; col++) {
            float x = GRID_X + col * SLOT, y = GRID_Y + row * SLOT;
            slotFrame(x, y);
            itemIcon(inv->main[9 + row * 9 + col], x, y);
        }
}

void BottomScreen::drawMenuTab() {
    C2D_DrawRectSolid(0, TAB_H, 0.5f, 320, HOTBAR_Y - 4 - TAB_H, rgb(0x3A2C1E));
    const Options& o = gOptions;
    char buf[64];
    button(40, MENU_Y0, 118, MENU_H, "Back to Game");
    button(162, MENU_Y0, 118, MENU_H, o.craftStyle ? "Crafting: Pocket" : "Crafting: Classic");
    auto slider = [&](int row, const char* label) {
        float y = MENU_Y0 + row * MENU_ROW;
        button(40, y, 34, MENU_H, "-");
        button(246, y, 34, MENU_H, "+");
        button(78, y, 164, MENU_H, label);
    };
    auto pct = [](float v) { return int(v * 100.0f + 0.5f); };
    if (o.music <= 0.0f) snprintf(buf, sizeof buf, "Music: OFF"); else snprintf(buf, sizeof buf, "Music: %d%%", pct(o.music));
    slider(1, buf);
    if (o.sound <= 0.0f) snprintf(buf, sizeof buf, "Sound: OFF"); else snprintf(buf, sizeof buf, "Sound: %d%%", pct(o.sound));
    slider(2, buf);
    if (o.sensitivity <= 0.0f) snprintf(buf, sizeof buf, "Look speed: *yawn*");
    else if (o.sensitivity >= 1.0f) snprintf(buf, sizeof buf, "Look speed: HYPERSPEED!!!");
    else snprintf(buf, sizeof buf, "Look speed: %d%%", pct(o.sensitivity * 2.0f));
    slider(3, buf);
    int rd = int(renderer->renderDistance);
    const char* rdName = rd >= 112 ? "Far" : rd >= 80 ? "Normal" : rd >= 48 ? "Short" : "Tiny";
    snprintf(buf, sizeof buf, "Render distance: %s", rdName);
    slider(4, buf);
    snprintf(buf, sizeof buf, "Difficulty: %s", Options::difficultyName(o.difficulty));
    button(40, MENU_Y0 + 5 * MENU_ROW, 118, MENU_H, buf);
    button(162, MENU_Y0 + 5 * MENU_ROW, 118, MENU_H, o.bobView ? "View bobbing: ON" : "View bobbing: OFF");
    button(40, MENU_Y0 + 6 * MENU_ROW, 118, MENU_H, o.invertY ? "Invert look: ON" : "Invert look: OFF");
    button(40, MENU_Y0 + 7 * MENU_ROW, 240, MENU_H, "Save and Quit to Title");
    if (o.showStats) { // (tap just right of Save and Quit to show or hide)
        snprintf(buf, sizeof buf, "%d fps  %d sections  %dk tris  %u KB", stats.fps, stats.sections, stats.triangles / 1000, stats.gpuKB);
        text(buf, 160, 22, 0.38f, rgb(0xC0C0C0), C2D_AlignCenter);
    }
}

void BottomScreen::draw() {
    if (!dirty) return;
    dirty = false;
    C2D_TextBufClear(textBuf);
    C3D_RenderTargetSetOutput(target, GFX_BOTTOM, GFX_LEFT, GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)); // the title screen may have taken it
    C2D_TargetClear(target, rgb(0x2A2A2A));
    C2D_SceneBegin(target);

    if (current == Tab::Map) {
        drawHome();
        return;
    }
    const char* title = "Inventory";
    switch (current) {
    case Tab::Items:
        if (mattisShowing()) { drawMattis(); title = "Craft"; }
        else if (buildShowing()) { drawBuild(); title = buildTier == 0 ? "Select block" : buildTier == 1 ? "Select block/item" : "Select item"; }
        else {
            drawItemsTab();
            if (inv->containerKind == Inventory::CHEST) title = "Chest";
            else if (inv->containerKind != Inventory::NONE) title = "Furnace";
            else if (inv->tableOpen) title = "Crafting";
        }
        break;
    case Tab::Menu: drawMenuTab(); title = "Game Paused"; break;
    default: break;
    }
    drawPeHeader(title, current == Tab::Items && gOptions.craftStyle == 1 && inv->containerKind == Inventory::NONE &&
                            !mattisShowing() && !buildShowing());
    if (current == Tab::Items && buildShowing()) peButton(2, 1, 70, TAB_H - 2, "Inventory");
    else if (current == Tab::Items && inv->creative && !mattisShowing() && inv->containerKind == Inventory::NONE &&
             !inv->tableOpen)
        peButton(2, 1, 70, TAB_H - 2, "Blocks");
    drawHotbar();

    // Stack being carried by the stylus
    if (current == Tab::Items && !inv->cursor.empty())
        itemIcon(inv->cursor, cursorX - SLOT_IN / 2, cursorY - SLOT_IN / 2);
}

// --------------------------------------------------------------------------
// Pocket Edition pieces (PE 0.5.0's spritesheet.png and gui.png)
// --------------------------------------------------------------------------

namespace {
Tex3DS_SubTexture peSub(float tw, float th, float x, float y, float w, float h) {
    return {uint16_t(w), uint16_t(h), x / tw, 1.0f - y / th, (x + w) / tw, 1.0f - (y + h) / th};
}
} // namespace

void BottomScreen::sprite(C3D_Tex* tex, float tw, float th, int sx, int sy, int sw, int sh, float x, float y,
                          float scale) {
    if (!tex) return;
    Tex3DS_SubTexture t = peSub(tw, th, sx, sy, sw, sh);
    C2D_DrawImageAt({tex, &t}, x, y, 0.5f, nullptr, scale, scale);
}

void BottomScreen::ninePatch(C3D_Tex* tex, float tw, float th, int sx, int sy, int sw, int sh, int b, float x,
                             float y, float w, float h, float s) {
    if (!tex) return;
    float bb = b * s; // the frame on screen
    float midW = w - 2 * bb, midH = h - 2 * bb;
    int mw = sw - 2 * b, mh = sh - 2 * b;
    auto piece = [&](int px, int py, int pw, int ph, float dx, float dy, float dw, float dh) {
        if (pw <= 0 || ph <= 0 || dw <= 0 || dh <= 0) return;
        Tex3DS_SubTexture t = peSub(tw, th, px, py, pw, ph);
        C2D_DrawImageAt({tex, &t}, dx, dy, 0.5f, nullptr, dw / pw, dh / ph);
    };
    piece(sx, sy, b, b, x, y, bb, bb);
    piece(sx + b, sy, mw, b, x + bb, y, midW, bb);
    piece(sx + sw - b, sy, b, b, x + w - bb, y, bb, bb);
    piece(sx, sy + b, b, mh, x, y + bb, bb, midH);
    piece(sx + b, sy + b, mw, mh, x + bb, y + bb, midW, midH);
    piece(sx + sw - b, sy + b, b, mh, x + w - bb, y + bb, bb, midH);
    piece(sx, sy + sh - b, b, b, x, y + h - bb, bb, bb);
    piece(sx + b, sy + sh - b, mw, b, x + bb, y + h - bb, midW, bb);
    piece(sx + sw - b, sy + sh - b, b, b, x + w - bb, y + h - bb, bb, bb);
}

void BottomScreen::peButton(float x, float y, float w, float h, const char* label) {
    // Touch::TButton: spritesheet (8,32) 8x8 with a 2-texel frame
    ninePatch(peSheet, 128, 128, 8, 32, 8, 8, 2, x, y, w, h);
    if (label && *label) {
        text(label, x + w / 2 + 1, y + h / 2 - 7, 0.5f, rgb(0x3F3F3F), C2D_AlignCenter);
        text(label, x + w / 2, y + h / 2 - 8, 0.5f, rgb(0xE0E0E0), C2D_AlignCenter);
    }
}

void BottomScreen::drawPeHeader(const char* title, bool craftButton) {
    // PE's screen header: a raised bar with the title, Craft on the left and the X on the right
    ninePatch(peSheet, 128, 128, 0, 0, 16, 16, 4, 0, 0, 320, TAB_H, 1.0f);
    if (craftButton) peButton(2, 1, 70, TAB_H - 2, "Craft");
    if (mattisShowing() && inv->containerKind == Inventory::NONE && !inv->tableOpen)
        peButton(2, 1, 70, TAB_H - 2, "Back");
    text(title, 161, 3, 0.55f, rgb(0x3F3F3F), C2D_AlignCenter);
    text(title, 160, 2, 0.55f, rgb(0xFFFFFF), C2D_AlignCenter);
    sprite(peSheet, 128, 128, 60, 0, 18, 18, PE_X_BTN + 2, 1, 1.12f); // the X
}

void BottomScreen::drawHome() {
    // PE's dirt behind everything
    C2D_DrawRectSolid(0, 0, 0.4f, 320, 240, rgb(0x3B2C20));
    // the hotbar, New 3DS Edition style: nine grey PE slots (spritesheet 8,32) with PE's green
    // selection frame (spritesheet 10,42) around the held one
    for (int i = 0; i < Inventory::HOTBAR; i++) {
        float x = HOME_SLOT_X + i * HOME_SLOT_PITCH;
        ninePatch(peSheet, 128, 128, 8, 32, 8, 8, 2, x, HOME_SLOT_Y, HOME_SLOT, HOME_SLOT);
        itemIcon(inv->main[i], x + (HOME_SLOT - 24) / 2 - 3, HOME_SLOT_Y + (HOME_SLOT - 24) / 2 - 3, 1.5f);
    }
    if (inv->currentItem < Inventory::HOTBAR)
        ninePatch(peSheet, 128, 128, 10, 42, 16, 16, 3, HOME_SLOT_X + inv->currentItem * HOME_SLOT_PITCH - 2,
                  HOME_SLOT_Y - 2, HOME_SLOT + 4, HOME_SLOT + 4);
    // the coordinates bar
    C2D_DrawRectSolid(0, 44, 0.5f, 320, 20, rgb(0x2B2B2B));
    char buf[64];
    snprintf(buf, sizeof buf, "X: %d, Y: %d, Z: %d", (int)std::floor(camera->x), (int)std::floor(camera->y),
             (int)std::floor(camera->z));
    text(buf, 161, 47, 0.6f, rgb(0x3F3F3F), C2D_AlignCenter);
    text(buf, 160, 46, 0.6f, rgb(0xFFFFFF), C2D_AlignCenter);
    // the map and the player's arrow
    drawMapTab();
    // Inventory / Crafting / Pause
    for (int b = 0; b < 3; b++) {
        float y = HOME_BTN_Y0 + b * HOME_BTN_STEP;
        peButton(HOME_BTN_X, y, HOME_BTN_W, HOME_BTN_H, "");
        float cx = HOME_BTN_X + HOME_BTN_W / 2.0f, cy = y + HOME_BTN_H / 2.0f;
        if (b == 2) {
            sprite(peGui, 256, 256, 200, 64, 18, 18, cx - 18, cy - 18, 2.0f); // gui.png's pause button
        } else {
            ItemStack icon;
            icon.id = b == 0 ? 54 : 58; // the chest, the workbench
            icon.count = 1;
            itemIcon(icon, cx - 16 - 3, cy - 16 - 3, 2.0f);
        }
    }
}

void BottomScreen::touchHome(int x, int y) {
    if (y < 44) { // the hotbar
        int i = int((x - HOME_SLOT_X) / HOME_SLOT_PITCH);
        if (i >= 0 && i < Inventory::HOTBAR) inv->currentItem = i;
        return;
    }
    if (in(x, y, MAP_X, MAP_Y, MAP_PX, MAP_PX)) { // zoom: 1, 2, 4, back to 1
        mapZoom = mapZoom >= 4 ? 1 : mapZoom * 2;
        mapTimer = 1;
        return;
    }
    for (int b = 0; b < 3; b++) {
        if (!in(x, y, HOME_BTN_X, HOME_BTN_Y0 + b * HOME_BTN_STEP, HOME_BTN_W, HOME_BTN_H)) continue;
        sound::playUi("random.click", 1.0f, 1.0f);
        if (b == 0) setTab(Tab::Items);
        if (b == 1) { setTab(Tab::Items); if (gOptions.craftStyle == 1) openMattis(true); }
        if (b == 2) setTab(Tab::Menu);
        return;
    }
}

// --------------------------------------------------------------------------
// MATTIS: Pocket Edition's crafting screen
// --------------------------------------------------------------------------

bool BottomScreen::mattisShowing() const {
    return current == Tab::Items && gOptions.craftStyle == 1 && inv->containerKind == Inventory::NONE &&
           (mattisOpen || inv->tableOpen);
}

void BottomScreen::returnCraftGrid() {
    for (ItemStack& c : inv->craft) {
        if (c.empty()) continue;
        ItemStack left = c;
        inv->addStack(left);
        if (left.count > 0) inv->spill.push_back(left);
        c.clear();
    }
    inv->updateCraftResult();
}

void BottomScreen::openMattis(bool on) {
    if (on) {
        inv->returnCursor();
        returnCraftGrid();
        mattisScroll = 0;
        mattisSelId = -1;
    }
    mattisOpen = on;
    dragging = false;
    dirty = true;
}

const recipes::Entry* BottomScreen::mattisSelected() const {
    for (const recipes::Entry& e : mattisList)
        if (isPicked(e)) return &e;
    return nullptr;
}

bool BottomScreen::isPicked(const recipes::Entry& e) const {
    return e.out.id == mattisSelId && e.out.damage == mattisSelDamage;
}

void BottomScreen::pick(const recipes::Entry& e) {
    mattisSelId = e.out.id;
    mattisSelDamage = e.out.damage;
}

void BottomScreen::touchMattis(int x, int y, bool alt) {
    for (int c = 0; c < recipes::CATEGORIES; ++c)
        if (in(x, y, MT_CAT_X, MT_LIST_Y + c * MT_CAT_STEP, MT_CAT_W, MT_CAT_H)) {
            if (c != mattisCat) {
                mattisCat = c;
                mattisScroll = 0;
                mattisSelId = -1;
                sound::playUi("random.click", 1.0f, 1.0f);
            }
            return;
        }
    if (in(x, y, 300, MT_LIST_Y, 18, 16)) { // close
        if (inv->tableOpen) inv->closeTable();
        openMattis(false);
        sound::playUi("random.click", 1.0f, 1.0f);
        return;
    }
    if (in(x, y, MT_LIST_X, MT_LIST_Y, MT_LIST_W + 4, MT_LIST_H)) {
        int row = (y - MT_LIST_Y + mattisScroll) / MT_ROW;
        if (row >= 0 && row < int(mattisList.size())) pick(mattisList[row]);
        dragging = true;
        dragMoved = false;
        dragY0 = y;
        dragScroll0 = mattisScroll;
        dragTarget = &mattisScroll;
        dragMax = std::max(0, int(mattisList.size()) * MT_ROW - MT_LIST_H);
        return;
    }
    const recipes::Entry* e = mattisSelected();
    if (!e) return;
    if (in(x, y, MT_CRAFT_X, MT_CRAFT_Y, MT_CRAFT_W, MT_CRAFT_H) || in(x, y, MT_PANEL_X + 2, MT_CRAFT_Y - 2, SLOT_IN, SLOT_IN)) {
        // tap: craft one; with L or R held: as many as one stack holds
        recipes::Entry pick = *e;
        int times = alt ? recipes::timesCraftable(*inv, pick) : 1;
        int made = 0;
        for (int i = 0; i < times; ++i)
            if (recipes::craft(*inv, pick)) ++made;
        if (made > 0) sound::playUi("random.pop", 0.4f, 1.6f);
    }
}

std::vector<std::string> BottomScreen::wrap(const std::string& s, float width, float scale) {
    std::vector<std::string> lines;
    std::string line, word;
    auto widthOf = [&](const std::string& t) {
        C2D_Text tx;
        C2D_TextBufClear(measureBuf);
        C2D_TextParse(&tx, measureBuf, t.c_str());
        float w = 0, h = 0;
        C2D_TextGetDimensions(&tx, scale, scale, &w, &h);
        return w;
    };
    size_t i = 0;
    while (i <= s.size()) {
        size_t sp = s.find(' ', i);
        if (sp == std::string::npos) sp = s.size();
        word = s.substr(i, sp - i);
        std::string trial = line.empty() ? word : line + " " + word;
        if (!line.empty() && widthOf(trial) > width) {
            lines.push_back(line);
            line = word;
        } else {
            line = trial;
        }
        i = sp + 1;
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

void BottomScreen::drawMattis() {
    // rebuilt every redraw: the inventory may have changed (a pickup, a craft)
    mattisList = recipes::list(*inv, inv->tableOpen, recipes::Category(mattisCat));
    if (!mattisSelected() && !mattisList.empty()) pick(mattisList[0]);
    int maxScroll = std::max(0, int(mattisList.size()) * MT_ROW - MT_LIST_H);
    if (mattisScroll > maxScroll) mattisScroll = maxScroll;

    C2D_DrawRectSolid(0, TAB_H, 0.5f, 320, HOTBAR_Y - 4 - TAB_H, rgb(0x5A5A5A));

    // the four groups
    static const uint16_t ICON[4] = {5, 274, 307, 321};
    for (int c = 0; c < int(recipes::CATEGORIES); ++c) {
        float y = MT_LIST_Y + c * MT_CAT_STEP;
        bool on = c == mattisCat;
        bevel(MT_CAT_X, y, MT_CAT_W, MT_CAT_H, !on, on ? 0xC6C6C6 : 0x8B8B8B);
        ItemStack ic;
        ic.id = ICON[c];
        ic.count = 1;
        itemIcon(ic, MT_CAT_X + 4, y + 4, 2.0f);
    }

    // the list
    C2D_DrawRectSolid(MT_LIST_X, MT_LIST_Y, 0.5f, MT_LIST_W, MT_LIST_H, rgb(0x8B8B8B));
    int first = mattisScroll / MT_ROW;
    for (int r = first; r < int(mattisList.size()); ++r) {
        float y = MT_LIST_Y + r * MT_ROW - mattisScroll;
        if (y >= MT_LIST_Y + MT_LIST_H) break;
        const recipes::Entry& e = mattisList[r];
        bool sel = isPicked(e);
        float top = std::max<float>(y, MT_LIST_Y), bottom = std::min<float>(y + MT_ROW - 1, MT_LIST_Y + MT_LIST_H);
        if (bottom <= top) continue;
        C2D_DrawRectSolid(MT_LIST_X, top, 0.5f, MT_LIST_W, bottom - top, rgb(sel ? 0xB8B8B8 : (e.craftable ? 0x9C9C9C : 0x7E7E7E)));
        if (y < MT_LIST_Y || y + MT_ROW > MT_LIST_Y + MT_LIST_H) continue; // half-shown rows: no text
        const ItemInfo* info = itemInfo(e.out.id, e.out.damage);
        std::string nm = info ? info->name : "?";
        { // too long for the row: cut it short
            std::vector<std::string> fit = wrap(nm, MT_LIST_W - 26, 0.4f);
            if (fit.size() > 1) nm = fit[0] + "...";
        }
        const char* name = nm.c_str();
        if (e.craftable) text(name, MT_LIST_X + 5, y + 3, 0.4f, rgb(0x3F3F3F));
        text(name, MT_LIST_X + 4, y + 2, 0.4f, rgb(e.craftable ? 0xFFFFFF : 0x505050));
        ItemStack ic;
        ic.id = uint16_t(e.out.id);
        ic.damage = e.out.damage;
        ic.count = 1;
        itemIcon(ic, MT_LIST_X + MT_LIST_W - 20, y - 2, 0.85f);
    }
    if (maxScroll > 0) { // scroll bar
        float h = MT_LIST_H * float(MT_LIST_H) / float(MT_LIST_H + maxScroll);
        float y = MT_LIST_Y + (MT_LIST_H - h) * mattisScroll / float(maxScroll);
        C2D_DrawRectSolid(MT_LIST_X + MT_LIST_W, MT_LIST_Y, 0.5f, 4, MT_LIST_H, rgb(0x404040));
        C2D_DrawRectSolid(MT_LIST_X + MT_LIST_W, y, 0.5f, 4, h, rgb(0xC6C6C6));
    }
    if (mattisList.empty())
        text(inv->tableOpen ? "Nothing here yet." : "Use a crafting table for more.",
             MT_LIST_X + 6, MT_LIST_Y + 6, 0.38f, rgb(0x303030));

    // the picked recipe
    C2D_DrawRectSolid(MT_PANEL_X, MT_LIST_Y, 0.5f, 320 - MT_PANEL_X - 2, MT_LIST_H, rgb(GUI_BG));
    bevel(300, MT_LIST_Y, 18, 16, true, 0x8B8B8B);
    text("x", 309, MT_LIST_Y - 1, 0.5f, rgb(0xFFFFFF), C2D_AlignCenter);
    const recipes::Entry* e = mattisSelected();
    if (!e) return;
    const ItemInfo* info = itemInfo(e->out.id, e->out.damage);
    std::vector<std::string> title = wrap(info ? info->name : "?", 94, 0.4f);
    for (size_t l = 0; l < title.size() && l < 1; ++l) text(title[l].c_str(), MT_PANEL_X + 4, MT_LIST_Y + 1, 0.4f, rgb(0x303030));
    ItemStack res;
    res.id = uint16_t(e->out.id);
    res.damage = e->out.damage;
    res.count = e->out.count;
    slotFrame(MT_PANEL_X + 2, MT_CRAFT_Y - 2);
    itemIcon(res, MT_PANEL_X + 2, MT_CRAFT_Y - 2);
    bevel(MT_CRAFT_X, MT_CRAFT_Y, MT_CRAFT_W, MT_CRAFT_H, true, e->craftable ? 0x6F6F6F : 0x9A9A9A);
    text("Craft", MT_CRAFT_X + MT_CRAFT_W / 2 + 1, MT_CRAFT_Y + 6, 0.5f, rgb(e->craftable ? 0x3F3F3F : 0x9A9A9A), C2D_AlignCenter);
    text("Craft", MT_CRAFT_X + MT_CRAFT_W / 2, MT_CRAFT_Y + 5, 0.5f, rgb(e->craftable ? 0xFFFFFF : 0x6A6A6A), C2D_AlignCenter);

    // ingredients: icon, then have / need (red when short)
    float iy = MT_CRAFT_Y + 32;
    for (size_t k = 0; k < e->needs.size() && k < 8; ++k) {
        const recipes::Need& n = e->needs[k];
        float x = MT_PANEL_X + 3 + float(k % 4) * 29, y = iy + float(k / 4) * 30;
        ItemStack ic;
        ic.id = uint16_t(n.what.id);
        ic.damage = int16_t(n.what.damage < 0 ? 0 : n.what.damage);
        ic.count = 1;
        bevel(x, y, 22, 22, false, SLOT_BG);
        itemIcon(ic, x - 2, y - 2, 1.0f);
        char buf[16];
        snprintf(buf, sizeof buf, "%d/%d", std::min(n.have, 999), n.count);
        text(buf, x + 11, y + 21, 0.32f, rgb(n.have >= n.count ? 0x303030 : 0xB02020), C2D_AlignCenter);
    }
    float dy = iy + (e->needs.size() > 4 ? 62 : 32);
    std::string d;
    std::vector<std::string> lines = wrap(d, 110, 0.34f);
    for (size_t l = 0; l < lines.size(); ++l) {
        float y = dy + float(l) * 10.5f;
        if (y > MT_LIST_Y + MT_LIST_H - 10) break;
        text(lines[l].c_str(), MT_PANEL_X + 4, y, 0.34f, rgb(0x303030));
    }
}

// --------------------------------------------------------------------------
// GuiBuild: NSSS's creative block picker. 45 to a page (9 x 5); tap one and it goes in your hand
// (L/R held: a full stack, like holding shift). The arrows step through the tiers.
// --------------------------------------------------------------------------

namespace {
constexpr int BM_X = 25, BM_Y = 30, BM_STEP = 30, BM_ARROW_Y = 180;
}

bool BottomScreen::buildShowing() const {
    return inv->creative && !buildOwnPage && inv->containerKind == Inventory::NONE && !inv->tableOpen;
}

void BottomScreen::touchBuild(int x, int y, bool alt) {
    if (in(x, y, 4, BM_ARROW_Y, 60, 22)) { buildTier = buildTier > 0 ? buildTier - 1 : buildmenu::MAX_TIER; return; }
    if (in(x, y, 256, BM_ARROW_Y, 60, 22)) { buildTier = buildTier < buildmenu::MAX_TIER ? buildTier + 1 : 0; return; }
    if (!in(x, y, BM_X, BM_Y, 9 * BM_STEP, 5 * BM_STEP)) return;
    int n = (y - BM_Y) / BM_STEP * 9 + (x - BM_X) / BM_STEP;
    int i = buildTier * buildmenu::PER_PAGE + n;
    if (i >= buildmenu::pageEnd(buildTier)) return;
    int id = buildmenu::idAt(i);
    int count = 1;
    if (alt) // (GuiBuild's shift path reads the item 81 back for the stack size)
        count = survival::maxStack(i < buildmenu::BLOCK_COUNT ? id : buildmenu::ITEMS[i - 81]);
    inv->setCurrentItem(id, count);
    sound::playUi("random.click", 1.0f, 1.0f);
    setTab(Tab::Map);
}

void BottomScreen::drawBuild() {
    C2D_DrawRectSolid(0, TAB_H, 0.5f, 320, HOTBAR_Y - 4 - TAB_H, rgb(0x101018));
    int end = buildmenu::pageEnd(buildTier);
    for (int i = buildTier * buildmenu::PER_PAGE; i < end; ++i) {
        int n = i - buildTier * buildmenu::PER_PAGE;
        ItemStack st;
        st.id = uint16_t(buildmenu::idAt(i));
        st.count = 1;
        itemIcon(st, BM_X + n % 9 * BM_STEP, BM_Y + n / 9 * BM_STEP, 1.0f);
    }
    peButton(4, BM_ARROW_Y, 60, 22, "<");
    peButton(256, BM_ARROW_Y, 60, 22, ">");
    char buf[16];
    snprintf(buf, sizeof buf, "%d / %d", buildTier + 1, buildmenu::MAX_TIER + 1);
    text(buf, 160, BM_ARROW_Y + 3, 0.5f, rgb(0xFFFFFF), C2D_AlignCenter);
}
