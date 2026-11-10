// citro2d stand-ins: the 3DS HUD, menus and bottom screen draw onto the Android batcher
// (see android/shim/citro2d.h)
#include <GLES2/gl2.h>
#include <cmath>
#include <cstdarg>
#include <string>
#include <vector>
#include "citro2d.h"
#include "Font.h"
#include "Gfx.h"
#include "Platform.h"

namespace {
C3D_RenderTarget gTop{0}, gBottom{1};
int gScreen = 0;                // what the drawing is for
bool gDrop = false;             // drawing for the bottom screen while its panel is closed
bool gClearBottom = false;      // a clear is waiting for the bottom panel's scene
u32 gClearColour = 0;
C2D_TintMode gTintMode = C2D_TintSolid;
// the 3DS system font is about 30 pixels a line at scale 1; Pocket Edition's is 8
constexpr float TEXT_K = 3.0f;
} // namespace

struct C2D_TextBuf_s {
    std::string chars;
    size_t max;
};

void C2D_SetTintMode(C2D_TintMode m) { gTintMode = m; }
void C2D_Flush() { gfx::flush(); }

C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t) { return screen == GFX_BOTTOM ? &gBottom : &gTop; }

void C2D_TargetClear(C3D_RenderTarget* t, u32 color) {
    if (t && t->screen == 1) {
        gClearBottom = true;
        gClearColour = color;
    }
}

void C2D_SceneBegin(C3D_RenderTarget* t) {
    gScreen = t ? t->screen : 0;
    gDrop = gScreen == 1 && !platform::bottomVisible();
    if (gDrop) return;
    gfx::restore();
    platform::beginScreen(gScreen);
    if (gScreen == 1) {
        // the panel: a shadow round it, then the screen's own background
        gfx::fill(-6, -6, 332, 252, gfx::rgb(0x000000, 140));
        if (gClearBottom) gfx::fill(0, 0, 320, 240, gClearColour);
        gClearBottom = false;
    }
}

namespace {
// a tinted image corner: what the vertex colour and the solid-tint mix are
void tintFor(const C2D_ImageTint* tint, int corner, u32& col, u32& mix) {
    if (!tint) {
        col = 0xFFFFFFFF;
        mix = 0;
        return;
    }
    const C2D_Tint& c = tint->corners[corner];
    u8 r = c.color & 255, g = (c.color >> 8) & 255, b = (c.color >> 16) & 255, a = c.color >> 24;
    float k = c.blend < 0 ? 0 : c.blend > 1 ? 1 : c.blend;
    if (gTintMode == C2D_TintMult) {
        auto lerp = [&](u8 v) { return u32(255.0f + (float(v) - 255.0f) * k + 0.5f); };
        col = lerp(r) | lerp(g) << 8 | lerp(b) << 16 | u32(a) << 24;
        mix = 0;
    } else {
        col = 0x00FFFFFFu | u32(a) << 24;
        mix = r | g << 8 | b << 16 | u32(k * 255.0f + 0.5f) << 24;
    }
}

void imageQuad(const C2D_Image& img, const float px[4], const float py[4], const C2D_ImageTint* tint) {
    if (gDrop || !img.tex || !img.subtex) return;
    const Tex3DS_SubTexture& s = *img.subtex;
    // citro2d's v is 1 at the top of the image; the GL textures are uploaded top row first
    float u[4] = {s.left, s.right, s.right, s.left}, v[4] = {1.0f - s.top, 1.0f - s.top, 1.0f - s.bottom, 1.0f - s.bottom};
    gfx::Vertex q[4];
    for (int i = 0; i < 4; ++i) {
        u32 col, mix;
        tintFor(tint, i == 0 ? 0 : i == 1 ? 1 : i == 2 ? 3 : 2, col, mix);
        q[i] = {px[i], py[i], 0, u[i], v[i], col, mix};
    }
    gfx::Vertex t[6] = {q[0], q[1], q[2], q[0], q[2], q[3]};
    gfx::trianglesGL(img.tex->id, t, 6);
}
} // namespace

bool C2D_DrawImageAt(C2D_Image img, float x, float y, float, const C2D_ImageTint* tint, float sx, float sy) {
    if (!img.subtex) return false;
    float w = img.subtex->width * sx, h = img.subtex->height * sy;
    float px[4] = {x, x + w, x + w, x}, py[4] = {y, y, y + h, y + h};
    imageQuad(img, px, py, tint);
    return true;
}

bool C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float, float angle, const C2D_ImageTint* tint, float sx, float sy) {
    if (!img.subtex) return false;
    float w = img.subtex->width * sx / 2, h = img.subtex->height * sy / 2, c = std::cos(angle), s = std::sin(angle);
    float lx[4] = {-w, w, w, -w}, ly[4] = {-h, -h, h, h}, px[4], py[4];
    for (int i = 0; i < 4; ++i) {
        px[i] = x + lx[i] * c - ly[i] * s;
        py[i] = y + lx[i] * s + ly[i] * c;
    }
    imageQuad(img, px, py, tint);
    return true;
}

bool C2D_DrawRectSolid(float x, float y, float, float w, float h, u32 clr) {
    if (!gDrop) gfx::fill(x, y, w, h, clr);
    return true;
}

bool C2D_DrawRectangle(float x, float y, float, float w, float h, u32 c0, u32 c1, u32 c2, u32 c3) {
    if (gDrop) return true;
    gfx::Vertex t[6] = {{x, y, 0, 0, 0, c0}, {x + w, y, 0, 1, 0, c1}, {x + w, y + h, 0, 1, 1, c3},
                        {x, y, 0, 0, 0, c0}, {x + w, y + h, 0, 1, 1, c3}, {x, y + h, 0, 0, 1, c2}};
    gfx::trianglesGL(0, t, 6);
    return true;
}

bool C2D_DrawTriangle(float x0, float y0, u32 c0, float x1, float y1, u32 c1, float x2, float y2, u32 c2, float) {
    if (gDrop) return true;
    gfx::Vertex t[3] = {{x0, y0, 0, 0, 0, c0}, {x1, y1, 0, 0, 0, c1}, {x2, y2, 0, 0, 0, c2}};
    gfx::trianglesGL(0, t, 3);
    return true;
}

// ---------------------------------------------------------------- text

C2D_TextBuf C2D_TextBufNew(size_t maxGlyphs) {
    C2D_TextBuf b = new C2D_TextBuf_s;
    b->max = maxGlyphs;
    b->chars.reserve(maxGlyphs);
    return b;
}
void C2D_TextBufDelete(C2D_TextBuf b) { delete b; }
void C2D_TextBufClear(C2D_TextBuf b) {
    if (b) b->chars.clear();
}

const char* C2D_TextParse(C2D_Text* text, C2D_TextBuf buf, const char* str) {
    text->buf = buf;
    text->begin = buf->chars.size();
    buf->chars += str;
    text->end = buf->chars.size();
    text->width = float(font::width(std::string(str)));
    text->lines = 1;
    text->words = 0;
    text->font = nullptr;
    return str + std::strlen(str);
}

void C2D_TextGetDimensions(const C2D_Text* text, float sx, float sy, float* w, float* h) {
    if (w) *w = text->width * sx * TEXT_K;
    if (h) *h = 30.0f * sy;
}

void C2D_DrawText(const C2D_Text* text, u32 flags, float x, float y, float, float sx, float sy, ...) {
    if (gDrop || !text->buf) return;
    u32 colour = 0xFF000000;
    if (flags & C2D_WithColor) {
        va_list ap;
        va_start(ap, sy);
        colour = va_arg(ap, u32);
        va_end(ap);
    }
    std::string s = text->buf->chars.substr(text->begin, text->end - text->begin);
    float kx = sx * TEXT_K, ky = sy * TEXT_K;
    float w = font::width(s) * kx;
    int align = flags & (3 << 2);
    if (align == C2D_AlignCenter) x -= w / 2;
    else if (align == C2D_AlignRight) x -= w;
    if (flags & C2D_AtBaseline) y -= 8 * ky;
    y += 1.5f * ky; // the system font sits a little lower in its line
    // citro2d colour is R | G << 8 | B << 16 | A << 24
    u32 argb = (colour & 0xFF) << 16 | (colour & 0xFF00) | (colour >> 16 & 0xFF) | (colour & 0xFF000000);
    font::drawScaled(s, x, y, kx, ky, argb);
}
