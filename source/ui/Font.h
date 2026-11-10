#pragma once
// Beta's FontRenderer on citro2d: the real /font/default.png, character widths measured from the
// glyphs the way FontRenderer does, drop shadow at +1,+1 in a quarter of the colour.
#include <citro2d.h>
#include <string>

class Renderer;

class Font {
public:
    bool load(Renderer& r);
    int width(const std::string& s) const;
    int glyphWidth(unsigned char c) const { return charWidth[c]; } // FontRenderer.charWidth
    // colour 0xRRGGBB; scale 1 = 8-pixel glyphs
    void draw(const std::string& s, float x, float y, uint32_t colour, bool shadow = true, float scale = 1.0f,
              float depth = 0.5f) const;
    void drawCentered(const std::string& s, float cx, float y, uint32_t colour, bool shadow = true,
                      float scale = 1.0f) const {
        draw(s, cx - width(s) * scale / 2.0f, y, colour, shadow, scale);
    }

    // Rotated about (ax, ay), the string centred on it horizontally, its middle line dy below it
    // (glTranslate + glRotate + glScale + drawCenteredString, as the title splash does)
    void drawRotated(const std::string& s, float ax, float ay, float dy, float degrees, uint32_t colour,
                     float scale) const;

private:
    C3D_Tex* tex = nullptr;
    Tex3DS_SubTexture glyph[256];
    int charWidth[256] = {};
    void drawRun(const std::string& s, float x, float y, uint32_t colour, float scale, float depth) const;
};
