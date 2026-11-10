#include "Hud.h"
#include <algorithm>
#include <cmath>
#include "render/Renderer.h"
#include "game/Player.h"
#include "game/Progress.h"
#include "game/ItemTable.h"
#include "audio/Sound.h"
#include "world/BlockTable.h"
#ifdef __ANDROID__
#include "../android/src/Platform.h"
#endif

namespace {
// the screen's edges in the top screen's units: 0..400 on the 3DS, the whole window on Android
float edgeL() {
#ifdef __ANDROID__
    return platform::topLeft();
#else
    return 0.0f;
#endif
}
float edgeR() {
#ifdef __ANDROID__
    return platform::topRight();
#else
    return 400.0f;
#endif
}
} // namespace

void Hud::load(Renderer& r) {
    renderer = &r;
    icons = r.texture("romfs:/textures/gui/icons.rgba");
    axIcons = r.texture("romfs:/axeolon/gui/icons.rgba");
    gui = r.texture("romfs:/textures/gui/gui.rgba");
    items = r.texture("romfs:/textures/gui/items.rgba");
    bossbar = r.texture("romfs:/axeolon/gui/bossbar.rgba");
    vignette = r.texture("romfs:/textures/misc/vignette.rgba");
    water = r.texture("romfs:/textures/misc/water.rgba");
    font.load(r);
}

void Hud::icon(C3D_Tex* t, float x, float y, int u, int v, int w, int h, bool flip) {
    if (!t) return;
    float l = u / 256.0f, rr = (u + w) / 256.0f;
    if (flip) std::swap(l, rr);
    Tex3DS_SubTexture sub = {uint16_t(w), uint16_t(h), l, 1.0f - v / 256.0f, rr, 1.0f - (v + h) / 256.0f};
    C2D_DrawImageAt({t, &sub}, x, y, 0.5f);
}

void Hud::draw(const Player& p, bool inWater, int updateCounter, bool survival) {
    if (!survival) return; // PlayerControllerCreative.shouldDrawHUD
    const float W = 400, H = 240;
#ifdef __ANDROID__
    // Pocket Edition (Gui::renderHearts): hearts across the top left, armour after them, and
    // Axeolon's mana mirrored across the top right; air bubbles under the hearts
    (void)W;
    (void)H;
    const float L = edgeL() + 2, R = edgeR() - 2, top = 2;
#endif
#ifndef __ANDROID__
    const float rowHearts = H - 13, rowAbove = rowHearts - 10;
#endif
    bool blink = p.hurtResistantTime / 3 % 2 == 1;
    if (p.hurtResistantTime < 10) blink = false;
    int hp = p.health, prev = p.prevHealth;
    rand.setSeed(int64_t(updateCounter) * 312871);
    int armour = p.armourValue();
    int hearts = std::max(10, (p.healthCap() + 1) / 2);
    for (int i = 0; i < hearts; ++i) {
#ifdef __ANDROID__
        float x = L + (i % 10) * 8, y = top + (i / 10) * 10;
        float ax = L + 84 + i * 8, ay = top;
#else
        float x = W / 2 - 91 + i * 8, y = rowHearts;
        float ax = x, ay = rowAbove;
#endif
        if (armour > 0 && i < 10) {
            if (i * 2 + 1 < armour) icon(icons, ax, ay, 34, 9);
            if (i * 2 + 1 == armour) icon(icons, ax, ay, 25, 9, 9, 9, true);
            if (i * 2 + 1 > armour) icon(icons, ax, ay, 16, 9);
        }
        if (hp <= 4) y += rand.nextInt(2);
        icon(icons, x, y, 16 + (blink ? 9 : 0), 0);
        if (blink) {
            if (i * 2 + 1 < prev) icon(icons, x, y, 70, 0);
            if (i * 2 + 1 == prev) icon(icons, x, y, 79, 0);
        }
        if (i * 2 + 1 < hp) icon(icons, x, y, 52, 0);
        if (i * 2 + 1 == hp) icon(icons, x, y, 61, 0);
    }
    if (inWater) {
        int full = int(std::ceil(double(p.air - 2) * 10.0 / 300.0));
        int popping = int(std::ceil(double(p.air) * 10.0 / 300.0)) - full;
        for (int i = 0; i < full + popping; ++i) {
#ifdef __ANDROID__
            float bx = L + i * 8, by = top + ((hearts + 9) / 10) * 10;
#else
            float bx = W / 2 + 91 - i * 8 - 9, by = rowAbove;
#endif
            icon(icons, bx, by, i < full ? 16 : 25, 18);
        }
    }
    for (int i = 0; i < 10; ++i) { // mana, right to left
#ifdef __ANDROID__
        float x = R - 9 - i * 8, y = top;
#else
        float x = W / 2 + 91 - i * 8 - 9, y = rowHearts;
#endif
        icon(axIcons, x, y, 18, 0);
        if (i * 2 + 1 < p.mana) icon(axIcons, x, y, 0, 0);
        if (i * 2 + 1 == p.mana) icon(axIcons, x, y, 9, 0);
    }
}

void Hud::button(float x, float y, float w, const char* label, bool enabled, bool hover) {
    int state = !enabled ? 0 : hover ? 2 : 1;
    float half = w / 2.0f;
    if (gui) {
        Tex3DS_SubTexture left = {uint16_t(half), 20, 0.0f, 1.0f - (46 + state * 20) / 256.0f, half / 256.0f,
                                  1.0f - (66 + state * 20) / 256.0f};
        Tex3DS_SubTexture right = {uint16_t(half), 20, (200 - half) / 256.0f, 1.0f - (46 + state * 20) / 256.0f,
                                   200 / 256.0f, 1.0f - (66 + state * 20) / 256.0f};
        C2D_DrawImageAt({gui, &left}, x, y, 0.5f);
        C2D_DrawImageAt({gui, &right}, x + half, y, 0.5f);
    }
    uint32_t col = !enabled ? 0xA0A0A0 : hover ? 0xFFFFA0 : 0xE0E0E0;
    font.drawCentered(label, x + w / 2, y + 6, col);
}

void Hud::drawGameOver(int selected, bool enabled, int score) {
    // drawGradientRect(0, 0, w, h, 0x60500000, 0xA0803030)
    u32 top = C2D_Color32(0x50, 0, 0, 0x60), bottom = C2D_Color32(0x80, 0x30, 0x30, 0xA0);
    C2D_DrawRectangle(edgeL(), 0, 0.5f, edgeR() - edgeL(), 240, top, top, bottom, bottom);
    font.drawCentered("Game over!", 200, 30, 0xFFFFFF, true, 2.0f);
    std::string s = "Score: ";
    float sw = float(font.width(s + std::to_string(score)));
    font.draw(s, 200 - sw / 2, 100, 0xFFFFFF);
    font.draw(std::to_string(score), 200 - sw / 2 + font.width(s), 100, 0xFFFF55);
    button(100, 240 / 4 + 72, 200, "Respawn", enabled, selected == 0);
    button(100, 240 / 4 + 96, 200, "Title menu", enabled, selected == 1);
}

void Hud::drawOverlays(float brightness, bool underwater, bool burning, float yaw, float pitch, Renderer& r) {
    // the vignette: multiply the screen by (1 - vignette * level)
    float v = 1.0f - brightness;
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    vignetteLevel += (v - vignetteLevel) * 0.01f;
    if (vignette) {
        C2D_Flush();
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ZERO, GPU_ONE_MINUS_SRC_COLOR, GPU_ZERO, GPU_ONE);
        C2D_SetTintMode(C2D_TintMult);
        C2D_ImageTint t;
        int c = int(vignetteLevel * 255.0f);
        C2D_PlainImageTint(&t, C2D_Color32(c, c, c, 255), 1.0f);
        Tex3DS_SubTexture sub = {256, 256, 0.0f, 1.0f, 1.0f, 0.0f};
        C2D_DrawImageAt({vignette, &sub}, edgeL(), 0, 0.5f, &t, (edgeR() - edgeL()) / 256.0f, 240.0f / 256.0f);
        C2D_Flush();
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                       GPU_ONE_MINUS_SRC_ALPHA);
        C2D_SetTintMode(C2D_TintSolid);
    }
    // renderWarpedTextureOverlay: the water texture, four across, drifting as you turn
    if (underwater && water) {
        C2D_SetTintMode(C2D_TintMult);
        C2D_ImageTint t;
        int b = int(brightness * 255.0f);
        C2D_PlainImageTint(&t, C2D_Color32(b, b, b, 128), 1.0f);
        float ox = std::fmod(-yaw / 64.0f * 100.0f, 100.0f), oy = std::fmod(pitch / 64.0f * 100.0f, 100.0f);
        if (ox > 0) ox -= 100.0f;
        if (oy > 0) oy -= 100.0f;
        Tex3DS_SubTexture sub = {16, 16, 0.0f, 1.0f, 1.0f, 0.0f};
        for (float x = ox + std::floor(edgeL() / 100.0f) * 100.0f; x < edgeR(); x += 100.0f)
            for (float y = oy; y < 240.0f; y += 100.0f) C2D_DrawImageAt({water, &sub}, x, y, 0.5f, &t, 100.0f / 16.0f, 100.0f / 16.0f);
        C2D_SetTintMode(C2D_TintSolid);
    }
    // renderFireInFirstPerson: the fire texture licking up both lower corners
    if (burning) {
        C3D_Tex* atlas = r.terrainTexture();
        const int tile = 31;
        float u0 = float(tile & 15) / 16.0f, v0 = 1.0f - float(tile >> 4) / 64.0f;
        Tex3DS_SubTexture sub = {16, 16, u0, v0, u0 + 1.0f / 16.0f, v0 - 1.0f / 64.0f};
        C2D_ImageTint t;
        C2D_AlphaImageTint(&t, 0.9f);
        C2D_DrawImageAt({atlas, &sub}, edgeL() - 20, 70, 0.5f, &t, 11.0f, 11.0f);
        Tex3DS_SubTexture flip = {16, 16, u0 + 1.0f / 16.0f, v0, u0, v0 - 1.0f / 64.0f};
        C2D_DrawImageAt({atlas, &flip}, edgeR() - 156, 70, 0.5f, &t, 11.0f, 11.0f);
    }
}

void Hud::drawPortal(float f, Renderer& r) {
    if (f <= 0.0f) return;
    if (f < 1.0f) {
        f *= f;
        f *= f;
        f = f * 0.8f + 0.2f;
    }
    C3D_Tex* atlas = r.terrainTexture();
    if (!atlas) return;
    const int tile = 14; // Block.portal
    float u0 = float(tile & 15) / 16.0f, v0 = 1.0f - float(tile >> 4) / 64.0f;
    Tex3DS_SubTexture sub = {16, 16, u0, v0, u0 + 1.0f / 16.0f, v0 - 1.0f / 64.0f};
    C2D_ImageTint t;
    C2D_AlphaImageTint(&t, f);
    C2D_DrawImageAt({atlas, &sub}, edgeL(), 0, 0.5f, &t, (edgeR() - edgeL()) / 16.0f, 240.0f / 16.0f);
}

void Hud::drawPause(int selected) {
    // drawDefaultBackground in a world: a dark gradient over the game
    u32 top = C2D_Color32(0x10, 0x10, 0x10, 0xC0), bottom = C2D_Color32(0x10, 0x10, 0x10, 0xD0);
    C2D_DrawRectangle(edgeL(), 0, 0.5f, edgeR() - edgeL(), 240, top, top, bottom, bottom);
    font.drawCentered("Game menu", 200, 40, 0xFFFFFF);
    button(100, 240 / 4 + 24, 200, "Back to game", true, selected == 0);
    button(100, 240 / 4 + 48, 200, "Save game", true, selected == 1);
    button(100, 240 / 4 + 72, 200, "Chat and commands", true, selected == 2);
    button(100, 240 / 4 + 96, 200, "Save and quit to title", true, selected == 3);
    font.draw("A  Select    B  Back", 8, 240 - 12, 0xA0A0A0);
}

void Hud::drawSaving(int frame) {
    // a little turning block of dots in the corner, and the word
    const float cx = edgeR() - 14, cy = 240 - 14;
    for (int i = 0; i < 8; ++i) {
        float a = float(i) * 3.1415927f / 4.0f;
        int lit = (frame / 4 + i) % 8;
        int c = 80 + lit * 22;
        C2D_DrawRectSolid(cx + std::cos(a) * 6.0f - 1.5f, cy + std::sin(a) * 6.0f - 1.5f, 0.5f, 3, 3, C2D_Color32(c, c, c, 255));
    }
    font.draw("Saving", edgeR() - 30 - font.width("Saving"), 240 - 18, 0xFFFFFF);
}

void Hud::drawToast(const std::string& s, float alpha) {
    if (alpha <= 0.0f) return;
    int a = int(alpha * 255.0f);
    if (a > 255) a = 255;
    float w = float(font.width(s)) + 8;
    C2D_DrawRectSolid(200 - w / 2, 36, 0.5f, w, 12, C2D_Color32(0, 0, 0, a / 2));
    font.drawCentered(s, 200, 38, 0xFFFFFF);
}

void Hud::addChat(const std::string& s) {
    if (s.empty()) return;
    // split to the chat width (320 pixels in GuiIngame; a bit narrower here)
    std::string line, word;
    std::vector<std::string> out;
    auto flush = [&] { if (!line.empty()) out.push_back(line); line.clear(); };
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ' ') {
            std::string cand = line.empty() ? word : line + " " + word;
            if (font.width(cand) > 300 && !line.empty()) { flush(); line = word; }
            else line = cand;
            word.clear();
        } else word += s[i];
    }
    flush();
    for (const std::string& l : out) chat.insert(chat.begin(), ChatLine{l, 0});
    if (chat.size() > 50) chat.resize(50);
}

void Hud::tickChat() {
    for (ChatLine& c : chat) ++c.age;
    if (recordTicks > 0) --recordTicks;
}

void Hud::setRecordPlaying(const std::string& s) {
    recordText = "Now playing: " + s;
    recordTicks = 60;
}

void Hud::drawRecordPlaying(float partial) {
    if (recordTicks <= 0) return;
    float t = float(recordTicks) - partial;
    int a = int(t * 256.0f / 20.0f);
    if (a > 255) a = 255;
    if (a <= 0) return;
    // Color.HSBtoRGB(t / 50, 0.7, 0.6): the colour slides round the rainbow as it fades
    float h = t / 50.0f, sat = 0.7f, val = 0.6f;
    h = (h - std::floor(h)) * 6.0f;
    float f = h - std::floor(h), p = val * (1.0f - sat), q = val * (1.0f - sat * f), u = val * (1.0f - sat * (1.0f - f));
    float r = 0, g = 0, b = 0;
    switch (int(h)) {
    case 0: r = val; g = u; b = p; break;
    case 1: r = q; g = val; b = p; break;
    case 2: r = p; g = val; b = u; break;
    case 3: r = p; g = q; b = val; break;
    case 4: r = u; g = p; b = val; break;
    default: r = val; g = p; b = q; break;
    }
    uint32_t col = uint32_t(int(r * 255.0f + 0.5f)) << 16 | uint32_t(int(g * 255.0f + 0.5f)) << 8 | uint32_t(int(b * 255.0f + 0.5f));
    font.drawCentered(recordText, 200, 240 - 48 - 4, col | uint32_t(a) << 24, false);
}

void Hud::drawChat() {
    // GuiIngame: 10 lines, solid for 180 ticks then fading out over the last 20 (GuiChat: 20, all solid)
    size_t lines = chatOpen ? 20 : 10;
    auto alpha = [&](size_t i) {
        if (chatOpen) return 255;
        int age = chat[i].age;
        if (age >= 200) return 0;
        double f = 1.0 - double(age) / 200.0;
        f *= 10.0;
        if (f < 0.0) f = 0.0;
        if (f > 1.0) f = 1.0;
        f *= f;
        return int(255.0 * f);
    };
#ifdef __ANDROID__
    // Pocket Edition: under the hearts at the top left, newest at the bottom (the pad is down below)
    size_t n = 0;
    while (n < chat.size() && n < lines && alpha(n) > 2) ++n;
    if (chatOpen && chat.empty()) {
        C2D_DrawRectSolid(edgeL() + 2, 25, 0.5f, 320, 9, C2D_Color32(0, 0, 0, 0x80));
        font.draw("No messages yet", edgeL() + 2, 26, 0xFFA0A0A0);
    }
    for (size_t k = 0; k < n; ++k) {
        size_t i = n - 1 - k;
        int a = alpha(i);
        float y = 26 + float(k) * 9.0f;
        C2D_DrawRectSolid(edgeL() + 2, y - 1, 0.5f, 320, 9, C2D_Color32(0, 0, 0, a / 2));
        font.draw(chat[i].text, edgeL() + 2, y, 0xFFFFFF | (uint32_t(a) << 24));
    }
#else
    const float bottom = 240 - 36;
    for (size_t i = 0; i < chat.size() && i < lines; ++i) {
        int a = alpha(i);
        if (!chatOpen && chat[i].age >= 200) break;
        if (a <= 2) continue;
        float y = bottom - float(i) * 9.0f;
        C2D_DrawRectSolid(2, y - 1, 0.5f, 320, 9, C2D_Color32(0, 0, 0, a / 2));
        font.draw(chat[i].text, 2, y, 0xFFFFFF | (uint32_t(a) << 24));
    }
#endif
}

void Hud::drawSpellPicker(const std::vector<uint8_t>& ids, const std::vector<int>& uses, int selected, int cursor,
                          int mana, bool creative) {
    static const char* NAMES[6] = {"Magic Missile", "Fireball", "Heal", "Frost Bolt", "Light", "Blink"};
    static const int COST[6] = {2, 6, 8, 4, 3, 20};
    const int SLOT = 20, ROW = 22;
    int n = int(ids.size());
    C2D_DrawRectSolid(edgeL(), 0, 0.5f, edgeR() - edgeL(), 240, C2D_Color32(0, 0, 0, 0xA0));
    int cols = std::min(9, n), rows = (n + 8) / 9;
    int left = 200 - cols * SLOT / 2, top = 120 - rows * ROW / 2;
    font.drawCentered("Spells", 200, float(top - 14), 0xFFFFFF);
    for (int i = 0; i < n; ++i) {
        float x = float(left + i % 9 * SLOT), y = float(top + i / 9 * ROW);
        icon(gui, x - 1, y - 1, 0, 0, 21, 22);
        if (i % 9 == 8 || i == n - 1) icon(gui, x + 20, y - 1, 181, 0, 1, 22);
    }
    if (selected >= 0 && selected < n)
        icon(gui, float(left + selected % 9 * SLOT - 2), float(top + selected / 9 * ROW - 2), 0, 22, 24, 24);
    for (int i = 0; i < n; ++i) {
        int ic = 224 + ids[i];
        float x = float(left + i % 9 * SLOT), y = float(top + i / 9 * ROW);
        icon(items, x + 2, y + 3, ic % 16 * 16, ic / 16 * 16, 16, 16);
        if (i == cursor) { // the console cursor: a white frame over the slot
            uint32_t c = C2D_Color32(255, 255, 255, 200);
            C2D_DrawRectSolid(x, y, 0.5f, 20, 1, c);
            C2D_DrawRectSolid(x, y + 19, 0.5f, 20, 1, c);
            C2D_DrawRectSolid(x, y, 0.5f, 1, 20, c);
            C2D_DrawRectSolid(x + 19, y, 0.5f, 1, 20, c);
        }
        if (uses[i] != 255) {
            std::string t = std::to_string(uses[i]);
            font.draw(t, x + 19 - font.width(t), y + 11, 0xFFFFFF);
        }
    }
    int shown = cursor >= 0 ? cursor : selected;
    if (shown >= 0 && shown < n && ids[shown] < 6) {
        int id = ids[shown];
        uint32_t col = creative || mana >= COST[id] ? 0xFFFFFF : 0xFF5555;
        std::string u = uses[shown] == 255 ? "permanent" : std::to_string(uses[shown]) + (uses[shown] == 1 ? " use left" : " uses left");
        font.drawCentered(std::string(NAMES[id]) + " - " + std::to_string(COST[id]) + " mana - " + u, 200,
                          float(top + rows * ROW + 6), col);
    }
    font.drawCentered("A  Select    B  Back", 200, 222, 0xA0A0A0);
}

void Hud::drawBoss(bool cutscene, int elapsed, int left, int titleTick, float health, bool enraged, const char* name,
                   const char* title) {
    if (cutscene) {
        int t = elapsed - titleTick;
        int a = t < 0 ? 0 : std::min(255, std::min(t * 8, left * 10));
        if (a > 8) {
            uint32_t al = uint32_t(a) << 24;
            font.drawCentered(name, 200, 120 - 30, 0xFFFFFF | al, true, 3.0f);
            if (title && *title) font.drawCentered(title, 200, 120 + 2, 0xAAAAAA | al);
        }
        return;
    }
    const int barW = 182;
#ifdef __ANDROID__
    float x = 200 - barW / 2, y = 34; // under the hearts and mana
#else
    float x = 200 - barW / 2, y = 12;
#endif
    int fill = int(health * float(barW + 1));
    auto blit = [&](float bx, float by, int u, int v, int w, int h) {
        if (!bossbar || w <= 0) return;
        Tex3DS_SubTexture sub = {uint16_t(w), uint16_t(h), u / 256.0f, 1.0f - v / 16.0f, (u + w) / 256.0f, 1.0f - (v + h) / 16.0f};
        C2D_DrawImageAt({bossbar, &sub}, bx, by, 0.5f);
    };
    blit(x, y, 0, 0, barW, 5);
    if (fill > 0) blit(x, y, 0, enraged ? 10 : 5, std::min(fill, barW), 5);
    font.drawCentered(name, 200, y - 10, enraged ? 0xFF5555 : 0xFF55FF);
}

void Hud::drawCinemaBars(float w) {
    if (w <= 0.0f) return;
    float bar = std::floor(240.0f * 0.11f * w);
    C2D_DrawRectSolid(edgeL(), 0, 0.5f, edgeR() - edgeL(), bar, C2D_Color32(0, 0, 0, 255));
    C2D_DrawRectSolid(edgeL(), 240 - bar, 0.5f, edgeR() - edgeL(), bar, C2D_Color32(0, 0, 0, 255));
}

void Hud::itemIcon(int id, int damage, float x, float y) { // RenderItem.renderItemIntoGUI, flat
    const ItemInfo* info = itemInfo(id, damage);
    C3D_Tex* tex;
    int tile;
    float th; // texture height in pixels
    if (id < 256) {
        tile = info && info->icon >= 0 ? info->icon : 0;
        if (info && (info->flags & IF_BLOCK3D)) tile = BLOCKS[id].tex[damage & 15][2];
        else if (info && (info->flags & IF_TERRAIN2)) tile = 256 + (tile & 255);
        tex = renderer ? renderer->terrainTexture() : nullptr;
        th = 1024.0f;
    } else {
        if (!info || info->icon < 0) return;
        tile = info->icon;
        tex = items;
        th = 256.0f;
    }
    if (!tex) return;
    float u = float(tile & 15) * 16.0f, v = float(tile >> 4) * 16.0f;
    Tex3DS_SubTexture sub = {16, 16, u / 256.0f, 1.0f - v / th, (u + 16.0f) / 256.0f, 1.0f - (v + 16.0f) / th};
    uint32_t rgb = info && id >= 256 ? info->colour : 0xFFFFFF;
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255), 0.5f);
    C2D_DrawImageAt({tex, &sub}, x, y, 0.5f, rgb == 0xFFFFFF ? nullptr : &tint);
}

void Hud::drawToasts() {
    const unsigned long long SHOW_MS = 4000, SLIDE_MS = 300;
    unsigned long long now = osGetTime();
    if (!toastShowing || now - toastAt > SHOW_MS) {
        progress::Toast t;
        toastShowing = progress::popToast(t);
        toastAt = now;
        if (toastShowing) {
            toastTitle = t.title;
            toastText = t.text;
            toastColour = t.colour;
            toastIcon = t.icon;
            toastRare = t.rare;
            sound::playUi(t.rare ? "axeolon.ui.challenge" : "axeolon.ui.toast", 1.0f, 1.0f);
        }
    }
    if (!toastShowing) return;
    unsigned long long age = now - toastAt;
    float slide = 1.0f;
    if (age < SLIDE_MS) slide = float(age) / float(SLIDE_MS);
    else if (age > SHOW_MS - SLIDE_MS) slide = float(SHOW_MS - age) / float(SLIDE_MS);
    const float w = 160, h = 32;
    float x = edgeR() - std::floor(w * slide), y = 4;
    C2D_DrawRectSolid(x, y, 0.5f, w, h, C2D_Color32(0x55, 0x55, 0x55, 255));
    C2D_DrawRectSolid(x + 1, y + 1, 0.5f, w - 1, h - 2, C2D_Color32(0x21, 0x21, 0x21, 255));
    if (!toastTitle.empty()) {
        font.draw(toastTitle, x + 30, y + 7, toastColour);
        font.draw(toastText, x + 30, y + 18, 0xFFFFFF);
    } else { // wrapped
        std::string line, word;
        float ly = y + 6;
        for (size_t i = 0; i <= toastText.size(); ++i) {
            if (i == toastText.size() || toastText[i] == ' ') {
                std::string next = line.empty() ? word : line + " " + word;
                if (font.width(next) > w - 34 && !line.empty()) {
                    font.draw(line, x + 30, ly, 0xE0E0E0);
                    ly += 10;
                    line = word;
                } else {
                    line = next;
                }
                word.clear();
            } else {
                word += toastText[i];
            }
        }
        if (!line.empty()) font.draw(line, x + 30, ly, 0xE0E0E0);
    }
    itemIcon(toastIcon, 0, x + 8, y + 8);
}

void Hud::drawHint(const std::string& title, const std::string& text, float alpha) {
    if (alpha <= 0.01f) return;
    int a = std::min(255, int(alpha * 255.0f));
    const float x = 4, w = 196;
    // wrap the text to the box
    std::vector<std::string> lines;
    std::string line, word;
    for (size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == ' ') {
            std::string next = line.empty() ? word : line + " " + word;
            if (font.width(next) > w - 12 && !line.empty()) {
                lines.push_back(line);
                line = word;
            } else {
                line = next;
            }
            word.clear();
        } else {
            word += text[i];
        }
    }
    if (!line.empty()) lines.push_back(line);
    float h = 18.0f + float(lines.size()) * 10.0f + 4.0f;
    float y = 4.0f - (1.0f - alpha) * 10.0f;
    C2D_DrawRectSolid(x, y, 0.5f, w, h, C2D_Color32(0x10, 0x10, 0x10, uint8_t(a * 0.75f)));
    C2D_DrawRectSolid(x, y, 0.5f, w, 1, C2D_Color32(0x80, 0x80, 0x80, uint8_t(a)));
    C2D_DrawRectSolid(x, y + h - 1, 0.5f, w, 1, C2D_Color32(0x80, 0x80, 0x80, uint8_t(a)));
    uint32_t al = uint32_t(a) << 24;
    font.draw(title, x + 6, y + 5, 0xFFFF55 | al);
    for (size_t i = 0; i < lines.size(); ++i) font.draw(lines[i], x + 6, y + 18 + float(i) * 10.0f, 0xFFFFFF | al);
}
