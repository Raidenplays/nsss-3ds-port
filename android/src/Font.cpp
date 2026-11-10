#include "Font.h"
#include <cmath>
#include <vector>
#include "stb_image.h"

namespace font {
namespace {
gfx::Texture* gTex = nullptr;
int gWidth[256];

uint32_t shadowOf(uint32_t c) { return (c & 0xFCFCFC) >> 2; }

// one glyph's quad, optionally turned and scaled about an origin
void glyph(unsigned char ch, float x, float y, uint32_t rgb, float ox, float oy, float cs, float sn, float scale) {
    if (!gTex || ch == ' ') return;
    float u0 = float(ch % 16 * 8) / 128.0f, v0 = float(ch / 16 * 8) / 128.0f, u1 = u0 + 8.0f / 128.0f, v1 = v0 + 8.0f / 128.0f;
    auto at = [&](float px, float py, float u, float v) {
        float dx = (px - ox) * scale, dy = (py - oy) * scale;
        return gfx::Vertex{ox + dx * cs - dy * sn, oy + dx * sn + dy * cs, 0, u, v, gfx::rgb(rgb)};
    };
    std::vector<gfx::Vertex> q = {at(x, y, u0, v0), at(x + 8, y, u1, v0), at(x + 8, y + 8, u1, v1),
                                  at(x, y, u0, v0), at(x + 8, y + 8, u1, v1), at(x, y + 8, u0, v1)};
    gfx::triangles(gTex, q);
}

void run(const std::string& s, float x, float y, uint32_t rgb, float ox, float oy, float cs, float sn, float scale) {
    for (unsigned char ch : s) {
        glyph(ch, x, y, rgb, ox, oy, cs, sn, scale);
        x += float(gWidth[ch]);
    }
}
} // namespace

void load() {
    gTex = gfx::texture("images/font/default8.png");
    for (int i = 0; i < 256; ++i) gWidth[i] = 6;
    std::vector<uint8_t> png;
    if (!gfx::readAsset("images/font/default8.png", png)) return;
    int w, h, n;
    uint8_t* px = stbi_load_from_memory(png.data(), int(png.size()), &w, &h, &n, 4);
    if (!px) return;
    for (int c = 0; c < 256; ++c) {
        int cx = c % 16 * 8, cy = c / 16 * 8, last = -1;
        for (int col = 7; col >= 0 && last < 0; --col)
            for (int row = 0; row < 8; ++row)
                if (px[((cy + row) * w + cx + col) * 4 + 3] > 0) { last = col; break; }
        gWidth[c] = c == ' ' ? 4 : last + 2;
    }
    stbi_image_free(px);
}

void drawScaled(const std::string& s, float x, float y, float sx, float sy, uint32_t argb) {
    if (!gTex) return;
    uint8_t a = uint8_t(argb >> 24);
    uint32_t col = gfx::rgb(argb & 0xFFFFFF, a ? a : 255);
    std::vector<gfx::Vertex> q;
    q.reserve(s.size() * 6);
    for (unsigned char ch : s) {
        if (ch != ' ') {
            float u0 = float(ch % 16 * 8) / 128.0f, v0 = float(ch / 16 * 8) / 128.0f, u1 = u0 + 8.0f / 128.0f, v1 = v0 + 8.0f / 128.0f;
            float x1 = x + 8 * sx, y1 = y + 8 * sy;
            q.push_back({x, y, 0, u0, v0, col});
            q.push_back({x1, y, 0, u1, v0, col});
            q.push_back({x1, y1, 0, u1, v1, col});
            q.push_back({x, y, 0, u0, v0, col});
            q.push_back({x1, y1, 0, u1, v1, col});
            q.push_back({x, y1, 0, u0, v1, col});
        }
        x += float(gWidth[ch]) * sx;
    }
    gfx::triangles(gTex, q);
}

int width(const std::string& s) {
    int w = 0;
    for (unsigned char ch : s) w += gWidth[ch];
    return w;
}

void draw(const std::string& s, float x, float y, uint32_t rgb, bool shadow) {
    if (shadow) run(s, x + 1, y + 1, shadowOf(rgb), 0, 0, 1, 0, 1);
    run(s, x, y, rgb, 0, 0, 1, 0, 1);
}

void drawCentered(const std::string& s, float cx, float y, uint32_t rgb, bool shadow) {
    draw(s, cx - width(s) / 2.0f, y, rgb, shadow);
}

void drawTransformed(const std::string& s, float x, float y, uint32_t rgb, float degrees, float scale) {
    float a = degrees * 3.14159265f / 180.0f, cs = std::cos(a), sn = std::sin(a);
    float left = x - width(s) / 2.0f, top = y - 4.0f; // centred on (x, y)
    run(s, left + 1, top + 1, shadowOf(rgb), x, y, cs, sn, scale);
    run(s, left, top, rgb, x, y, cs, sn, scale);
}

} // namespace font
