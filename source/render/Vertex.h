#pragma once
// A vertex of the immediate-mode geometry (mobs, items, particles, signs, the first-person hand):
// position, a brightness to multiply the texture by, and texture coordinates (0..1, v down as in Java).
struct QuadVertex {
    float x, y, z, shade;
    float u, v;
};
