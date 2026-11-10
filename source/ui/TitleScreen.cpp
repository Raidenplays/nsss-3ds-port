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

// GuiMainMenu.minecraftLogo / subtitleLogo
const char* const LOGO[5] = {" *   * * *   * *** *** *** *** *** ***", " ** ** * **  * *   *   * * * * *    * ",
                             " * * * * * * * **  *   **  *** **   * ", " *   * * *  ** *   *   * * * * *    * ",
                             " *   * * *   * *** *** * * * * *    * "};
const char* const SUBTITLE[5] = {" *  * * *** *** *   *** *  *", "* * * * *   * * *   * * ** *",
                                 "***  *  **  * * *   * * * **", "* * * * *   * * *   * * *  *",
                                 "* * * * *** *** *** *** *  *"};
constexpr float SUBTITLE_SCALE = 0.55f, SUBTITLE_GAP = 3.9f;

// ---- tiny matrix helpers (row-major, column vectors, like glTranslate/glRotate/glScale) ----
struct M4 {
    float m[16];
    M4() { std::memset(m, 0, sizeof m); m[0] = m[5] = m[10] = m[15] = 1; }
    void mul(const float* b) {
        float r[16];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r[i * 4 + j] = m[i * 4] * b[j] + m[i * 4 + 1] * b[4 + j] + m[i * 4 + 2] * b[8 + j] + m[i * 4 + 3] * b[12 + j];
        std::memcpy(m, r, sizeof r);
    }
    void translate(float x, float y, float z) { float t[16] = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1}; mul(t); }
    void scale(float x, float y, float z) { float t[16] = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1}; mul(t); }
    void rotate(float deg, int axis) {
        float a = deg * PI / 180.0f, c = std::cos(a), s = std::sin(a);
        float r[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (axis == 0) { r[5] = c; r[6] = -s; r[9] = s; r[10] = c; }
        else if (axis == 1) { r[0] = c; r[2] = s; r[8] = -s; r[10] = c; }
        else { r[0] = c; r[1] = -s; r[4] = s; r[5] = c; }
        mul(r);
    }
    void apply(float x, float y, float z, float& ox, float& oy, float& oz) const {
        ox = m[0] * x + m[1] * y + m[2] * z + m[3];
        oy = m[4] * x + m[5] * y + m[6] * z + m[7];
        oz = m[8] * x + m[9] * y + m[10] * z + m[11];
    }
};

void quad(std::vector<axm::ModelVertex>& out, const M4& m, const float p[4][3], const float uv[4][2], float shade) {
    axm::ModelVertex v[4];
    for (int k = 0; k < 4; k++) {
        m.apply(p[k][0], p[k][1], p[k][2], v[k].x, v[k].y, v[k].z);
        v[k].shade = shade;
        v[k].u = uv[k][0];
        v[k].v = uv[k][1];
    }
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int k : T) out.push_back(v[k]);
}

// RenderBlocks.func_1238_a for a full cube centred on the origin, one texture rectangle
void cube(std::vector<axm::ModelVertex>& out, const M4& m, float u0, float v0, float u1, float v1, float shade) {
    const float a = -0.5f, b = 0.5f;
    const float F[6][4][3] = {
        {{a, a, b}, {a, a, a}, {b, a, a}, {b, a, b}}, // bottom
        {{b, b, b}, {b, b, a}, {a, b, a}, {a, b, b}}, // top
        {{a, b, a}, {b, b, a}, {b, a, a}, {a, a, a}}, // north
        {{a, b, b}, {a, a, b}, {b, a, b}, {b, b, b}}, // south
        {{a, b, b}, {a, b, a}, {a, a, a}, {a, a, b}}, // west
        {{b, a, b}, {b, a, a}, {b, b, a}, {b, b, b}}, // east
    };
    const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    for (auto& f : F) quad(out, m, f, uv, shade);
}

std::string pickSplash(uint32_t& rng) {
    std::vector<std::string> lines;
    FILE* f = fopen("romfs:/axeolon/title/splashes.txt", "r");
    if (f) {
        char buf[256];
        while (fgets(buf, sizeof buf, f)) {
            std::string s(buf);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            size_t st = s.find_first_not_of(' ');
            if (st == std::string::npos || s[st] == '#') continue;
            lines.push_back(s.substr(st));
        }
        fclose(f);
    }
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    std::string s = lines.empty() ? "missingno" : lines[rng % lines.size()];
    // GuiMainMenu.initGui dates, then AxeolonSeasons.splash
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    if (t) {
        int m = t->tm_mon + 1, d = t->tm_mday;
        if (m == 11 && d == 9) s = "Happy birthday, ez!";
        else if (m == 6 && d == 1) s = "Happy birthday, Notch!";
        else if (m == 12 && d == 24) s = "Merry X-mas!";
        else if (m == 1 && d == 1) s = "Happy new year!";
        if (m == 12 && d >= 24 && d <= 26) s = "Merry X-mas!";
        else if (m == 10 && d == 31) s = "OOoooOOOoooo! Spooky!";
        else if (m == 1 && d == 1) s = "Happy new year!";
        else if (m == 6 && d == 20) s = "Happy Birthday Raiden!";
        else if (m == 4 && d == 1) {
            static const char* const CODE[] = {"func_21110_c()!", "field_1756_g = true;", "Now with 100% more func_6255_d!",
                                               "var1 + var2 = var3!", "Obfuscated!"};
            s = CODE[rng % 5];
        }
    }
    return s;
}

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
    for (int i = 0; i < 6; i++)
        panorama[i] = r.texture("romfs:/axeolon/title/panorama_" + std::to_string(i) + ".rgba");
    black = r.texture("romfs:/textures/title/black.rgba");
    gui = r.texture("romfs:/textures/gui/gui.rgba");
    background = r.texture("romfs:/textures/gui/background.rgba");
    panoSub = {256, 256, 0, 1, 1, 0};
    bgSub = {16, 16, 0, 1, 1, 0};
    splash = pickSplash(rng);
    // LogoEffectRandomizer: every block starts (10 + row + rand*32 + column) above its place
    int lw = int(std::strlen(LOGO[0])), sw = int(std::strlen(SUBTITLE[0]));
    for (int x = 0; x < lw; x++)
        for (int y = 0; y < 5; y++) {
            double h = 10 + y + randf() * 32.0 + x;
            logoFx.push_back({h, h, 0});
        }
    for (int x = 0; x < sw; x++)
        for (int y = 0; y < 5; y++) {
            double h = 10 + y + randf() * 32.0 + x;
            subFx.push_back({h, h, 0});
        }
    open(MAIN);
    return true;
}

void TitleScreen::shutdown() {
    if (bottom) C3D_RenderTargetDelete(bottom);
    bottom = nullptr;
}

void TitleScreen::tick() {
    ++panoramaTimer;
    auto step = [](Fall& f) {
        f.prev = f.now;
        if (f.now > 0.0) f.speed -= 0.6;
        f.now += f.speed;
        f.speed *= 0.9;
        if (f.now < 0.0) { f.now = 0.0; f.speed = 0.0; }
    };
    for (auto& f : logoFx) step(f);
    for (auto& f : subFx) step(f);
}

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
    case MAIN:
        buttons.push_back({1, cx - 100, 44, 200, 20, "Singleplayer"});
        buttons.push_back({2, cx - 100, 68, 200, 20, "Multiplayer", false});
        buttons.push_back({3, cx - 100, 92, 200, 20, "Mods and Texture Packs", false});
        buttons.push_back({0, cx - 100, 128, 98, 20, "Options..."});
        buttons.push_back({4, cx + 2, 128, 98, 20, "Quit Game"});
        break;
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
        static const char* const DYN[3] = {"Dynamic lights: OFF", "Dynamic lights: Fast", "Dynamic lights: Fancy"};
        buttons.push_back({66, cx - 155, 112, 150, 20, DYN[o.dynamicLights % 3]});
        buttons.push_back({67, cx + 5, 112, 150, 20, "Controls..."});
        buttons.push_back({69, cx - 155, 136, 150, 20, o.craftStyle ? "Crafting: Pocket" : "Crafting: Classic"});
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
    case 66: o.dynamicLights = (o.dynamicLights + (dir < 0 ? 2 : 1)) % 3; break;
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

void TitleScreen::drawPanorama(float partial) {
    // GuiMainMenu.drawPanorama (sharp: one pass, 85 degree view)
    tris.clear();
    C3D_Mtx proj;
    Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(85.0f), C3D_AspectRatioTop, 0.05f, 10.0f, false);
    float t = float(panoramaTimer) + partial;
    for (int face = 0; face < 6; face++) {
        if (!panorama[face]) continue;
        M4 m;
        m.rotate(180.0f, 0);
        m.rotate(std::sin(t / 400.0f) * 25.0f + 20.0f, 0);
        m.rotate(-t * 0.1f, 1);
        if (face == 1) m.rotate(90.0f, 1);
        if (face == 2) m.rotate(180.0f, 1);
        if (face == 3) m.rotate(-90.0f, 1);
        if (face == 4) m.rotate(90.0f, 0);
        if (face == 5) m.rotate(-90.0f, 0);
        const float p[4][3] = {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}};
        const float uv[4][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        tris.clear();
        quad(tris, m, p, uv, 1.0f);
        renderer->drawFree(proj, tris, panorama[face], 0, false);
    }
}

void TitleScreen::drawLogo(float partial) {
    C3D_Mtx proj;
    Mtx_PerspTilt(&proj, C3D_AngleFromDegrees(70.0f), 400.0f / 120.0f, 0.05f, 100.0f, false);
    const int lw = int(std::strlen(LOGO[0])), sw = int(std::strlen(SUBTITLE[0]));
    // stone, tile 1 of terrain.png in the 256x1024 atlas
    const float u0 = 16.0f / 256.0f, u1 = 32.0f / 256.0f, v0 = 0.0f, v1 = 16.0f / 1024.0f;
    for (int pass = 0; pass < 3; ++pass) {
        tris.clear();
        M4 base;
        base.translate(0.4f, 0.6f, -13.0f);
        if (pass == 0) {
            base.translate(0.0f, -0.4f, 0.0f);
            base.scale(0.98f, 1.0f, 1.0f);
        }
        base.scale(1.0f, -1.0f, 1.0f);
        base.rotate(15.0f, 0);
        base.scale(0.89f, 1.0f, 0.4f);
        base.translate(-lw * 0.5f, -5 * 0.5f, 0.0f);
        auto pattern = [&](const char* const* rows, int w, const std::vector<Fall>& fx, const M4& at) {
            for (int row = 0; row < 5; ++row)
                for (int col = 0; col < w; ++col) {
                    if (rows[row][col] == ' ') continue;
                    const Fall& f = fx[col * 5 + row];
                    float height = float(f.prev + (f.now - f.prev) * partial);
                    float scale = 1.0f, bright = 1.0f;
                    if (pass == 0) {
                        scale = height * 0.04f + 1.0f;
                        bright = 1.0f / scale;
                        height = 0.0f;
                    }
                    M4 m = at;
                    m.translate(float(col), float(row), height);
                    m.scale(scale, scale, scale);
                    if (pass == 0) cube(tris, m, 0, 0, 1, 1, bright);
                    else cube(tris, m, u0, v0, u1, v1, bright);
                }
        };
        pattern(LOGO, lw, logoFx, base);
        M4 sub = base;
        sub.translate(lw * 0.5f, 5 * 0.5f, 0.0f);
        sub.translate(0.0f, SUBTITLE_GAP, 0.0f);
        sub.scale(SUBTITLE_SCALE, SUBTITLE_SCALE, SUBTITLE_SCALE);
        sub.translate(-sw * 0.5f, -5 * 0.5f, 0.0f);
        pattern(SUBTITLE, sw, subFx, sub);
        if (pass == 0) renderer->drawFree(proj, tris, black, 1, true, 1);
        else if (pass == 1) renderer->drawFree(proj, tris, renderer->terrainTexture(), 0, true, 1);
        else renderer->drawFree(proj, tris, renderer->terrainTexture(), 3, true, 1);
    }
}

void TitleScreen::drawTop2D() {
    // Darken the panorama (two gradients in Java: a flat 25% and a top-to-bottom one)
    C2D_DrawRectangle(0, 0, 0.1f, 400, 240, C2D_Color32(0, 0, 0, 64), C2D_Color32(0, 0, 0, 64), C2D_Color32(0, 0, 0, 191),
                      C2D_Color32(0, 0, 0, 191));
    font.draw("Minecraft Axeolon 1.2", 2, 2, 0x505050);
    std::string c = "Copyright Mojang AB. Do not distribute.";
    font.draw(c, 400 - font.width(c) - 2, 240 - 10, 0xFFFFFF);
    // the splash: yellow, tilted 20 degrees, pulsing (Java: width/2 + 125, 74 on a ~427-wide GUI)
    float pulse = 1.8f - std::fabs(std::sin(float(osGetTime() % 1000) / 1000.0f * PI * 2.0f) * 0.1f);
    float s = pulse * 100.0f / float(font.width(splash) + 32);
    font.drawRotated(splash, 200 + 125 * 400.0f / 427.0f, 74, -8, -20.0f, 0xFFFF00, s);
}

void TitleScreen::drawButton(const Button& b, bool hover) {
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
    if (screen == MAIN && panorama[0]) {
        // the front of the panorama behind the menu, darkened like the top
        C2D_DrawImageAt({panorama[0], &panoSub}, -32, -40, 0.1f, nullptr, 1.5f, 1.5f);
        C2D_DrawRectSolid(0, 0, 0.2f, 320, 240, C2D_Color32(0, 0, 0, 120));
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
    for (size_t i = 0; i < buttons.size(); i++) drawButton(buttons[i], int(i) == selected);
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
    renderer->beginTitle();
    drawPanorama(partial);
    drawLogo(partial);
    renderer->prepare2D();
    C2D_SceneBegin(renderer->topTarget(0));
    drawTop2D();
    drawBottom();
}
