#pragma once
// ModelRenderer for the few Beta models built in code (the boat, the minecart): a matrix stack
// and addBox's textured boxes, in the 64 x 32 texture layout of TexturedQuad.
#include <cmath>
#include <cstring>
#include <vector>
#include "model/AxmModel.h"

namespace boxmodel {

struct M4 {
    float m[16];
    M4() { std::memset(m, 0, sizeof m); m[0] = m[5] = m[10] = m[15] = 1; }
    void mul(const float* b) {
        float r[16];
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                r[i * 4 + j] = m[i * 4] * b[j] + m[i * 4 + 1] * b[4 + j] + m[i * 4 + 2] * b[8 + j] + m[i * 4 + 3] * b[12 + j];
        std::memcpy(m, r, sizeof r);
    }
    void translate(float x, float y, float z) { float t[16] = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1}; mul(t); }
    void scale(float x, float y, float z) { float t[16] = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1}; mul(t); }
    void rotate(float deg, int axis) { // glRotatef about x (0), y (1) or z (2)
        float a = deg * 3.1415927f / 180.0f, c = std::cos(a), s = std::sin(a);
        float r[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (axis == 0) { r[5] = c; r[6] = -s; r[9] = s; r[10] = c; }
        if (axis == 1) { r[0] = c; r[2] = s; r[8] = -s; r[10] = c; }
        if (axis == 2) { r[0] = c; r[1] = -s; r[4] = s; r[5] = c; }
        mul(r);
    }
    void apply(const float p[3], float o[3]) const {
        for (int i = 0; i < 3; ++i) o[i] = m[i * 4] * p[0] + m[i * 4 + 1] * p[1] + m[i * 4 + 2] * p[2] + m[i * 4 + 3];
    }
};

inline void quad(std::vector<axm::ModelVertex>& out, const M4& t, const float p[4][3], const float uv[4][2], float shade) {
    axm::ModelVertex v[4];
    for (int k = 0; k < 4; ++k) {
        float o[3];
        t.apply(p[k], o);
        v[k] = {o[0], o[1], o[2], shade, uv[k][0], uv[k][1]};
    }
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i : T) out.push_back(v[i]);
}

// ModelBox(u, v, x0, y0, z0, w, h, d) drawn at render(1/16) under t
inline void box(std::vector<axm::ModelVertex>& out, const M4& t, float x0, float y0, float z0, int w, int h, int d, int u,
                int v, float shade, float texW = 64.0f, float texH = 32.0f) {
    const float s = 1.0f / 16.0f;
    float x1 = x0 + w, y1 = y0 + h, z1 = z0 + d;
    float V[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                     {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
    for (auto& p : V)
        for (float& c : p) c *= s;
    struct F { int a, b, c, dd; float u1, v1, u2, v2, sh; };
    const float fu = float(u), fv = float(v), fw = float(w), fh = float(h), fd = float(d);
    const F faces[6] = {
        {5, 1, 2, 6, fu + fd + fw, fv + fd, fu + fd + fw + fd, fv + fd + fh, 0.6f},
        {0, 4, 7, 3, fu, fv + fd, fu + fd, fv + fd + fh, 0.6f},
        {5, 4, 0, 1, fu + fd, fv, fu + fd + fw, fv + fd, 1.0f},
        {2, 3, 7, 6, fu + fd + fw, fv, fu + fd + fw + fw, fv + fd, 0.5f},
        {1, 0, 3, 2, fu + fd, fv + fd, fu + fd + fw, fv + fd + fh, 0.8f},
        {4, 5, 6, 7, fu + fd + fw + fd, fv + fd, fu + fd + fw + fd + fw, fv + fd + fh, 0.8f},
    };
    for (const F& f : faces) {
        float p[4][3];
        const int idx[4] = {f.a, f.b, f.c, f.dd};
        for (int k = 0; k < 4; ++k) std::memcpy(p[k], V[idx[k]], sizeof p[k]);
        float uv[4][2] = {{f.u2 / texW, f.v1 / texH}, {f.u1 / texW, f.v1 / texH}, {f.u1 / texW, f.v2 / texH},
                          {f.u2 / texW, f.v2 / texH}};
        quad(out, t, p, uv, shade * f.sh);
    }
}

// ModelRenderer.render: to the rotation point, then Z, Y, X turns (radians, as Beta stores them)
inline M4 part(const M4& parent, float px, float py, float pz, float rx, float ry, float rz) {
    M4 t = parent;
    t.translate(px / 16.0f, py / 16.0f, pz / 16.0f);
    if (rz != 0.0f) t.rotate(rz * 180.0f / 3.1415927f, 2);
    if (ry != 0.0f) t.rotate(ry * 180.0f / 3.1415927f, 1);
    if (rx != 0.0f) t.rotate(rx * 180.0f / 3.1415927f, 0);
    return t;
}

} // namespace boxmodel
