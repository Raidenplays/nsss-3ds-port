// Rails placed in a fixed order on the 3DS World; compare the shapes with tools/java/RailTest.java.
// usage: railtest <worldDir>   (build with the genpop sources plus world/Rails.cpp)
#include "world/World.h"
#include <cstdio>
int main(int argc, char** argv) {
    SaveHandler s(argv[1], 0); LevelInfo li; s.readLevel(li); World w(&s, li, false);
    w.setWindow(0, 0);
    for (int cx = -1; cx <= 1; ++cx) for (int cz = -1; cz <= 1; ++cz) w.provideChunk(cx, cz);
    int Y = 120;
    for (int x = -2; x < 14; x++) for (int z = -2; z < 14; z++) { w.setBlockWithNotify(x, Y - 1, z, 1); for (int y = Y; y < Y + 4; y++) w.setBlockWithNotify(x, y, z, 0); }
    int seq[][3] = {{0,0,0},{1,0,0},{2,0,0},{2,0,1},{2,0,2},{1,0,2},{0,0,2},{0,0,1},{5,0,5},{5,0,6},{6,0,5},{4,0,5},{5,0,4},{8,0,8},{9,1,8},{10,1,8},{8,0,9}};
    for (auto& q : seq) {
        if (q[1] == 1) w.setBlockWithNotify(q[0], Y, q[2], 1);
        w.setBlockWithNotify(q[0], Y + q[1], q[2], 66);
    }
    for (auto& q : seq) printf("%d ", w.metaAt(q[0], Y + q[1], q[2]));
    printf("\n");
}
