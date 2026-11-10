// citro3d stand-ins: matrices, textures, frames (see android/shim/citro3d.h)
#include <GLES2/gl2.h>
#include <cmath>
#include <cstring>
#include <vector>
#include "citro3d.h"
#include "Gfx.h"
#include "Platform.h"

// ---------------------------------------------------------------- matrices (row-major, v' = M v)

void Mtx_Zeros(C3D_Mtx* out) { std::memset(out, 0, sizeof(*out)); }
void Mtx_Identity(C3D_Mtx* out) {
    Mtx_Zeros(out);
    out->r[0].x = out->r[1].y = out->r[2].z = out->r[3].w = 1.0f;
}
void Mtx_Copy(C3D_Mtx* out, const C3D_Mtx* in) { *out = *in; }

namespace {
// element (row i, column j); the FVec stores w, z, y, x in memory
inline float& at(C3D_Mtx& m, int i, int j) { return m.r[i].c[3 - j]; }
inline float at(const C3D_Mtx& m, int i, int j) { return m.r[i].c[3 - j]; }

void apply(C3D_Mtx* mtx, const C3D_Mtx& t, bool right) {
    C3D_Mtx res;
    if (right) Mtx_Multiply(&res, mtx, &t);
    else Mtx_Multiply(&res, &t, mtx);
    *mtx = res;
}
} // namespace

void Mtx_Multiply(C3D_Mtx* out, const C3D_Mtx* a, const C3D_Mtx* b) {
    C3D_Mtx r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += at(*a, i, k) * at(*b, k, j);
            at(r, i, j) = s;
        }
    *out = r;
}

void Mtx_Translate(C3D_Mtx* mtx, float x, float y, float z, bool right) {
    C3D_Mtx t;
    Mtx_Identity(&t);
    at(t, 0, 3) = x;
    at(t, 1, 3) = y;
    at(t, 2, 3) = z;
    apply(mtx, t, right);
}

void Mtx_Scale(C3D_Mtx* mtx, float x, float y, float z) {
    for (int i = 0; i < 4; ++i) {
        at(*mtx, i, 0) *= x;
        at(*mtx, i, 1) *= y;
        at(*mtx, i, 2) *= z;
    }
}

void Mtx_RotateX(C3D_Mtx* mtx, float a, bool right) {
    C3D_Mtx t;
    Mtx_Identity(&t);
    float c = std::cos(a), s = std::sin(a);
    at(t, 1, 1) = c; at(t, 1, 2) = -s;
    at(t, 2, 1) = s; at(t, 2, 2) = c;
    apply(mtx, t, right);
}
void Mtx_RotateY(C3D_Mtx* mtx, float a, bool right) {
    C3D_Mtx t;
    Mtx_Identity(&t);
    float c = std::cos(a), s = std::sin(a);
    at(t, 0, 0) = c; at(t, 0, 2) = s;
    at(t, 2, 0) = -s; at(t, 2, 2) = c;
    apply(mtx, t, right);
}
void Mtx_RotateZ(C3D_Mtx* mtx, float a, bool right) {
    C3D_Mtx t;
    Mtx_Identity(&t);
    float c = std::cos(a), s = std::sin(a);
    at(t, 0, 0) = c; at(t, 0, 1) = -s;
    at(t, 1, 0) = s; at(t, 1, 1) = c;
    apply(mtx, t, right);
}

void Mtx_PerspTilt(C3D_Mtx* mtx, float fovy, float aspect, float n, float f, bool) {
    // the 3DS top screen's aspect means "the screen": the window here
    if (std::fabs(aspect - C3D_AspectRatioTop) < 0.001f) aspect = platform::topAspect();
    float t = 1.0f / std::tan(fovy / 2.0f);
    Mtx_Zeros(mtx);
    at(*mtx, 0, 0) = t / aspect;
    at(*mtx, 1, 1) = t;
    at(*mtx, 2, 2) = (f + n) / (n - f);
    at(*mtx, 2, 3) = 2.0f * f * n / (n - f);
    at(*mtx, 3, 2) = -1.0f;
}

void Mtx_PerspStereoTilt(C3D_Mtx* mtx, float fovy, float aspect, float n, float f, float, float, bool lh) {
    Mtx_PerspTilt(mtx, fovy, aspect, n, f, lh);
}

void Mtx_OrthoTilt(C3D_Mtx* mtx, float l, float r, float b, float t, float n, float f, bool) {
    Mtx_Zeros(mtx);
    at(*mtx, 0, 0) = 2.0f / (r - l);
    at(*mtx, 1, 1) = 2.0f / (t - b);
    at(*mtx, 2, 2) = -2.0f / (f - n);
    at(*mtx, 0, 3) = -(r + l) / (r - l);
    at(*mtx, 1, 3) = -(t + b) / (t - b);
    at(*mtx, 2, 3) = -(f + n) / (f - n);
    at(*mtx, 3, 3) = 1.0f;
}

// ---------------------------------------------------------------- textures

void axRegisterTex(C3D_Tex* t);

bool C3D_TexInit(C3D_Tex* tex, u16 w, u16 h, GPU_TEXCOLOR) {
    tex->width = w;
    tex->height = h;
    tex->data = std::calloc(size_t(w) * h, 4);
    GLuint id = 0;
    glGenTextures(1, &id);
    tex->id = id;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (tex->data) axRegisterTex(tex);
    return tex->data != nullptr;
}

void C3D_TexDelete(C3D_Tex* tex) {
    if (tex->id) {
        GLuint id = tex->id;
        glDeleteTextures(1, &id);
    }
    std::free(tex->data);
    tex->data = nullptr;
    tex->id = 0;
}

void C3D_TexFlush(C3D_Tex* tex) {
    if (!tex->data || !tex->id) return;
    // u32 0xRRGGBBAA -> bytes R, G, B, A
    size_t n = size_t(tex->width) * tex->height;
    std::vector<uint8_t> rgba(n * 4);
    const u32* src = static_cast<const u32*>(tex->data);
    for (size_t i = 0; i < n; ++i) {
        u32 p = src[i];
        rgba[i * 4] = u8(p >> 24);
        rgba[i * 4 + 1] = u8(p >> 16);
        rgba[i * 4 + 2] = u8(p >> 8);
        rgba[i * 4 + 3] = u8(p);
    }
    gfx::flush();
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->width, tex->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

void C3D_TexSetFilter(C3D_Tex* tex, GPU_TEXTURE_FILTER_PARAM mag, GPU_TEXTURE_FILTER_PARAM min) {
    tex->filter = mag;
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag == GPU_LINEAR ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min == GPU_LINEAR ? GL_LINEAR : GL_NEAREST);
}

void C3D_TexSetWrap(C3D_Tex* tex, GPU_TEXTURE_WRAP_PARAM s, GPU_TEXTURE_WRAP_PARAM t) {
    tex->wrap = s;
    glBindTexture(GL_TEXTURE_2D, tex->id);
    auto w = [](GPU_TEXTURE_WRAP_PARAM p) { return p == GPU_REPEAT ? GL_REPEAT : p == GPU_MIRRORED_REPEAT ? GL_MIRRORED_REPEAT : GL_CLAMP_TO_EDGE; };
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, w(s));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, w(t));
}

void C3D_SyncDisplayTransfer(u32* in, u32 indim, u32* out, u32, u32) {
    // the bottom screen's map: find the texture that owns `out` and give it the new pixels
    extern C3D_Tex* axFindTexByData(void* data);
    int w = int(indim & 0xFFFF), h = int(indim >> 16);
    std::memcpy(out, in, size_t(w) * h * 4);
    if (C3D_Tex* t = axFindTexByData(out)) C3D_TexFlush(t);
}

// textures made with C3D_TexInit, so a display transfer into one can find it
#include <unordered_map>
namespace {
std::unordered_map<void*, C3D_Tex*>& texByData() {
    static std::unordered_map<void*, C3D_Tex*> m;
    return m;
}
} // namespace
C3D_Tex* axFindTexByData(void* data) {
    auto it = texByData().find(data);
    return it == texByData().end() ? nullptr : it->second;
}
void axRegisterTex(C3D_Tex* t) { texByData()[t->data] = t; }

// ---------------------------------------------------------------- frames

bool C3D_FrameBegin(u8) {
    platform::ensureSurface();
    glViewport(0, 0, platform::windowWidth(), platform::windowHeight());
    return true;
}

void C3D_FrameEnd(u8) { platform::presentFrame(); }

void C3D_AlphaBlend(GPU_BLENDEQUATION, GPU_BLENDEQUATION, GPU_BLENDFACTOR src, GPU_BLENDFACTOR dst, GPU_BLENDFACTOR,
                    GPU_BLENDFACTOR) {
    static const unsigned F[10] = {GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR,
                                   GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA,
                                   GL_ONE_MINUS_DST_ALPHA};
    gfx::setBlend(F[src], F[dst]);
}
