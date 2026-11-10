#pragma once
// Axeolon's Blockbench models on the 3DS.
//
// One loader for both kinds of .bbmodel the mod uses (converted to .axm by
// tools/bbmodel_convert.py, which reads the JSON the same way the Java does):
//
//  * Mobs (AxeolonBBModel): Box-UV bones and cubes in Beta model space, every animation on the
//    timeline (rotation/position/scale, linear/smooth/step, loop/once/hold), blended by weight.
//  * Blocks (AxeolonBlockRig): per-face-UV cubes in Blockbench space, posed by an animation
//    at a time, turned by yaw/pitch, lit per face from the world.
//
// Both are straight ports: the maths follows the Java line by line, and tools/java/CheckModels
// compares the results against the running mod's own classes.
#include <cstdint>
#include <string>
#include <vector>

namespace axm {

struct Channel {
    std::vector<float> times;
    std::vector<float> values; // 3 per key
    std::vector<uint8_t> interp; // 0 linear, 1 smooth (catmullrom), 2 step
};

struct Animation {
    std::string name;
    float length = 1.0f; // seconds
    int loop = 0;        // 0 once, 1 loop, 2 hold last frame
    std::vector<Channel> channels;
    // per bone index: channel index for rotation / position / scale, -1 if none
    std::vector<int16_t> rot, pos, scl;
};

struct Cube {
    // as stored in the .bbmodel (Blockbench values)
    float from[3], to[3], origin[3], rot[3];
    float inflate = 0, uvOffset[2] = {0, 0};
    bool mirror = false, visible = true;
    std::string name;
    uint8_t faceMask = 0;
    float faceUV[6][4] = {};
    uint8_t faceRot[6] = {};

    // ---- mob form (ModelRenderer) ----
    float off[3] = {0, 0, 0};   // offsetX/Y/Z
    float mrot[3] = {0, 0, 0};  // rotateAngleX/Y/Z
    bool shown = true;          // field_1403_h
    float quads[6][4][5] = {};  // TexturedQuad corners: x, y, z, u, v (u/v 0..1, v down)
};

struct Bone {
    std::string name;
    int parent = -1;
    float origin[3], rot[3]; // Blockbench values
    std::vector<int> cubes, kids;
    // mob form
    float px = 0, py = 0, pz = 0, rx = 0, ry = 0, rz = 0;
    bool inMap = true; // reachable by name (Java HashMap: the last bone of a name wins)
    // posed each frame from the animations
    float arx = 0, ary = 0, arz = 0, apx = 0, apy = 0, apz = 0, asx = 1, asy = 1, asz = 1;
};

// A vertex for models drawn on their own (mobs, moving blocks)
struct ModelVertex {
    float x, y, z, shade;
    float u, v; // 0..1, v down (like Java)
};

class Model {
public:
    enum Kind { MOB = 0, RIG = 1 };

    // Load romfs/axeolon/<mob|block>/<name>.axm. path is a full file path.
    bool load(const char* path);
    bool loadMemory(const uint8_t* data, size_t size);
    bool ok() const { return loaded; }
    Kind kind() const { return Kind(kind_); }
    int texW() const { return texW_; }
    int texH() const { return texH_; }

    const Animation* animation(const std::string& name) const;
    std::vector<std::string> animationNames() const;
    float animationLength(const std::string& name) const;
    int boneIndex(const std::string& name) const; // -1 if none
    const std::vector<Bone>& bones() const { return bones_; }

    // ---- AxeolonBBModel ----
    static float sample(const Channel& c, float t, float out[3]);
    // sampleBone: a bone's channel (0 rot, 1 pos, 2 scale) at `seconds`, raw keyframe values.
    // false if the animation or bone/channel is missing.
    bool sampleBone(const std::string& anim, const std::string& bone, int channel, float seconds, float out[3]) const;

    // Armour layer: the same model with only some cubes, each puffed out (AxeolonBBModel.armour)
    void makeArmour(float inflate, const std::vector<std::string>& cubes);

    void resetPose();
    void apply(const Animation* a, float t, float weight);
    // head turn (render(): head follows where the mob looks, scaled by lookWeight)
    void lookAt(float headYawDeg, float headPitchDeg, float lookWeight);
    // AxeolonBBModel.renderAnimated pose (blocks, items, title screens)
    void poseAnimated(const std::string& anim, float seconds, const std::string& extra, float extraSeconds,
                      float extraWeight);
    // Draw the posed mob: triangles appended to out, positions * scale in model space
    // (Beta: y down, feet at 24/16). boneVisible(name) may hide bones (dire wolf saddle).
    // m is a column-major-free 4x4 row-major matrix applied first (the RenderLiving transform).
    void emitMob(std::vector<ModelVertex>& out, const float m[16], float scale, float brightness,
                 bool (*boneVisible)(const char* name, void* user) = nullptr, void* user = nullptr) const;
    // AxeolonBBModel.postRender: the matrix of a bone as last posed (held items)
    bool boneMatrix(const std::string& bone, float scale, float m[16]) const;

    // ---- AxeolonBlockRig ----
    enum Half { HALF_ALL = 0, HALF_THIS = 1, HALF_OTHER = 2 };
    // Light for one face: light(cx, cy, cz, nx, ny, nz) as AxeolonBlockRig.faceLight does,
    // with block-relative face centre and normal. nullptr means full bright.
    typedef float (*FaceLight)(double cx, double cy, double cz, double nx, double ny, double nz, void* user);
    // Quads (4 vertices each, counter-clockwise seen from outside) at the block's corner (ox, oy, oz).
    void emitRig(std::vector<ModelVertex>& out, const char* anim, float seconds, float yaw, float pitch, int half,
                 double ox, double oy, double oz, FaceLight light, void* user) const;

private:
    bool loaded = false;
    uint8_t kind_ = 0;
    int texW_ = 64, texH_ = 32;
    std::vector<Bone> bones_;
    std::vector<Cube> cubes_;
    std::vector<int> roots_;   // bone indices; a "_root" bone holds loose cubes (index looseBone_)
    int looseBone_ = -1;
    std::vector<Animation> anims_;
    float extraInflate = 0;

    void buildMob();
    void buildBox(Cube& c, float x, float y, float z, int w, int h, int d, float inflate);
    void emitBone(std::vector<ModelVertex>& out, int b, const float* parent, float scale, float brightness,
                  bool (*vis)(const char*, void*), void* user) const;
    void rigGroup(std::vector<ModelVertex>& out, int g, const double* parent, const char* anim, float sec, int half,
                  double ox, double oy, double oz, FaceLight light, void* user) const;
};

} // namespace axm
