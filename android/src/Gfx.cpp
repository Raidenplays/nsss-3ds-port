#include "Gfx.h"
#include <android/asset_manager.h>
#include <android/log.h>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#include "stb_image.h"

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "axeolon", __VA_ARGS__)

namespace gfx {
namespace {

AAssetManager* gAssets = nullptr;
std::unordered_map<std::string, Texture*> gTextures;
int gW = 1, gH = 1, gScale = 1;
GLuint gProgram = 0;
GLint aPos = -1, aUv = -1, aCol = -1, aTint = -1, uMat = -1, uTex = -1, uUseTex = -1;
GLuint gWhite = 0;
std::vector<Vertex> gBatch;
GLuint gBatchTex = 0;
float gMatrix[16];

const char* VS = R"(
attribute vec3 aPos;
attribute vec2 aUv;
attribute vec4 aCol;
attribute vec4 aTint;
uniform mat4 uMat;
varying vec2 vUv;
varying vec4 vCol;
varying vec4 vTint;
void main() {
    vUv = aUv;
    vCol = aCol;
    vTint = aTint;
    gl_Position = uMat * vec4(aPos, 1.0);
}
)";
const char* FS = R"(
precision mediump float;
varying vec2 vUv;
varying vec4 vCol;
varying vec4 vTint;
uniform sampler2D uTex;
void main() {
    vec4 t = texture2D(uTex, vUv);
    t.rgb = mix(t.rgb, vTint.rgb, vTint.a);
    vec4 c = t * vCol;
    if (c.a < 0.004) discard;
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
        char log[512];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        LOG("shader: %s", log);
    }
    return s;
}

Texture* upload(const uint8_t* rgba, int w, int h) {
    Texture* t = new Texture;
    t->w = w;
    t->h = h;
    glGenTextures(1, &t->id);
    glBindTexture(GL_TEXTURE_2D, t->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gBatchTex = 0;
    return t;
}

void ortho(float* m, float l, float r, float b, float t) {
    std::memset(m, 0, sizeof(float) * 16);
    m[0] = 2.0f / (r - l);
    m[5] = 2.0f / (t - b);
    m[10] = -1.0f / 1000.0f;
    m[12] = -(r + l) / (r - l);
    m[13] = -(t + b) / (t - b);
    m[15] = 1.0f;
}

void pushId(GLuint id) {
    if (!id) id = gWhite;
    if (id != gBatchTex) {
        flush();
        gBatchTex = id;
    }
}
void push(Texture* tex) { pushId(tex ? tex->id : gWhite); }

} // namespace

void init(AAssetManager* assets) {
    gAssets = assets;
    gProgram = glCreateProgram();
    glAttachShader(gProgram, compile(GL_VERTEX_SHADER, VS));
    glAttachShader(gProgram, compile(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(gProgram);
    aPos = glGetAttribLocation(gProgram, "aPos");
    aUv = glGetAttribLocation(gProgram, "aUv");
    aCol = glGetAttribLocation(gProgram, "aCol");
    aTint = glGetAttribLocation(gProgram, "aTint");
    uMat = glGetUniformLocation(gProgram, "uMat");
    uTex = glGetUniformLocation(gProgram, "uTex");
    uint8_t white[4] = {255, 255, 255, 255};
    gWhite = upload(white, 1, 1)->id;
    gBatch.reserve(6 * 2048);
    // GL objects die with the context: forget what was loaded before
    gTextures.clear();
}

bool readAsset(const std::string& path, std::vector<uint8_t>& out) {
    if (!gAssets) return false;
    AAsset* a = AAssetManager_open(gAssets, path.c_str(), AASSET_MODE_BUFFER);
    if (!a) return false;
    size_t n = size_t(AAsset_getLength(a));
    out.resize(n);
    bool ok = AAsset_read(a, out.data(), n) == int(n);
    AAsset_close(a);
    return ok;
}

std::vector<std::string> listAssets(const std::string& dir) {
    std::vector<std::string> out;
    if (!gAssets) return out;
    AAssetDir* d = AAssetManager_openDir(gAssets, dir.c_str());
    if (!d) return out;
    while (const char* n = AAssetDir_getNextFileName(d)) out.push_back(n);
    AAssetDir_close(d);
    return out;
}

void restore() {
    flush(); // whatever is queued goes out with its own texture first
    glUseProgram(gProgram);
    glUniform1i(uTex, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(aPos);
    glEnableVertexAttribArray(aUv);
    glEnableVertexAttribArray(aCol);
    glEnableVertexAttribArray(aTint);
    gBatchTex = 0;
}

Texture* texture(const std::string& path) {
    auto it = gTextures.find(path);
    if (it != gTextures.end()) return it->second;
    Texture* t = nullptr;
    std::vector<uint8_t> data;
    if (readAsset(path, data)) {
        if (data.size() >= 8 && std::memcmp(data.data(), "AXTX", 4) == 0) {
            int w = data[4] | data[5] << 8, h = data[6] | data[7] << 8;
            if (data.size() >= 8 + size_t(w) * h * 4) t = upload(data.data() + 8, w, h);
        } else {
            int w, h, n;
            uint8_t* px = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &n, 4);
            if (px) {
                t = upload(px, w, h);
                stbi_image_free(px);
            }
        }
    }
    if (!t) LOG("missing texture %s", path.c_str());
    gTextures[path] = t;
    return t;
}

void resize(int w, int h) {
    gW = w > 0 ? w : 1;
    gH = h > 0 ? h : 1;
    // Minecraft::setSize: the GUI scale goes up with the screen
    gScale = gW >= 1000 ? 4 : gW >= 800 ? 3 : gW >= 400 ? 2 : 1;
    if (gW >= 1800) gScale = 6;
    else if (gW >= 1400) gScale = 5;
}
int pixelWidth() { return gW; }
int pixelHeight() { return gH; }
int guiScale() { return gScale; }
int guiWidth() { return (gW + gScale - 1) / gScale; }
int guiHeight() { return (gH + gScale - 1) / gScale; }

void beginFrame(uint32_t clearRGB) {
    glViewport(0, 0, gW, gH);
    glClearColor(((clearRGB >> 16) & 255) / 255.0f, ((clearRGB >> 8) & 255) / 255.0f, (clearRGB & 255) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(gProgram);
    glUniform1i(uTex, 0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    gBatchTex = 0;
}

void setMatrix(const float m[16]) {
    flush();
    std::memcpy(gMatrix, m, sizeof gMatrix);
    glUniformMatrix4fv(uMat, 1, GL_FALSE, gMatrix);
}

void begin2D() {
    float m[16];
    ortho(m, 0, float(gW) / float(gScale), float(gH) / float(gScale), 0);
    setMatrix(m);
}

void quad(Texture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, uint32_t c) {
    push(tex);
    Vertex a{x, y, 0, u0, v0, c}, b{x + w, y, 0, u1, v0, c}, d{x + w, y + h, 0, u1, v1, c}, e{x, y + h, 0, u0, v1, c};
    gBatch.push_back(a); gBatch.push_back(b); gBatch.push_back(d);
    gBatch.push_back(a); gBatch.push_back(d); gBatch.push_back(e);
}

void blit(Texture* tex, float x, float y, float w, float h, int sx, int sy, int sw, int sh, uint32_t c) {
    if (!tex) return;
    quad(tex, x, y, w, h, float(sx) / tex->w, float(sy) / tex->h, float(sx + sw) / tex->w, float(sy + sh) / tex->h, c);
}

void fill(float x, float y, float w, float h, uint32_t c) { quad(nullptr, x, y, w, h, 0, 0, 1, 1, c); }

void gradient(float x, float y, float w, float h, uint32_t top, uint32_t bottom) {
    push(nullptr);
    Vertex a{x, y, 0, 0, 0, top}, b{x + w, y, 0, 1, 0, top}, d{x + w, y + h, 0, 1, 1, bottom}, e{x, y + h, 0, 0, 1, bottom};
    gBatch.push_back(a); gBatch.push_back(b); gBatch.push_back(d);
    gBatch.push_back(a); gBatch.push_back(d); gBatch.push_back(e);
}

void ninePatch(Texture* tex, float x, float y, float w, float h, int sx, int sy, int sw, int sh, int b) {
    if (!tex) return;
    float xs[4] = {x, x + b, x + w - b, x + w}, ys[4] = {y, y + b, y + h - b, y + h};
    int us[4] = {sx, sx + b, sx + sw - b, sx + sw}, vs[4] = {sy, sy + b, sy + sh - b, sy + sh};
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i) {
            float qw = xs[i + 1] - xs[i], qh = ys[j + 1] - ys[j];
            if (qw <= 0 || qh <= 0) continue;
            quad(tex, xs[i], ys[j], qw, qh, float(us[i]) / tex->w, float(vs[j]) / tex->h, float(us[i + 1]) / tex->w,
                 float(vs[j + 1]) / tex->h, 0xFFFFFFFF);
        }
}

void triangles(Texture* tex, const std::vector<Vertex>& v) {
    push(tex);
    gBatch.insert(gBatch.end(), v.begin(), v.end());
}

void trianglesGL(unsigned glTex, const Vertex* v, size_t n) {
    pushId(glTex);
    gBatch.insert(gBatch.end(), v, v + n);
}

void setBlend(unsigned src, unsigned dst) {
    flush();
    glEnable(GL_BLEND);
    glBlendFunc(src, dst);
}

void defaultBlend() { setBlend(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); }

void flush() {
    if (gBatch.empty()) return;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBatchTex ? gBatchTex : gWhite);
    const uint8_t* base = reinterpret_cast<const uint8_t*>(gBatch.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(aPos);
    glEnableVertexAttribArray(aUv);
    glEnableVertexAttribArray(aCol);
    glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), base);
    glVertexAttribPointer(aUv, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), base + 12);
    glVertexAttribPointer(aCol, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), base + 20);
    glEnableVertexAttribArray(aTint);
    glVertexAttribPointer(aTint, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), base + 24);
    glDrawArrays(GL_TRIANGLES, 0, GLsizei(gBatch.size()));
    gBatch.clear();
}

void setScissor(bool on, float x, float y, float w, float h) {
    flush();
    if (!on) {
        glDisable(GL_SCISSOR_TEST);
        return;
    }
    glEnable(GL_SCISSOR_TEST);
    int s = gScale;
    glScissor(int(x * s), gH - int((y + h) * s), int(w * s), int(h * s));
}

} // namespace gfx
