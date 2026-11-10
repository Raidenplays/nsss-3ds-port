#pragma once
// Pocket Edition's bitmap font (images/font/default8.png: 16x16 cells of 8x8), with each glyph's
// width measured from its pixels the way the Java FontRenderer does.
#include <string>
#include "Gfx.h"

namespace font {

void load();
int width(const std::string& s);
// draws in GUI units at 1 texel = 1 unit; shadow like drawString (a darker copy one unit down-right)
void draw(const std::string& s, float x, float y, uint32_t rgb, bool shadow = true);
void drawCentered(const std::string& s, float cx, float y, uint32_t rgb, bool shadow = true);
// rotated (degrees) and scaled about (x, y): the title screen's splash
// at any scale, colour 0xAARRGGBB (alpha 0 = opaque): the 3DS UI's text (android/src/Citro2dShim.cpp)
void drawScaled(const std::string& s, float x, float y, float scaleX, float scaleY, uint32_t argb);
void drawTransformed(const std::string& s, float x, float y, uint32_t rgb, float degrees, float scale);

} // namespace font
