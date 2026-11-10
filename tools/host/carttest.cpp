// minecart physics on the host, against gentest/CartTest.java
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
    for (int x = -4; x < 30; x++) for (int z = -4; z < 30; z++) { w.setBlockWithNotify(x, Y - 1, z, 1); for (int y = Y; y < Y + 6; y++) w.setBlockWithNotify(x, y, z, 0); }
    for (int x = 0; x < 8; x++) w.setBlockWithNotify(x, Y, 0, 66);
    w.setBlockWithNotify(8, Y, 0, 1); w.setBlockWithNotify(8, Y + 1, 0, 66);
    w.setBlockWithNotify(9, Y, 0, 1); w.setBlockWithNotify(9, Y + 1, 0, 66);
    w.setBlockWithNotify(10, Y, 0, 1); w.setBlockWithNotify(10, Y + 1, 0, 66);
    for (int z = 1; z < 10; z++) { w.setBlockWithNotify(10, Y, z, 1); w.setBlockWithNotify(10, Y + 1, z, 66); }
    printf("rails ");
    for (int x = 0; x <= 10; x++) printf("%d ", w.metaAt(x, x >= 8 ? Y + 1 : Y, 0));
    printf("\n");
    Vehicles v;
    v.placeCart(w, 0, Y, 0, 0);
    Vehicles::Cart& c = v.carts[0];
    c.mx = atof(argv[2]);
    Player p;
    p.place(1000, 200, 1000, 0, 0);
    ItemEntities* items = nullptr;
    for (int t = 0; t < atoi(argv[3]); t++) {
        v.tick(w, p, *items);
        const Vehicles::Cart& k = v.carts[0];
        printf("%d %.6f %.6f %.6f %.6f %.6f %.6f %.3f\n", t, k.x, k.y, k.z, k.mx, k.my, k.mz, k.yaw);
    }
}
