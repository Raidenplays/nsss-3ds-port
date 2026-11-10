#include "Font.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include "render/Renderer.h"

namespace {
const char* const PATH = "romfs:/textures/font/default.rgba";
}

bool Font::load(Renderer& r) {
    tex = r.texture(PATH);
    if (!tex) return false;
    // FontRenderer: a glyph's width is its last column with any (blue) ink, + 2; space is 4
    FILE* f = fopen(PATH, "rb");
    if (!f) return false;
    uint8_t head[8];
    std::vector<uint8_t> px(128 * 128 * 4);
    bool ok = fread(head, 1, 8, f) == 8 && fread(px.data(), 1, px.size(), f) == px.size();
    fclose(f);
    if (!ok) return false;
    for (int c = 0; c < 256; c++) {
        int gx = c % 16, gy = c / 16;
        int col = 7;
        for (; col >= 0; --col) {
            bool empty = true;
            for (int row = 0; row < 8 && empty; ++row) {
                const uint8_t* p = &px[((gy * 8 + row) * 128 + gx * 8 + col) * 4];
                // getRGB of a transparent pixel is 0; blue of an opaque one counts
                if (p[3] && p[2] > 0) empty = false;
            }
            if (!empty) break;
        }
        if (c == 32) col = 2;
        charWidth[c] = col + 2;
        glyph[c] = {8, 8, gx / 16.0f, 1.0f - gy / 16.0f, (gx + 1) / 16.0f, 1.0f - (gy + 1) / 16.0f};
    }
    return true;
}

int Font::width(const std::string& s) const {
    int w = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if (c == 0xA7) { ++i; continue; } // colour code
        if (c >= 32 && c < 127) w += charWidth[c];
    }
    return w;
}

void Font::drawRun(const std::string& s, float x, float y, uint32_t colour, float scale, float depth) const {
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32((colour >> 16) & 255, (colour >> 8) & 255, colour & 255, (colour >> 24) ? (colour >> 24) : 255), 1.0f);
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if (c == 0xA7) { ++i; continue; }
        if (c < 32 || c >= 127) continue;
        if (c != 32) {
            C2D_Image img = {tex, &glyph[c]};
            C2D_DrawImageAt(img, x, y, depth, &tint, scale, scale);
        }
        x += charWidth[c] * scale;
    }
}

void Font::drawRotated(const std::string& s, float ax, float ay, float dy, float degrees, uint32_t colour,
                       float scale) const {
    if (!tex) return;
    float a = degrees * 3.14159265f / 180.0f, ca = std::cos(a), sa = std::sin(a);
    float total = float(width(s));
    for (int pass = 0; pass < 2; ++pass) { // shadow, then the text
        uint32_t c = pass == 0 ? (colour & 0xFCFCFC) >> 2 : colour;
        float off = pass == 0 ? 1.0f : 0.0f;
        C2D_ImageTint tint;
        C2D_PlainImageTint(&tint, C2D_Color32((c >> 16) & 255, (c >> 8) & 255, c & 255, 255), 1.0f);
        float cursor = -total / 2.0f;
        for (size_t i = 0; i < s.size(); ++i) {
            unsigned char ch = s[i];
            if (ch < 32 || ch >= 127) continue;
            if (ch != 32) {
                // glyph centre in the text's own space, then rotated and scaled about the anchor
                float lx = (cursor + 4.0f + off) * scale, ly = (dy + 4.0f + off) * scale;
                float gx = ax + lx * ca - ly * sa, gy = ay + lx * sa + ly * ca;
                C2D_DrawImageAtRotated({tex, &glyph[ch]}, gx, gy, 0.6f, a, &tint, scale, scale);
            }
            cursor += charWidth[ch];
        }
    }
}

void Font::draw(const std::string& s, float x, float y, uint32_t colour, bool shadow, float scale, float depth) const {
    if (!tex) return;
    // whole pixels: the glyphs are sampled nearest-neighbour, so half-pixel positions tear them
    x = std::floor(x + 0.5f);
    y = std::floor(y + 0.5f);
    if (shadow) drawRun(s, x + scale, y + scale, ((colour & 0xFCFCFC) >> 2) | (colour & 0xFF000000u), scale, depth);
    drawRun(s, x, y, colour, scale, depth);
}
