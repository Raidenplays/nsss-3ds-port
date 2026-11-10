#pragma once
// The jar's entity models, built in code like the Java: ModelRenderer (one textured box with a
// rotation point and angles, TexturedQuad's 64 x 32 layout) and ModelBase's subclasses -
// ModelQuadraped (ModelPig, ModelCow, ModelSheep1/2), ModelBiped (ModelZombie, ModelSkeleton),
// ModelChicken, ModelCreeper, ModelSpider and ModelSlime - with their setRotationAngles.
//
// render() emits triangles in world space through a GL-style matrix; each face is shaded by
// RenderHelper's two standard lights (fixed in the world) and multiplied by the brightness.
#include <cstring>
#include <vector>
#include "render/Vertex.h"

namespace alphamodel {

using Vertex = QuadVertex;

struct M4 { // a GL matrix stack entry: every call multiplies on the right (row-major, column vectors)
    float m[16];
    M4() { std::memset(m, 0, sizeof m); m[0] = m[5] = m[10] = m[15] = 1; }
    void mul(const float* b);
    void translate(float x, float y, float z);
    void scale(float x, float y, float z);
    void rotate(float deg, float x, float y, float z); // glRotatef about any axis
};

class ModelRenderer {
public:
    ModelRenderer(int u = 0, int v = 0) : texU(u), texV(v) {}
    void addBox(float x, float y, float z, int w, int h, int d, float grow = 0.0f);
    void setRotationPoint(float x, float y, float z) { rotationPointX = x; rotationPointY = y; rotationPointZ = z; }
    void render(std::vector<Vertex>& out, const M4& parent, float scale, float bright) const;

    float rotationPointX = 0, rotationPointY = 0, rotationPointZ = 0;
    float rotateAngleX = 0, rotateAngleY = 0, rotateAngleZ = 0;
    bool mirror = false, showModel = true;

private:
    int texU, texV;
    struct Face { float p[4][3]; float uv[4][2]; };
    Face faces[6];
    bool built = false;
};

// ModelBase: the parts, posed by setRotationAngles(limbSwing, limbAmount, ticks, headYaw, headPitch)
class Model {
public:
    virtual ~Model() {}
    virtual void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) = 0;
    virtual void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const = 0;
    void setRotationAnglesAndRender(std::vector<Vertex>& out, const M4& t, float swing, float amount, float ticks,
                                    float yaw, float pitch, float scale, float bright) {
        setRotationAngles(swing, amount, ticks, yaw, pitch);
        render(out, t, scale, bright);
    }
    float onGround = 0;   // the swing progress
    bool riding = false;  // field_1243_l: something rides it
};

class Quadruped : public Model {
public:
    Quadruped(int legHeight, float grow);
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer head, body, leg1, leg2, leg3, leg4;
};
class Pig : public Quadruped { public: explicit Pig(float grow = 0.0f) : Quadruped(6, grow) {} };
class Cow : public Quadruped {
public:
    Cow();
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer udders, horn1, horn2;
};
class Sheep1 : public Quadruped { public: Sheep1(); };  // the wool
class Sheep2 : public Quadruped { public: Sheep2(); };  // the body

class Biped : public Model {
public:
    explicit Biped(float grow = 0.0f, float yOffset = 0.0f);
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer head, headwear, bodyPart, rightArm, leftArm, rightLeg, leftLeg;
    bool sneaking = false;
};
class Zombie : public Biped {
public:
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
};
class Skeleton : public Zombie { public: Skeleton(); };

class Chicken : public Model {
public:
    Chicken();
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer head, bodyPart, rightLeg, leftLeg, rightWing, leftWing, bill, chin;
};

class Creeper : public Model {
public:
    Creeper();
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer head, unused, bodyPart, leg1, leg2, leg3, leg4;
};

class Spider : public Model {
public:
    Spider();
    void setRotationAngles(float swing, float amount, float ticks, float yaw, float pitch) override;
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer head, neck, bodyPart, legs[8];
};

class Slime : public Model {
public:
    explicit Slime(int v);
    void setRotationAngles(float, float, float, float, float) override {}
    void render(std::vector<Vertex>& out, const M4& t, float scale, float bright) const override;
    ModelRenderer bodies, rightEye, leftEye, mouth;
    bool face;
};

} // namespace alphamodel
