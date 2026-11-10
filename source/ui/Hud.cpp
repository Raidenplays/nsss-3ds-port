#include "Hud.h"
#include <algorithm>
#include <cmath>
#include "render/Renderer.h"
#include "game/Player.h"
#include "game/ItemTable.h"
#include "audio/Sound.h"
#include "world/BlockTable.h"

namespace {
// the screen's edges in the top screen's units: 0..400 on the 3DS, the whole window on Android
float edgeL() {
    return 0.0f;
}
float edgeR() {
    return 400.0f;
}
} // namespace

void Hud::load(Renderer& r) {
    renderer = &r;
    icons = r.texture("romfs:/textures/gui/icons.rgba");
    gui = r.texture("romfs:/textures/gui/gui.rgba");
    items = r.texture("romfs:/textures/gui/items.rgba");
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
    // NSSS's GuiIngame icons (hearts, half hearts, blinking, NSSS's exhaustion greying the hearts
    // from the right with icons.png 88/97/106, armour, air), placed like the New 3DS Edition:
    // hearts across the top left, armour across the top right (where 3DS Edition has hunger),
    // air bubbles under the hearts
    if (!survival) return; // isVulnerable
    const float rowY = 4, L = 4, R = 400 - 4 - 9;
    int hp = p.health, prev = p.prevHealth;
    bool blink = p.hurtResistantTime / 3 % 2 == 1;
    if (p.hurtResistantTime < 10) blink = false;
    double exhaustion = p.exhaustion;
    rand.setSeed(int64_t(updateCounter) * 312871);
    int armour = p.armourValue();
    for (int i = 0; i < 10; ++i) {
        float y = rowY;
        if (armour > 0) {
            float ax = R - i * 8; // right to left, like Java's armour bar
            if (i * 2 + 1 < armour) icon(icons, ax, rowY, 34, 9);
            if (i * 2 + 1 == armour) icon(icons, ax, rowY, 25, 9, 9, 9, true); // the half points inward
            if (i * 2 + 1 > armour) icon(icons, ax, rowY, 16, 9);
        }
        float x = L + i * 8;
        int ex = int(exhaustion * 20.0);
        if (hp <= 4 || exhaustion > 0.8 || p.exhausted) y += rand.nextInt(2);
        icon(icons, x, y, 16 + (blink ? 9 : 0), 0);
        if (blink) {
            if (i * 2 + 1 < prev) icon(icons, x, y, 70, 0);
            if (i * 2 + 1 == prev) icon(icons, x, y, 79, 0);
        }
        if (i * 2 + 1 < hp) icon(icons, x, y, 52, 0);
        if (i * 2 + 1 == hp) icon(icons, x, y, 61, 0);
        if (i * 2 + 1 > 20 - ex) {
            if (i * 2 + 1 < hp) icon(icons, x, y, 97, 0);
            else if (i * 2 + 1 <= hp) icon(icons, x, y, 106, 0);
        }
        if (i * 2 + 1 == 20 - ex && i * 2 + 1 < hp) icon(icons, x, y, 88, 0);
    }
    if (inWater) {
        int full = int(std::ceil(double(p.air - 2) * 10.0 / 300.0));
        int popping = int(std::ceil(double(p.air) * 10.0 / 300.0)) - full;
        for (int i = 0; i < full + popping; ++i)
            icon(icons, L + i * 8, rowY + 10, i < full ? 16 : 25, 18);
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
        float u0 = float(tile & 15) / 16.0f, v0 = 1.0f - float(tile >> 4) / 16.0f;
        Tex3DS_SubTexture sub = {16, 16, u0, v0, u0 + 1.0f / 16.0f, v0 - 1.0f / 16.0f};
        C2D_ImageTint t;
        C2D_AlphaImageTint(&t, 0.9f);
        C2D_DrawImageAt({atlas, &sub}, edgeL() - 20, 70, 0.5f, &t, 11.0f, 11.0f);
        Tex3DS_SubTexture flip = {16, 16, u0 + 1.0f / 16.0f, v0, u0, v0 - 1.0f / 16.0f};
        C2D_DrawImageAt({atlas, &flip}, edgeR() - 156, 70, 0.5f, &t, 11.0f, 11.0f);
    }
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
    const float bottom = 240 - 36;
    for (size_t i = 0; i < chat.size() && i < lines; ++i) {
        int a = alpha(i);
        if (!chatOpen && chat[i].age >= 200) break;
        if (a <= 2) continue;
        float y = bottom - float(i) * 9.0f;
        C2D_DrawRectSolid(2, y - 1, 0.5f, 320, 9, C2D_Color32(0, 0, 0, a / 2));
        font.draw(chat[i].text, 2, y, 0xFFFFFF | (uint32_t(a) << 24));
    }
}

void Hud::itemIcon(int id, int damage, float x, float y) { // RenderItem.renderItemIntoGUI, flat
    const ItemInfo* info = itemInfo(id, damage);
    C3D_Tex* tex;
    int tile;
    float th; // texture height in pixels
    if (id < 256) {
        tile = info && info->icon >= 0 ? info->icon : 0;
        if (info && (info->flags & IF_BLOCK3D)) tile = BLOCKS[id].tex[damage & 15][2];
        tex = renderer ? renderer->terrainTexture() : nullptr;
        th = 256.0f;
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

