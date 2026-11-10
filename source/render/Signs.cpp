#include "Signs.h"
#include <cmath>
#include <cstring>
#include "Renderer.h"
#include "model/BlockModels.h"
#include "world/Chunk.h"
#include "world/World.h"

namespace {

constexpr float PI_F = 3.1415927f;

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
    void rotateY(float deg) {
        float a = deg * PI_F / 180.0f, c = std::cos(a), s = std::sin(a);
        float r[16] = {c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1};
        mul(r);
    }
    void apply(const float p[3], float o[3]) const {
        for (int i = 0; i < 3; ++i) o[i] = m[i * 4] * p[0] + m[i * 4 + 1] * p[1] + m[i * 4 + 2] * p[2] + m[i * 4 + 3];
    }
};

void quad(std::vector<axm::ModelVertex>& out, const M4& t, const float p[4][3], const float uv[4][2], float shade) {
    axm::ModelVertex v[4];
    for (int k = 0; k < 4; ++k) {
        float o[3];
        t.apply(p[k], o);
        v[k] = {o[0], o[1], o[2], shade, uv[k][0], uv[k][1]};
    }
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i : T) out.push_back(v[i]);
}

// ModelRenderer.addBox + render(1/16): a textured box in a 64 x 32 texture
void box(std::vector<axm::ModelVertex>& out, const M4& t, float x0, float y0, float z0, int w, int h, int d, int u, int v,
         float shade) {
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
        float uv[4][2] = {{f.u2 / 64.0f, f.v1 / 32.0f}, {f.u1 / 64.0f, f.v1 / 32.0f}, {f.u1 / 64.0f, f.v2 / 32.0f},
                          {f.u2 / 64.0f, f.v2 / 32.0f}};
        quad(out, t, p, uv, shade * f.sh);
    }
}

} // namespace

void Signs::refresh(World& w, double px, double pz, int radius) {
    dirty = false;
    signs.clear();
    int pcx = int(std::floor(px)) >> 4, pcz = int(std::floor(pz)) >> 4;
    w.forEachChunk([&](Chunk& c) {
        if (std::abs(c.xPosition - pcx) > radius || std::abs(c.zPosition - pcz) > radius || !c.level) return;
        nbt::Tag* list = c.level->getList("TileEntities");
        if (!list) return;
        for (auto& t : list->items) {
            if (!t || t->getString("id") != "Sign") continue;
            Sign s;
            s.x = t->getInt("x");
            s.y = t->getInt("y");
            s.z = t->getInt("z");
            int id = w.getBlockId(s.x, s.y, s.z);
            if (id != 63 && id != 68) continue;
            s.post = id == 63;
            s.meta = w.getBlockMetadata(s.x, s.y, s.z);
            for (int i = 0; i < 4; ++i) {
                s.text[i] = t->getString(("Text" + std::to_string(i + 1)).c_str());
                if (s.text[i].size() > 15) s.text[i].resize(15);
            }
            signs.push_back(std::move(s));
        }
    });
}

void Signs::render(const World& w, Renderer& r, const Font& font, double camX, double camY, double camZ) {
    if (signs.empty()) return;
    C3D_Tex* fontTex = r.texture("romfs:/textures/font/default.rgba");
    {
        board.clear();
        text.clear();
        bool any = false;
        for (const Sign& s : signs) {
            double dx = s.x + 0.5 - camX, dy = s.y + 0.5 - camY, dz = s.z + 0.5 - camZ;
            if (dx * dx + dy * dy + dz * dz > 48.0 * 48.0) continue;
            any = true;
            float light = w.getBrightness(s.x, s.y, s.z);
            const float f = 2.0f / 3.0f;
            M4 t;
            if (s.post) {
                t.translate(s.x + 0.5f, s.y + 0.75f * f, s.z + 0.5f);
                t.rotateY(-float(s.meta * 360) / 16.0f);
            } else {
                float rot = s.meta == 2 ? 180.0f : s.meta == 4 ? 90.0f : s.meta == 5 ? -90.0f : 0.0f;
                t.translate(s.x + 0.5f, s.y + 0.75f * f, s.z + 0.5f);
                t.rotateY(-rot);
                t.translate(0.0f, -5.0f / 16.0f, -7.0f / 16.0f);
            }
            M4 m = t;
            m.scale(f, -f, -f);
            box(board, m, -12, -14, -1, 24, 12, 2, 0, 0, light);
            if (s.post) box(board, m, -1, -2, -1, 2, 14, 2, 0, 14, light);
            // the writing
            M4 tt = t;
            const float k = float(1.0 / 60.0) * f;
            tt.translate(0.0f, 0.5f * f, 0.07f * f);
            tt.scale(k, -k, k);
            const float colour = 0.0f; // black writing
            for (int i = 0; i < 4; ++i) {
                const std::string& line = s.text[i];
                if (line.empty()) continue;
                float x = float(-font.width(line) / 2), y = float(i * 10 - 4 * 5);
                for (unsigned char c : line) {
                    if (c < 32 || c > 126) c = '?';
                    float cu = float(c % 16 * 8), cv = float(c / 16 * 8);
                    float p[4][3] = {{x, y + 7.99f, 0}, {x + 7.99f, y + 7.99f, 0}, {x + 7.99f, y, 0}, {x, y, 0}};
                    float uv[4][2] = {{cu / 128.0f, (cv + 7.99f) / 128.0f}, {(cu + 7.99f) / 128.0f, (cv + 7.99f) / 128.0f},
                                      {(cu + 7.99f) / 128.0f, cv / 128.0f}, {cu / 128.0f, cv / 128.0f}};
                    quad(text, tt, p, uv, colour);
                    x += float(font.glyphWidth(c));
                }
            }
        }
        if (!any) return;
        r.drawTriangles(board, r.texture("romfs:/textures/item/sign.rgba"));
        if (!text.empty() && fontTex) r.drawTriangles(text, fontTex, nullptr, 1);
    }
}
