// BlockMinecartTrack + MinecartTrackLogic: a rail bends to join the rails next to it, and they
// bend to join it, the same way round as Java (a powered junction picks the other corner).
#include "Rails.h"
#include <vector>
#include "Blocks.h"
#include "Redstone.h"
#include "World.h"

namespace rails {
namespace {
constexpr int RAIL = 66;
struct Pos { int x, y, z; };

struct Logic { // MinecartTrackLogic
    World& w;
    int x, y, z, meta;
    std::vector<Pos> conn;
    Logic(World& w, int x, int y, int z) : w(w), x(x), y(y), z(z), meta(w.metaAt(x, y, z)) { calculate(); }

    void calculate() { // calculateConnectedTracks
        conn.clear();
        switch (meta) {
        case 0: conn = {{x, y, z - 1}, {x, y, z + 1}}; break;
        case 1: conn = {{x - 1, y, z}, {x + 1, y, z}}; break;
        case 2: conn = {{x - 1, y, z}, {x + 1, y + 1, z}}; break;
        case 3: conn = {{x - 1, y + 1, z}, {x + 1, y, z}}; break;
        case 4: conn = {{x, y + 1, z - 1}, {x, y, z + 1}}; break;
        case 5: conn = {{x, y, z - 1}, {x, y + 1, z + 1}}; break;
        case 6: conn = {{x + 1, y, z}, {x, y, z + 1}}; break;
        case 7: conn = {{x - 1, y, z}, {x, y, z + 1}}; break;
        case 8: conn = {{x - 1, y, z}, {x, y, z - 1}}; break;
        case 9: conn = {{x + 1, y, z}, {x, y, z - 1}}; break;
        default: break;
        }
    }
    bool isRail(int a, int b, int c) { // a rail here, or one up or down
        return w.blockAt(a, b, c) == RAIL || w.blockAt(a, b + 1, c) == RAIL || w.blockAt(a, b - 1, c) == RAIL;
    }
    bool logicAt(const Pos& p, Pos& out) {
        if (w.blockAt(p.x, p.y, p.z) == RAIL) { out = p; return true; }
        if (w.blockAt(p.x, p.y + 1, p.z) == RAIL) { out = {p.x, p.y + 1, p.z}; return true; }
        if (w.blockAt(p.x, p.y - 1, p.z) == RAIL) { out = {p.x, p.y - 1, p.z}; return true; }
        return false;
    }
    void dropBroken() { // func_785_b: only the ends that still lead to a rail that leads back
        for (size_t i = 0; i < conn.size(); ++i) {
            Pos q;
            bool keep = false;
            if (logicAt(conn[i], q)) {
                Logic o(w, q.x, q.y, q.z);
                if (o.connectedTo(*this)) { conn[i] = q; keep = true; }
            }
            if (!keep) conn.erase(conn.begin() + long(i--));
        }
    }
    bool connectedTo(const Logic& o) const {
        for (const Pos& p : conn)
            if (p.x == o.x && p.z == o.z) return true;
        return false;
    }
    bool hasEnd(int a, int c) const {
        for (const Pos& p : conn)
            if (p.x == a && p.z == c) return true;
        return false;
    }
    int adjacent() { // getAdjacentTracks
        return int(isRail(x, y, z - 1)) + int(isRail(x, y, z + 1)) + int(isRail(x - 1, y, z)) + int(isRail(x + 1, y, z));
    }
    bool canJoin(const Logic& o) { // handleKeyPress
        if (connectedTo(o)) return true;
        if (conn.size() == 2) return false;
        return true;
    }
    int slopes(int m) { // a straight rail climbs to a rail one up at an end
        if (m == 0) {
            if (w.blockAt(x, y + 1, z - 1) == RAIL) m = 4;
            if (w.blockAt(x, y + 1, z + 1) == RAIL) m = 5;
        }
        if (m == 1) {
            if (w.blockAt(x + 1, y + 1, z) == RAIL) m = 2;
            if (w.blockAt(x - 1, y + 1, z) == RAIL) m = 3;
        }
        return m < 0 ? 0 : m;
    }
    void join(const Logic& o) { // func_788_d: bend toward the new rail
        conn.push_back({o.x, o.y, o.z});
        bool n = hasEnd(x, z - 1), s = hasEnd(x, z + 1), wv = hasEnd(x - 1, z), e = hasEnd(x + 1, z);
        int m = -1;
        if (n || s) m = 0;
        if (wv || e) m = 1;
        if (s && e && !n && !wv) m = 6;
        if (s && wv && !n && !e) m = 7;
        if (n && wv && !s && !e) m = 8;
        if (n && e && !s && !wv) m = 9;
        w.setBlockMetadataWithNotify(x, y, z, slopes(m));
    }
    bool wantsMe(int a, int b, int c) { // func_786_c
        Pos q;
        if (!logicAt({a, b, c}, q)) return false;
        Logic o(w, q.x, q.y, q.z);
        o.dropBroken();
        return o.canJoin(*this);
    }
    void place(bool powered) { // func_792_a
        bool n = wantsMe(x, y, z - 1), s = wantsMe(x, y, z + 1), wv = wantsMe(x - 1, y, z), e = wantsMe(x + 1, y, z);
        int m = -1;
        if ((n || s) && !wv && !e) m = 0;
        if ((wv || e) && !n && !s) m = 1;
        if (s && e && !n && !wv) m = 6;
        if (s && wv && !n && !e) m = 7;
        if (n && wv && !s && !e) m = 8;
        if (n && e && !s && !wv) m = 9;
        if (m == -1) {
            if (n || s) m = 0;
            if (wv || e) m = 1;
            if (powered) {
                if (s && e) m = 6;
                if (wv && s) m = 7;
                if (e && n) m = 9;
                if (n && wv) m = 8;
            } else {
                if (n && wv) m = 8;
                if (e && n) m = 9;
                if (wv && s) m = 7;
                if (s && e) m = 6;
            }
        }
        meta = slopes(m);
        calculate();
        w.setBlockMetadataWithNotify(x, y, z, meta);
        std::vector<Pos> ends = conn;
        for (const Pos& p : ends) {
            Pos q;
            if (!logicAt(p, q)) continue;
            Logic o(w, q.x, q.y, q.z);
            o.dropBroken();
            if (o.canJoin(*this)) o.join(*this);
        }
    }
};

void refresh(World& w, int x, int y, int z) { // func_4031_h
    Logic(w, x, y, z).place(Redstone::isBlockIndirectlyGettingPowered(w, x, y, z));
}
} // namespace

bool canPlace(World& w, int x, int y, int z) { return w.isBlockOpaqueCube(x, y - 1, z); }

void onBlockAdded(World& w, int x, int y, int z) {
    w.setBlockMetadataWithNotify(x, y, z, 15);
    refresh(w, x, y, z);
}

void onNeighborChange(World& w, int x, int y, int z, int neighbour) {
    int m = w.metaAt(x, y, z);
    bool drop = !w.isBlockOpaqueCube(x, y - 1, z);
    if (m == 2 && !w.isBlockOpaqueCube(x + 1, y, z)) drop = true;
    if (m == 3 && !w.isBlockOpaqueCube(x - 1, y, z)) drop = true;
    if (m == 4 && !w.isBlockOpaqueCube(x, y, z - 1)) drop = true;
    if (m == 5 && !w.isBlockOpaqueCube(x, y, z + 1)) drop = true;
    if (drop) {
        Blocks::dropBlockAsItem(w, x, y, z, RAIL, m);
        w.setBlockWithNotify(x, y, z, 0);
    } else if (neighbour > 0 && Redstone::canProvidePower(neighbour) && Logic(w, x, y, z).adjacent() == 3) {
        refresh(w, x, y, z);
    }
}
} // namespace rails
