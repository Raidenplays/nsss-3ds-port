#pragma once
// EffectRenderer and the particles NSSS draws: block debris (EntityDiggingFX) when a block breaks
// or while you dig, smoke, flames, explosions, lava pops and bubbles, plus World.randomDisplay-
// Updates (torch flames, fire smoke, lava). Billboards facing the camera, like the jar's.
#include <vector>
#include "render/Vertex.h"
#include "world/JavaRandom.h"

class World;
class Renderer;

class Particles {
public:
    enum Type : uint8_t { DIG, SMOKE, FLAME, EXPLODE, LAVA, BUBBLE, REDDUST, BUG, SNOWPOOF, SPLASH, SLIME };
    // EffectRenderer.func_1186_a: a block breaking, in 4 x 4 x 4 pieces
    void blockBroken(const World& w, int x, int y, int z, int id, int meta);
    // func_1191_a: chips flying off the face being dug (side as the jar numbers it)
    void blockHit(const World& w, int x, int y, int z, int side);
    // World.spawnParticle
    void spawn(Type t, double x, double y, double z, double mx, double my, double mz);
    void spawnLargeSmoke(double x, double y, double z); // "largesmoke": EntitySmokeFX at 2.5 times the size
    // World.randomDisplayUpdates around the player (1000 random blocks within 16)
    void ambient(const World& w, double px, double py, double pz);
    void tick(const World& w);
    void render(const World& w, Renderer& r, float partial, float yaw, float pitch);
    void clear() { list.clear(); }
    size_t count() const { return list.size(); }

private:
    struct P {
        Type type;
        double x, y, z, px, py, pz, mx, my, mz;
        float r, g, b, scale, baseScale, gravity;
        int age, maxAge, tex;     // tex: particles.png index, or the terrain tile for DIG
        float jx, jy;             // DIG: which 4 x 4 corner of the tile
        bool onGround = false, noClip = false;
    };
    std::vector<P> list;
    JavaRandom rand{0x9A57};
    std::vector<QuadVertex> quads[2];
    P make(Type t, double x, double y, double z, double mx, double my, double mz);
    void move(const World& w, P& p);
    double rnd() { return rand.nextDouble(); }
};

// The game's one effect renderer (mobs and explosions reach it from anywhere)
extern Particles* gParticles;
