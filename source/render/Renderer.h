#pragma once
// citro3d renderer — the 3DS replacement for Beta's RenderGlobal/WorldRenderer
// (OpenGL display lists -> linear-memory vertex buffers drawn per section).
// Sections are stored per loaded chunk column and follow the world as it streams.
//
// Shares the GPU with citro2d (bottom-screen UI + HUD), so all 3D state is
// re-applied at the start of renderWorld() every frame.
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <unordered_map>
#include <vector>
#include "ChunkMesh.h"
#include "Frustum.h"
#include "world/Chunk.h"
#include "model/AxmModel.h"
#include "model/BlockModels.h"
#include <string>

#ifdef __ANDROID__
struct GpuMesh { // OpenGL ES buffers (android/src/RendererGL.cpp)
    unsigned vbo = 0, ibo = 0;
    int indexCount = 0;
    uint32_t faceStart[8] = {0, 0, 0, 0, 0, 0, 0, 0};
};
#else
struct GpuMesh {
    BlockVertex* vbo = nullptr;
    uint16_t* ibo = nullptr;
    int indexCount = 0;
    uint32_t faceStart[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // see MeshData::faceStart
};
#endif

class Renderer {
public:
    static constexpr int SECTIONS = Chunk::HEIGHT / 16;

    // Call after C3D_Init + C2D_Init.
    bool init();
    void shutdown();

    C3D_RenderTarget* topTarget(int eye = 0) { return eye ? targetRight : target; }

    // Stereoscopic 3D: call before renderWorld for each eye. iod = how far apart the eyes are
    // (the 3D slider; 0 = flat). eye 0 = left (also the only one drawn when the slider is down).
    void setEye(int eye, float iod);
    C3D_Tex* terrainTexture() { return &terrain; }
    // RenderEngine.func_1067_a: one 16x16 atlas tile replaced (RGBA bytes, rows top first)
    void updateTerrainTile(int tile, const uint8_t* rgba);
    void updateTile(C3D_Tex* tex, int tile, const uint8_t* rgba); // any 16-tile-wide sheet (gui/items)

    // Replace one section's mesh. Old buffers are freed next frame, once the
    // GPU is guaranteed to be done with them.
    void setSectionMesh(int cx, int sy, int cz, const MeshData& data);
    void dropColumn(int cx, int cz);
    void clearWorld(); // drop every chunk mesh (leaving a world) // chunk unloaded

    // Time of day and fog for the next renderWorld: World.skylightSubtracted, the darkest
    // brightness (0.05, the Nether 0.1), the fog/clear colour 0xRRGGBB and the linear fog range.
    void setAtmosphere(int skylightSubtracted, float ambient, uint32_t fogRGB, float fogStart, float fogEnd);
    uint32_t fogColour() const { return fogRGB; }
    // RenderGlobal.func_4142_a / func_4141_b for the next renderWorld (drawn when enabled)
    struct SkyState {
        bool enabled = false;          // false in the Nether (WorldProvider.field_4220_c)
        float angle = 0;               // celestial angle
        float sky[3] = {0, 0, 0};      // World.func_4079_a
        bool sunrise = false;
        float sunriseColour[4] = {0, 0, 0, 0};
        float stars = 0;               // func_679_f
        bool clouds = true;
        float cloud[3] = {1, 1, 1};    // func_628_d
        float cloudU = 0, cloudV = 0;  // texture scroll (player position + ticks * 0.03, in blocks)
        float cloudY = 0;              // cloud height above the eye
    } sky;

    // Call right after C3D_FrameBegin(C3D_FRAME_SYNCDRAW).
    void beginFrame();
    void renderWorld(const C3D_Mtx& view, const Frustum& frustum, float camX, float camY, float camZ);
    // ---- models (after renderWorld, before prepare2D) ----
    // Quads (4 vertices each) in world coordinates, textured from the terrain atlas
    // (animating model blocks).
    void drawAtlasQuads(const std::vector<axm::ModelVertex>& quads, const std::vector<BlockModels::Batch>& batches);
    // Triangles in world coordinates with their own texture (mobs). Culling off, like RenderLiving.
    // tint multiplies the colour (glColor4f); blend: 0 opaque (alpha test), 1 alpha blend, 2 additive
    // (glowing eyes); depthEqual draws only over what's already there (the red hurt flash).
    void drawTriangles(const std::vector<axm::ModelVertex>& tris, C3D_Tex* tex, const float tint[4] = nullptr,
                       int blend = 0, bool depthEqual = false, bool untextured = false);
    // romfs .rgba texture, loaded on first use (e.g. "romfs:/textures/mob/pig.rgba"). Null if missing.
    C3D_Tex* texture(const std::string& path);

    // ---- free 3D drawing (title screen) ----
    // Starts the top screen for the title: cleared to black, no world.
    void beginTitle();
    // Triangles with their own projection (view = identity), positions as given, UV 0..1 from the
    // top of the texture. blend: 0 opaque (alpha test), 1 alpha, 2 additive (alpha, one),
    // 3 (src colour, one). area: 0 whole screen, 1 top half only (the logo's viewport).
    void drawFree(const C3D_Mtx& proj, const std::vector<axm::ModelVertex>& tris, C3D_Tex* tex, int blend,
                  bool cull, int area = 0);

    // The current eye's projection (for the first-person hand)
    const C3D_Mtx& currentProjection() const { return projection; }

    // Switch the GPU over to citro2d with depth test/culling off.
    void prepare2D();

    void setBackfaceCulling(bool on) { cullBack = on; }
    bool backfaceCulling() const { return cullBack; }
    size_t linearBytesUsed() const { return linearBytes; }
    int lastDrawnSections() const { return drawnSections; }
    int lastDrawnTriangles() const { return drawnTris; }

    float renderDistance = 56.0f; // blocks; Old 3DS default
    static constexpr float FOV_DEG = 70.0f;
    float fovDeg = FOV_DEG; // the boss intro camera zooms

private:
    struct Column { int cx, cz; GpuMesh sections[SECTIONS]; };
#ifdef __ANDROID__
public:
    struct Gl; // the OpenGL ES side lives in android/src/RendererGL.cpp
private:
    Gl* gl = nullptr;
    C3D_RenderTarget* target = nullptr;
    C3D_RenderTarget* targetRight = nullptr;
    int curEye = 0;
    int skySub = 0;
    float ambientLight = 0.05f, fogStart = 14.0f, fogEnd = 56.0f;
    uint32_t fogRGB = 0x99BFFF;
    std::unordered_map<std::string, C3D_Tex*> textures;
    C3D_Mtx projection;
    C3D_Tex terrain;
    std::unordered_map<int64_t, Column> columns;
    size_t linearBytes = 0;
    bool cullBack = true;
    int drawnSections = 0, drawnTris = 0;
    static int64_t key(int cx, int cz) { return int64_t(cx) << 32 ^ uint32_t(cz); }
    void freeMesh(GpuMesh& m);
#else

    C3D_RenderTarget* target = nullptr;
    C3D_RenderTarget* targetRight = nullptr;
    int curEye = 0;
    DVLB_s* shaderDvlb = nullptr;
    shaderProgram_s program;
    int uLocProjection = -1, uLocModelView = -1, uLocOrigin = -1, uLocParams = -1, uLocTexScale = -1,
        uLocTexOffset = -1, uLocLight = -1, uLocFog = -1;
    int skySub = 0;
    float ambientLight = 0.05f, fogStart = 14.0f, fogEnd = 56.0f;
    uint32_t fogRGB = 0x99BFFF;
    void worldTexEnv();                 // stage 0 texture x shade, stage 1 fog
    void fogStage(int stage, uint32_t rgb);
    void setFogUniform(bool on, float start, float end);
    struct FloatVertex { float x, y, z, shade, u, v; };
    FloatVertex* modelBuf = nullptr; // per-frame vertex ring for models
    size_t modelUsed = 0;
    static constexpr size_t MODEL_VERTS = 24576;
    std::unordered_map<std::string, C3D_Tex*> textures;
    FloatVertex* reserveModel(size_t n);
    FloatVertex* skyPlane = nullptr; int skyPlaneN = 0;   // field_1433_z: 64-block grid at +16
    FloatVertex* voidPlane = nullptr; int voidPlaneN = 0; // field_1432_A: the same at -16
    FloatVertex* starBuf = nullptr; int starN = 0;        // field_1434_y: 1500 stars, Random(10842)
    FloatVertex* cloudGrid = nullptr; int cloudN = 0;     // fast clouds: 32-block quads over +-256
    void buildSkyBuffers();
    void drawSky(const C3D_Mtx& viewRot);
    void drawClouds(const C3D_Mtx& viewRot);
    void constStage0(uint32_t abgr, bool textured);
    void drawBuffer(FloatVertex* v, int n);
    void bindModelState(bool cull);
    C3D_Mtx projection;
    C3D_Tex terrain;
    std::unordered_map<int64_t, Column> columns;
    std::vector<void*> pendingFree;
    size_t linearBytes = 0;
    bool cullBack = true;
    int drawnSections = 0, drawnTris = 0;

    static int64_t key(int cx, int cz) { return int64_t(cx) << 32 ^ uint32_t(cz); }
    bool loadTerrainAtlas();
    void freeMesh(GpuMesh& m, bool deferred);
#endif
};
