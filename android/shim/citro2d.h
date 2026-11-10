#pragma once
// Android stand-in for citro2d: the 3DS menus, HUD and bottom screen draw through it onto the
// Android batcher. The top screen's 400x240 is fitted to the window's height (centred, the 3D
// view fills the whole width); the bottom screen's 320x240 is a panel over the game when it's
// open (see android/src/Platform.cpp). Text uses Pocket Edition's font.
#include "citro3d.h"
#include "tex3ds.h"

#define C2D_DEFAULT_MAX_OBJECTS 4096

struct C2D_Image {
    C3D_Tex* tex;
    const Tex3DS_SubTexture* subtex;
};
struct C2D_Tint {
    u32 color;
    float blend;
};
struct C2D_ImageTint {
    C2D_Tint corners[4];
};
enum C2D_TintMode { C2D_TintSolid = 0, C2D_TintMult = 1, C2D_TintLuma = 2 };

inline constexpr u32 C2D_Color32(u8 r, u8 g, u8 b, u8 a) { return r | (g << 8) | (b << 16) | ((u32)a << 24); }
inline constexpr u32 C2D_Color32f(float r, float g, float b, float a) {
    return C2D_Color32(u8(r * 255), u8(g * 255), u8(b * 255), u8(a * 255));
}
inline void C2D_PlainImageTint(C2D_ImageTint* t, u32 color, float blend) {
    for (auto& c : t->corners) c = {color, blend};
}
inline void C2D_AlphaImageTint(C2D_ImageTint* t, float alpha) {
    C2D_PlainImageTint(t, C2D_Color32(255, 255, 255, u8(alpha * 255)), 0.0f);
}
void C2D_SetTintMode(C2D_TintMode mode);

inline bool C2D_Init(size_t) { return true; }
inline void C2D_Fini() {}
inline void C2D_Prepare() {}
void C2D_Flush();
C3D_RenderTarget* C2D_CreateScreenTarget(gfxScreen_t screen, gfx3dSide_t side);
void C2D_TargetClear(C3D_RenderTarget* target, u32 color);
void C2D_SceneBegin(C3D_RenderTarget* target);

bool C2D_DrawImageAt(C2D_Image img, float x, float y, float depth, const C2D_ImageTint* tint = nullptr,
                     float scaleX = 1.0f, float scaleY = 1.0f);
bool C2D_DrawImageAtRotated(C2D_Image img, float x, float y, float depth, float angle, const C2D_ImageTint* tint = nullptr,
                            float scaleX = 1.0f, float scaleY = 1.0f);
bool C2D_DrawRectSolid(float x, float y, float z, float w, float h, u32 clr);
bool C2D_DrawRectangle(float x, float y, float z, float w, float h, u32 clr0, u32 clr1, u32 clr2, u32 clr3);
bool C2D_DrawTriangle(float x0, float y0, u32 clr0, float x1, float y1, u32 clr1, float x2, float y2, u32 clr2, float depth);

// ---- text ----
struct C2D_TextBuf_s;
typedef C2D_TextBuf_s* C2D_TextBuf;
struct C2D_Text {
    C2D_TextBuf buf;
    size_t begin, end;
    float width;
    u32 lines, words;
    void* font;
};
enum {
    C2D_AtBaseline = 1 << 0, C2D_WithColor = 1 << 1, C2D_AlignLeft = 0 << 2, C2D_AlignRight = 1 << 2,
    C2D_AlignCenter = 2 << 2, C2D_AlignJustified = 3 << 2, C2D_WordWrap = 1 << 4
};
C2D_TextBuf C2D_TextBufNew(size_t maxGlyphs);
void C2D_TextBufDelete(C2D_TextBuf buf);
void C2D_TextBufClear(C2D_TextBuf buf);
const char* C2D_TextParse(C2D_Text* text, C2D_TextBuf buf, const char* str);
inline void C2D_TextOptimize(const C2D_Text*) {}
void C2D_TextGetDimensions(const C2D_Text* text, float scaleX, float scaleY, float* outWidth, float* outHeight);
void C2D_DrawText(const C2D_Text* text, u32 flags, float x, float y, float z, float scaleX, float scaleY, ...);
