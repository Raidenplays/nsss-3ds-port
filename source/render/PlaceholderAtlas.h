#pragma once
// Generates a 256x256 stand-in terrain atlas in code, laid out like Beta's
// terrain.png, so the project builds and runs without shipping Mojang art.
// Pixels are 0xRRGGBBAA, row 0 = top of the image.
#include <cstdint>

namespace PlaceholderAtlas {
constexpr int SIZE = 256;
void generate(uint32_t* pixels); // pixels must hold SIZE*SIZE entries
}
