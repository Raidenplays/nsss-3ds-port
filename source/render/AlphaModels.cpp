#include "AlphaModels.h"
#include <algorithm>
#include <cmath>
#include "world/MathHelper.h"

namespace alphamodel {

namespace {
constexpr float PI_F = 3.1415927f;
constexpr float DEG = float(180.0 / 3.141592653589793); // (float) (180.0 / Math.PI)

// RenderHelper.enableStandardItemLighting: two lights fixed in the world, diffuse 0.6, ambient 0.4
struct Lights {
    float a[3], b[3];
    Lights() {
        double x = double(0.2f), y = 1.0, z = double(-0.7f), l = std::sqrt(x * x + y * y + z * z);
        a[0] = float(x / l); a[1] = float(y / l); a[2] = float(z / l);
        x = double(-0.2f); z = double(0.7f); l = std::sqrt(x * x + y * y + z * z);
        b[0] = float(x / l); b[1] = float(y / l); b[2] = float(z / l);
    }
};
const Lights LIGHTS;

float lit(const float n[3]) {
    float d0 = n[0] * LIGHTS.a[0] + n[1] * LIGHTS.a[1] + n[2] * LIGHTS.a[2];
    float d1 = n[0] * LIGHTS.b[0] + n[1] * LIGHTS.b[1] + n[2] * LIGHTS.b[2];
    return std::min(1.0f, 0.4f + 0.6f * (std::max(0.0f, d0) + std::max(0.0f, d1)));
}
} // namespace

void M4::mul(const float* b) {
    float r[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = m[i * 4] * b[j] + m[i * 4 + 1] * b[4 + j] + m[i * 4 + 2] * b[8 + j] + m[i * 4 + 3] * b[12 + j];
    std::memcpy(m, r, sizeof r);
}
void M4::translate(float x, float y, float z) {
    float t[16] = {1, 0, 0, x, 0, 1, 0, y, 0, 0, 1, z, 0, 0, 0, 1};
    mul(t);
}
void M4::scale(float x, float y, float z) {
    float t[16] = {x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1};
    mul(t);
}
void M4::rotate(float deg, float x, float y, float z) {
    float l = std::sqrt(x * x + y * y + z * z);
    if (l == 0.0f) return;
    x /= l; y /= l; z /= l;
    float a = deg * PI_F / 180.0f, c = std::cos(a), s = std::sin(a), k = 1.0f - c;
    float r[16] = {x * x * k + c,     x * y * k - z * s, x * z * k + y * s, 0,
                   y * x * k + z * s, y * y * k + c,     y * z * k - x * s, 0,
                   x * z * k - y * s, y * z * k + x * s, z * z * k + c,     0,
                   0, 0, 0, 1};
    mul(r);
}

// ---------------------------------------------------------------- ModelRenderer

void ModelRenderer::addBox(float f, float f1, float f2, int i, int j, int k, float f3) {
    float f4 = f + float(i), f5 = f1 + float(j), f6 = f2 + float(k);
    f -= f3; f1 -= f3; f2 -= f3;
    f4 += f3; f5 += f3; f6 += f3;
    if (mirror) std::swap(f, f4);
    const float c[8][3] = {{f, f1, f2}, {f4, f1, f2}, {f4, f5, f2}, {f, f5, f2},
                           {f, f1, f6}, {f4, f1, f6}, {f4, f5, f6}, {f, f5, f6}};
    const int u = texU, v = texV;
    // TexturedQuad(corners, u1, v1, u2, v2)
    const int F[6][4] = {{5, 1, 2, 6}, {0, 4, 7, 3}, {5, 4, 0, 1}, {7, 6, 2, 3}, {1, 0, 3, 2}, {4, 5, 6, 7}};
    const int UV[6][4] = {{u + k + i, v + k, u + k + i + k, v + k + j},
                          {u, v + k, u + k, v + k + j},
                          {u + k, v, u + k + i, v + k},
                          {u + k + i, v, u + k + i + i, v + k},
                          {u + k, v + k, u + k + i, v + k + j},
                          {u + k + i + k, v + k, u + k + i + k + i, v + k + j}};
    const float eu = 0.0015625f, ev = 0.003125f;
    for (int n = 0; n < 6; ++n) {
        float u1 = float(UV[n][0]) / 64.0f, v1 = float(UV[n][1]) / 32.0f, u2 = float(UV[n][2]) / 64.0f,
              v2 = float(UV[n][3]) / 32.0f;
        const float uv[4][2] = {{u2 - eu, v1 + ev}, {u1 + eu, v1 + ev}, {u1 + eu, v2 - ev}, {u2 - eu, v2 - ev}};
        for (int q = 0; q < 4; ++q) {
            int src = mirror ? 3 - q : q; // TexturedQuad.func_809_a: the corners reversed
            std::memcpy(faces[n].p[q], c[F[n][src]], sizeof faces[n].p[q]);
            faces[n].uv[q][0] = uv[src][0];
            faces[n].uv[q][1] = uv[src][1];
        }
    }
    built = true;
}

void ModelRenderer::render(std::vector<Vertex>& out, const M4& parent, float f, float bright) const {
    if (!built || !showModel) return;
    M4 t = parent;
    if (rotateAngleX != 0.0f || rotateAngleY != 0.0f || rotateAngleZ != 0.0f) {
        t.translate(rotationPointX * f, rotationPointY * f, rotationPointZ * f);
        if (rotateAngleZ != 0.0f) t.rotate(rotateAngleZ * DEG, 0, 0, 1);
        if (rotateAngleY != 0.0f) t.rotate(rotateAngleY * DEG, 0, 1, 0);
        if (rotateAngleX != 0.0f) t.rotate(rotateAngleX * DEG, 1, 0, 0);
    } else if (rotationPointX != 0.0f || rotationPointY != 0.0f || rotationPointZ != 0.0f) {
        t.translate(rotationPointX * f, rotationPointY * f, rotationPointZ * f);
    }
    const float* m = t.m;
    for (const Face& fc : faces) {
        Vertex v[4];
        for (int q = 0; q < 4; ++q) {
            float x = fc.p[q][0] * f, y = fc.p[q][1] * f, z = fc.p[q][2] * f;
            v[q].x = m[0] * x + m[1] * y + m[2] * z + m[3];
            v[q].y = m[4] * x + m[5] * y + m[6] * z + m[7];
            v[q].z = m[8] * x + m[9] * y + m[10] * z + m[11];
            v[q].u = fc.uv[q][0];
            v[q].v = fc.uv[q][1];
        }
        // TexturedQuad.func_808_a: the normal from the first three corners, then GL_RESCALE_NORMAL
        float ax = v[1].x - v[0].x, ay = v[1].y - v[0].y, az = v[1].z - v[0].z;
        float bx = v[1].x - v[2].x, by = v[1].y - v[2].y, bz = v[1].z - v[2].z;
        float n[3] = {by * az - bz * ay, bz * ax - bx * az, bx * ay - by * ax};
        float nl = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        float shade = bright;
        if (nl > 0.0f) {
            n[0] /= nl; n[1] /= nl; n[2] /= nl;
            // a mirrored transform (RenderLiving's scale(-1, -1, 1)) turns the winding, not the normal
            float det = m[0] * (m[5] * m[10] - m[6] * m[9]) - m[1] * (m[4] * m[10] - m[6] * m[8]) + m[2] * (m[4] * m[9] - m[5] * m[8]);
            if (det < 0.0f) { n[0] = -n[0]; n[1] = -n[1]; n[2] = -n[2]; }
            shade = bright * lit(n);
        }
        for (Vertex& q : v) q.shade = shade;
        out.push_back(v[0]); out.push_back(v[1]); out.push_back(v[2]);
        out.push_back(v[0]); out.push_back(v[2]); out.push_back(v[3]);
    }
}

// ---------------------------------------------------------------- ModelQuadraped and family

Quadruped::Quadruped(int i, float f) : head(0, 0), body(28, 8), leg1(0, 16), leg2(0, 16), leg3(0, 16), leg4(0, 16) {
    head.addBox(-4.0f, -4.0f, -8.0f, 8, 8, 8, f);
    head.setRotationPoint(0.0f, float(18 - i), -6.0f);
    body.addBox(-5.0f, -10.0f, -7.0f, 10, 16, 8, f);
    body.setRotationPoint(0.0f, float(17 - i), 2.0f);
    leg1.addBox(-2.0f, 0.0f, -2.0f, 4, i, 4, f);
    leg1.setRotationPoint(-3.0f, float(24 - i), 7.0f);
    leg2.addBox(-2.0f, 0.0f, -2.0f, 4, i, 4, f);
    leg2.setRotationPoint(3.0f, float(24 - i), 7.0f);
    leg3.addBox(-2.0f, 0.0f, -2.0f, 4, i, 4, f);
    leg3.setRotationPoint(-3.0f, float(24 - i), -5.0f);
    leg4.addBox(-2.0f, 0.0f, -2.0f, 4, i, 4, f);
    leg4.setRotationPoint(3.0f, float(24 - i), -5.0f);
}

void Quadruped::setRotationAngles(float f, float f1, float, float f3, float f4) {
    head.rotateAngleX = -f4 / DEG;
    head.rotateAngleY = f3 / DEG;
    body.rotateAngleX = 1.570796f;
    leg1.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
    leg2.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    leg3.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    leg4.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
}

void Quadruped::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    head.render(out, t, s, b);
    body.render(out, t, s, b);
    leg1.render(out, t, s, b);
    leg2.render(out, t, s, b);
    leg3.render(out, t, s, b);
    leg4.render(out, t, s, b);
}

Cow::Cow() : Quadruped(12, 0.0f), udders(52, 0), horn1(22, 0), horn2(22, 0) {
    head = ModelRenderer(0, 0);
    head.addBox(-4.0f, -4.0f, -6.0f, 8, 8, 6, 0.0f);
    head.setRotationPoint(0.0f, 4.0f, -8.0f);
    horn1.addBox(-5.0f, -5.0f, -4.0f, 1, 3, 1, 0.0f);
    horn1.setRotationPoint(0.0f, 3.0f, -7.0f);
    horn2.addBox(4.0f, -5.0f, -4.0f, 1, 3, 1, 0.0f);
    horn2.setRotationPoint(0.0f, 3.0f, -7.0f);
    udders.addBox(-2.0f, -3.0f, 0.0f, 4, 6, 2, 0.0f);
    udders.setRotationPoint(0.0f, 14.0f, 6.0f);
    udders.rotateAngleX = 1.570796f;
    body = ModelRenderer(18, 4);
    body.addBox(-6.0f, -10.0f, -7.0f, 12, 18, 10, 0.0f);
    body.setRotationPoint(0.0f, 5.0f, 2.0f);
    leg1.rotationPointX--;
    leg2.rotationPointX++;
    leg3.rotationPointX--;
    leg4.rotationPointX++;
    leg3.rotationPointZ--;
    leg4.rotationPointZ--;
}

void Cow::setRotationAngles(float f, float f1, float f2, float f3, float f4) {
    Quadruped::setRotationAngles(f, f1, f2, f3, f4);
    horn1.rotateAngleY = head.rotateAngleY;
    horn1.rotateAngleX = head.rotateAngleX;
    horn2.rotateAngleY = head.rotateAngleY;
    horn2.rotateAngleX = head.rotateAngleX;
}

void Cow::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    Quadruped::render(out, t, s, b);
    horn1.render(out, t, s, b);
    horn2.render(out, t, s, b);
    udders.render(out, t, s, b);
}

Sheep1::Sheep1() : Quadruped(12, 0.0f) {
    head = ModelRenderer(0, 0);
    head.addBox(-3.0f, -4.0f, -4.0f, 6, 6, 6, 0.6f);
    head.setRotationPoint(0.0f, 6.0f, -8.0f);
    body = ModelRenderer(28, 8);
    body.addBox(-4.0f, -10.0f, -7.0f, 8, 16, 6, 1.75f);
    body.setRotationPoint(0.0f, 5.0f, 2.0f);
    const float f = 0.5f;
    ModelRenderer* legs[4] = {&leg1, &leg2, &leg3, &leg4};
    const float pos[4][3] = {{-3.0f, 12.0f, 7.0f}, {3.0f, 12.0f, 7.0f}, {-3.0f, 12.0f, -5.0f}, {3.0f, 12.0f, -5.0f}};
    for (int i = 0; i < 4; ++i) {
        *legs[i] = ModelRenderer(0, 16);
        legs[i]->addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, f);
        legs[i]->setRotationPoint(pos[i][0], pos[i][1], pos[i][2]);
    }
}

Sheep2::Sheep2() : Quadruped(12, 0.0f) {
    head = ModelRenderer(0, 0);
    head.addBox(-3.0f, -4.0f, -6.0f, 6, 6, 8, 0.0f);
    head.setRotationPoint(0.0f, 6.0f, -8.0f);
    body = ModelRenderer(28, 8);
    body.addBox(-4.0f, -10.0f, -7.0f, 8, 16, 6, 0.0f);
    body.setRotationPoint(0.0f, 5.0f, 2.0f);
}

// ---------------------------------------------------------------- ModelBiped, ModelZombie, ModelSkeleton

Biped::Biped(float f, float f1)
    : head(0, 0), headwear(32, 0), bodyPart(16, 16), rightArm(40, 16), leftArm(40, 16), rightLeg(0, 16), leftLeg(0, 16) {
    head.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8, f);
    head.setRotationPoint(0.0f, 0.0f + f1, 0.0f);
    headwear.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8, f + 0.5f);
    headwear.setRotationPoint(0.0f, 0.0f + f1, 0.0f);
    bodyPart.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4, f);
    bodyPart.setRotationPoint(0.0f, 0.0f + f1, 0.0f);
    rightArm.addBox(-3.0f, -2.0f, -2.0f, 4, 12, 4, f);
    rightArm.setRotationPoint(-5.0f, 2.0f + f1, 0.0f);
    leftArm.mirror = true;
    leftArm.addBox(-1.0f, -2.0f, -2.0f, 4, 12, 4, f);
    leftArm.setRotationPoint(5.0f, 2.0f + f1, 0.0f);
    rightLeg.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4, f);
    rightLeg.setRotationPoint(-2.0f, 12.0f + f1, 0.0f);
    leftLeg.mirror = true;
    leftLeg.addBox(-2.0f, 0.0f, -2.0f, 4, 12, 4, f);
    leftLeg.setRotationPoint(2.0f, 12.0f + f1, 0.0f);
}

// ModelBiped.setRotationAngles with oldWalking (NSSS sets it): the arms also swing out sideways
void Biped::setRotationAngles(float f, float f1, float f2, float f3, float f4) {
    head.rotateAngleY = f3 / DEG;
    head.rotateAngleX = f4 / DEG;
    headwear.rotateAngleY = head.rotateAngleY;
    headwear.rotateAngleX = head.rotateAngleX;
    rightArm.rotateAngleX = MathHelper::cos(f * 0.6662f + PI_F) * 2.0f * f1;
    leftArm.rotateAngleX = MathHelper::cos(f * 0.6662f) * 2.0f * f1;
    leftArm.rotateAngleZ = (MathHelper::cos(f * 0.2812f) - 1.0f) * 1.0f * f1;
    rightArm.rotateAngleZ = (MathHelper::cos(f * 0.2312f) + 1.0f) * 1.0f * f1;
    rightLeg.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
    leftLeg.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    rightLeg.rotateAngleY = 0.0f;
    leftLeg.rotateAngleY = 0.0f;
    if (riding) {
        rightArm.rotateAngleX += -0.6283185f;
        leftArm.rotateAngleX += -0.6283185f;
        rightLeg.rotateAngleX = -1.256637f;
        leftLeg.rotateAngleX = -1.256637f;
        rightLeg.rotateAngleY = 0.3141593f;
        leftLeg.rotateAngleY = -0.3141593f;
    }
    rightArm.rotateAngleY = 0.0f;
    leftArm.rotateAngleY = 0.0f;
    if (onGround > -9990.0f) {
        float f6 = onGround;
        bodyPart.rotateAngleY = MathHelper::sin(MathHelper::sqrt_float(f6) * 3.141593f * 2.0f) * 0.2f;
        rightArm.rotationPointZ = MathHelper::sin(bodyPart.rotateAngleY) * 5.0f;
        rightArm.rotationPointX = -MathHelper::cos(bodyPart.rotateAngleY) * 5.0f;
        leftArm.rotationPointZ = -MathHelper::sin(bodyPart.rotateAngleY) * 5.0f;
        leftArm.rotationPointX = MathHelper::cos(bodyPart.rotateAngleY) * 5.0f;
        rightArm.rotateAngleY = rightArm.rotateAngleY + bodyPart.rotateAngleY;
        leftArm.rotateAngleY = leftArm.rotateAngleY + bodyPart.rotateAngleY;
        leftArm.rotateAngleX = leftArm.rotateAngleX + bodyPart.rotateAngleY;
        f6 = 1.0f - onGround;
        f6 *= f6;
        f6 *= f6;
        f6 = 1.0f - f6;
        float f7 = MathHelper::sin(f6 * 3.141593f);
        float f8 = MathHelper::sin(onGround * 3.141593f) * -(head.rotateAngleX - 0.7f) * 0.75f;
        rightArm.rotateAngleX = float(double(rightArm.rotateAngleX) - (double(f7) * 1.2 + double(f8)));
        rightArm.rotateAngleY = rightArm.rotateAngleY + bodyPart.rotateAngleY * 2.0f;
        rightArm.rotateAngleZ = MathHelper::sin(onGround * 3.141593f) * -0.4f;
    }
    if (sneaking) {
        bodyPart.rotateAngleX = 0.5f;
        rightArm.rotateAngleX += 0.4f;
        leftArm.rotateAngleX += 0.4f;
        rightLeg.rotationPointZ = 4.0f;
        leftLeg.rotationPointZ = 4.0f;
        rightLeg.rotationPointY = 9.0f;
        leftLeg.rotationPointY = 9.0f;
        head.rotationPointY = 1.0f;
        headwear.rotationPointY = 1.0f;
    } else {
        bodyPart.rotateAngleX = 0.0f;
        rightLeg.rotationPointZ = 0.0f;
        leftLeg.rotationPointZ = 0.0f;
        rightLeg.rotationPointY = 12.0f;
        leftLeg.rotationPointY = 12.0f;
        head.rotationPointY = 0.0f;
        headwear.rotationPointY = 0.0f;
    }
    rightArm.rotateAngleZ = rightArm.rotateAngleZ + (MathHelper::cos(f2 * 0.09f) * 0.05f + 0.05f);
    leftArm.rotateAngleZ = leftArm.rotateAngleZ - (MathHelper::cos(f2 * 0.09f) * 0.05f + 0.05f);
    rightArm.rotateAngleX = rightArm.rotateAngleX + MathHelper::sin(f2 * 0.067f) * 0.05f;
    leftArm.rotateAngleX = leftArm.rotateAngleX - MathHelper::sin(f2 * 0.067f) * 0.05f;
}

void Biped::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    head.render(out, t, s, b);
    bodyPart.render(out, t, s, b);
    rightArm.render(out, t, s, b);
    leftArm.render(out, t, s, b);
    rightLeg.render(out, t, s, b);
    leftLeg.render(out, t, s, b);
    headwear.render(out, t, s, b);
}

// ModelZombie: the head's pitch goes in turned round (-f4), the arms held out in front
void Zombie::setRotationAngles(float f, float f1, float f2, float f3, float f4) {
    Biped::setRotationAngles(f, f1, f2, f3, -f4);
    float f6 = MathHelper::sin(onGround * 3.141593f);
    float f7 = MathHelper::sin((1.0f - (1.0f - onGround) * (1.0f - onGround)) * 3.141593f);
    rightArm.rotateAngleZ = 0.0f;
    leftArm.rotateAngleZ = 0.0f;
    rightArm.rotateAngleY = -(0.1f - f6 * 0.6f);
    leftArm.rotateAngleY = 0.1f - f6 * 0.6f;
    rightArm.rotateAngleX = -1.570796f;
    leftArm.rotateAngleX = -1.570796f;
    rightArm.rotateAngleX -= f6 * 1.2f - f7 * 0.4f;
    leftArm.rotateAngleX -= f6 * 1.2f - f7 * 0.4f;
    rightArm.rotateAngleZ = rightArm.rotateAngleZ + (MathHelper::cos(f2 * 0.09f) * 0.05f + 0.05f);
    leftArm.rotateAngleZ = leftArm.rotateAngleZ - (MathHelper::cos(f2 * 0.09f) * 0.05f + 0.05f);
    rightArm.rotateAngleX = rightArm.rotateAngleX + MathHelper::sin(f2 * 0.067f) * 0.05f;
    leftArm.rotateAngleX = leftArm.rotateAngleX - MathHelper::sin(f2 * 0.067f) * 0.05f;
}

Skeleton::Skeleton() {
    rightArm = ModelRenderer(40, 16);
    rightArm.addBox(-1.0f, -2.0f, -1.0f, 2, 12, 2, 0.0f);
    rightArm.setRotationPoint(-5.0f, 2.0f, 0.0f);
    leftArm = ModelRenderer(40, 16);
    leftArm.mirror = true;
    leftArm.addBox(-1.0f, -2.0f, -1.0f, 2, 12, 2, 0.0f);
    leftArm.setRotationPoint(5.0f, 2.0f, 0.0f);
    rightLeg = ModelRenderer(0, 16);
    rightLeg.addBox(-1.0f, 0.0f, -1.0f, 2, 12, 2, 0.0f);
    rightLeg.setRotationPoint(-2.0f, 12.0f, 0.0f);
    leftLeg = ModelRenderer(0, 16);
    leftLeg.mirror = true;
    leftLeg.addBox(-1.0f, 0.0f, -1.0f, 2, 12, 2, 0.0f);
    leftLeg.setRotationPoint(2.0f, 12.0f, 0.0f);
}

// ---------------------------------------------------------------- ModelChicken

Chicken::Chicken()
    : head(0, 0), bodyPart(0, 9), rightLeg(26, 0), leftLeg(26, 0), rightWing(24, 13), leftWing(24, 13), bill(14, 0), chin(14, 4) {
    const int b = 16;
    head.addBox(-2.0f, -6.0f, -2.0f, 4, 6, 3, 0.0f);
    head.setRotationPoint(0.0f, float(-1 + b), -4.0f);
    bill.addBox(-2.0f, -4.0f, -4.0f, 4, 2, 2, 0.0f);
    bill.setRotationPoint(0.0f, float(-1 + b), -4.0f);
    chin.addBox(-1.0f, -2.0f, -3.0f, 2, 2, 2, 0.0f);
    chin.setRotationPoint(0.0f, float(-1 + b), -4.0f);
    bodyPart.addBox(-3.0f, -4.0f, -3.0f, 6, 8, 6, 0.0f);
    bodyPart.setRotationPoint(0.0f, float(0 + b), 0.0f);
    rightLeg.addBox(-1.0f, 0.0f, -3.0f, 3, 5, 3);
    rightLeg.setRotationPoint(-2.0f, float(3 + b), 1.0f);
    leftLeg.addBox(-1.0f, 0.0f, -3.0f, 3, 5, 3);
    leftLeg.setRotationPoint(1.0f, float(3 + b), 1.0f);
    rightWing.addBox(0.0f, 0.0f, -3.0f, 1, 4, 6);
    rightWing.setRotationPoint(-4.0f, float(-3 + b), 0.0f);
    leftWing.addBox(-1.0f, 0.0f, -3.0f, 1, 4, 6);
    leftWing.setRotationPoint(4.0f, float(-3 + b), 0.0f);
}

void Chicken::setRotationAngles(float f, float f1, float f2, float f3, float f4) {
    head.rotateAngleX = -(f4 / DEG);
    head.rotateAngleY = f3 / DEG;
    bill.rotateAngleX = head.rotateAngleX;
    bill.rotateAngleY = head.rotateAngleY;
    chin.rotateAngleX = head.rotateAngleX;
    chin.rotateAngleY = head.rotateAngleY;
    bodyPart.rotateAngleX = 1.570796f;
    rightLeg.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
    leftLeg.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    rightWing.rotateAngleZ = f2;
    leftWing.rotateAngleZ = -f2;
}

void Chicken::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    head.render(out, t, s, b);
    bill.render(out, t, s, b);
    chin.render(out, t, s, b);
    bodyPart.render(out, t, s, b);
    rightLeg.render(out, t, s, b);
    leftLeg.render(out, t, s, b);
    rightWing.render(out, t, s, b);
    leftWing.render(out, t, s, b);
}

// ---------------------------------------------------------------- ModelCreeper

Creeper::Creeper() : head(0, 0), unused(32, 0), bodyPart(16, 16), leg1(0, 16), leg2(0, 16), leg3(0, 16), leg4(0, 16) {
    const int i = 4;
    head.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8, 0.0f);
    head.setRotationPoint(0.0f, float(i), 0.0f);
    unused.addBox(-4.0f, -8.0f, -4.0f, 8, 8, 8, 0.5f);
    unused.setRotationPoint(0.0f, float(i), 0.0f);
    bodyPart.addBox(-4.0f, 0.0f, -2.0f, 8, 12, 4, 0.0f);
    bodyPart.setRotationPoint(0.0f, float(i), 0.0f);
    leg1.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f);
    leg1.setRotationPoint(-2.0f, float(12 + i), 4.0f);
    leg2.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f);
    leg2.setRotationPoint(2.0f, float(12 + i), 4.0f);
    leg3.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f);
    leg3.setRotationPoint(-2.0f, float(12 + i), -4.0f);
    leg4.addBox(-2.0f, 0.0f, -2.0f, 4, 6, 4, 0.0f);
    leg4.setRotationPoint(2.0f, float(12 + i), -4.0f);
}

void Creeper::setRotationAngles(float f, float f1, float, float f3, float f4) {
    head.rotateAngleY = f3 / DEG;
    head.rotateAngleX = -f4 / DEG;
    leg1.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
    leg2.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    leg3.rotateAngleX = MathHelper::cos(f * 0.6662f + 3.141593f) * 1.4f * f1;
    leg4.rotateAngleX = MathHelper::cos(f * 0.6662f) * 1.4f * f1;
}

void Creeper::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    head.render(out, t, s, b);
    bodyPart.render(out, t, s, b);
    leg1.render(out, t, s, b);
    leg2.render(out, t, s, b);
    leg3.render(out, t, s, b);
    leg4.render(out, t, s, b);
}

// ---------------------------------------------------------------- ModelSpider

Spider::Spider() : head(32, 4), neck(0, 0), bodyPart(0, 12) {
    const int i = 15;
    head.addBox(-4.0f, -4.0f, -8.0f, 8, 8, 8, 0.0f);
    head.setRotationPoint(0.0f, float(0 + i), -3.0f);
    neck.addBox(-3.0f, -3.0f, -3.0f, 6, 6, 6, 0.0f);
    neck.setRotationPoint(0.0f, float(i), 0.0f);
    bodyPart.addBox(-5.0f, -4.0f, -6.0f, 10, 8, 12, 0.0f);
    bodyPart.setRotationPoint(0.0f, float(0 + i), 9.0f);
    const float z[8] = {2.0f, 2.0f, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, -1.0f};
    for (int k = 0; k < 8; ++k) {
        legs[k] = ModelRenderer(18, 0);
        bool right = k % 2 == 0;
        legs[k].addBox(right ? -15.0f : -1.0f, -1.0f, -1.0f, 16, 2, 2, 0.0f);
        legs[k].setRotationPoint(right ? -4.0f : 4.0f, float(0 + i), z[k]);
    }
}

void Spider::setRotationAngles(float f, float f1, float, float f3, float f4) {
    head.rotateAngleY = f3 / DEG;
    head.rotateAngleX = -f4 / DEG;
    const float f6 = float(3.141592653589793 / 4);
    legs[0].rotateAngleZ = -f6;
    legs[1].rotateAngleZ = f6;
    legs[2].rotateAngleZ = -f6 * 0.74f;
    legs[3].rotateAngleZ = f6 * 0.74f;
    legs[4].rotateAngleZ = -f6 * 0.74f;
    legs[5].rotateAngleZ = f6 * 0.74f;
    legs[6].rotateAngleZ = -f6;
    legs[7].rotateAngleZ = f6;
    const float f7 = -0.0f, f8 = float(3.141592653589793 / 8);
    legs[0].rotateAngleY = f8 * 2.0f + f7;
    legs[1].rotateAngleY = -f8 * 2.0f - f7;
    legs[2].rotateAngleY = f8 * 1.0f + f7;
    legs[3].rotateAngleY = -f8 * 1.0f - f7;
    legs[4].rotateAngleY = -f8 * 1.0f + f7;
    legs[5].rotateAngleY = f8 * 1.0f - f7;
    legs[6].rotateAngleY = -f8 * 2.0f + f7;
    legs[7].rotateAngleY = f8 * 2.0f - f7;
    const float h32 = float(3.141592653589793 * 3.0 / 2.0);
    float f9 = -(MathHelper::cos(f * 0.6662f * 2.0f + 0.0f) * 0.4f) * f1;
    float f10 = -(MathHelper::cos(f * 0.6662f * 2.0f + 3.141593f) * 0.4f) * f1;
    float f11 = -(MathHelper::cos(f * 0.6662f * 2.0f + 1.570796f) * 0.4f) * f1;
    float f12 = -(MathHelper::cos(f * 0.6662f * 2.0f + h32) * 0.4f) * f1;
    float f13 = std::fabs(MathHelper::sin(f * 0.6662f + 0.0f) * 0.4f) * f1;
    float f14 = std::fabs(MathHelper::sin(f * 0.6662f + 3.141593f) * 0.4f) * f1;
    float f15 = std::fabs(MathHelper::sin(f * 0.6662f + 1.570796f) * 0.4f) * f1;
    float f16 = std::fabs(MathHelper::sin(f * 0.6662f + h32) * 0.4f) * f1;
    const float ys[4] = {f9, f10, f11, f12}, zs[4] = {f13, f14, f15, f16};
    for (int k = 0; k < 4; ++k) {
        legs[k * 2].rotateAngleY += ys[k];
        legs[k * 2 + 1].rotateAngleY += -ys[k];
        legs[k * 2].rotateAngleZ += zs[k];
        legs[k * 2 + 1].rotateAngleZ += -zs[k];
    }
}

void Spider::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    head.render(out, t, s, b);
    neck.render(out, t, s, b);
    bodyPart.render(out, t, s, b);
    for (const ModelRenderer& l : legs) l.render(out, t, s, b);
}

// ---------------------------------------------------------------- ModelSlime

Slime::Slime(int i) : bodies(0, i), rightEye(32, 0), leftEye(32, 4), mouth(32, 8), face(i > 0) {
    if (!face) {
        bodies.addBox(-4.0f, 16.0f, -4.0f, 8, 8, 8);
    } else {
        bodies.addBox(-3.0f, 17.0f, -3.0f, 6, 6, 6);
        rightEye.addBox(-3.25f, 18.0f, -3.5f, 2, 2, 2);
        leftEye.addBox(1.25f, 18.0f, -3.5f, 2, 2, 2);
        mouth.addBox(0.0f, 21.0f, -3.5f, 1, 1, 1);
    }
}

void Slime::render(std::vector<Vertex>& out, const M4& t, float s, float b) const {
    bodies.render(out, t, s, b);
    if (face) {
        rightEye.render(out, t, s, b);
        leftEye.render(out, t, s, b);
        mouth.render(out, t, s, b);
    }
}

} // namespace alphamodel
