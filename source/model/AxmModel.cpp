#include "AxmModel.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace axm {
namespace {

struct Reader {
    const uint8_t* p;
    const uint8_t* end;
    bool bad = false;
    bool need(size_t n) {
        if (size_t(end - p) < n) bad = true;
        return !bad;
    }
    uint8_t u8() { if (!need(1)) return 0; return *p++; }
    uint16_t u16() { if (!need(2)) return 0; uint16_t v = uint16_t(p[0] | p[1] << 8); p += 2; return v; }
    int16_t i16() { return int16_t(u16()); }
    float f32() {
        if (!need(4)) return 0;
        uint32_t v = uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
        p += 4;
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
    std::string str() {
        uint8_t n = u8();
        if (!need(n)) return {};
        std::string s(reinterpret_cast<const char*>(p), n);
        p += n;
        return s;
    }
};

inline float rad(float deg) { return deg / (180.0f / 3.14159265358979323846f); }
inline int javaRound(float f) { return int(std::floor(f + 0.5f)); }

// ---- float 4x4, row-major, column vectors (like the Java rig maths) ----
void fIdentity(float* m) {
    std::memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1;
}
void fMul(const float* a, const float* b, float* o) {
    float r[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + a[i * 4 + 3] * b[12 + j];
    std::memcpy(o, r, 64);
}
void fTranslate(float* m, float x, float y, float z) { // m = m * T
    float t[16];
    fIdentity(t);
    t[3] = x; t[7] = y; t[11] = z;
    fMul(m, t, m);
}
void fRotate(float* m, float a, int axis) { // m = m * R(axis, a radians) - glRotatef
    if (a == 0.0f) return;
    float c = std::cos(a), s = std::sin(a), r[16];
    fIdentity(r);
    if (axis == 0) { r[5] = c; r[6] = -s; r[9] = s; r[10] = c; }
    else if (axis == 1) { r[0] = c; r[2] = s; r[8] = -s; r[10] = c; }
    else { r[0] = c; r[1] = -s; r[4] = s; r[5] = c; }
    fMul(m, r, m);
}
void fScale(float* m, float x, float y, float z) {
    for (int i = 0; i < 4; i++) { m[i * 4] *= x; m[i * 4 + 1] *= y; m[i * 4 + 2] *= z; }
}
void fApply(const float* m, const float* v, float* o) {
    float x = v[0], y = v[1], z = v[2];
    o[0] = m[0] * x + m[1] * y + m[2] * z + m[3];
    o[1] = m[4] * x + m[5] * y + m[6] * z + m[7];
    o[2] = m[8] * x + m[9] * y + m[10] * z + m[11];
}

// ---- double 4x4 (AxeolonBlockRig) ----
void dIdentity(double* m) {
    for (int i = 0; i < 16; i++) m[i] = (i % 5 == 0) ? 1.0 : 0.0;
}
void dMul(const double* a, const double* b, double* o) {
    double r[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += a[i * 4 + k] * b[k * 4 + j];
            r[i * 4 + j] = s;
        }
    std::memcpy(o, r, sizeof r);
}
void dTranslate(double* o, double x, double y, double z) {
    dIdentity(o);
    o[3] = x; o[7] = y; o[11] = z;
}
void dRot(double* o, double deg, int axis) {
    double a = deg * (3.14159265358979323846 / 180.0), c = std::cos(a), s = std::sin(a);
    dIdentity(o);
    if (axis == 0) { o[5] = c; o[6] = -s; o[9] = s; o[10] = c; }
    else if (axis == 1) { o[0] = c; o[2] = s; o[8] = -s; o[10] = c; }
    else { o[0] = c; o[1] = -s; o[4] = s; o[5] = c; }
}
void dMulBy(double* m, const double* b) { dMul(m, b, m); }
void dEuler(double* o, const float r[3]) { // Rz * Ry * Rx
    double z[16], y[16], x[16];
    dRot(z, r[2], 2); dRot(y, r[1], 1); dRot(x, r[0], 0);
    dMul(z, y, o);
    dMul(o, x, o);
}
void dApply(const double* m, const float* v, double* o) {
    o[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
    o[1] = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7];
    o[2] = m[8] * v[0] + m[9] * v[1] + m[10] * v[2] + m[11];
}

float cr(float p0, float p1, float p2, float p3, float t) {
    float t2 = t * t, t3 = t2 * t;
    return 0.5f * (2.0f * p1 + (-p0 + p2) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

// Beta's RenderHelper.enableStandardItemLighting: two lights, diffuse 0.6, ambient 0.4
float itemLighting(float nx, float ny, float nz) {
    static const float L0[3] = {0.16169041f, 0.80845207f, -0.56591645f}; // (0.2, 1, -0.7) normalised
    static const float L1[3] = {-0.16169041f, 0.80845207f, 0.56591645f};
    float a = nx * L0[0] + ny * L0[1] + nz * L0[2];
    float b = nx * L1[0] + ny * L1[1] + nz * L1[2];
    float s = 0.4f + 0.6f * (a > 0 ? a : 0) + 0.6f * (b > 0 ? b : 0);
    return s > 1.0f ? 1.0f : s;
}

void readCube(Reader& r, Cube& c) {
    for (float& v : c.from) v = r.f32();
    for (float& v : c.to) v = r.f32();
    for (float& v : c.origin) v = r.f32();
    for (float& v : c.rot) v = r.f32();
    c.inflate = r.f32();
    c.uvOffset[0] = r.f32();
    c.uvOffset[1] = r.f32();
    uint8_t flags = r.u8();
    c.mirror = flags & 1;
    c.visible = flags & 2;
    c.name = r.str();
    c.faceMask = r.u8();
    for (int d = 0; d < 6; d++) {
        if (!(c.faceMask & (1 << d))) continue;
        for (float& v : c.faceUV[d]) v = r.f32();
        c.faceRot[d] = r.u8();
    }
}

} // namespace

bool Model::load(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(n > 0 ? size_t(n) : 0);
    bool ok = n > 0 && fread(buf.data(), 1, buf.size(), f) == buf.size();
    fclose(f);
    return ok && loadMemory(buf.data(), buf.size());
}

bool Model::loadMemory(const uint8_t* data, size_t size) {
    Reader r{data, data + size};
    if (size < 9 || std::memcmp(data, "AXM1", 4) != 0) return false;
    r.p += 4;
    kind_ = r.u8();
    texW_ = r.u16();
    texH_ = r.u16();
    bones_.clear();
    cubes_.clear();
    roots_.clear();
    anims_.clear();

    int nb = r.u16();
    bones_.resize(nb);
    for (int i = 0; i < nb && !r.bad; i++) {
        Bone& b = bones_[i];
        b.name = r.str();
        b.parent = r.i16();
        for (float& v : b.origin) v = r.f32();
        for (float& v : b.rot) v = r.f32();
        int nc = r.u16();
        for (int k = 0; k < nc && !r.bad; k++) {
            Cube c;
            readCube(r, c);
            b.cubes.push_back(int(cubes_.size()));
            cubes_.push_back(std::move(c));
        }
        if (b.parent >= 0 && b.parent < i) bones_[b.parent].kids.push_back(i);
        else roots_.push_back(i);
    }
    int loose = r.u16();
    if (loose > 0) {
        Bone b;
        b.name = "_root";
        b.inMap = false;
        for (int k = 0; k < 3; k++) b.origin[k] = b.rot[k] = 0;
        for (int k = 0; k < loose && !r.bad; k++) {
            Cube c;
            readCube(r, c);
            b.cubes.push_back(int(cubes_.size()));
            cubes_.push_back(std::move(c));
        }
        looseBone_ = int(bones_.size());
        bones_.push_back(std::move(b));
        roots_.push_back(looseBone_);
    }

    // Name lookup like Java's HashMap filled in post-order (children first): the last put wins.
    std::unordered_map<std::string, int> byName;
    {
        std::vector<int> order;
        std::vector<std::pair<int, size_t>> stack;
        for (int root : roots_) {
            if (root == looseBone_) continue;
            stack.push_back({root, 0});
            while (!stack.empty()) {
                auto& top = stack.back();
                const Bone& b = bones_[top.first];
                if (top.second < b.kids.size()) {
                    int k = b.kids[top.second++];
                    stack.push_back({k, 0});
                } else {
                    order.push_back(top.first);
                    stack.pop_back();
                }
            }
        }
        for (int i : order) byName[bones_[i].name] = i;
        for (int i = 0; i < int(bones_.size()); i++) {
            auto it = byName.find(bones_[i].name);
            bones_[i].inMap = i != looseBone_ && it != byName.end() && it->second == i;
        }
    }

    int na = r.u16();
    for (int i = 0; i < na && !r.bad; i++) {
        Animation a;
        a.name = r.str();
        a.length = r.f32();
        a.loop = r.u8();
        a.rot.assign(bones_.size(), -1);
        a.pos.assign(bones_.size(), -1);
        a.scl.assign(bones_.size(), -1);
        int nt = r.u16();
        for (int t = 0; t < nt && !r.bad; t++) {
            std::string bone = r.str();
            int ch = r.u8();
            int nk = r.u16();
            Channel c;
            for (int k = 0; k < nk && !r.bad; k++) {
                c.times.push_back(r.f32());
                c.values.push_back(r.f32());
                c.values.push_back(r.f32());
                c.values.push_back(r.f32());
                c.interp.push_back(r.u8());
            }
            int ci = int(a.channels.size());
            a.channels.push_back(std::move(c));
            // every bone of this name reads the channel (sampleBone / the rig look up by name)
            for (size_t b = 0; b < bones_.size(); b++) {
                if (int(b) == looseBone_ || bones_[b].name != bone) continue;
                (ch == 0 ? a.rot : ch == 1 ? a.pos : a.scl)[b] = int16_t(ci);
            }
        }
        anims_.push_back(std::move(a));
    }
    if (r.bad) return false;
    buildMob(); // any model can be drawn the mob way (spawn crystal: a Box-UV model in block/)
    loaded = true;
    return true;
}

// ---- AxeolonBBModel.build: Blockbench -> Beta model space ----

void Model::buildBox(Cube& c, float x, float y, float z, int w, int h, int d, float inflate) {
    // ModelRenderer.addBox + TexturedQuad (texture size = the model's resolution)
    float x1 = x + float(w), y1 = y + float(h), z1 = z + float(d);
    x -= inflate; y -= inflate; z -= inflate;
    x1 += inflate; y1 += inflate; z1 += inflate;
    if (c.mirror) std::swap(x, x1);
    const float P[8][3] = {{x, y, z}, {x1, y, z}, {x1, y1, z}, {x, y1, z},
                           {x, y, z1}, {x1, y, z1}, {x1, y1, z1}, {x, y1, z1}};
    const int u = int(c.uvOffset[0]), v = int(c.uvOffset[1]);
    // corners per face, then the texture rectangle (u1, v1, u2, v2) in texels
    const int F[6][4] = {{5, 1, 2, 6}, {0, 4, 7, 3}, {5, 4, 0, 1}, {2, 3, 7, 6}, {1, 0, 3, 2}, {4, 5, 6, 7}};
    const int R[6][4] = {{u + d + w, v + d, u + d + w + d, v + d + h}, {u, v + d, u + d, v + d + h},
                         {u + d, v, u + d + w, v + d},                 {u + d + w, v, u + d + w + w, v + d},
                         {u + d, v + d, u + d + w, v + d + h},         {u + d + w + d, v + d, u + d + w + d + w, v + d + h}};
    const float tw = float(texW_), th = float(texH_);
    const float eu = 0.1f / tw, ev = 0.1f / th;
    for (int f = 0; f < 6; f++) {
        float uv[4][2] = {{float(R[f][2]) / tw - eu, float(R[f][1]) / th + ev},
                          {float(R[f][0]) / tw + eu, float(R[f][1]) / th + ev},
                          {float(R[f][0]) / tw + eu, float(R[f][3]) / th - ev},
                          {float(R[f][2]) / tw - eu, float(R[f][3]) / th - ev}};
        for (int k = 0; k < 4; k++) {
            int src = c.mirror ? 3 - k : k; // func_809_a reverses the corners (with their UVs)
            const float* p = P[F[f][src]];
            float* q = c.quads[f][k];
            q[0] = p[0]; q[1] = p[1]; q[2] = p[2];
            q[3] = uv[src][0]; q[4] = uv[src][1];
        }
    }
}

void Model::buildMob() {
    for (size_t bi = 0; bi < bones_.size(); bi++) {
        Bone& b = bones_[bi];
        if (int(bi) == looseBone_) {
            b.px = b.py = b.pz = b.rx = b.ry = b.rz = 0;
        } else {
            b.px = -b.origin[0];
            b.py = 24.0f - b.origin[1];
            b.pz = b.origin[2];
            b.rx = -rad(b.rot[0]);
            b.ry = -rad(b.rot[1]);
            b.rz = rad(b.rot[2]);
        }
        for (int ci : b.cubes) {
            Cube& c = cubes_[ci];
            float ox = -c.origin[0], oy = 24.0f - c.origin[1], oz = c.origin[2];
            float x0 = -c.to[0], y0 = 24.0f - c.to[1], z0 = c.from[2];
            int w = javaRound(c.to[0] - c.from[0]);
            int h = javaRound(c.to[1] - c.from[1]);
            int d = javaRound(c.to[2] - c.from[2]);
            buildBox(c, x0 - ox, y0 - oy, z0 - oz, w, h, d, c.inflate + extraInflate);
            c.off[0] = ox; c.off[1] = oy; c.off[2] = oz;
            c.mrot[0] = -rad(c.rot[0]);
            c.mrot[1] = -rad(c.rot[1]);
            c.mrot[2] = rad(c.rot[2]);
            c.shown = c.visible;
            if (int(bi) != looseBone_) { // cubes are drawn inside the bone's pivot space
                c.off[0] -= b.px; c.off[1] -= b.py; c.off[2] -= b.pz;
            }
        }
    }
}

void Model::makeArmour(float inflate, const std::vector<std::string>& names) {
    if (kind_ != MOB) return;
    extraInflate = inflate;
    buildMob();
    for (Cube& c : cubes_) {
        bool listed = false;
        for (auto& n : names) listed |= n == c.name;
        c.shown = c.visible && listed;
    }
}

const Animation* Model::animation(const std::string& name) const {
    for (auto& a : anims_)
        if (a.name == name) return &a;
    return nullptr;
}

std::vector<std::string> Model::animationNames() const {
    std::vector<std::string> r;
    for (auto& a : anims_) r.push_back(a.name);
    return r;
}

float Model::animationLength(const std::string& name) const {
    const Animation* a = animation(name);
    return a ? a->length : 0.0f;
}

int Model::boneIndex(const std::string& name) const {
    for (size_t i = 0; i < bones_.size(); i++)
        if (bones_[i].inMap && bones_[i].name == name) return int(i);
    return -1;
}

// ---- sampling ----

float Model::sample(const Channel& c, float t, float out[3]) {
    int n = int(c.times.size());
    const float* V = c.values.data();
    if (n == 1 || t <= c.times[0]) { std::memcpy(out, V, 12); return 0; }
    if (t >= c.times[n - 1]) { std::memcpy(out, V + 3 * (n - 1), 12); return 0; }
    int i = 0;
    while (i < n - 2 && c.times[i + 1] <= t) ++i;
    float span = c.times[i + 1] - c.times[i];
    float u = span <= 0.0f ? 1.0f : (t - c.times[i]) / span;
    if (c.interp[i] == 2) { std::memcpy(out, V + 3 * i, 12); return 0; }
    bool smooth = c.interp[i] == 1 || c.interp[i + 1] == 1;
    for (int k = 0; k < 3; ++k) {
        if (smooth) {
            float p0 = V[3 * (i > 0 ? i - 1 : 0) + k];
            float p3 = V[3 * (i + 2 < n - 1 ? i + 2 : n - 1) + k];
            out[k] = cr(p0, V[3 * i + k], V[3 * (i + 1) + k], p3, u);
        } else {
            out[k] = V[3 * i + k] + (V[3 * (i + 1) + k] - V[3 * i + k]) * u;
        }
    }
    return 0;
}

bool Model::sampleBone(const std::string& anim, const std::string& bone, int channel, float seconds, float out[3]) const {
    const Animation* a = animation(anim);
    if (!a) return false;
    int b = -1;
    for (size_t i = 0; i < bones_.size(); i++)
        if (int(i) != looseBone_ && bones_[i].name == bone) { b = int(i); break; }
    if (b < 0) return false;
    int ci = (channel == 0 ? a->rot : channel == 1 ? a->pos : a->scl)[b];
    if (ci < 0) return false;
    float t = a->loop == 1 && a->length > 0.0f ? std::fmod(seconds, a->length) : std::fmin(seconds, a->length);
    sample(a->channels[ci], t, out);
    return true;
}

void Model::resetPose() {
    for (Bone& b : bones_) {
        b.arx = b.ary = b.arz = b.apx = b.apy = b.apz = 0.0f;
        b.asx = b.asy = b.asz = 1.0f;
    }
}

void Model::apply(const Animation* a, float t, float weight) {
    if (!a || weight <= 0.0f) return;
    float v[3];
    for (size_t i = 0; i < bones_.size(); i++) {
        Bone& b = bones_[i];
        if (!b.inMap) continue;
        if (a->rot[i] >= 0) {
            sample(a->channels[a->rot[i]], t, v);
            b.arx += rad(v[0]) * weight;
            b.ary += rad(v[1]) * weight;
            b.arz += rad(v[2]) * weight;
        }
        if (a->pos[i] >= 0) {
            sample(a->channels[a->pos[i]], t, v);
            b.apx += v[0] * weight;
            b.apy += -v[1] * weight;
            b.apz += v[2] * weight;
        }
        if (a->scl[i] >= 0) {
            sample(a->channels[a->scl[i]], t, v);
            b.asx += (v[0] - 1.0f) * weight;
            b.asy += (v[1] - 1.0f) * weight;
            b.asz += (v[2] - 1.0f) * weight;
        }
    }
}

void Model::lookAt(float headYaw, float headPitch, float look) {
    int h = boneIndex("head");
    if (h < 0) return;
    bones_[h].ary += rad(headYaw) * look;
    bones_[h].arx += rad(headPitch) * look;
}

void Model::poseAnimated(const std::string& anim, float seconds, const std::string& extra, float extraSeconds,
                         float extraWeight) {
    resetPose();
    if (const Animation* a = animation(anim)) {
        float t = a->loop == 1 && a->length > 0.0f ? std::fmod(seconds, a->length) : std::fmin(seconds, a->length);
        apply(a, t, 1.0f - extraWeight);
    }
    const Animation* x = extra.empty() ? nullptr : animation(extra);
    if (x && extraWeight > 0.0f) apply(x, std::fmin(extraSeconds, x->length), extraWeight);
}

// ---- drawing a mob (renderBone / ModelRenderer.render) ----

void Model::emitMob(std::vector<ModelVertex>& out, const float m[16], float scale, float brightness,
                    bool (*vis)(const char*, void*), void* user) const {
    if (!loaded) return;
    for (int r : roots_) emitBone(out, r, m, scale, brightness, vis, user);
}

void Model::emitBone(std::vector<ModelVertex>& out, int bi, const float* parent, float s, float brightness,
                     bool (*vis)(const char*, void*), void* user) const {
    const Bone& b = bones_[bi];
    if (vis && !vis(b.name.c_str(), user)) return;
    float m[16];
    std::memcpy(m, parent, 64);
    fTranslate(m, (b.px + b.apx) * s, (b.py + b.apy) * s, (b.pz + b.apz) * s);
    fRotate(m, b.rz + b.arz, 2);
    fRotate(m, b.ry + b.ary, 1);
    fRotate(m, b.rx + b.arx, 0);
    if (b.asx != 1.0f || b.asy != 1.0f || b.asz != 1.0f) fScale(m, b.asx, b.asy, b.asz);
    for (int ci : b.cubes) {
        const Cube& c = cubes_[ci];
        if (!c.shown) continue;
        float cm[16];
        std::memcpy(cm, m, 64);
        fTranslate(cm, c.off[0] * s, c.off[1] * s, c.off[2] * s);
        fRotate(cm, c.mrot[2], 2);
        fRotate(cm, c.mrot[1], 1);
        fRotate(cm, c.mrot[0], 0);
        for (int f = 0; f < 6; f++) {
            float p[4][3];
            for (int k = 0; k < 4; k++) {
                float v[3] = {c.quads[f][k][0] * s, c.quads[f][k][1] * s, c.quads[f][k][2] * s};
                fApply(cm, v, p[k]);
            }
            // TexturedQuad normal: (v1 - v2) x (v1 - v0)
            float a[3] = {p[1][0] - p[2][0], p[1][1] - p[2][1], p[1][2] - p[2][2]};
            float bb[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
            float n[3] = {a[1] * bb[2] - a[2] * bb[1], a[2] * bb[0] - a[0] * bb[2], a[0] * bb[1] - a[1] * bb[0]};
            float l = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            float shade = brightness;
            if (l > 1e-12f) shade *= itemLighting(n[0] / l, n[1] / l, n[2] / l);
            else shade *= 0.4f;
            // quad 0 1 2 3 -> triangles 0 1 2, 0 2 3
            static const int T[6] = {0, 1, 2, 0, 2, 3};
            for (int k : T) out.push_back({p[k][0], p[k][1], p[k][2], shade, c.quads[f][k][3], c.quads[f][k][4]});
        }
    }
    for (int k : b.kids) {
        float cm[16];
        std::memcpy(cm, m, 64);
        fTranslate(cm, -b.px * s, -b.py * s, -b.pz * s);
        emitBone(out, k, cm, s, brightness, vis, user);
    }
}

bool Model::boneMatrix(const std::string& name, float s, float out[16]) const {
    int bi = boneIndex(name);
    if (bi < 0) return false;
    std::vector<int> chain;
    for (int c = bi; c >= 0; c = bones_[c].parent) chain.insert(chain.begin(), c);
    fIdentity(out);
    for (size_t i = 0; i < chain.size(); ++i) {
        const Bone& c = bones_[chain[i]];
        if (i > 0) {
            const Bone& par = bones_[chain[i - 1]];
            fTranslate(out, -par.px * s, -par.py * s, -par.pz * s);
        }
        fTranslate(out, (c.px + c.apx) * s, (c.py + c.apy) * s, (c.pz + c.apz) * s);
        fRotate(out, c.rz + c.arz, 2);
        fRotate(out, c.ry + c.ary, 1);
        fRotate(out, c.rx + c.arx, 0);
        if (c.asx != 1.0f || c.asy != 1.0f || c.asz != 1.0f) fScale(out, c.asx, c.asy, c.asz);
    }
    return true;
}

// ---- AxeolonBlockRig.emit ----

void Model::emitRig(std::vector<ModelVertex>& out, const char* anim, float seconds, float yaw, float pitch, int half,
                    double ox, double oy, double oz, FaceLight light, void* user) const {
    if (!loaded) return;
    double m[16], t[16];
    dTranslate(m, 0.5, 0.5, 0.5);
    dRot(t, yaw, 1); dMulBy(m, t);
    dRot(t, pitch, 0); dMulBy(m, t);
    dTranslate(t, 0.0, -0.5, 0.0); dMulBy(m, t);
    dIdentity(t); t[0] = t[5] = t[10] = 1.0 / 16.0; dMulBy(m, t);
    for (int r : roots_) rigGroup(out, r, m, anim, seconds, half, ox, oy, oz, light, user);
}

void Model::rigGroup(std::vector<ModelVertex>& out, int gi, const double* parent, const char* anim, float sec, int half,
                     double ox, double oy, double oz, FaceLight light, void* user) const {
    const Bone& g = bones_[gi];
    float r[3] = {g.rot[0], g.rot[1], g.rot[2]};
    float p[3] = {0, 0, 0};
    float sc[3];
    bool hasScale = false;
    if (anim && gi != looseBone_) {
        std::string a(anim);
        float kr[3], kp[3];
        hasScale = sampleBone(a, g.name, 2, sec, sc);
        // AxeolonBBModel hands keyframes back in the Blockbench 4 convention: undo it
        if (sampleBone(a, g.name, 0, sec, kr)) { r[0] -= kr[0]; r[1] -= kr[1]; r[2] += kr[2]; }
        if (sampleBone(a, g.name, 1, sec, kp)) { p[0] = -kp[0]; p[1] = kp[1]; p[2] = kp[2]; }
    }
    double m[16], t[16];
    dTranslate(t, g.origin[0] + p[0], g.origin[1] + p[1], g.origin[2] + p[2]);
    dMul(parent, t, m);
    dEuler(t, r); dMulBy(m, t);
    if (hasScale) {
        dIdentity(t); t[0] = sc[0]; t[5] = sc[1]; t[10] = sc[2];
        dMulBy(m, t);
    }
    dTranslate(t, -g.origin[0], -g.origin[1], -g.origin[2]); dMulBy(m, t);

    for (int ci : g.cubes) {
        const Cube& c = cubes_[ci];
        if (!c.visible) continue;
        if (half != HALF_ALL && (((c.from[0] + c.to[0]) / 2.0f < 8.0f) != (half == HALF_THIS))) continue;
        double cm[16];
        std::memcpy(cm, m, sizeof cm);
        if (c.rot[0] != 0.0f || c.rot[1] != 0.0f || c.rot[2] != 0.0f) {
            dTranslate(t, c.origin[0], c.origin[1], c.origin[2]); dMulBy(cm, t);
            dEuler(t, c.rot); dMulBy(cm, t);
            dTranslate(t, -c.origin[0], -c.origin[1], -c.origin[2]); dMulBy(cm, t);
        }
        float x0 = c.from[0], y0 = c.from[1], z0 = c.from[2], x1 = c.to[0], y1 = c.to[1], z1 = c.to[2];
        const float q[6][4][3] = {
            {{x1, y1, z0}, {x0, y1, z0}, {x0, y0, z0}, {x1, y0, z0}}, // north
            {{x1, y1, z1}, {x1, y1, z0}, {x1, y0, z0}, {x1, y0, z1}}, // east
            {{x0, y1, z1}, {x1, y1, z1}, {x1, y0, z1}, {x0, y0, z1}}, // south
            {{x0, y1, z0}, {x0, y1, z1}, {x0, y0, z1}, {x0, y0, z0}}, // west
            {{x0, y1, z0}, {x1, y1, z0}, {x1, y1, z1}, {x0, y1, z1}}, // up
            {{x0, y0, z1}, {x1, y0, z1}, {x1, y0, z0}, {x0, y0, z0}}, // down
        };
        for (int d = 0; d < 6; ++d) {
            if (!(c.faceMask & (1 << d))) continue;
            double w[4][3];
            for (int i = 0; i < 4; ++i) dApply(cm, q[d][i], w[i]);
            double ax = w[1][0] - w[0][0], ay = w[1][1] - w[0][1], az = w[1][2] - w[0][2];
            double bx = w[3][0] - w[0][0], by = w[3][1] - w[0][1], bz = w[3][2] - w[0][2];
            double nx = -(ay * bz - az * by), ny = -(az * bx - ax * bz), nz = -(ax * by - ay * bx);
            double l = std::sqrt(nx * nx + ny * ny + nz * nz);
            float shade = 1.0f;
            if (l > 1.0E-9) {
                nx /= l; ny /= l; nz /= l;
                shade = float(nx * nx * 0.6 + nz * nz * 0.8 + ny * ny * (ny > 0.0 ? 1.0 : 0.5));
            }
            double cx = (w[0][0] + w[2][0]) / 2.0, cy = (w[0][1] + w[2][1]) / 2.0, cz = (w[0][2] + w[2][2]) / 2.0;
            float lv = shade * (light ? light(cx, cy, cz, nx, ny, nz, user) : 1.0f);
            const float* f = c.faceUV[d];
            const float uv[4][2] = {{f[0], f[1]}, {f[2], f[1]}, {f[2], f[3]}, {f[0], f[3]}};
            for (int j = 0; j < 4; ++j) {
                int i = (4 - j) & 3;
                const float* u = uv[(i - c.faceRot[d] + 4) & 3];
                out.push_back({float(ox + w[i][0]), float(oy + w[i][1]), float(oz + w[i][2]), lv,
                               u[0] / float(texW_), u[1] / float(texH_)});
            }
        }
    }
    for (int k : g.kids) rigGroup(out, k, m, anim, sec, half, ox, oy, oz, light, user);
}

} // namespace axm
