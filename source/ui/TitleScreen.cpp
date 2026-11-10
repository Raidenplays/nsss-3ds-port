#include "TitleScreen.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "render/Renderer.h"
#include "world/Nbt.h"
#include "game/Options.h"
#include "audio/Sound.h"

bool TitleScreen::canCreateWorlds = true;

namespace {

constexpr float PI = 3.14159265358979f;

bool removeTree(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return true;
    if (S_ISDIR(st.st_mode)) {
        DIR* d = opendir(path.c_str());
        if (d) {
            while (dirent* e = readdir(d)) {
                std::string n = e->d_name;
                if (n == "." || n == "..") continue;
                removeTree(path + "/" + n);
            }
            closedir(d);
        }
        return rmdir(path.c_str()) == 0;
    }
    return unlink(path.c_str()) == 0;
}

} // namespace

float TitleScreen::randf() {
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return float(rng & 0xFFFFFF) / float(0x1000000);
}

bool TitleScreen::init(Renderer& r, const std::string& saves) {
    renderer = &r;
    savesDir = saves;
    rng ^= uint32_t(time(nullptr));
    bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (!font.load(r)) return false;
    peTouch = r.texture("romfs:/textures/pe/touchgui.rgba");
    peLogo = r.texture("romfs:/textures/pe/title.rgba");
    peDirt = r.texture("romfs:/textures/pe/bg32.rgba");
    black = r.texture("romfs:/textures/title/black.rgba");
    gui = r.texture("romfs:/textures/gui/gui.rgba");
    background = r.texture("romfs:/textures/gui/background.rgba");
    bgSub = {16, 16, 0, 1, 1, 0};
    open(MAIN);
    return true;
}

void TitleScreen::shutdown() {
    if (bottom) C3D_RenderTargetDelete(bottom);
    bottom = nullptr;
}

void TitleScreen::tick() {}

void TitleScreen::scanSlots() {
    for (int i = 0; i < 5; i++) {
        Slot& s = slots[i];
        s = Slot();
        std::string dir = savesDir + "/World" + std::to_string(i + 1);
        struct stat st;
        if (stat(dir.c_str(), &st) != 0) continue;
        nbt::TagPtr root = nbt::readGzipFile(dir + "/level.dat");
        if (!root) root = nbt::readGzipFile(dir + "/level.dat_old");
        nbt::Tag* d = root ? root->getCompound("Data") : nullptr;
        if (!d) { s.unreadable = true; continue; }
        s.exists = true;
        s.mb = float(d->getLong("SizeOnDisk") / 1024LL * 100LL / 1024LL) / 100.0f;
        s.gameType = d->getInt("GameType");
    }
}

void TitleScreen::open(Screen s) {
    screen = s;
    buttons.clear();
    selected = 0;
    const float cx = 160; // width / 2
    switch (s) {
    case MAIN: {
        // PE 0.5.0: Join Game, Start Game, Options; touchgui.png's 76x75 squares at 1.25x
        const float w = 95, h = 94, gap = (320 - 3 * w) / 4, y = 72;
        Button join{2, gap, y, w, h, "Join Game", false};
        join.icon = 2;
        Button start{1, gap * 2 + w, y, w, h, "Start Game"};
        start.icon = 1;
        Button opts{0, gap * 3 + w * 2, y, w, h, "Options"};
        opts.icon = 0;
        buttons = {join, start, opts};
        selected = 1;
        held = -1;
        padUsed = false;
        break;
    }
    case SELECT:
    case DELETE: {
        scanSlots();
        for (int i = 0; i < 5; i++) {
            std::string t;
            if (slots[i].exists) {
                char mb[32];
                snprintf(mb, sizeof mb, "%g", slots[i].mb);
                t = "World " + std::to_string(i + 1) + " (" + mb + " MB)";
            } else {
                t = slots[i].unreadable ? "World " + std::to_string(i + 1) + " (unreadable)" : "- empty -";
            }
            buttons.push_back({10 + i, cx - 100, 40.0f + 24 * i, 200, 20, t});
        }
        if (s == SELECT) buttons.push_back({20, cx - 100, 40 + 120 + 12, 200, 20, "Delete world..."});
        buttons.push_back({21, cx - 100, 40 + 168, 200, 20, "Cancel"});
        break;
    }
    case CREATE:
        buttons.push_back({30, cx - 100, 60 + 24, 200, 20,
                           std::string("Game Mode: ") + (createGameType == 1 ? "Creative" : "Survival")});
        buttons.push_back({31, cx - 100, 60 + 96, 200, 20, "Create World", canCreateWorlds});
        buttons.push_back({32, cx - 100, 60 + 120, 200, 20, "Cancel"});
        selected = canCreateWorlds ? 1 : 2;
        break;
    case CONFIRM:
        buttons.push_back({40, cx - 155, 40 + 96, 150, 20, "Yes"});
        buttons.push_back({41, cx - 155 + 160, 40 + 96, 150, 20, "No"});
        selected = 1;
        break;
    case OPTIONS: { // GuiOptions: two columns; left / right moves a slider
        const Options& o = gOptions;
        char b[64];
        auto pct = [](float v) { return int(v * 100.0f + 0.5f); };
        if (o.music <= 0.0f) snprintf(b, sizeof b, "Music: OFF"); else snprintf(b, sizeof b, "Music: %d%%", pct(o.music));
        buttons.push_back({60, cx - 155, 40, 150, 20, b});
        if (o.sound <= 0.0f) snprintf(b, sizeof b, "Sound: OFF"); else snprintf(b, sizeof b, "Sound: %d%%", pct(o.sound));
        buttons.push_back({61, cx + 5, 40, 150, 20, b});
        if (o.sensitivity <= 0.0f) snprintf(b, sizeof b, "Look speed: *yawn*");
        else if (o.sensitivity >= 1.0f) snprintf(b, sizeof b, "Look speed: HYPERSPEED!!!");
        else snprintf(b, sizeof b, "Look speed: %d%%", pct(o.sensitivity * 2.0f));
        buttons.push_back({62, cx - 155, 64, 150, 20, b});
        buttons.push_back({63, cx + 5, 64, 150, 20, std::string("Difficulty: ") + Options::difficultyName(o.difficulty)});
        buttons.push_back({64, cx - 155, 88, 150, 20, o.bobView ? "View bobbing: ON" : "View bobbing: OFF"});
        buttons.push_back({65, cx + 5, 88, 150, 20, o.invertY ? "Invert look: ON" : "Invert look: OFF"});
        buttons.push_back({69, cx - 155, 112, 150, 20, o.craftStyle ? "Crafting: Pocket" : "Crafting: Classic"});
        buttons.push_back({67, cx + 5, 112, 150, 20, "Controls..."});
        buttons.push_back({50, cx - 100, 40 + 168, 200, 20, "Done"});
        break;
    }
    case CONTROLS:
        buttons.push_back({68, cx - 100, 40 + 168, 200, 20, "Done"});
        break;
    }
}

// left / right on an options button: -1 or +1 a step (sliders), or the same as pressing it
void TitleScreen::nudge(const Button& b, int dir) {
    Options& o = gOptions;
    auto step = [&](float& v) { v += 0.1f * float(dir); v = v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; };
    int keep = selected;
    switch (b.id) {
    case 60: step(o.music); break;
    case 61: step(o.sound); break;
    case 62: step(o.sensitivity); break;
    case 63: o.difficulty = (o.difficulty + (dir < 0 ? 3 : 1)) & 3; break;
    case 64: o.bobView = !o.bobView; break;
    case 65: o.invertY = !o.invertY; break;
    case 69: o.craftStyle ^= 1; break;
    default: return;
    }
    sound::setVolumes(o.sound, o.music);
    o.save();
    open(OPTIONS);
    selected = keep;
}

void TitleScreen::press(const Button& b) {
    if (!b.enabled) return;
    switch (b.id) {
    case 0: open(OPTIONS); break;
    case 1: open(SELECT); break;
    case 4: chosen.clear(); break;
    case 20: open(DELETE); break;
    case 21: open(screen == DELETE ? SELECT : MAIN); break;
    case 30:
        createGameType = createGameType == 1 ? 0 : 1;
        open(CREATE);
        break;
    case 31:
        chosen = "World" + std::to_string(pendingSlot);
        newWorld = true;
        chosenGameType = createGameType;
        break;
    case 32: open(SELECT); break;
    case 40:
        deleteWorld(pendingSlot);
        open(SELECT);
        break;
    case 41: open(DELETE); break;
    case 50: open(MAIN); break;
    case 60: case 61: case 62: case 63: case 64: case 65: case 66: case 69: nudge(b, 1); break;
    case 67: open(CONTROLS); break;
    case 68: open(OPTIONS); break;
    default:
        if (b.id >= 10 && b.id < 15) {
            int slot = b.id - 9;
            pendingSlot = slot;
            const Slot& s = slots[slot - 1];
            if (screen == DELETE) {
                if (s.exists || s.unreadable) open(CONFIRM);
            } else if (s.unreadable) {
                // GuiWorldUnreadable: never offer to create a new world over it
            } else if (!s.exists) {
                createGameType = 0;
                open(CREATE);
            } else {
                chosen = "World" + std::to_string(slot);
                newWorld = false;
                chosenGameType = s.gameType;
            }
        }
        break;
    }
}

TitleScreen::Result TitleScreen::input(u32 down, const touchPosition& touch) {
    chosen.clear();
    bool quit = false;
    auto activate = [&](const Button& b) {
        if (b.id == 4 && b.enabled) quit = true;
        press(b);
    };
    if (screen == MAIN) {
        // PE: a button lights while the touch is on it and acts when the touch lifts there
        auto under = [&](const touchPosition& t) {
            for (size_t i = 0; i < buttons.size(); i++) {
                const Button& b = buttons[i];
                if (t.px >= b.x && t.px < b.x + b.w && t.py >= b.y && t.py < b.y + b.h) return int(i);
            }
            return -1;
        };
        u32 heldKeys = hidKeysHeld(), up = hidKeysUp();
        if (down & KEY_TOUCH) { held = under(touch); padUsed = false; }
        else if (heldKeys & KEY_TOUCH) { if (held >= 0 && under(touch) != held) held = -1; }
        if ((up & KEY_TOUCH) && held >= 0) {
            selected = held;
            Button copy = buttons[held];
            held = -1;
            activate(copy);
        }
        if (!(heldKeys & KEY_TOUCH) && !(up & KEY_TOUCH)) held = -1;
        if (down & KEY_DLEFT) { selected = (selected + 2) % 3; padUsed = true; }
        if (down & KEY_DRIGHT) { selected = (selected + 1) % 3; padUsed = true; }
        if (down & KEY_A) {
            Button copy = buttons[selected];
            activate(copy);
        }
        if (down & KEY_START) quit = true;
        if (quit) return QUIT;
        return chosen.empty() ? NONE : PLAY;
    }
    if (down & KEY_TOUCH) {
        for (size_t i = 0; i < buttons.size(); i++) {
            const Button& b = buttons[i];
            if (touch.px >= b.x && touch.px < b.x + b.w && touch.py >= b.y && touch.py < b.y + b.h) {
                selected = int(i);
                Button copy = b;
                activate(copy);
                break;
            }
        }
    }
    if (!buttons.empty()) {
        if (down & KEY_DDOWN) selected = (selected + 1) % int(buttons.size());
        if (down & KEY_DUP) selected = (selected + int(buttons.size()) - 1) % int(buttons.size());
        if (down & KEY_A) {
            Button copy = buttons[selected];
            activate(copy);
        }
        if (screen == OPTIONS && (down & (KEY_DLEFT | KEY_DRIGHT))) {
            Button copy = buttons[selected];
            nudge(copy, (down & KEY_DLEFT) ? -1 : 1);
        }
    }
    if (down & KEY_B) {
        if (screen == SELECT || screen == OPTIONS) open(MAIN);
        else if (screen == CONTROLS) open(OPTIONS);
        else if (screen == DELETE || screen == CREATE) open(SELECT);
        else if (screen == CONFIRM) open(DELETE);
    }
    if (quit) return QUIT;
    return chosen.empty() ? NONE : PLAY;
}

bool TitleScreen::deleteWorld(int slot) {
    return removeTree(savesDir + "/World" + std::to_string(slot));
}

// ---------------------------------------------------------------- drawing

namespace {
// a w x h piece of a texW x texH texture at (x, y), in citro2d's flipped v
Tex3DS_SubTexture subOf(float texW, float texH, float x, float y, float w, float h) {
    return {uint16_t(w), uint16_t(h), x / texW, 1.0f - y / texH, (x + w) / texW, 1.0f - (y + h) / texH};
}
} // namespace

void TitleScreen::drawPeDirt(float w, float h) {
    if (!peDirt) return;
    Tex3DS_SubTexture d = subOf(32, 32, 0, 0, 32, 32);
    for (float y = 0; y < h; y += 64)
        for (float x = 0; x < w; x += 64) C2D_DrawImageAt({peDirt, &d}, x, y, 0.1f, nullptr, 2.0f, 2.0f);
}

void TitleScreen::drawPeTop() {
    // PE 0.5.0's title: dirt, the Pocket Edition logo (0.5.0's title.png: 256x64, logo at y 1-63),
    // the version line under it, and the credit in the corner
    drawPeDirt(400, 240);
    if (peLogo) {
        Tex3DS_SubTexture logo = subOf(256, 64, 0, 1, 256, 63);
        const float s = 1.4f;
        C2D_DrawImageAt({peLogo, &logo}, 200 - 256 * s / 2, 28, 0.5f, nullptr, s, s);
    }
    font.drawCentered("NSSS a1.1.12", 200, 28 + 63 * 1.4f + 10, 0xE0E0E0, true, 1.5f);
    font.draw("Mojang AB", 400 - font.width("Mojang AB") * 1.5f - 4, 240 - 16, 0xFFFFFF, true, 1.5f);
}

void TitleScreen::drawPeButton(const Button& b, bool lit) {
    // touchgui.png: 76x75 squares, normal at x 0 and pressed at x 76; rows y 26 / 101 / 176
    if (peTouch) {
        Tex3DS_SubTexture sq = subOf(256, 256, lit ? 76 : 0, 26 + 75 * b.icon, 76, 75);
        C2D_ImageTint dim;
        C2D_PlainImageTint(&dim, C2D_Color32(0, 0, 0, 255), 0.45f); // greyed out: 45% toward black
        C2D_DrawImageAt({peTouch, &sq}, b.x, b.y, 0.5f, b.enabled ? nullptr : &dim, b.w / 76.0f, b.h / 75.0f);
    }
    font.drawCentered(b.text, b.x + b.w / 2, b.y + 12, b.enabled ? 0xFFFFFF : 0x909090);
}

void TitleScreen::drawButton(const Button& b, bool hover) {
    if (b.icon >= 0) return drawPeButton(b, hover);
    // GuiButton: the 200x20 sprite at gui.png (0, 46 + state*20), drawn as two halves
    int state = !b.enabled ? 0 : hover ? 2 : 1;
    float half = b.w / 2.0f;
    Tex3DS_SubTexture left = {uint16_t(half), 20, 0.0f, 1.0f - (46 + state * 20) / 256.0f, half / 256.0f,
                              1.0f - (66 + state * 20) / 256.0f};
    Tex3DS_SubTexture right = {uint16_t(half), 20, (200 - half) / 256.0f, 1.0f - (46 + state * 20) / 256.0f,
                               200 / 256.0f, 1.0f - (66 + state * 20) / 256.0f};
    if (gui) {
        C2D_DrawImageAt({gui, &left}, b.x, b.y, 0.5f);
        C2D_DrawImageAt({gui, &right}, b.x + half, b.y, 0.5f);
    }
    uint32_t col = !b.enabled ? 0xA0A0A0 : hover ? 0xFFFFA0 : 0xE0E0E0;
    font.drawCentered(b.text, b.x + b.w / 2, b.y + (b.h - 8) / 2, col);
}

void TitleScreen::drawBottom() {
    // the title and the in-game bottom screen each have a target on the bottom screen; whichever
    // was linked last is shown, so take it back before drawing
    C3D_RenderTargetSetOutput(bottom, GFX_BOTTOM, GFX_LEFT, GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(bottom);
    if (screen == MAIN) {
        drawPeDirt(320, 240);
    } else if (background) {
        // GuiScreen.drawWorldBackground: dirt at 32 px a tile, tinted 0x404040
        for (int y = 0; y < 240; y += 32)
            for (int x = 0; x < 320; x += 32) C2D_DrawImageAt({background, &bgSub}, x, y, 0.1f, nullptr, 2.0f, 2.0f);
        C2D_DrawRectSolid(0, 0, 0.2f, 320, 240, C2D_Color32(0, 0, 0, 191)); // 0x40 brightness
    }
    switch (screen) {
    case MAIN:
        // Credits, small bottom left
        break;
    case SELECT: font.drawCentered("Select World", 160, 20, 0xFFFFFF); break;
    case DELETE: font.drawCentered("Delete world", 160, 20, 0xFFFFFF); break;
    case CREATE: {
        font.drawCentered("Create World " + std::to_string(pendingSlot), 160, 60 - 20, 0xFFFFFF);
        std::string d = createGameType == 1 ? "Unlimited blocks, free flight, no damage"
                                            : "Gather resources, craft, and survive";
        font.drawCentered(d, 160, 60 + 50, 0xA0A0A0);
        if (!canCreateWorlds) font.drawCentered("(world generation is still being ported)", 160, 60 + 64, 0xA0A0A0);
        break;
    }
    case CONFIRM:
        font.drawCentered("Are you sure you want to delete this world?", 160, 70, 0xFFFFFF);
        font.drawCentered("'World" + std::to_string(pendingSlot) + "' will be lost forever!", 160, 90, 0xFFFFFF);
        break;
    case OPTIONS:
        font.drawCentered("Options", 160, 20, 0xFFFFFF);
        font.drawCentered("Left / right changes the one picked", 160, 150, 0xA0A0A0);
        break;
    case CONTROLS:
        font.drawCentered("Controls", 160, 20, 0xFFFFFF);
        font.drawCentered("Old 3DS: Circle Pad move, ABXY look,", 160, 50, 0xA0A0A0);
        font.drawCentered("D-pad up jump, down sneak, R break, L use", 160, 62, 0xA0A0A0);
        font.drawCentered("New 3DS / Circle Pad Pro: stick look,", 160, 86, 0xA0A0A0);
        font.drawCentered("A jump, B sneak, ZR break, ZL use,", 160, 98, 0xA0A0A0);
        font.drawCentered("L / R hotbar, X items", 160, 110, 0xA0A0A0);
        break;
    }
    for (size_t i = 0; i < buttons.size(); i++)
        drawButton(buttons[i], screen == MAIN ? (int(i) == held || (padUsed && int(i) == selected)) : int(i) == selected);
}

void TitleScreen::drawLoading(const std::string& title, const std::string& status, float progress) {
    renderer->beginTitle();
    renderer->prepare2D();
    auto dirt = [&](float w, float h) {
        if (background)
            for (int y = 0; y < h; y += 32)
                for (int x = 0; x < w; x += 32) C2D_DrawImageAt({background, &bgSub}, x, y, 0.1f, nullptr, 2.0f, 2.0f);
        C2D_DrawRectSolid(0, 0, 0.2f, w, h, C2D_Color32(0, 0, 0, 191));
    };
    C2D_SceneBegin(renderer->topTarget(0));
    dirt(400, 240);
    font.drawCentered(title, 200, 120 - 4 - 16, 0xFFFFFF);
    font.drawCentered(status, 200, 120 - 4 + 8, 0xFFFFFF);
    if (progress >= 0) {
        float x = 200 - 50, y = 120 + 16;
        C2D_DrawRectSolid(x, y, 0.5f, 100, 2, C2D_Color32(0x80, 0x80, 0x80, 255));
        C2D_DrawRectSolid(x, y, 0.6f, 100 * (progress > 1 ? 1 : progress), 2, C2D_Color32(0x80, 0xFF, 0x80, 255));
    }
    // the title and the in-game bottom screen each have a target on the bottom screen; whichever
    // was linked last is shown, so take it back before drawing
    C3D_RenderTargetSetOutput(bottom, GFX_BOTTOM, GFX_LEFT, GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    C2D_TargetClear(bottom, C2D_Color32(0, 0, 0, 255));
    C2D_SceneBegin(bottom);
    dirt(320, 240);
}

void TitleScreen::draw(float partial) {
    (void)partial;
    renderer->beginTitle();
    renderer->prepare2D();
    C2D_SceneBegin(renderer->topTarget(0));
    drawPeTop();
    drawBottom();
}
