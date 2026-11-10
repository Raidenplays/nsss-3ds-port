#include "FirstPerson.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "Renderer.h"
#include "client/Camera.h"
#include "game/ItemTable.h"
#include "game/Player.h"
#include "world/BlockTable.h"
#include "world/MathHelper.h"

namespace {

constexpr float PI_F = 3.1415927f;

// A GL-style matrix stack entry (row-major, column vectors): every call multiplies on the right
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
    void rotate(float deg, int axis) { // glRotatef about the X (0), Y (1) or Z (2) axis
        float a = deg * PI_F / 180.0f, c = std::cos(a), s = std::sin(a);
        float r[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        if (axis == 0) { r[5] = c; r[6] = -s; r[9] = s; r[10] = c; }
        else if (axis == 1) { r[0] = c; r[2] = s; r[8] = -s; r[10] = c; }
        else { r[0] = c; r[1] = -s; r[4] = s; r[5] = c; }
        mul(r);
    }
};

} // namespace

void FirstPerson::addQuad(const float m[16], const float p[4][3], const float uv[4][2], float shade) {
    axm::ModelVertex v[4];
    for (int k = 0; k < 4; ++k) {
        const float* q = p[k];
        v[k].x = m[0] * q[0] + m[1] * q[1] + m[2] * q[2] + m[3];
        v[k].y = m[4] * q[0] + m[5] * q[1] + m[6] * q[2] + m[7];
        v[k].z = m[8] * q[0] + m[9] * q[1] + m[10] * q[2] + m[11];
        v[k].shade = shade;
        v[k].u = uv[k][0];
        v[k].v = uv[k][1];
    }
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int i : T) tris.push_back(v[i]);
}

void FirstPerson::tick(const Inventory& inv) {
    prevEquipped = equipped;
    const ItemStack& cur = inv.main[inv.currentItem];
    bool same = shownSlot == inv.currentItem && cur.id == shown.id && cur.damage == shown.damage &&
                cur.empty() == shown.empty();
    if (shown.empty() && cur.empty()) same = true;
    float want = same ? 1.0f : 0.0f;
    float d = want - equipped;
    if (d < -0.4f) d = -0.4f;
    if (d > 0.4f) d = 0.4f;
    equipped += d;
    if (equipped < 0.1f) {
        shown = cur;
        shownSlot = inv.currentItem;
    }
}

void FirstPerson::draw(Renderer& r, const Player& p, const Camera& cam, float partial, float bright) {
    if (p.dead()) return;
    tris.clear();
    float eq = prevEquipped + (equipped - prevEquipped) * partial;
    float sp = p.swing(partial);
    float s1 = MathHelper::sin(sp * PI_F), s2 = MathHelper::sin(MathHelper::sqrt_float(sp) * PI_F);
    float s3 = MathHelper::sin(sp * sp * PI_F);

    // EntityRenderer: hurtCameraEffect + setupViewBobbing, then the hand
    M4 base;
    if (cam.deathRoll != 0.0f) base.rotate(cam.deathRoll * 180.0f / PI_F, 2);
    if (cam.hurtRoll != 0.0f) {
        base.rotate(-cam.hurtYaw * 180.0f / PI_F, 1);
        base.rotate(-cam.hurtRoll * 180.0f / PI_F, 2);
        base.rotate(cam.hurtYaw * 180.0f / PI_F, 1);
    }
    base.translate(cam.bobX, cam.bobY, 0.0f);
    base.rotate(cam.bobRoll * 180.0f / PI_F, 2);
    base.rotate(cam.bobPitch * 180.0f / PI_F, 0);

    // ItemRenderer.renderItem: an icon with thickness, at the transform t
    auto icon3d = [&](M4 t, float u0, float u1, float v0, float v1) {
        t.translate(-15.0f / 16.0f, -1.0f / 16.0f, 0.0f);
        const float d = 1.0f / 16.0f;
        float sh = bright;
        {
            float pq[4][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
            float uv[4][2] = {{u1, v1}, {u0, v1}, {u0, v0}, {u1, v0}};
            addQuad(t.m, pq, uv, sh);
        }
        {
            float pq[4][3] = {{0, 1, -d}, {1, 1, -d}, {1, 0, -d}, {0, 0, -d}};
            float uv[4][2] = {{u1, v0}, {u0, v0}, {u0, v1}, {u1, v1}};
            addQuad(t.m, pq, uv, sh * 0.8f);
        }
        for (int i = 0; i < 16; ++i) {
            float f = float(i) / 16.0f;
            float uu = u1 + (u0 - u1) * f - 0.001953125f;
            float x0 = f, x1 = f + d;
            float a[4][3] = {{x0, 0, -d}, {x0, 0, 0}, {x0, 1, 0}, {x0, 1, -d}};
            float ua[4][2] = {{uu, v1}, {uu, v1}, {uu, v0}, {uu, v0}};
            addQuad(t.m, a, ua, sh * 0.6f);
            float b[4][3] = {{x1, 1, -d}, {x1, 1, 0}, {x1, 0, 0}, {x1, 0, -d}};
            float ub[4][2] = {{uu, v0}, {uu, v0}, {uu, v1}, {uu, v1}};
            addQuad(t.m, b, ub, sh * 0.6f);
            float vv = v1 + (v0 - v1) * f - 0.001953125f / 4.0f;
            float y0 = f, y1 = f + d;
            float c[4][3] = {{0, y1, 0}, {1, y1, 0}, {1, y1, -d}, {0, y1, -d}};
            float uc[4][2] = {{u1, vv}, {u0, vv}, {u0, vv}, {u1, vv}};
            addQuad(t.m, c, uc, sh);
            float e[4][3] = {{1, y0, 0}, {0, y0, 0}, {0, y0, -d}, {1, y0, -d}};
            float ue[4][2] = {{u0, vv}, {u1, vv}, {u1, vv}, {u0, vv}};
            addQuad(t.m, e, ue, sh * 0.5f);
        }
            };

    C3D_Tex* tex = nullptr;
    if (!shown.empty()) {
        M4 t = base;
        t.translate(-s2 * 0.4f, MathHelper::sin(MathHelper::sqrt_float(sp) * PI_F * 2.0f) * 0.2f, -s1 * 0.2f);
        t.translate(0.7f * 0.8f, -0.65f * 0.8f - (1.0f - eq) * 0.6f, -0.9f * 0.8f);
        t.rotate(45.0f, 1);
        t.rotate(-s3 * 20.0f, 1);
        t.rotate(-s2 * 20.0f, 2);
        t.rotate(-s2 * 80.0f, 0);
        t.scale(0.4f, 0.4f, 0.4f);
        const ItemInfo* info = itemInfo(shown.id, shown.damage);
        bool cube = shown.id < 256 && info && (info->flags & IF_BLOCK3D);
        if (cube) {
            // RenderBlocks.renderBlockOnInventory: a cube centred on the hand
            tex = r.terrainTexture();
            t.translate(-0.5f, -0.5f, -0.5f);
            const int meta = shown.damage & 15;
            static const float FACE[6] = {0.5f, 1.0f, 0.8f, 0.8f, 0.6f, 0.6f};
            static const float P[6][4][3] = {
                {{0, 0, 1}, {0, 0, 0}, {1, 0, 0}, {1, 0, 1}}, // bottom
                {{1, 1, 1}, {1, 1, 0}, {0, 1, 0}, {0, 1, 1}}, // top
                {{0, 1, 0}, {1, 1, 0}, {1, 0, 0}, {0, 0, 0}}, // -Z
                {{0, 1, 1}, {0, 0, 1}, {1, 0, 1}, {1, 1, 1}}, // +Z
                {{0, 1, 1}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}}, // -X
                {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}}, // +X
            };
            static const float UV[6][4][2] = {
                {{0, 1}, {0, 0}, {1, 0}, {1, 1}}, {{1, 1}, {1, 0}, {0, 0}, {0, 1}}, {{1, 0}, {0, 0}, {0, 1}, {1, 1}},
                {{0, 0}, {0, 1}, {1, 1}, {1, 0}}, {{1, 0}, {0, 0}, {0, 1}, {1, 1}}, {{0, 1}, {1, 1}, {1, 0}, {0, 0}},
            };
            for (int side = 0; side < 6; ++side) {
                int tile = BLOCKS[shown.id].tex[meta][side];
                float u0 = float(tile & 15) / 16.0f, v0 = float(tile >> 4) / 64.0f;
                float uv[4][2];
                for (int k = 0; k < 4; ++k) {
                    uv[k][0] = u0 + UV[side][k][0] * (15.99f / 256.0f);
                    uv[k][1] = v0 + UV[side][k][1] * (15.99f / 1024.0f);
                }
                addQuad(t.m, P[side], uv, bright * FACE[side]);
            }
        } else if (info && info->icon >= 0) {
            // an icon with thickness (ItemRenderer.renderItem)
            int icon = info->icon & 255;
            float u0, u1, v0, v1;
            if (shown.id < 256) {
                tex = r.terrainTexture();
                int tile = (info->flags & IF_TERRAIN2) ? 256 + icon : icon;
                u0 = float(tile & 15) * 16.0f / 256.0f;
                u1 = (float(tile & 15) * 16.0f + 15.99f) / 256.0f;
                v0 = float(tile >> 4) * 16.0f / 1024.0f;
                v1 = (float(tile >> 4) * 16.0f + 15.99f) / 1024.0f;
            } else {
                tex = r.texture("romfs:/textures/gui/items.rgba");
                u0 = float(icon % 16 * 16) / 256.0f;
                u1 = (float(icon % 16 * 16) + 15.99f) / 256.0f;
                v0 = float(icon / 16 * 16) / 256.0f;
                v1 = (float(icon / 16 * 16) + 15.99f) / 256.0f;
            }
            t.translate(0.0f, -0.3f, 0.0f);
            t.scale(1.5f, 1.5f, 1.5f);
            t.rotate(50.0f, 1);
            t.rotate(335.0f, 2);
            icon3d(t, u0, u1, v0, v1);
        }
    } else {
        // the bare right arm (RenderPlayer.drawFirstPersonHand)
        tex = r.texture("romfs:/textures/mob/char.rgba");
        M4 t = base;
        t.translate(-s2 * 0.3f, MathHelper::sin(MathHelper::sqrt_float(sp) * PI_F * 2.0f) * 0.4f, -s1 * 0.4f);
        t.translate(0.8f * 0.8f, -0.75f * 0.8f - (1.0f - eq) * 0.6f, -0.9f * 0.8f);
        t.rotate(45.0f, 1);
        t.rotate(s2 * 70.0f, 1);
        t.rotate(-s3 * 20.0f, 2);
        t.translate(-1.0f, 3.6f, 3.5f);
        t.rotate(120.0f, 2);
        t.rotate(200.0f, 0);
        t.rotate(-135.0f, 1);
        t.translate(5.6f, 0.0f, 0.0f);
        // bipedRightArm: rotation point (-5, 2, 0), the idle sway of 0.1 rad about Z
        const float sc = 1.0f / 16.0f;
        t.translate(-5.0f * sc, 2.0f * sc, 0.0f);
        t.rotate(0.1f * 180.0f / PI_F, 2);
        t.scale(sc, sc, sc);
        // ModelBox (-3, -2, -2) 4 x 12 x 4 at (40, 16) of the 64 x 32 skin
        const float x0 = -3, y0 = -2, z0 = -2, x1 = 1, y1 = 10, z1 = 2;
        const float u = 40, v = 16, w = 4, h = 12, dd = 4;
        float V[8][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y1, z0}, {x0, y1, z0},
                         {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
        struct F { int a, b, c, d; float u1, v1, u2, v2; float shade; };
        const F faces[6] = {
            {5, 1, 2, 6, u + dd + w, v + dd, u + dd + w + dd, v + dd + h, 0.6f},
            {0, 4, 7, 3, u, v + dd, u + dd, v + dd + h, 0.6f},
            {5, 4, 0, 1, u + dd, v, u + dd + w, v + dd, 1.0f},
            {2, 3, 7, 6, u + dd + w, v, u + dd + w + w, v + dd, 0.5f},
            {1, 0, 3, 2, u + dd, v + dd, u + dd + w, v + dd + h, 0.8f},
            {4, 5, 6, 7, u + dd + w + dd, v + dd, u + dd + w + dd + w, v + dd + h, 0.8f},
        };
        for (const F& f : faces) {
            float pq[4][3];
            const int idx[4] = {f.a, f.b, f.c, f.d};
            for (int k = 0; k < 4; ++k) std::memcpy(pq[k], V[idx[k]], sizeof pq[k]);
            // TexturedQuad: (u2, v1) (u1, v1) (u1, v2) (u2, v2), in 64 x 32 texels
            float uv[4][2] = {{f.u2 / 64.0f, f.v1 / 32.0f}, {f.u1 / 64.0f, f.v1 / 32.0f}, {f.u1 / 64.0f, f.v2 / 32.0f},
                              {f.u2 / 64.0f, f.v2 / 32.0f}};
            addQuad(t.m, pq, uv, bright * f.shade);
        }
    }
    if (!tex || tris.empty()) return;
    // no depth buffer of its own here: draw the far triangles first
    struct Tri { float z; int i; };
    std::vector<Tri> order(tris.size() / 3);
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = {tris[i * 3].z + tris[i * 3 + 1].z + tris[i * 3 + 2].z, int(i)};
    std::sort(order.begin(), order.end(), [](const Tri& a, const Tri& b) { return a.z < b.z; });
    std::vector<axm::ModelVertex> sorted;
    sorted.reserve(tris.size());
    for (const Tri& t : order)
        for (int k = 0; k < 3; ++k) sorted.push_back(tris[t.i * 3 + k]);
    r.drawFree(r.currentProjection(), sorted, tex, 0, false);
}

