// The 3D renderer on OpenGL ES 2: the same Renderer as the 3DS (source/render/Renderer.h), the
// same pictures. One shader does what the 3DS vertex shader and texture combiners do: chunk light
// worked out per vertex from the packed sky/block light and the time of day, Beta's face shading,
// linear fog, the cut-out alpha test, tints and the blend modes models ask for.
#include <GLES2/gl2.h>
#include <android/log.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "render/Renderer.h"
#include "Gfx.h"
#include "Platform.h"
#include "world/JavaRandom.h"
#include "world/MathHelper.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "axeolon", __VA_ARGS__)

using ChunkMesher::ATLAS_H;
using ChunkMesher::ATLAS_W;

namespace {
constexpr float SECTION_RADIUS = 13.86f; // half the diagonal of a 16x16x16 section

const char* VS = R"(
attribute vec4 aPos;   // x, y, z, shade (chunks: packed light)
attribute vec2 aUv;
uniform mat4 uProj;
uniform mat4 uView;
uniform vec3 uOrigin;
uniform vec3 uParams;   // x: position scale, y: shade scale (models), z: 1 = chunk light in w
uniform vec4 uTex;      // xy: scale, zw: offset
uniform vec4 uLight;    // x: skylightSubtracted, w: the darkest brightness
uniform vec3 uFog;      // visibility = clamp(y * (x - distance) + z)
varying vec2 vUv;
varying float vShade;
varying float vVis;
void main() {
    vec4 eye = uView * vec4(uOrigin + aPos.xyz * uParams.x, 1.0);
    gl_Position = uProj * eye;
    vec2 tc = aUv * uTex.xy + uTex.zw;
    vUv = vec2(tc.x, 1.0 - tc.y); // the 3DS counts v from the bottom; GL textures go top first
    float shade;
    if (uParams.z > 0.5) {
        float w = aPos.w;
        float face = floor(w / 256.0);
        float sky = floor(w / 16.0) - face * 16.0;
        float block = w - face * 256.0 - sky * 16.0;
        float l = max(sky - uLight.x, block);
        float f = 1.0 - l / 15.0;
        shade = (1.0 - f) / (3.0 * f + 1.0) * (1.0 - uLight.w) + uLight.w;
        shade *= face / 20.0;
    } else {
        shade = aPos.w * uParams.y;
    }
    vShade = shade;
    vVis = clamp(uFog.y * (uFog.x - length(eye.xyz)) + uFog.z, 0.0, 1.0);
}
)";

const char* FS = R"(
precision mediump float;
varying vec2 vUv;
varying float vShade;
varying float vVis;
uniform sampler2D uSampler;
uniform vec4 uConst;    // multiplies everything (tints, sky colours)
uniform vec3 uFogColour;
uniform int uMode;      // 0 texture x shade, 1 untextured shade, 2 constant colour with shade as alpha, 3 constant colour
uniform float uAlphaRef; // < 0: no alpha test
void main() {
    vec4 c;
    if (uMode == 0) {
        c = texture2D(uSampler, vUv);
        c.rgb *= vShade;
    } else if (uMode == 1) {
        c = vec4(vec3(vShade), 1.0);
    } else if (uMode == 2) {
        c = vec4(1.0, 1.0, 1.0, vShade);
    } else {
        c = vec4(1.0);
    }
    c *= uConst;
    if (c.a <= uAlphaRef) discard;
    c.rgb = mix(uFogColour, c.rgb, vVis);
    gl_FragColor = c;
}
)";

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        LOG("renderer shader: %s", log);
    }
    return s;
}

// citro3d matrices are row-major; GL wants columns
void toGl(const C3D_Mtx& m, float out[16]) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) out[j * 4 + i] = m.r[i].c[3 - j];
}

struct FloatVertex {
    float x, y, z, shade, u, v;
};

} // namespace

struct Renderer::Gl {
    GLuint program = 0;
    GLint aPos = -1, aUv = -1;
    GLint uProj = -1, uView = -1, uOrigin = -1, uParams = -1, uTex = -1, uLight = -1, uFog = -1, uSampler = -1,
          uConst = -1, uFogColour = -1, uMode = -1, uAlphaRef = -1;
    std::vector<FloatVertex> skyPlane, voidPlane, stars, clouds;
    C3D_Mtx view;
};

namespace {
void fogRgb(uint32_t rgb, float out[3]) {
    out[0] = ((rgb >> 16) & 255) / 255.0f;
    out[1] = ((rgb >> 8) & 255) / 255.0f;
    out[2] = (rgb & 255) / 255.0f;
}
} // namespace

bool Renderer::init() {
    gl = new Gl;
    Gl& g = *gl;
    g.program = glCreateProgram();
    glAttachShader(g.program, compile(GL_VERTEX_SHADER, VS));
    glAttachShader(g.program, compile(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(g.program);
    g.aPos = glGetAttribLocation(g.program, "aPos");
    g.aUv = glGetAttribLocation(g.program, "aUv");
    g.uProj = glGetUniformLocation(g.program, "uProj");
    g.uView = glGetUniformLocation(g.program, "uView");
    g.uOrigin = glGetUniformLocation(g.program, "uOrigin");
    g.uParams = glGetUniformLocation(g.program, "uParams");
    g.uTex = glGetUniformLocation(g.program, "uTex");
    g.uLight = glGetUniformLocation(g.program, "uLight");
    g.uFog = glGetUniformLocation(g.program, "uFog");
    g.uSampler = glGetUniformLocation(g.program, "uSampler");
    g.uConst = glGetUniformLocation(g.program, "uConst");
    g.uFogColour = glGetUniformLocation(g.program, "uFogColour");
    g.uMode = glGetUniformLocation(g.program, "uMode");
    g.uAlphaRef = glGetUniformLocation(g.program, "uAlphaRef");
    target = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    targetRight = target;
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(FOV_DEG), C3D_AspectRatioTop, 0.05f, 200.0f, false);

    // RenderGlobal's sky and void planes, the stars (Random(10842)) and the fast cloud grid
    const int step = 64, n = 256 / step + 2;
    static const int T[6] = {0, 1, 2, 0, 2, 3};
    for (int x = -step * n; x <= step * n; x += step)
        for (int z = -step * n; z <= step * n; z += step) {
            float a[4][2] = {{float(x), float(z)}, {float(x + step), float(z)}, {float(x + step), float(z + step)}, {float(x), float(z + step)}};
            float b[4][2] = {{float(x + step), float(z)}, {float(x), float(z)}, {float(x), float(z + step)}, {float(x + step), float(z + step)}};
            for (int k : T) {
                g.skyPlane.push_back({a[k][0], 16.0f, a[k][1], 1.0f, 0, 0});
                g.voidPlane.push_back({b[k][0], -16.0f, b[k][1], 1.0f, 0, 0});
            }
        }
    JavaRandom r(10842);
    for (int i = 0; i < 1500; ++i) {
        double x = double(r.nextFloat() * 2.0f - 1.0f), y = double(r.nextFloat() * 2.0f - 1.0f), z = double(r.nextFloat() * 2.0f - 1.0f);
        double size = double(0.25f + r.nextFloat() * 0.25f);
        double d = x * x + y * y + z * z;
        if (!(d < 1.0 && d > 0.01)) continue;
        d = 1.0 / std::sqrt(d);
        x *= d; y *= d; z *= d;
        double px = x * 100.0, py = y * 100.0, pz = z * 100.0;
        double a = std::atan2(x, z), sa = std::sin(a), ca = std::cos(a);
        double bb = std::atan2(std::sqrt(x * x + z * z), y), sb = std::sin(bb), cb = std::cos(bb);
        double c = r.nextDouble() * 3.141592653589793 * 2.0, sc = std::sin(c), cc = std::cos(c);
        float q[4][3];
        for (int k = 0; k < 4; ++k) {
            double v1 = double((k & 2) - 1) * size, v2 = double(((k + 1) & 2) - 1) * size;
            double e = v1 * cc - v2 * sc, f = v2 * cc + v1 * sc;
            double gg = e * sb, h = -e * cb;
            q[k][0] = float(px + h * sa - f * ca);
            q[k][1] = float(py + gg);
            q[k][2] = float(pz + f * sa + h * ca);
        }
        for (int k : T) g.stars.push_back({q[k][0], q[k][1], q[k][2], 1.0f, 0, 0});
    }
    const int cs = 32, cn = 256 / cs;
    for (int x = -cs * cn; x < cs * cn; x += cs)
        for (int z = -cs * cn; z < cs * cn; z += cs) {
            float q[4][2] = {{float(x), float(z + cs)}, {float(x + cs), float(z + cs)}, {float(x + cs), float(z)}, {float(x), float(z)}};
            for (int k : T) g.clouds.push_back({q[k][0], 0.0f, q[k][1], 1.0f, q[k][0], q[k][1]});
        }

    // the terrain atlas
    if (!C3D_TexInit(&terrain, ATLAS_W, ATLAS_H, GPU_RGBA8)) return false;
    if (FILE* f = std::fopen("romfs:/textures/terrain.rgba", "rb")) {
        std::fseek(f, 8, SEEK_SET);
        std::vector<uint8_t> px(size_t(ATLAS_W) * ATLAS_H * 4);
        if (std::fread(px.data(), 1, px.size(), f) == px.size()) {
            u32* d = static_cast<u32*>(terrain.data);
            for (size_t i = 0; i < size_t(ATLAS_W) * ATLAS_H; ++i)
                d[i] = u32(px[i * 4]) << 24 | u32(px[i * 4 + 1]) << 16 | u32(px[i * 4 + 2]) << 8 | px[i * 4 + 3];
            C3D_TexFlush(&terrain);
        }
        std::fclose(f);
    }
    return true;
}

void Renderer::shutdown() {
    clearWorld();
    C3D_TexDelete(&terrain);
    for (auto& kv : textures)
        if (kv.second) { C3D_TexDelete(kv.second); delete kv.second; }
    textures.clear();
    if (gl) {
        glDeleteProgram(gl->program);
        delete gl;
        gl = nullptr;
    }
}

void Renderer::setEye(int eye, float) {
    curEye = eye;
    Mtx_PerspTilt(&projection, C3D_AngleFromDegrees(fovDeg), C3D_AspectRatioTop, 0.05f, 200.0f, false);
}

void Renderer::setAtmosphere(int sub, float ambient, uint32_t rgb, float start, float end) {
    skySub = sub;
    ambientLight = ambient;
    fogRGB = rgb;
    fogStart = start;
    fogEnd = end > start + 0.01f ? end : start + 0.01f;
}

void Renderer::updateTerrainTile(int tile, const uint8_t* rgba) { updateTile(&terrain, tile, rgba); }

void Renderer::updateTile(C3D_Tex* tex, int tile, const uint8_t* rgba) {
    if (!tex || !tex->data) return;
    const int x0 = (tile & 15) * 16, y0 = (tile >> 4) * 16;
    u32* d = static_cast<u32*>(tex->data);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const uint8_t* p = rgba + (y * 16 + x) * 4;
            d[(y0 + y) * tex->width + x0 + x] = u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3];
        }
    gfx::flush();
    glBindTexture(GL_TEXTURE_2D, tex->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

void Renderer::freeMesh(GpuMesh& m) {
    if (m.vbo) {
        GLuint b[2] = {m.vbo, m.ibo};
        glDeleteBuffers(m.ibo ? 2 : 1, b);
        linearBytes -= size_t(m.indexCount) * 2;
    }
    m = GpuMesh{};
}

void Renderer::setSectionMesh(int cx, int sy, int cz, const MeshData& data) {
    if (sy < 0 || sy >= SECTIONS) return;
    Column& col = columns[key(cx, cz)];
    col.cx = cx;
    col.cz = cz;
    GpuMesh& m = col.sections[sy];
    freeMesh(m);
    if (data.indices.empty()) return;
    GLuint b[2];
    glGenBuffers(2, b);
    m.vbo = b[0];
    m.ibo = b[1];
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(data.vertices.size() * sizeof(BlockVertex)), data.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(data.indices.size() * 2), data.indices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m.indexCount = int(data.indices.size());
    for (int f = 0; f < 8; ++f) m.faceStart[f] = data.faceStart[f];
    linearBytes += data.vertices.size() * sizeof(BlockVertex) + data.indices.size() * 2;
}

void Renderer::dropColumn(int cx, int cz) {
    auto it = columns.find(key(cx, cz));
    if (it == columns.end()) return;
    for (auto& m : it->second.sections) freeMesh(m);
    columns.erase(it);
}

void Renderer::clearWorld() {
    for (auto& kv : columns)
        for (auto& m : kv.second.sections) freeMesh(m);
    columns.clear();
}

void Renderer::beginFrame() {}

// ---------------------------------------------------------------- drawing helpers

namespace {
struct State {
    int mode = 0;
    float c[4] = {1, 1, 1, 1};
    float alphaRef = -1;
};
} // namespace

#define GLR (*gl)

static void useProgram(Renderer::Gl& g, const C3D_Mtx& proj, const C3D_Mtx& view) {
    glUseProgram(g.program);
    float m[16];
    toGl(proj, m);
    glUniformMatrix4fv(g.uProj, 1, GL_FALSE, m);
    toGl(view, m);
    glUniformMatrix4fv(g.uView, 1, GL_FALSE, m);
    glUniform1i(g.uSampler, 0);
}

static void setFog(Renderer::Gl& g, bool on, float start, float end) {
    if (on) glUniform3f(g.uFog, end, 1.0f / (end - start), 0.0f);
    else glUniform3f(g.uFog, 0.0f, 0.0f, 1.0f);
}

static void setLook(Renderer::Gl& g, int mode, const float c[4], float alphaRef) {
    glUniform1i(g.uMode, mode);
    glUniform4f(g.uConst, c[0], c[1], c[2], c[3]);
    glUniform1f(g.uAlphaRef, alphaRef);
}

static void drawFloats(Renderer::Gl& g, const FloatVertex* v, size_t n) {
    if (!n) return;
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(g.aPos);
    glEnableVertexAttribArray(g.aUv);
    glVertexAttribPointer(g.aPos, 4, GL_FLOAT, GL_FALSE, sizeof(FloatVertex), &v->x);
    glVertexAttribPointer(g.aUv, 2, GL_FLOAT, GL_FALSE, sizeof(FloatVertex), &v->u);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(n));
}

static void modelParams(Renderer::Gl& g) {
    glUniform3f(g.uOrigin, 0, 0, 0);
    glUniform3f(g.uParams, 1.0f, 1.0f, 0.0f);
    glUniform4f(g.uLight, 0.0f, 0.0f, 0.0f, 0.05f);
}

// ---------------------------------------------------------------- the world

void Renderer::renderWorld(const C3D_Mtx& view, const Frustum& frustum, float camX, float camY, float camZ) {
    Gl& g = GLR;
    gfx::flush();
    g.view = view;
    float fc[3];
    fogRgb(fogRGB, fc);
    glViewport(0, 0, platform::windowWidth(), platform::windowHeight());
    glClearColor(fc[0], fc[1], fc[2], 1.0f);
    glDepthMask(GL_TRUE);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    C3D_Mtx viewRot = view; // the sky moves with the eye
    viewRot.r[0].w = viewRot.r[1].w = viewRot.r[2].w = 0.0f;
    useProgram(g, projection, viewRot);
    glUniform3f(g.uFogColour, fc[0], fc[1], fc[2]);
    glDisable(GL_CULL_FACE);

    if (sky.enabled) {
        modelParams(g);
        glUniform4f(g.uTex, 1.0f, -1.0f, 0.0f, 1.0f);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        // the sky plane, fogged out to 0.8 of the distance
        float sc[4] = {sky.sky[0], sky.sky[1], sky.sky[2], 1};
        setLook(g, 3, sc, -1);
        setFog(g, true, 0.0f, fogEnd * 0.8f);
        drawFloats(g, g.skyPlane.data(), g.skyPlane.size());
        setFog(g, false, 0, 0);
        glEnable(GL_BLEND);
        if (sky.sunrise) { // the glow: opaque in the middle, clear at the rim
            C3D_Mtx m = viewRot;
            Mtx_RotateX(&m, C3D_AngleFromDegrees(90.0f), true);
            if (sky.angle > 0.5f) Mtx_RotateZ(&m, C3D_AngleFromDegrees(180.0f), true);
            float gm[16];
            toGl(m, gm);
            glUniformMatrix4fv(g.uView, 1, GL_FALSE, gm);
            float a = sky.sunriseColour[3];
            std::vector<FloatVertex> fan;
            for (int i = 0; i < 16; ++i) {
                float t0 = float(i) * 3.1415927f * 2.0f / 16.0f, t1 = float(i + 1) * 3.1415927f * 2.0f / 16.0f;
                fan.push_back({0.0f, 100.0f, 0.0f, 1.0f, 0, 0});
                fan.push_back({MathHelper::sin(t0) * 120.0f, MathHelper::cos(t0) * 120.0f, -MathHelper::cos(t0) * 40.0f * a, 0.0f, 0, 0});
                fan.push_back({MathHelper::sin(t1) * 120.0f, MathHelper::cos(t1) * 120.0f, -MathHelper::cos(t1) * 40.0f * a, 0.0f, 0, 0});
            }
            float c[4] = {sky.sunriseColour[0], sky.sunriseColour[1], sky.sunriseColour[2], a};
            setLook(g, 2, c, -1);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawFloats(g, fan.data(), fan.size());
        }
        // the sun, the moon and the stars turn with the celestial angle; added on
        C3D_Mtx m = viewRot;
        Mtx_RotateX(&m, sky.angle * 3.1415927f * 2.0f, true);
        float gm[16];
        toGl(m, gm);
        glUniformMatrix4fv(g.uView, 1, GL_FALSE, gm);
        glBlendFunc(GL_ONE, GL_ONE);
        const float s = 30.0f, mo = 20.0f;
        FloatVertex sun[6] = {{-s, 100, -s, 1, 0, 0}, {s, 100, -s, 1, 1, 0}, {s, 100, s, 1, 1, 1}, {-s, 100, -s, 1, 0, 0}, {s, 100, s, 1, 1, 1}, {-s, 100, s, 1, 0, 1}};
        FloatVertex moon[6] = {{-mo, -100, mo, 1, 1, 1}, {mo, -100, mo, 1, 0, 1}, {mo, -100, -mo, 1, 0, 0}, {-mo, -100, mo, 1, 1, 1}, {mo, -100, -mo, 1, 0, 0}, {-mo, -100, -mo, 1, 1, 0}};
        float white[4] = {1, 1, 1, 1};
        glUniform3f(g.uParams, 1.0f, 1.0f, 0.0f);
        if (C3D_Tex* t = texture("romfs:/textures/terrain/sun.rgba")) {
            glBindTexture(GL_TEXTURE_2D, t->id);
            setLook(g, 0, white, -1);
            drawFloats(g, sun, 6);
        }
        if (C3D_Tex* t = texture("romfs:/textures/terrain/moon.rgba")) {
            glBindTexture(GL_TEXTURE_2D, t->id);
            setLook(g, 0, white, -1);
            drawFloats(g, moon, 6);
        }
        if (sky.stars > 0.0f) {
            float c[4] = {sky.stars, sky.stars, sky.stars, sky.stars};
            setLook(g, 3, c, -1);
            drawFloats(g, g.stars.data(), g.stars.size());
        }
        // the dark plane underneath
        toGl(viewRot, gm);
        glUniformMatrix4fv(g.uView, 1, GL_FALSE, gm);
        glDisable(GL_BLEND);
        float vc[4] = {sky.sky[0] * 0.2f + 0.04f, sky.sky[1] * 0.2f + 0.04f, sky.sky[2] * 0.6f + 0.1f, 1};
        setLook(g, 3, vc, -1);
        setFog(g, true, 0.0f, fogEnd * 0.8f);
        drawFloats(g, g.voidPlane.data(), g.voidPlane.size());
    }

    // ---- the chunks ----
    float gm[16];
    toGl(view, gm);
    glUniformMatrix4fv(g.uView, 1, GL_FALSE, gm);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (cullBack) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    } else {
        glDisable(GL_CULL_FACE);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, terrain.id);
    glUniform3f(g.uParams, 1.0f / 64.0f, 1.0f / 255.0f, 1.0f);
    glUniform4f(g.uLight, float(skySub), 1.0f / 16.0f, 16.0f, ambientLight);
    glUniform4f(g.uTex, 1.0f / (ATLAS_W * 4), 1.0f / (ATLAS_H * 4), 0.0f, 0.0f);
    float white[4] = {1, 1, 1, 1};
    setLook(g, 0, white, 0.125f); // leaves and plants cut out (Beta: alpha > 0.1)
    setFog(g, true, fogStart, fogEnd);
    glEnableVertexAttribArray(g.aPos);
    glEnableVertexAttribArray(g.aUv);
    const float maxD = renderDistance + SECTION_RADIUS;
    drawnSections = 0;
    drawnTris = 0;
    for (auto& kv : columns) {
        Column& col = kv.second;
        float x0 = col.cx * 16.0f, z0 = col.cz * 16.0f;
        float dx = x0 + 8 - camX, dz = z0 + 8 - camZ;
        if (dx * dx + dz * dz > maxD * maxD) continue;
        for (int sy = 0; sy < SECTIONS; sy++) {
            const GpuMesh& m = col.sections[sy];
            if (!m.indexCount) continue;
            float y0 = sy * 16.0f;
            if (!frustum.sphereVisible(x0 + 8, y0 + 8, z0 + 8, SECTION_RADIUS)) continue;
            bool vis[7] = {camY < y0 + 16, camY > y0, camZ < z0 + 16, camZ > z0, camX < x0 + 16, camX > x0, true};
            if (!cullBack) for (bool& v : vis) v = true;
            glUniform3f(g.uOrigin, x0, y0, z0);
            glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
            glVertexAttribPointer(g.aPos, 4, GL_SHORT, GL_FALSE, sizeof(BlockVertex), (void*)0);
            glVertexAttribPointer(g.aUv, 2, GL_SHORT, GL_FALSE, sizeof(BlockVertex), (void*)8);
            for (int f = 0; f < 7;) {
                if (!vis[f]) { f++; continue; }
                int e = f;
                while (e < 7 && vis[e]) e++;
                int start = int(m.faceStart[f]), count = int(m.faceStart[e]) - start;
                if (count > 0) {
                    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, (void*)(uintptr_t(start) * 2));
                    drawnTris += count / 3;
                }
                f = e;
            }
            drawnSections++;
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (sky.enabled && sky.clouds) {
        if (C3D_Tex* t = texture("romfs:/textures/environment/clouds.rgba")) {
            C3D_TexSetWrap(t, GPU_REPEAT, GPU_REPEAT);
            toGl(viewRot, gm);
            glUniformMatrix4fv(g.uView, 1, GL_FALSE, gm);
            glDisable(GL_CULL_FACE);
            glUniform3f(g.uOrigin, 0.0f, sky.cloudY, 0.0f);
            glUniform3f(g.uParams, 1.0f, 1.0f, 0.0f);
            glUniform4f(g.uLight, 0, 0, 0, 0.05f);
            glUniform4f(g.uTex, 1.0f / 2048.0f, 1.0f / 2048.0f, sky.cloudU / 2048.0f, sky.cloudV / 2048.0f);
            glBindTexture(GL_TEXTURE_2D, t->id);
            float c[4] = {sky.cloud[0], sky.cloud[1], sky.cloud[2], 0.8f};
            setLook(g, 0, c, -1);
            setFog(g, true, fogStart, fogEnd);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            drawFloats(g, g.clouds.data(), g.clouds.size());
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, terrain.id);
        }
    }
}

// ---------------------------------------------------------------- models

void Renderer::drawAtlasQuads(const std::vector<axm::ModelVertex>& quads, const std::vector<BlockModels::Batch>& batches) {
    if (quads.empty()) return;
    Gl& g = GLR;
    std::vector<FloatVertex> v;
    for (auto& b : batches) {
        const RigTexture& t = *b.tex;
        for (size_t q = b.start; q + 4 <= b.start + b.count; q += 4) {
            static const int T[6] = {0, 1, 2, 0, 2, 3};
            for (int k : T) {
                const axm::ModelVertex& m = quads[q + k];
                v.push_back({m.x, m.y, m.z, m.shade, (t.x + m.u * t.w) * 4.0f, (ATLAS_H - (t.y + m.v * t.h)) * 4.0f});
            }
        }
    }
    useProgram(g, projection, g.view);
    modelParams(g);
    float fc[3];
    fogRgb(fogRGB, fc);
    glUniform3f(g.uFogColour, fc[0], fc[1], fc[2]);
    setFog(g, true, fogStart, fogEnd);
    glUniform4f(g.uTex, 1.0f / (ATLAS_W * 4), 1.0f / (ATLAS_H * 4), 0.0f, 0.0f);
    float white[4] = {1, 1, 1, 1};
    setLook(g, 0, white, 0.125f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if (cullBack) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, terrain.id);
    drawFloats(g, v.data(), v.size());
}

void Renderer::drawTriangles(const std::vector<axm::ModelVertex>& tris, C3D_Tex* tex, const float tint[4], int blend,
                             bool depthEqual, bool untextured) {
    if (tris.empty() || (!tex && !untextured)) return;
    Gl& g = GLR;
    std::vector<FloatVertex> v(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) v[i] = {tris[i].x, tris[i].y, tris[i].z, tris[i].shade, tris[i].u, tris[i].v};
    useProgram(g, projection, g.view);
    modelParams(g);
    float fc[3];
    fogRgb(blend == 2 ? 0 : fogRGB, fc); // additive glows fade to nothing
    glUniform3f(g.uFogColour, fc[0], fc[1], fc[2]);
    setFog(g, true, fogStart, fogEnd);
    glUniform4f(g.uTex, 1.0f, -1.0f, 0.0f, 1.0f); // model UVs from the top
    float c[4] = {1, 1, 1, 1};
    if (tint) for (int i = 0; i < 4; i++) c[i] = tint[i] < 0 ? 0 : tint[i] > 1 ? 1 : tint[i];
    setLook(g, untextured ? 1 : 0, c, blend == 0 ? 0.1f : -1.0f);
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(depthEqual ? GL_EQUAL : GL_LEQUAL);
    glDepthMask(blend ? GL_FALSE : GL_TRUE);
    if (blend == 0) glDisable(GL_BLEND);
    else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, blend == 2 ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex ? tex->id : terrain.id);
    drawFloats(g, v.data(), v.size());
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
}

C3D_Tex* Renderer::texture(const std::string& path) {
    auto it = textures.find(path);
    if (it != textures.end()) return it->second;
    C3D_Tex* tex = nullptr;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char magic[4];
        uint16_t wh[2] = {0, 0};
        if (std::fread(magic, 1, 4, f) == 4 && std::memcmp(magic, "AXTX", 4) == 0 && std::fread(wh, 2, 2, f) == 2 && wh[0] && wh[1]) {
            std::vector<uint8_t> px(size_t(wh[0]) * wh[1] * 4);
            if (std::fread(px.data(), 1, px.size(), f) == px.size()) {
                tex = new C3D_Tex;
                if (C3D_TexInit(tex, wh[0], wh[1], GPU_RGBA8)) {
                    u32* d = static_cast<u32*>(tex->data);
                    for (size_t i = 0; i < size_t(wh[0]) * wh[1]; ++i)
                        d[i] = u32(px[i * 4]) << 24 | u32(px[i * 4 + 1]) << 16 | u32(px[i * 4 + 2]) << 8 | px[i * 4 + 3];
                    C3D_TexFlush(tex);
                } else {
                    delete tex;
                    tex = nullptr;
                }
            }
        }
        std::fclose(f);
    }
    textures[path] = tex;
    return tex;
}

// ---------------------------------------------------------------- the title, 2D

void Renderer::beginTitle() {
    curEye = 0;
    gfx::flush();
    glViewport(0, 0, platform::windowWidth(), platform::windowHeight());
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawFree(const C3D_Mtx& proj, const std::vector<axm::ModelVertex>& tris, C3D_Tex* tex, int blend, bool cull,
                        int area) {
    if (tris.empty() || !tex) return;
    Gl& g = GLR;
    gfx::flush();
    std::vector<FloatVertex> v(tris.size());
    for (size_t i = 0; i < tris.size(); ++i) v[i] = {tris[i].x, tris[i].y, tris[i].z, tris[i].shade, tris[i].u, tris[i].v};
    C3D_Mtx id;
    Mtx_Identity(&id);
    useProgram(g, proj, id);
    modelParams(g);
    setFog(g, false, 0, 0);
    glUniform3f(g.uFogColour, 0, 0, 0);
    glUniform4f(g.uTex, 1.0f, -1.0f, 0.0f, 1.0f);
    float white[4] = {1, 1, 1, 1};
    setLook(g, 0, white, blend == 0 ? 0.1f : -1.0f);
    glDisable(GL_DEPTH_TEST);
    if (cull && cullBack) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blend == 0) glDisable(GL_BLEND);
    else {
        glEnable(GL_BLEND);
        glBlendFunc(blend == 3 ? GL_SRC_COLOR : GL_SRC_ALPHA, blend == 1 ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);
    }
    int W = platform::windowWidth(), H = platform::windowHeight();
    if (area == 1) glViewport(0, H / 2, W, H / 2);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex->id);
    drawFloats(g, v.data(), v.size());
    if (area == 1) glViewport(0, 0, W, H);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

void Renderer::prepare2D() {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    gfx::restore();
}
