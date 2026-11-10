// boat physics on the host, against gentest/BoatTest.java (stubs as in carttest.cpp)
#include "world/World.h"
#include "game/Vehicles.h"
#include "game/Player.h"
#include "game/ItemEntities.h"
#include "render/Renderer.h"
#include "render/Particles.h"
#include "model/BlockModels.h"
#include <cstdio>
#include <cstdlib>
Particles* gParticles = nullptr;
void Renderer::drawTriangles(const std::vector<axm::ModelVertex>&, C3D_Tex*, const float*, int, bool, bool) {}
C3D_Tex* Renderer::texture(const std::string&) { return nullptr; }
void ItemEntities::spawn(double, double, double, const ItemStack&, int) {}
void ItemEntities::spawnMoving(double, double, double, const ItemStack&, double, double, double, int) {}
void Particles::spawn(Type, double, double, double, double, double, double) {}
void Particles::spawnLargeSmoke(double, double, double) {}
namespace sound { void play(const std::string&, double, double, double, float, float) {} }
namespace BlockModels { float lightBrightness(const World&, int, int, int) { return 1.0f; } }
namespace containers { ItemStack fromNBT(const nbt::Tag&) { return ItemStack{}; } void toNBT(const ItemStack&, nbt::Tag&) {} }
int main(int argc, char** argv) {
    SaveHandler s(argv[1], 0); LevelInfo li; s.readLevel(li); World w(&s, li, false);
    w.setWindow(0, 0);
    for (int cx = -1; cx <= 2; ++cx) for (int cz = -1; cz <= 2; ++cz) w.provideChunk(cx, cz);
    int Y = 120;
    for (int x = -4; x < 30; x++) for (int z = -4; z < 30; z++) { w.setBlockWithNotify(x, Y - 2, z, 1); w.setBlockWithNotify(x, Y - 1, z, x >= 0 && x < 20 && z >= 0 && z < 20 ? 9 : 1); for (int y = Y; y < Y + 6; y++) w.setBlockWithNotify(x, y, z, 0); }
    w.setBlockWithNotify(15, Y, 5, 1); w.setBlockWithNotify(15, Y - 1, 5, 1);
    Vehicles v;
    Vehicles::Boat b;
    // EntityBoat(world, x, y, z): setPosition(x, y + yOffset, z)
    b.x = 5.5; b.y = Y + 0.5 + 0.3; b.z = 5.5;
    b.box = AABB{b.x - 0.75, b.y - 0.3, b.z - 0.75, b.x + 0.75, b.y + 0.3, b.z + 0.75};
    b.id = 99;
    b.mx = atof(argv[2]); b.mz = atof(argv[3]);
    v.boats.push_back(b);
    Player p;
    p.place(1000, 200, 1000, 0, 0);
    ItemEntities* items = nullptr;
    for (int t = 0; t < atoi(argv[4]); t++) {
        v.tick(w, p, *items);
        if (v.boats.empty()) { printf("%d dead\n", t); break; }
        const Vehicles::Boat& k = v.boats[0];
        printf("%d %.6f %.6f %.6f %.6f %.6f %.6f %.3f false\n", t, k.x, k.y, k.z, k.mx, k.my, k.mz, k.yaw);
    }
}
