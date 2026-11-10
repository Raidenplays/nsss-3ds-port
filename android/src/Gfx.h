#pragma once
// The Android renderer's 2D side: one OpenGL ES 2 shader, textures loaded from the APK's assets,
// and a quad batcher for the Pocket Edition-style menus (sprites, nine-patches, text, fills).
#include <GLES2/gl2.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct AAssetManager;

namespace gfx {

struct Texture {
    GLuint id = 0;
    int w = 0, h = 0;
};

struct Vertex {
    float x, y, z, u, v;
    uint32_t rgba;     // multiplies the texture; bytes R,G,B,A
    uint32_t tint = 0; // R,G,B + how far (A) the texture's colour goes toward it first (citro2d's solid tint)
};

// colour helpers: 0xRRGGBB (+ alpha) to the vertex byte order
inline uint32_t rgb(uint32_t c, uint8_t a = 255) {
    return (uint32_t(a) << 24) | ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF);
}

void init(AAssetManager* assets);
bool readAsset(const std::string& path, std::vector<uint8_t>& out);
std::vector<std::string> listAssets(const std::string& dir); // the files in one asset folder
void restore(); // the menus' GL state again, after something else drew (the world)

// PNG/TGA (stb_image) or the game's own .rgba files ("AXTX" + width + height + RGBA rows)
Texture* texture(const std::string& path);

// size of the window in pixels, and the PE GUI scale (Minecraft::setSize: 1..6 by width)
void resize(int pixelW, int pixelH);
int pixelWidth();
int pixelHeight();
int guiScale();
int guiWidth();  // the screen in GUI units (what the menus lay out in)
int guiHeight();

void beginFrame(uint32_t clearRGB);
void begin2D();                          // orthographic, GUI units, origin top-left
void setMatrix(const float m[16]);       // anything else (the title panorama)

// queue a textured quad (tex null = solid colour); flushed when the texture changes or at flush()
void quad(Texture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t rgba);
// a sprite cut from a texture, in texel coordinates
void blit(Texture* tex, float x, float y, float w, float h, int sx, int sy, int sw, int sh, uint32_t rgba = 0xFFFFFFFF);
void fill(float x, float y, float w, float h, uint32_t rgba);
void gradient(float x, float y, float w, float h, uint32_t top, uint32_t bottom);
// a nine-patch: corners of `border` texels kept, edges and middle stretched (NinePatchLayer)
void ninePatch(Texture* tex, float x, float y, float w, float h, int sx, int sy, int sw, int sh, int border);
void triangles(Texture* tex, const std::vector<Vertex>& v); // already transformed by the current matrix
void trianglesGL(unsigned glTex, const Vertex* v, size_t n); // a raw GL texture (0 = untextured)
void setBlend(unsigned srcFactor, unsigned dstFactor);       // GL blend factors (flushes)
void defaultBlend();
void flush();

void setScissor(bool on, float x = 0, float y = 0, float w = 0, float h = 0); // in GUI units

} // namespace gfx
