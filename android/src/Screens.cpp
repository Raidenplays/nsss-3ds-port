#include "Screens.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <random>
#include "Font.h"
#include "Gfx.h"
#include "world/SaveHandler.h"
#include <sys/stat.h>
#include "Edition.h"

namespace ui {
namespace {

std::unique_ptr<Screen> gScreen, gNext;
Choice gChoice;
bool gChosen = false;
const char* SAVES = "sdmc:/3ds/axeolon3ds/saves";
bool worldExists(int i) {
    std::string p = std::string(SAVES) + "/World" + std::to_string(i) + "/level.dat";
    if (FILE* f = std::fopen(p.c_str(), "rb")) { std::fclose(f); return true; }
    return false;
}
const char* SPRITES = "images/gui/spritesheet.png";

// ---- 4x4 matrices (column-major, like GL) ----
struct M4 {
    float m[16];
    static M4 identity() {
        M4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1;
        return r;
    }
    M4 operator*(const M4& b) const {
        M4 r{};
        for (int c = 0; c < 4; ++c)
            for (int rr = 0; rr < 4; ++rr) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k * 4 + rr] * b.m[c * 4 + k];
                r.m[c * 4 + rr] = s;
            }
        return r;
    }
    static M4 rotate(float deg, float x, float y, float z) {
        float a = deg * 3.14159265f / 180.0f, c = std::cos(a), s = std::sin(a), t = 1 - c;
        M4 r = identity();
        r.m[0] = t * x * x + c;     r.m[4] = t * x * y - s * z; r.m[8] = t * x * z + s * y;
        r.m[1] = t * x * y + s * z; r.m[5] = t * y * y + c;     r.m[9] = t * y * z - s * x;
        r.m[2] = t * x * z - s * y; r.m[6] = t * y * z + s * x; r.m[10] = t * z * z + c;
        return r;
    }
    static M4 perspective(float fovDeg, float aspect, float n, float f) {
        float t = 1.0f / std::tan(fovDeg * 3.14159265f / 360.0f);
        M4 r{};
        r.m[0] = t / aspect;
        r.m[5] = t;
        r.m[10] = (f + n) / (n - f);
        r.m[11] = -1;
        r.m[14] = 2 * f * n / (n - f);
        return r;
    }
};

// The Axeolon title logo, from the 3DS title screen (GuiMainMenu.minecraftLogo / subtitleLogo)
const char* const LOGO[5] = {" *   * * *   * *** *** *** *** *** ***", " ** ** * **  * *   *   * * * * *    * ",
                             " * * * * * * * **  *   **  *** **   * ", " *   * * *  ** *   *   * * * * *    * ",
                             " *   * * *   * *** *** * * * * *    * "};
const char* const SUBTITLE[5] = {" *  * * *** *** *   *** *  *", "* * * * *   * * *   * * ** *",
                                 "***  *  **  * * *   * * * **", "* * * * *   * * *   * * *  *",
                                 "* * * * *** *** *** *** *  *"};

std::string pickSplash() {
    std::vector<std::string> lines;
    std::vector<uint8_t> data;
    if (gfx::readAsset("axeolon/title/splashes.txt", data)) {
        std::string all(data.begin(), data.end()), line;
        for (size_t i = 0; i <= all.size(); ++i) {
            if (i == all.size() || all[i] == '\n') {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                size_t st = line.find_first_not_of(' ');
                if (st != std::string::npos && line[st] != '#') lines.push_back(line.substr(st));
                line.clear();
            } else {
                line += all[i];
            }
        }
    }
    std::mt19937 rng(uint32_t(std::time(nullptr)));
    std::string s = lines.empty() ? "missingno" : lines[rng() % lines.size()];
    time_t now = std::time(nullptr);
    if (struct tm* t = std::localtime(&now)) { // the same dates as the 3DS title screen
        int m = t->tm_mon + 1, d = t->tm_mday;
        if (m == 11 && d == 9) s = "Happy birthday, ez!";
        else if (m == 6 && d == 1) s = "Happy birthday, Notch!";
        if (m == 12 && d >= 24 && d <= 26) s = "Merry X-mas!";
        else if (m == 10 && d == 31) s = "OOoooOOOoooo! Spooky!";
        else if (m == 1 && d == 1) s = "Happy new year!";
        else if (m == 6 && d == 20) s = "Happy Birthday Raiden!";
    }
    return s;
}

// The title screen's background: the panorama on the inside of a slowly turning cube, under a
// light-to-dark wash (renderMenuBackground)
void drawPanorama(float time) {
    float aspect = float(gfx::pixelWidth()) / float(gfx::pixelHeight());
    M4 view = M4::rotate(180, 1, 0, 0) * M4::rotate(std::sin(time / 400.0f) * 25.0f + 20.0f, 1, 0, 0) *
              M4::rotate(-time * 0.1f, 0, 1, 0);
    M4 proj = M4::perspective(120.0f, aspect, 0.05f, 10.0f);
    static const float TURN[6][4] = {{0, 0, 1, 0}, {90, 0, 1, 0}, {180, 0, 1, 0}, {-90, 0, 1, 0}, {90, 1, 0, 0}, {-90, 1, 0, 0}};
    for (int f = 0; f < 6; ++f) {
        M4 m = proj * view * M4::rotate(TURN[f][0], TURN[f][1], TURN[f][2], TURN[f][3]);
        gfx::setMatrix(m.m);
        uint32_t c = 0xFFFFFFFF;
        std::vector<gfx::Vertex> q = {{-1, -1, 1, 0, 0, c}, {1, -1, 1, 1, 0, c}, {1, 1, 1, 1, 1, c},
                                      {-1, -1, 1, 0, 0, c}, {1, 1, 1, 1, 1, c},  {-1, 1, 1, 0, 1, c}};
        gfx::triangles(gfx::texture("axeolon/title/panorama_" + std::to_string(f) + ".rgba"), q);
    }
    gfx::begin2D();
    gfx::gradient(0, 0, float(gfx::guiWidth()), float(gfx::guiHeight()), gfx::rgb(0xFFFFFF, 89), gfx::rgb(0x000000, 89));
}

// The logo as flat stone blocks with a shadow: Pocket Edition's flat title, Axeolon's letters
void drawLogo(float x, float y, float cell) {
    gfx::Texture* terrain = gfx::texture("textures/terrain.rgba");
    auto pattern = [&](const char* const* rows, float ox, float oy, float c, int tile) {
        int tx = tile % 16 * 16, ty = tile / 16 * 16;
        for (int r = 0; r < 5; ++r)
            for (int i = 0; rows[r][i]; ++i)
                if (rows[r][i] == '*') gfx::fill(ox + i * c + c * 0.25f, oy + r * c + c * 0.25f, c, c, gfx::rgb(0x000000, 110));
        for (int r = 0; r < 5; ++r)
            for (int i = 0; rows[r][i]; ++i)
                if (rows[r][i] == '*') gfx::blit(terrain, ox + i * c, oy + r * c, c, c, tx, ty, 16, 16);
    };
    int lw = int(std::strlen(LOGO[0])), sw = int(std::strlen(SUBTITLE[0]));
    pattern(LOGO, x, y, cell, 1); // stone
    float sc = cell * 0.55f;
    pattern(SUBTITLE, x + (lw * cell - sw * sc) / 2.0f, y + cell * 5.6f, sc, 16); // cobblestone
}

// ------------------------------------------------------------------ the start menu

class TitleScreen : public Screen {
public:
    TitleScreen() : splash(pickSplash()) {}
    void layout() override {
        float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
        int lw = int(std::strlen(LOGO[0]));
        cell = std::floor(std::min(w * 0.8f, 300.0f) / float(lw) * 2.0f) / 2.0f;
        logoW = cell * lw;
        logoX = (w - logoW) / 2.0f;
        logoY = 12;
        logoBottom = logoY + cell * 5.6f + cell * 0.55f * 5.0f;
        // StartMenuScreen.setupPlayButtons / setupPositions: 100x30, centred in the space under the logo
        play = Button{1, w / 2 - 50, 0, 100, 30, "Play"};
        float free = h - logoBottom;
        play.y = logoBottom + (free - play.h) / 2.0f - 4.0f;
        settings = Button{2, w - 32 - 2, h - 32 - 2, 32, 32, ""};
        buy = Button{3, w / 2 - 50, play.y + 36, 100, 29, ""};
        if (edition::LITE) play.y -= 16, buy.y -= 16;
    }
    void tick(float dt) override { time += dt * 30.0f; pulse += dt; }
    void render(const Touch& t) override {
        float h = float(gfx::guiHeight());
        if (edition::LITE) { // Pocket Edition Lite: no panorama, the dirt background
            float w = float(gfx::guiWidth());
            gfx::Texture* bg = gfx::texture("gui/background.png");
            if (bg) gfx::quad(bg, 0, 0, w, h, 0, 0, w / 32.0f, h / 32.0f, gfx::rgb(0x404040));
        } else {
            drawPanorama(time);
        }
        drawLogo(logoX, logoY, cell);
        if (edition::LITE) { // the "Lite" tag under the logo
            float lx = logoX + logoW - 34, ly = logoBottom - 2;
            gfx::fill(lx, ly, 32, 12, gfx::rgb(0x000000, 140));
            font::drawCentered("LITE", lx + 16, ly + 2, 0xFFFF55);
        }
        play.render(t);
        settings.render(t);
        if (edition::LITE) { // Pocket Edition Lite's Buy button (gui.png), for the full game
            gfx::blit(gfx::texture("gui/gui.png"), buy.x, buy.y, buy.w, buy.h, 64, 182, 190, 55,
                      buy.held && t.down && buy.contains(t.x, t.y) ? gfx::rgb(0xC0C0C0) : 0xFFFFFFFF);
        }
        gfx::Texture* tg = gfx::texture("images/gui/touchgui.png");
        gfx::blit(tg, settings.x + 5, settings.y + 5, 22, 21, 218, 0, 22, 21); // the cog
        font::draw(edition::LITE ? "Axeolon Lite 1.2" : "Minecraft Axeolon 1.2", 1, h - 20, 0xFFFFFF);
        font::draw("Mojang AB", 1, h - 10, 0xFFFFFF);
        float s = 1.3f + std::pow(std::sin(pulse * 3.14f * 2.3f), 4.0f) * 0.06f;
        float maxW = float(gfx::guiWidth()) * 0.3125f;
        float fit = font::width(splash) * s > maxW ? maxW / float(font::width(splash)) : s;
        font::drawTransformed(splash, logoX + logoW * 0.71f + 20, logoBottom - 12, 0xFFFF00, -20, fit);
    }
    void touch(const Touch& t) override {
        for (Button* b : {&play, &settings, &buy}) {
            if (b == &buy && !edition::LITE) continue;
            if (t.pressed) b->held = b->contains(t.x, t.y);
            if (t.released && b->held && b->contains(t.x, t.y)) {
                b->held = false;
                if (b->id == 1) setScreen(makeSelectWorld());
                if (b->id == 3) setScreen(makeBuy());
            }
            if (t.released) b->held = false;
        }
    }

private:
    std::string splash;
    float time = 0, pulse = 0, cell = 6, logoW = 0, logoX = 0, logoY = 12, logoBottom = 60;
    Button play, settings, buy;
};

// Lite's Buy button: the full game is free, so this just says where to get it
class BuyScreen : public Screen {
public:
    void layout() override {
        float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
        ok = Button{1, w / 2 - 50, h - 44, 100, 30, "Back"};
    }
    void render(const Touch& t) override {
        float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
        gfx::Texture* bg = gfx::texture("gui/background.png");
        if (bg) gfx::quad(bg, 0, 0, w, h, 0, 0, w / 32.0f, h / 32.0f, gfx::rgb(0x404040));
        font::drawCentered("Get the full game", w / 2, 24, 0xFFFF55);
        static const char* const LINES[] = {"Axeolon Lite is the start of the story.",
                                            "The full game adds the Nether, the Bear Queen",
                                            "and The Hunt, spell books and lapis power,",
                                            "every charm, and five worlds.",
                                            "", "It's free: install Minecraft Axeolon."};
        for (int i = 0; i < 6; ++i) font::drawCentered(LINES[i], w / 2, 48 + i * 12.0f, 0xE0E0E0);
        ok.render(t);
        (void)h;
    }
    void touch(const Touch& t) override {
        if (t.pressed) ok.held = ok.contains(t.x, t.y);
        if (t.released) {
            if (ok.held && ok.contains(t.x, t.y)) setScreen(makeTitle());
            ok.held = false;
        }
    }
    bool back() override {
        setScreen(makeTitle());
        return true;
    }

private:
    Button ok;
};

// ------------------------------------------------------------------ choosing a world (first pass)

class SelectWorldScreen : public Screen {
public:
    void layout() override {
        float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
        backBtn = Button{1, 2, 2, 50, 24, "Back"};
        worlds.clear();
        int free = 0;
        for (int i = 1; i <= edition::MAX_WORLDS; ++i) {
            if (worldExists(i)) worlds.push_back(Button{10 + i, w / 2 - 100, 0, 200, 30, "World " + std::to_string(i)});
            else if (!free) free = i;
        }
        for (size_t i = 0; i < worlds.size(); ++i) worlds[i].y = 36 + float(i) * 34;
        create = Button{free ? 100 + free : 0, w / 2 - 100, h - 36, 200, 30, free ? "Create new world" : "No room for more worlds"};
    }
    void render(const Touch& t) override {
        float w = float(gfx::guiWidth()), h = float(gfx::guiHeight());
        gfx::Texture* bg = gfx::texture("images/gui/background.png");
        if (bg) gfx::quad(bg, 0, 0, w, h, 0, 0, w / 32.0f, h / 32.0f, gfx::rgb(0x404040));
        gfx::fill(0, 0, w, 28, gfx::rgb(0x000000, 120));
        font::drawCentered("Select world", w / 2, 10, 0xFFFFFF);
        backBtn.render(t);
        for (Button& b : worlds) b.render(t);
        create.render(t);
        if (worlds.empty()) font::drawCentered("No worlds yet.", w / 2, h / 2 - 20, 0xA0A0A0);
    }
    void touch(const Touch& t) override {
        std::vector<Button*> all = {&backBtn, &create};
        for (Button& b : worlds) all.push_back(&b);
        for (Button* b : all) {
            if (t.pressed) b->held = b->contains(t.x, t.y);
            if (t.released) {
                if (b->held && b->contains(t.x, t.y)) {
                    if (b->id == 1) setScreen(makeTitle());
                    else if (b->id > 100) { // a new world, survival, in the first free slot
                        gChoice = Choice();
                        gChoice.world = "World" + std::to_string(b->id - 100);
                        gChoice.created = true;
                        gChosen = true;
                    } else if (b->id > 10) {
                        gChoice = Choice();
                        gChoice.world = "World" + std::to_string(b->id - 10);
                        gChosen = true;
                    }
                }
                b->held = false;
            }
        }
    }
    bool back() override {
        setScreen(makeTitle());
        return true;
    }

private:
    Button backBtn, create;
    std::vector<Button> worlds;
};

} // namespace

void Button::render(const Touch& t) {
    bool down = held && t.down && contains(t.x, t.y);
    if (edition::LITE) { // Lite 0.2.1: the Java-era button bars from its gui.png (200x20)
        gfx::Texture* g = gfx::texture("gui/gui.png");
        int v = down ? 86 : 66;
        float half = w / 2;
        gfx::blit(g, x, y, half, h, 0, v, int(half), 20);
        gfx::blit(g, x + half, y, w - half, h, 200 - int(w - half), v, int(w - half), 20);
        if (!label.empty()) font::drawCentered(label, x + w / 2, y + (h - 8) / 2, down ? 0xFFFFA0 : 0xE0E0E0);
        return;
    }
    gfx::Texture* sheet = gfx::texture(SPRITES);
    // Touch::TButton: spritesheet (8,32) normally, (0,32) pressed; 8x8 with a 2-texel border
    gfx::ninePatch(sheet, x, y, w, h, down ? 0 : 8, 32, 8, 8, 2);
    if (!label.empty()) font::drawCentered(label, x + w / 2, y + (h - 8) / 2, down ? 0xFFFFA0 : 0xE0E0E0);
}

void setScreen(std::unique_ptr<Screen> s) { gNext = std::move(s); }
Screen* current() { return gScreen.get(); }
std::unique_ptr<Screen> makeTitle() { return std::unique_ptr<Screen>(new TitleScreen()); }
std::unique_ptr<Screen> makeSelectWorld() { return std::unique_ptr<Screen>(new SelectWorldScreen()); }
std::unique_ptr<Screen> makeBuy() { return std::unique_ptr<Screen>(new BuyScreen()); }

void tick(float dt) {
    if (gNext) {
        gScreen = std::move(gNext);
        gScreen->layout();
    }
    if (gScreen) gScreen->tick(dt);
}

void frame(const Touch& t) {
    if (!gScreen) return;
    gScreen->touch(t);
    gfx::begin2D();
    gScreen->render(t);
    gfx::flush();
}

} // namespace ui

namespace ui {
void setChoice(const Choice* c) {
    gChosen = c != nullptr;
    if (c) gChoice = *c;
}
bool choiceMade() { return gChosen; }
const Choice& choice() { return gChoice; }
} // namespace ui
