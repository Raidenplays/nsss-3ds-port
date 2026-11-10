#pragma once
// Tex3DS sub-texture: a rectangle of a texture, in 0..1 texture coordinates with v = 1 at the top
// of the image (citro2d's convention)
#include "3ds.h"

struct Tex3DS_SubTexture {
    u16 width, height;
    float left, top, right, bottom;
};
