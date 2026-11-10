// Axeolon 3DS
// Top screen: gameplay. Bottom screen: Almanac map, inventory (with battle slots and charms),
// menus, hotbar.
// Budget: 60 fps on an Old 3DS (268 MHz, no C-stick, no ZL/ZR).
//
// Worlds: your Java Axeolon saves, copied as-is to sdmc:/3ds/nsss3ds/saves/<World>/.
// Loads player position, look, inventory, armour, charms, battle slots and the Almanac map
// and waypoints; saves them all back on quit in the same format, so the world goes straight
// back to the PC.
//
// Controls, Old 3DS:
//   Circle Pad        move              ABXY      look (X up, B down, Y left, A right)
//   R                 break block       L         use (doors, levers, buttons, chests) or place
//   D-pad left/right  hotbar (battle mode: weapon pair)
//   D-pad up          jump (swim up)    D-pad down  sneak
// New 3DS, or an Old 3DS with the Circle Pad Pro (console "Xbox" layout):
//   Circle Pad move, C-stick / CPP stick look, A jump, B sneak, X items (D-pad still works).
//   New 3DS: ZR break, ZL use/place, L / R hotbar. Old 3DS + CPP: R break, L use, D-pad hotbar.
//   SELECT            fly on/off (noclip, for getting unstuck)   START  Menu tab
//   Touch             bottom screen (hold L or R while tapping = split stack)
#include <3ds.h>
#include <citro3d.h>
#include <citro2d.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <malloc.h>
#include <climits>
#include <cmath>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

#include "world/Nbt.h"
#include "world/World.h"
#include "world/MathHelper.h"
#include "world/Blocks.h"
#include "world/Redstone.h"
#include "world/SaveHandler.h"
#include "world/BlockTable.h"
#include "render/ChunkMesh.h"
#include "render/Renderer.h"
#include "render/Frustum.h"
#include "render/Atmosphere.h"
#include "render/FirstPerson.h"
#include "render/Particles.h"
#include "render/Signs.h"
#include "client/Camera.h"
#include "ui/Hud.h"
#include "audio/Sound.h"
#include "game/Inventory.h"
#include "game/AlmanacMap.h"
#include "game/Spells.h"
#include "game/Teleporter.h"
#include "game/Vehicles.h"
#include "game/Options.h"
#include "world/Physics.h"
#include "render/TextureFX.h"
#include "render/DynamicLights.h"
#include "game/Progress.h"
#include "game/Commands.h"
#include "game/Paintings.h"
#include "game/Mobs.h"
#include "game/EntityDamage.h"
#include "game/ItemUse.h"
#include "world/Bosses.h"
#include "game/ItemEntities.h"
#include "game/Survival.h"
#include "game/Containers.h"
#include "game/Player.h"
#include "model/BlockModels.h"
#include "ui/BottomScreen.h"
#include "ui/TitleScreen.h"
#include "world/gen/TerrainWorker.h"
#include "world/GenFiber.h"
#include "world/gen/ChunkGenerator.h"

// libctru gives the main thread 32 KB of stack by default; world loading and meshing want more
u32 __stacksize__ = 512 * 1024;
// The linear (GPU) heap: libctru's default 32 MB leaves an Old 3DS (64 MB in all) short of normal heap - the
// world ran out of it saving and loading. The game peaks under 20 MB of linear memory, so 24 MB is plenty,
// and the rest goes to the normal heap (a New 3DS gets far more either way).
#ifdef __3DS__
u32 __ctru_linear_heap_size = 24 * 1024 * 1024;
extern u32 __ctru_heap_size;
#endif


namespace {
// Startup log on the SD card (sdmc:/3ds/nsss3ds/log.txt), flushed every line, so a crash
// leaves a trail of how far it got.
void dbg(const char* fmt, ...) {
    { // and to the debugger / emulator log, in order (the emulator's SD appends can shuffle)
        char line[256];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(line, sizeof line, fmt, ap);
        va_end(ap);
        if (n > 0) svcOutputDebugString(line, std::min(n, int(sizeof line) - 1));
    }
    FILE* f = fopen("sdmc:/3ds/nsss3ds/log.txt", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

} // namespace
#ifdef __3DS__
// abort() (a failed check in the C++ library, out of memory...) normally exits quietly, and libctru's own
// threads then fault as the heap goes - a misleading crash screen. Say where it came from and stop right here
// instead, so Luma's screen shows the caller (LR).
extern "C" void abort(void) {
    dbg("abort() called from %p", __builtin_return_address(0));
    svcBreak(USERBREAK_PANIC);
    for (;;) {}
}
#endif
namespace {

constexpr const char* SAVES_DIR = "sdmc:/3ds/nsss3ds/saves";
constexpr float REACH = 5.0f;   // Beta survival reach
constexpr float REPEAT = 0.25f; // break/place repeat while held (Beta: 4/sec)
constexpr int LOAD_RADIUS = 4;  // chunks around the player kept loaded (Old 3DS)
constexpr float GEN_BUDGET_MS = 8.0f; // generation time per frame (GenFiber)
constexpr float PI = 3.14159265358979f;

// ---------------------------------------------------------------- section rebuilds
struct RebuildQueue {
    std::vector<int64_t> items;
    std::unordered_set<int64_t> pending;
    static int64_t key(int cx, int sy, int cz) {
        return (int64_t(cx) & 0x3FFFFFF) << 32 | (int64_t(cz) & 0x3FFFFFF) << 6 | (sy & 63);
    }
    static void unkey(int64_t k, int& cx, int& sy, int& cz) {
        cx = int(int32_t(uint32_t(k >> 32) << 6) >> 6);
        cz = int(int32_t(uint32_t((k >> 6) & 0x3FFFFFF) << 6) >> 6);
        sy = int(k & 63);
    }
    void push(int cx, int sy, int cz, bool front = false) {
        if (sy < 0 || sy >= Renderer::SECTIONS) return;
        int64_t k = key(cx, sy, cz);
        if (!pending.insert(k).second) return;
        if (front) items.insert(items.begin(), k); else items.push_back(k);
    }
    bool has(int cx, int sy, int cz) const { return pending.count(key(cx, sy, cz)) != 0; }
    void markBlock(int x, int y, int z) {
        int sx = x >> 4, sy = y >> 4, sz = z >> 4;
        push(sx, sy, sz, true); // the edited section first: it's the one you're looking at
        if ((x & 15) == 0) push(sx - 1, sy, sz, true);
        if ((x & 15) == 15) push(sx + 1, sy, sz, true);
        if ((y & 15) == 0) push(sx, sy - 1, sz, true);
        if ((y & 15) == 15) push(sx, sy + 1, sz, true);
        if ((z & 15) == 0) push(sx, sy, sz - 1, true);
        if ((z & 15) == 15) push(sx, sy, sz + 1, true);
    }
    void markColumn(int cx, int cz) {
        for (int sy = 0; sy < Renderer::SECTIONS; sy++) push(cx, sy, cz);
    }
    bool pop(int& cx, int& sy, int& cz) {
        if (items.empty()) return false;
        int64_t k = items.front();
        items.erase(items.begin());
        pending.erase(k);
        unkey(k, cx, sy, cz);
        return true;
    }
    void dropColumn(int cx, int cz) {
        for (int i = int(items.size()) - 1; i >= 0; --i) {
            int x, y, z;
            unkey(items[i], x, y, z);
            if (x == cx && z == cz) { pending.erase(items[i]); items.erase(items.begin() + i); }
        }
    }
};
RebuildQueue rebuild;

void hookMarkDirty(int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int sx = x0 >> 4; sx <= x1 >> 4; sx++)
        for (int sy = (y0 < 0 ? 0 : y0) >> 4; sy <= (y1 > 255 ? 255 : y1) >> 4; sy++)
            for (int sz = z0 >> 4; sz <= z1 >> 4; sz++) rebuild.push(sx, sy, sz, true);
}
bool xboxLayout = false; // second-stick look, A jump, B sneak
bool zButtons = false;   // New 3DS: ZR break, ZL use, L/R hotbar (Old 3DS + Circle Pad Pro keeps R/L)

// World changes (setBlock, lighting) -> section rebuilds, for columns the renderer is showing
int viewCx = 0, viewCz = 0;
void worldDirty(int x0, int y0, int z0, int x1, int y1, int z1) {
    for (int sx = (x0 - 1) >> 4; sx <= (x1 + 1) >> 4; sx++) {
        if (std::abs(sx - viewCx) > LOAD_RADIUS) continue;
        for (int sz = (z0 - 1) >> 4; sz <= (z1 + 1) >> 4; sz++) {
            if (std::abs(sz - viewCz) > LOAD_RADIUS) continue;
            for (int sy = (y0 - 1 < 0 ? 0 : y0 - 1) >> 4; sy <= (y1 + 1 > 255 ? 255 : y1 + 1) >> 4; sy++) rebuild.push(sx, sy, sz);
        }
    }
}
// RenderGlobal.drawBlockBreaking: the crack texture (terrain.png 240-249) over the block
void drawCracks(Renderer& r, int x, int y, int z, int stage) {
    if (stage < 0) return;
    if (stage > 9) stage = 9;
    int tile = 240 + stage;
    float u0 = (tile & 15) / 16.0f, v0 = (tile >> 4) * 16.0f / 1024.0f, u1 = u0 + 1 / 16.0f, v1 = v0 + 16.0f / 1024.0f;
    const float e = 0.002f;
    float a0 = x - e, b0 = y - e, c0 = z - e, a1 = x + 1 + e, b1 = y + 1 + e, c1 = z + 1 + e;
    float F[6][4][3] = {{{a0, b0, c0}, {a1, b0, c0}, {a1, b0, c1}, {a0, b0, c1}}, {{a0, b1, c1}, {a1, b1, c1}, {a1, b1, c0}, {a0, b1, c0}},
                        {{a1, b1, c0}, {a0, b1, c0}, {a0, b0, c0}, {a1, b0, c0}}, {{a0, b1, c1}, {a1, b1, c1}, {a1, b0, c1}, {a0, b0, c1}},
                        {{a0, b1, c0}, {a0, b1, c1}, {a0, b0, c1}, {a0, b0, c0}}, {{a1, b1, c1}, {a1, b1, c0}, {a1, b0, c0}, {a1, b0, c1}}};
    const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    std::vector<axm::ModelVertex> tris;
    for (auto& f : F) {
        axm::ModelVertex q[4];
        for (int k = 0; k < 4; ++k) q[k] = {f[k][0], f[k][1], f[k][2], 1.0f, uv[k][0], uv[k][1]};
        tris.insert(tris.end(), {q[0], q[1], q[2], q[0], q[2], q[3]});
    }
    const float tint[4] = {1, 1, 1, 0.5f};
    r.drawTriangles(tris, r.terrainTexture(), tint, 1);
}

bool hookChunkPending(int x, int y, int z) { return rebuild.has(x >> 4, y >> 4, z >> 4); }

// ---------------------------------------------------------------- bundled worlds
// Worlds shipped inside the app (romfs:/worlds/<name>) are copied to the SD card the first time,
// so they show up in the world list and save like any other world.
bool copyFile(const std::string& from, const std::string& to) {
    FILE* in = fopen(from.c_str(), "rb");
    if (!in) return false;
    FILE* out = fopen(to.c_str(), "wb");
    if (!out) { fclose(in); return false; }
    static char buf[64 * 1024];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof buf, in)) > 0) ok &= fwrite(buf, 1, n, out) == n;
    fclose(in);
    fclose(out);
    return ok;
}

// copies what's missing: a file already on the SD card with something in it is kept (it may be progress),
// except level.dat / level.dat_old when `freshLevel` (they were unreadable)
bool copyDir(const std::string& from, const std::string& to, bool freshLevel = false) {
    DIR* d = opendir(from.c_str());
    if (!d) return false;
    mkdir(to.c_str(), 0777);
    bool ok = true;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string src = from + "/" + n, dst = to + "/" + n;
        struct stat st;
        if (stat(src.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { ok &= copyDir(src, dst, false); continue; }
        struct stat have;
        bool levelFile = n == "level.dat" || n == "level.dat_old";
        if (!(freshLevel && levelFile) && stat(dst.c_str(), &have) == 0 && have.st_size > 0) continue;
        ok &= copyFile(src, dst);
    }
    closedir(d);
    return ok;
}

static bool levelReadable(const std::string& dir) {
    return nbt::readGzipFile(dir + "/level.dat") || nbt::readGzipFile(dir + "/level.dat_old");
}

// Copies the worlds shipped in romfs to the SD card. Straight into place, no directory rename: on
// Azahar for Android the SD card is a storage-access-framework folder where renaming a folder can
// leave it empty. A world whose level.dat can't be read (a copy cut short) is repaired from romfs
// without touching the chunks already there.
void installBundledWorlds() {
    DIR* d = opendir("romfs:/worlds");
    if (!d) return;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        std::string dst = std::string(SAVES_DIR) + "/" + n;
        struct stat st;
        bool there = stat(dst.c_str(), &st) == 0;
        if (there && levelReadable(dst)) continue; // already on the SD card: never overwrite progress
        if (!copyDir("romfs:/worlds/" + n, dst, there)) continue;
    }
    closedir(d);
}

// ---------------------------------------------------------------- world list
// ---------------------------------------------------------------- player <-> save
void loadPlayer(SaveHandler* save, const LevelInfo& li, Camera& cam, Inventory& inv, AlmanacMap& map) {
    if (li.hasPlayer) {
        cam.x = float(li.playerX);
        cam.y = float(li.playerY); // Beta saves posY at eye height
        cam.z = float(li.playerZ);
        cam.yaw = li.playerYaw * PI / 180.0f + PI;
        cam.pitch = li.playerPitch * PI / 180.0f;
    } else {
        cam.x = li.spawnX + 0.5f;
        cam.y = li.spawnY + 1.62f;
        cam.z = li.spawnZ + 0.5f;
    }
    nbt::Tag* p = save ? save->player() : nullptr;
    if (!p) return;
    if (nbt::Tag* list = p->getList("Inventory")) inv.readFromNBT(*list);
    // EntityPlayer.readEntityFromNBT order: slot first, then the mode
    inv.selectBattleSlot(p->getInt("AxBattleSlot"));
    inv.setBattleMode(p->getBool("AxBattleMode"));
    map.readWaypoints(p->getString("AxWaypoints"));
    inv.achievements = p->getString("AxAchievements");
    inv.sawNight = p->getBool("AxSawNight");
    inv.seenItems = p->getString("AxSeenItems");
    // the how-to-play hints: from the start in a new world, skipped in one that's been played
    if (p->has("AxTutorial")) inv.tutorial = p->getInt("AxTutorial");
    else {
        bool played = !inv.achievements.empty();
        for (int i = 0; i < Inventory::MAIN && !played; ++i) played = !inv.main[i].empty();
        inv.tutorial = played ? 99 : 0;
    }
}

void storePlayer(SaveHandler* save, World& world, const Player& pl, const Inventory& inv, const AlmanacMap& map) {
    LevelInfo& li = world.info;
    li.hasPlayer = true;
    li.playerX = pl.posX; // posY at eye level, like Java
    li.playerY = pl.posY;
    li.playerZ = pl.posZ;
    li.playerYaw = pl.rotationYaw;
    li.playerPitch = pl.rotationPitch;
    if (!save) return;
    nbt::Tag* p = save->player();
    if (!p) return; // writeLevel creates the compound; inventory goes in on the next save
    p->set("Inventory", inv.writeToNBT());
    p->setInt("AxBattleSlot", inv.battleSlot);
    p->setBool("AxBattleMode", inv.battleMode);
    p->setString("AxWaypoints", map.writeWaypoints());
    p->setString("AxAchievements", inv.achievements);
    p->setBool("AxSawNight", inv.sawNight);
    p->setString("AxSeenItems", inv.seenItems);
    p->setInt("AxTutorial", inv.tutorial);
    p->setShort("Health", int16_t(pl.health));
    p->setShort("HurtTime", int16_t(pl.hurtTime));
    p->setShort("DeathTime", int16_t(pl.deathTime));
    p->setShort("AttackTime", int16_t(pl.attackTime));
    p->setFloat("FallDistance", pl.fallDistance);
    p->setShort("Fire", int16_t(pl.fire));
    p->setShort("Air", int16_t(pl.air));
    p->setShort("Mana", int16_t(pl.mana));
}

// Where right-click and left-click aim: the screen centre on the 3DS; on Android, through the finger
// (Pocket Edition has no crosshair: you tap the chest, the dog, the block you mean)
static void aimDir(const Camera& cam, float fovDeg, float& fx, float& fy, float& fz) {
    cam.forward(fx, fy, fz);
    (void)fovDeg;
}

void drawCrosshair() {
    const float cx = 200, cy = 120;
    u32 dark = C2D_Color32(0, 0, 0, 160), light = C2D_Color32(240, 240, 240, 230);
    C2D_DrawRectSolid(cx - 6, cy - 1.5f, 0.5f, 12, 3, dark);
    C2D_DrawRectSolid(cx - 1.5f, cy - 6, 0.5f, 3, 12, dark);
    C2D_DrawRectSolid(cx - 5, cy - 0.5f, 0.5f, 10, 1, light);
    C2D_DrawRectSolid(cx - 0.5f, cy - 5, 0.5f, 1, 10, light);
}

bool isDoorId(int id) { return id == 64 || id == 71 || (id >= 145 && id <= 153); }

// Right-click on a block (Block.blockActivated) for the blocks with Axeolon animations.
// Returns true if the block took the click (nothing gets placed).
struct OpenChest { int x, y, z; bool open = false; } chest;

bool useBlock(World& w, int x, int y, int z, bool emptyHand) {
    int id = w.getBlockId(x, y, z);
    int meta = w.getBlockMetadata(x, y, z);
    if (isDoorId(id)) {
        if (id == 71) return true; // iron: redstone only
        if (meta & 8) {
            if (w.getBlockId(x, y - 1, z) == id) return useBlock(w, x, y - 1, z, emptyHand);
            return true;
        }
        BlockModels::door(w, x, y, z, meta);
        sound::play(w.rand.nextFloat() < 0.5f ? "random.door_open" : "random.door_close", x + 0.5, y + 0.5, z + 0.5, 1.0f,
                    w.rand.nextFloat() * 0.1f + 0.9f);
        if (w.getBlockId(x, y + 1, z) == id) w.setBlockAndMetadata(x, y + 1, z, uint8_t(id), (meta ^ 4) + 8);
        w.setBlockAndMetadata(x, y, z, uint8_t(id), meta ^ 4);
        rebuild.markBlock(x, y, z);
        rebuild.markBlock(x, y + 1, z);
        return true;
    }
    // levers, buttons, repeater delays and note blocks (they power things, so they live with redstone)
    if (Redstone::blockActivated(w, x, y, z, emptyHand)) return true;
    if (id == 54 || id == 142) { // chest / crate: the lid opens while it's "open"
        if (chest.open) { chest.open = false; return true; }
        BlockModels::openContainer(w, x, y, z);
        chest = {x, y, z, true};
        return true;
    }
    return false;
}
} // namespace

int main() {
    osSetSpeedupEnable(true); // no-op on Old 3DS, 804 MHz on New
    gfxInitDefault();
    gfxSet3D(true); // stereoscopic 3D, depth from the 3D slider
    romfsInit();
    // Controls: the New 3DS (C-stick, ZL/ZR) gets the console "Xbox" layout. On an Old 3DS the
    // Circle Pad Pro talks through ir:rst; the first time it reports anything, switch layouts too.
    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);
    bool irrstOk = !isNew3DS && R_SUCCEEDED(irrstInit());
    xboxLayout = isNew3DS;
    zButtons = isNew3DS;
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/nsss3ds", 0777);
    mkdir(SAVES_DIR, 0777);
    remove("sdmc:/3ds/nsss3ds/log.txt");
    dbg("start");
    installBundledWorlds();
    dbg("bundled worlds ok");

    Renderer renderer;
    bool rok = renderer.init();
    dbg("renderer init %d", rok);
    TitleScreen title;
    bool tok = rok && title.init(renderer, SAVES_DIR);
    dbg("title init %d", tok);
    bool soundOk = sound::init();
    dbg("sound %d", soundOk);
    gOptions.load(); // options.txt
    sound::setVolumes(gOptions.sound, gOptions.music);
    if (gOptions.renderDistance >= 24 && gOptions.renderDistance <= 128) renderer.renderDistance = float(gOptions.renderDistance);
    if (!tok) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        C2D_Fini(); C3D_Fini(); romfsExit(); gfxExit();
        return 0;
    }
    BlockModels::markDirty = hookMarkDirty;
    BlockModels::chunkPending = hookChunkPending;

    // Automatic test (sdmc:/3ds/nsss3ds/autotest.txt present): the title screen for a few
    // seconds, then World1, replaying tools/java/SimPlayer's input script with every tick logged.
    struct stat ast;
    bool autotestOn = stat("sdmc:/3ds/nsss3ds/autotest.txt", &ast) == 0;
    bool autotestNew = false; // "new" in the file: create World5 from seed 12345 instead
    bool autotestNether = false;
    bool autotestQuit = false;
    int autotestVisits = 0;
    if (autotestOn) {
        FILE* f = fopen("sdmc:/3ds/nsss3ds/autotest.txt", "r");
        char buf[8] = {0};
        if (f) { fread(buf, 1, 3, f); fclose(f); }
        autotestNew = std::strncmp(buf, "new", 3) == 0;
        autotestNether = std::strncmp(buf, "net", 3) == 0; // "nether": a new world, started in the Nether
        autotestQuit = std::strncmp(buf, "qui", 3) == 0;   // "quit": World5, saved and left every 25 s, again and again
        if (autotestQuit) autotestNew = true;
        if (autotestNether) autotestNew = true;
    }

    bool appRunning = true;
    std::string portalWorld; // Minecraft.usePortal: straight back into this world, in the other dimension
    bool respawnHome = false; // Minecraft.respawn from the Nether: back to the overworld's spawn
    while (appRunning) {
    // ================================================================ title screen
    std::string worldName;
    const bool arrivingByPortal = !portalWorld.empty() && !respawnHome;
    const bool arrivingRespawn = !portalWorld.empty() && respawnHome;
    respawnHome = false;
    bool titleCreated = false;
    int titleGameType = 0;
    if (arrivingByPortal) { worldName = portalWorld; portalWorld.clear(); }
    else {
        u64 last = osGetTime();
        float acc = 0;
        int frames = 0;
        while (true) {
            if (!aptMainLoop()) { appRunning = false; break; }
            hidScanInput();
            touchPosition t;
            hidTouchRead(&t);
            u64 now = osGetTime();
            acc += (now - last) / 1000.0f;
            last = now;
            if (acc > 0.25f) acc = 0.25f;
            while (acc >= 0.05f) { acc -= 0.05f; title.tick(); sound::tick(); }
            TitleScreen::Result r = title.input(hidKeysDown(), t);
            if (autotestOn && ++frames == 400) { r = TitleScreen::PLAY; }
            if (r == TitleScreen::QUIT) { appRunning = false; break; }
            if (r == TitleScreen::PLAY) {
                worldName = autotestOn ? (autotestNether ? "World6" : autotestNew ? "World5" : "World1") : title.world();
                titleCreated = title.createdNew();
                titleGameType = title.gameType();
                break;
            }
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            renderer.beginFrame();
            title.draw(acc / 0.05f);
            C3D_FrameEnd(0);
        }
    }
    if (!appRunning) break;
    {
    // ================================================================ playing a world
    dbg("world: '%s'", worldName.c_str());
#ifdef __3DS__
    dbg("memory: heap used %lu of %lu KB, linear free %lu KB", (unsigned long)(mallinfo().uordblks / 1024),
        (unsigned long)(__ctru_heap_size / 1024), (unsigned long)(linearSpaceFree() / 1024));
#endif
    // ---- open the world ----
    SaveHandler* save = nullptr;
    LevelInfo info;
    std::string worldDir;
    bool brandNew = false;
    if (!worldName.empty() && (autotestOn ? autotestNew : titleCreated)) {
        // GuiCreateWorld: a random seed; the spawn is found once the world exists (below)
        worldDir = std::string(SAVES_DIR) + "/" + worldName;
        SaveHandler fresh(worldDir, 0);
        LevelInfo ni;
        JavaRandom seedRand(int64_t(osGetTime()) * 1000003LL ^ int64_t(svcGetSystemTick()));
        ni.seed = autotestNew ? 12345 : seedRand.nextLong();
        ni.spawnX = 0; ni.spawnY = 64; ni.spawnZ = 0;
        brandNew = fresh.createLevel(ni, titleGameType);
        dbg("created %s seed %lld ok %d", worldName.c_str(), (long long)ni.seed, brandNew);
    }
    if (!worldName.empty()) {
        worldDir = std::string(SAVES_DIR) + "/" + worldName;
        save = new SaveHandler(worldDir, 0);
        if (!save->readLevel(info)) { delete save; save = nullptr; worldDir.clear(); }
        else if (autotestNether && brandNew) { // the Nether self-test: straight there
            info.dimension = -1;
            delete save;
            save = new SaveHandler(worldDir, -1);
            save->readLevel(info);
            info.dimension = -1;
            brandNew = false; // (no overworld spawn search)
        } else if (info.dimension == -1) { // standing in the Nether
            delete save;
            save = new SaveHandler(worldDir, -1);
            save->readLevel(info);
        }
    }
    dbg("level read: save=%d dim=%d player %.1f %.1f %.1f", save != nullptr, info.dimension, info.playerX, info.playerY, info.playerZ);
    bosses::open(save ? worldDir : std::string()); // AxeolonBosses: before anything generates
    World world(save, info, save == nullptr);
    ItemEntities itemEntities; // before anything loads chunks: they bring their dropped items
    Mobs mobs;                 // ... and their mobs
    Paintings paintings;       // ... and the paintings on their walls
    Vehicles vehicles;         // ... and the boats moored there, and the minecarts
    static Vehicles* cartOwner;
    cartOwner = &vehicles;
    mobs.cartSeat = [&](int id, double& x, double& y, double& z) { // a mob in a minecart
        Vehicles::Cart* k = vehicles.cartById(id);
        if (!k) return false;
        x = k->x;
        y = k->y - double(0.3f);
        z = k->z;
        return true;
    };
    containers::saveCart = [](int id, const std::vector<ItemStack>& items) { cartOwner->saveCartItems(id, items); };
    world.rand.setSeed(int64_t(osGetTime()) ^ int64_t(svcGetSystemTick())); // new Random()
    if (brandNew && save) {
        u64 t0 = osGetTime();
        world.findSpawn();
        info = world.info;
        save->writeLevel(world.info);
        dbg("spawn %d %d %d after %d ms", info.spawnX, info.spawnY, info.spawnZ, int(osGetTime() - t0));
    }
    world.onBlocksDirty = worldDirty;
    world.onSound = [](const char* n, double x, double y, double z, float v, float p) { sound::play(n, x, y, z, v, p); };
    world.updateLCG = world.rand.nextInt();
    world.onChunkLoaded = [&](Chunk& c) { itemEntities.chunkLoaded(c); mobs.chunkLoaded(c); paintings.chunkLoaded(c); vehicles.chunkLoaded(c); };
    world.beforeSave = [&](Chunk& c) { itemEntities.writeToChunk(c); mobs.writeToChunk(c); vehicles.writeToChunk(c); };
    world.onUnload = [&](int cx, int cz) { itemEntities.chunkUnloaded(cx, cz); mobs.chunkUnloaded(cx, cz); paintings.chunkUnloaded(cx, cz); vehicles.chunkUnloaded(cx, cz); };
    world.onSpawnEntity = [&](nbt::TagPtr t) { mobs.adopt(std::move(t)); };
    world.onFallingBlock = [&](int x, int y, int z, int id) { mobs.fallingBlock(x, y, z, id); };
    world.onBlockAnim = [&](int kind, int x, int y, int z, const char* name) { BlockModels::play(world, kind, x, y, z, name); };
    world.onDoorSwing = [&](int x, int y, int z, int meta) { BlockModels::door(world, x, y, z, meta); };
    world.onParticle = [&](int type, double x, double y, double z, double a, double b, double c) {
        if (!gParticles) return;
        if (type == 1) gParticles->spawn(Particles::SMOKE, x, y, z, a, b, c);
        else if (type == 100) gParticles->spawn(Particles::NOTE, x, y, z, a, b, c);
    };
    GenFiber genFiber; // chunk loading/generation in small slices between frames
    TerrainWorker terrainWorker;
    if (world.generates() && save) {
        bool ok = terrainWorker.start(info.seed, worldDir, world.generator()->manager->overrides, info.dimension);
        dbg("terrain worker on core 1: %d", ok);
        world.pregenerated = [&](int cx, int cz, uint8_t* out) {
            if (terrainWorker.take(cx, cz, out)) return true;
            if (!genFiber.onFiber()) return false; // the loading screen: just make it here
            // on the fiber: let core 1 make it while the game keeps drawing
            terrainWorker.prioritize(cx, cz);
            for (int i = 0; i < 120; i++) {
                genFiber.yieldNow();
                if (terrainWorker.take(cx, cz, out)) return true;
            }
            return false;
        };
    }
    Camera cam;
    Inventory inv;
    AlmanacMap almanacMap(world, worldDir.empty() ? "" : worldDir + (info.dimension == -1 ? "/axeolon_map_nether" : "/axeolon_map"));
    bool creative = info.gameType == 1;
    inv.creative = creative;
    struct Dig { bool active = false; int x = 0, y = 0, z = 0, id = 0, delay = 0; float damage = 0; int hits = 0, side = 1; } dig;
    BottomScreen bottom;

    bool ok = bottom.init(renderer, cam, inv, almanacMap);
    dbg("bottom init %d", ok);
    if (!ok) {
        while (aptMainLoop()) {
            hidScanInput();
            if (hidKeysDown() & KEY_START) break;
            gspWaitForVBlank();
        }
        C2D_Fini(); C3D_Fini(); romfsExit(); gfxExit();
        return 0;
    }

    loadPlayer(save, info, cam, inv, almanacMap);
    Player player;
    if (info.hasPlayer) // exact saved doubles, not the camera's floats
        player.place(info.playerX, info.playerY, info.playerZ, info.playerYaw, info.playerPitch);
    else {
        // EntityPlayer.preparePlayerToSpawn: at the spawn, moved up out of the ground
        world.setWindow(info.spawnX >> 4, info.spawnZ >> 4);
        double feet = info.spawnY;
        int fx = info.spawnX, fz = info.spawnZ;
        while (feet < 255 && (world.blockAt(fx, int(feet), fz) != 0 || world.blockAt(fx, int(feet) + 1, fz) != 0)) feet += 1.0;
        cam.y = float(feet + 1.62);
        player.place(cam.x, cam.y, cam.z, (cam.yaw - PI) * 180.0f / PI, cam.pitch * 180.0f / PI);
    }
    player.inv = &inv;
    player.invulnerable = creative;
    if (nbt::Tag* p = save ? save->player() : nullptr; p && info.hasPlayer) {
        // EntityLiving / Entity / EntityPlayer.readEntityFromNBT
        player.health = p->get("Health") ? p->getShort("Health") : 10;
        player.prevHealth = player.health;
        player.hurtTime = p->getShort("HurtTime");
        player.deathTime = p->getShort("DeathTime");
        player.attackTime = p->getShort("AttackTime");
        player.fallDistance = p->getFloat("FallDistance");
        player.fire = p->getShort("Fire");
        player.air = p->get("Air") ? p->getShort("Air") : Player::MAX_AIR;
        player.mana = p->get("Mana") ? p->getShort("Mana") : Player::MAX_MANA;
    }
    Hud hud;
    hud.load(renderer);
    FirstPerson firstPerson;
    Particles particles;
    gParticles = &particles;
    Signs signs;
    int hudCounter = 0;     // GuiIngame.updateCounter
    bool wasDead = player.dead();
    int gameOverSel = 0;
    bool paused = false;     // GuiIngameMenu (START)
    bool spellPicker = false; // GuiSpellPicker (hold the use button with a spell book)
    // the how-to-play hints
    double tutWalked = 0, tutPrevX = 0, tutPrevZ = 0;
    float tutTurned = 0, tutPrevYaw = 0, tutFade = 0;
    int tutTicks = 0, tutShown = -1;
    bool tutBroke = false, tutPlaced = false, tutJumped = false;
    int pickerCursor = 0, bookHold = 0;
    bool bookWasDown = false;
    int pauseSel = 0;
    bool chatRequest = false; // GuiChat: type a line or a /command
    int savingFrames = 0;    // the saving indicator stays up a moment after a save
    int autosaveTicks = 0;
    bool flying = false;
    // PlayerControllerSP.sendBlockRemoved + Block.harvestBlock
    auto earn = [&](const std::string& id) { (void)id; }; // NSSS (Alpha): no achievements
    static Mobs* plateMobs;
    static ItemEntities* plateItems;
    static Player* platePlayer;
    plateMobs = &mobs;
    plateItems = &itemEntities;
    platePlayer = &player;
    world.onDispense = [&](int x, int y, int z) { // BlockDispenser.dispenseItem
        static JavaRandom dispRand(0xD15BULL); // TileEntityDispenser.dispenserRandom
        int meta = world.getBlockMetadata(x, y, z);
        float fx = 0, fz = 0;
        if (meta == 3) fz = 1; else if (meta == 2) fz = -1; else if (meta == 5) fx = 1; else fx = -1;
        // getRandomStackFromInventory: a random non-empty slot gives up one
        ItemStack st;
        if (nbt::Tag* te = world.tileEntityAt(x, y, z))
            if (nbt::Tag* list = te->getList("Items")) {
                int pick = -1, n = 1;
                for (size_t i = 0; i < list->items.size(); ++i)
                    if (list->items[i]->getByte("Count") > 0 && dispRand.nextInt(n) == 0) { pick = int(i); ++n; }
                if (pick >= 0) {
                    nbt::Tag& t = *list->items[pick];
                    st.id = uint16_t(t.getShort("id"));
                    st.count = 1;
                    st.damage = t.getShort("Damage");
                    int left = t.getByte("Count") - 1;
                    if (left <= 0) list->items.erase(list->items.begin() + pick);
                    else t.setByte("Count", int8_t(left));
                }
            }
        double ox = x + fx * 0.5 + 0.5, oy = y + 0.5, oz = z + fz * 0.5 + 0.5;
        if (st.empty()) { sound::play("random.click", x, y, z, 1.0f, 1.2f); return; }
        if (st.id == 351 && st.damage == 4) { // Axeolon: lapis fires a homing magic bolt
            mobs.castBoltAt(Mobs::MISSILE, ox + fx * 0.2, oy, oz + fz * 0.2, fx, 0.0, fz);
            sound::play("random.bow", x, y, z, 1.0f, 1.6f);
        } else if (st.id == 262 || st.id == 344 || st.id == 332) {
            mobs.shootFrom(ox, oy, oz, fx, double(0.1f), fz, 1.1f, 6.0f,
                           st.id == 262 ? Mobs::T_ARROW : st.id == 344 ? Mobs::T_EGG : Mobs::T_SNOWBALL);
            sound::play("random.bow", x, y, z, 1.0f, 1.2f);
        } else {
            double v = dispRand.nextDouble() * 0.1 + 0.2;
            itemEntities.spawnMoving(ox, oy - 0.3, oz, st, fx * v + dispRand.nextGaussian() * 0.0075 * 6.0,
                                     double(0.2f) + dispRand.nextGaussian() * 0.0075 * 6.0,
                                     fz * v + dispRand.nextGaussian() * 0.0075 * 6.0, 10);
            sound::play("random.click", x, y, z, 1.0f, 1.0f);
        }
        if (gParticles)
            for (int i = 0; i < 10; ++i) {
                double k = dispRand.nextDouble() * 0.2 + 0.01;
                gParticles->spawn(Particles::SMOKE, ox + fx * 0.01 + (dispRand.nextDouble() - 0.5) * fz * 0.5,
                                  oy + (dispRand.nextDouble() - 0.5) * 0.5, oz + fz * 0.01 + (dispRand.nextDouble() - 0.5) * fx * 0.5,
                                  fx * k + dispRand.nextGaussian() * 0.01, -0.03 + dispRand.nextGaussian() * 0.01,
                                  fz * k + dispRand.nextGaussian() * 0.01);
            }
    };
    world.onLaunch = [&](const AABB& box, double mx, double my, double mz) { // BlockPistonBase.launch
        if (box.intersects(player.boundingBox)) {
            player.motionX += mx;
            player.motionY += my;
            player.motionZ += mz;
            if (my > 0) player.onGround = false;
        }
        mobs.launch(box, mx, my, mz);
        itemEntities.launch(box, mx, my, mz);
    };
    Redstone::anythingOn = [](const AABB& box, bool livingOnly) { // BlockPressurePlate: who's on it
        if (!platePlayer->dead() && platePlayer->boundingBox.intersects(box)) return true;
        if (plateMobs->anyIn(box)) return true;
        return !livingOnly && plateItems->anyIn(box);
    };
    static Mobs* tntMobs;
    tntMobs = &mobs;
    Blocks::onTntIgnited = [](int x, int y, int z) { // fire reached TNT
        tntMobs->primeTnt(x, y, z, false);
        sound::play("random.fuse", x + 0.5, y + 0.5, z + 0.5, 1.0f, 1.0f);
    };
    // ---- Axeolon spawn crystals: placed by the Spawn charm, they move your spawn ----
    struct Crystal { int x, y, z; };
    std::vector<Crystal> crystals;
    std::unordered_map<int64_t, int64_t> crystalSetAt; // when each was placed (the "activate" burst)
    auto crystalKey = [](int x, int y, int z) { return (int64_t(x) & 0xFFFFFF) << 32 | (int64_t(z) & 0xFFFFFF) << 8 | (y & 0xFF); };
    auto crystalActive = [&](int x, int y, int z) { // BlockSpawnCrystal.isActive
        return world.info.spawnX == x && world.info.spawnY == y + 1 && world.info.spawnZ == z;
    };
    auto refreshCrystals = [&]() {
        crystals.clear();
        world.forEachChunk([&](Chunk& c) {
            if (!c.level) return;
            if (nbt::Tag* list = c.level->getList("TileEntities"))
                for (auto& t : list->items)
                    if (t && t->getString("id") == "AxeolonSpawnCrystal") {
                        int x = t->getInt("x"), y = t->getInt("y"), z = t->getInt("z");
                        if (world.getBlockId(x, y, z) == 143) crystals.push_back({x, y, z});
                    }
        });
    };
    auto placeCrystal = [&](int x, int y, int z, int side) -> bool { // ItemSpawnCharm.onItemUse
        if (side == 0) --y;
        if (side == 1) ++y;
        if (side == 2) --z;
        if (side == 3) ++z;
        if (side == 4) --x;
        if (side == 5) ++x;
        if (!Blocks::canPlaceBlockAt(world, 143, x, y, z) || world.getBlockId(x, y + 1, z) != 0 || world.getBlockId(x, y + 2, z) != 0) {
            hud.addChat("The crystal needs two blocks of open air above it.");
            return true;
        }
        if (!world.setBlockWithNotify(x, y, z, 143)) return true;
        if (!bosses::hasInt("spawn.origX")) { // AxeolonSpawnCrystals.rememberOriginal
            bosses::setInt("spawn.origX", world.info.spawnX);
            bosses::setInt("spawn.origY", world.info.spawnY);
            bosses::setInt("spawn.origZ", world.info.spawnZ);
        }
        world.info.spawnX = x;
        world.info.spawnY = y + 1;
        world.info.spawnZ = z;
        crystalSetAt[crystalKey(x, y, z)] = world.info.time;
        sound::play("note.harp", x + 0.5, y + 1.5, z + 0.5, 1.0f, 1.5f);
        sound::play("note.harp", x + 0.5, y + 1.5, z + 0.5, 0.8f, 2.0f);
        sound::play(BS[143].step, x + 0.5, y + 0.5, z + 0.5, 1.0f, 0.8f);
        hud.addChat("Your spawn is set at the crystal.");
        if (!creative) inv.consumeHeld();
        refreshCrystals();
        return true;
    };
    auto ejectRecord = [&](int x, int y, int z) { // BlockJukeBox.ejectRecord
        int m = world.getBlockMetadata(x, y, z);
        if (m <= 0) return;
        sound::playRecord("", x, y, z);
        mobs.jukebox(x, y, z, false);
        world.setBlockMetadataWithNotify(x, y, z, 0);
        const float k = 0.7f;
        double ox = double(world.rand.nextFloat() * k) + double(1.0f - k) * 0.5;
        double oy = double(world.rand.nextFloat() * k) + double(1.0f - k) * 0.2 + 0.6;
        double oz = double(world.rand.nextFloat() * k) + double(1.0f - k) * 0.5;
        ItemStack disc;
        disc.id = uint16_t(2256 + m - 1);
        disc.count = 1;
        itemEntities.spawn(x + ox, y + oy, z + oz, disc, 10);
    };
    auto eatCake = [&](int x, int y, int z) { // BlockCake.func_21029_c: a slice heals 3 when you're hurt
        if (player.health >= 20 || creative) return;
        player.heal(3);
        int m = world.getBlockMetadata(x, y, z) + 1;
        if (m >= 6) world.setBlockWithNotify(x, y, z, 0);
        else world.setBlockMetadataWithNotify(x, y, z, m);
        rebuild.markBlock(x, y, z);
    };
    auto breakBlock = [&](int x, int y, int z) {
        int id = world.getBlockId(x, y, z);
        if (id == 0 || id == 7) return; // bedrock
        tutBroke = true;
        if (id == 84) ejectRecord(x, y, z); // a jukebox gives up its record as it breaks
        if (id == 143 && crystalActive(x, y, z)) { // BlockSpawnCrystal.onBlockRemoval: the spawn goes back
            if (bosses::hasInt("spawn.origX")) {
                world.info.spawnX = bosses::getInt("spawn.origX");
                world.info.spawnY = bosses::getInt("spawn.origY");
                world.info.spawnZ = bosses::getInt("spawn.origZ");
            }
            hud.addChat("The spawn crystal shattered. You'll respawn where the world began.");
            sound::play("random.glass", x + 0.5, y + 1.5, z + 0.5, 1.0f, 0.8f);
        }
        int meta = world.getBlockMetadata(x, y, z);
        ItemStack& h = inv.held();
        int heldId = h.empty() ? 0 : h.id;
        bool harvest = survival::canHarvest(heldId, id);
        particles.blockBroken(world, x, y, z, id, meta);
        // BlockIce.collecting: a pickaxe takes the ice whole instead of leaving water
        bool pickaxe = heldId == 257 || heldId == 270 || heldId == 274 || heldId == 278 || heldId == 285;
        Blocks::iceCollecting = id == 79 && pickaxe && !creative;
        world.setBlockAndMetadata(x, y, z, 0, 0);
        Blocks::iceCollecting = false;
        if (id == 46) { // BlockTNT.onBlockDestroyedByPlayer: lit, not dropped
            mobs.primeTnt(x, y, z, false);
            sound::play("random.fuse", x + 0.5, y + 0.5, z + 0.5, 1.0f, 1.0f);
            return;
        }
        sound::play(BS[id].brk, x + 0.5, y + 0.5, z + 0.5, (BS[id].volume + 1.0f) / 2.0f, BS[id].pitch * 0.8f);
        if (isDoorId(id)) { // the other half goes too
            int oy = (meta & 8) ? y - 1 : y + 1;
            if (world.getBlockId(x, oy, z) == id) world.setBlockAndMetadata(x, oy, z, 0, 0);
            rebuild.markBlock(x, oy, z);
            if (meta & 8) meta = world.getBlockMetadata(x, oy, z); // drop from the bottom half
        }
        if (!creative) {
            // ItemTool / ItemSword.hitBlock: wear (instant-break blocks don't wear tools, like modern)
            if (heldId >= 256 && survival::maxDamage(heldId) > 0 && survival::HARDNESS[id] > 0.0f &&
                survival::toolSpeed(heldId, 1) != 0.0f) {
                bool tool = false;
                for (int i = 0; i < survival::TOOL_ENTRY_COUNT; ++i)
                    if (survival::TOOL_ENTRIES[i].item == heldId) { tool = true; break; }
                bool sword = heldId == 267 || heldId == 268 || heldId == 272 || heldId == 276 || heldId == 283;
                if (tool) {
                    h.damage += sword ? 2 : 1;
                    if (h.damage > survival::maxDamage(heldId)) { // renderBrokenItemStack
                        h.clear();
                        sound::play("random.break", player.posX, player.posY - 1.62, player.posZ, 0.8f,
                                    0.8f + world.rand.nextFloat() * 0.4f);
                    }
                }
            }
            ItemStack* silk = player.findCharm(1014);
            if (id == 79 && pickaxe && !silk) // PlayerControllerSP: pickaxes collect ice
                world.drops.push_back(World::Drop{x + 0.5, y + 0.5, z + 0.5, 79, 1, 0});
            if (harvest) {
                // AxeolonCharms.harvest: Silk Touch and Lucky Mining decide the drops
                bool ore = id == 14 || id == 15 || id == 16 || id == 21 || id == 56 || id == 73 || id == 74 || (id >= 217 && id <= 223);
                bool silkable = ore || id == 1 || id == 2 || id == 20 || id == 79 || id == 18 || id == 89 || id == 47 || id == 82 || id == 80;
                if (silk && silkable) {
                    int dropId = id == 74 ? 73 : id == 223 ? 220 : id;
                    int leaf = (meta & 3) | ((meta & 8) >> 1); // BlockLeaves.leafType
                    int dropMeta = id == 18 ? (((leaf & 3) | ((leaf & 4) << 1)) & ~4) : (id >= 217 && id <= 223) ? (meta & 7) : 0;
                    world.drops.push_back(World::Drop{x + 0.5, y + 0.5, z + 0.5, dropId, 1, dropMeta});
                    player.wearCharm(silk, 1);
                } else if (id == 78) { // BlockSnow.harvestBlock: one snowball
                    world.drops.push_back(World::Drop{x + 0.5, y + 0.5, z + 0.5, 332, 1, 0});
                } else {
                    Blocks::dropBlockAsItem(world, x, y, z, id, meta);
                    if (ore) {
                        static const int LUCKY_DOUBLE[5] = {10, 20, 30, 35, 40}, LUCKY_TRIPLE[5] = {0, 0, 0, 10, 20};
                        for (int tier = 4; tier >= 0; --tier) {
                            ItemStack* lucky = player.findCharm(1009 + tier);
                            if (!lucky) continue;
                            int roll = world.rand.nextInt(100);
                            int extra = roll < LUCKY_TRIPLE[tier] ? 2 : (roll < LUCKY_TRIPLE[tier] + LUCKY_DOUBLE[tier] ? 1 : 0);
                            for (int i = 0; i < extra; ++i) Blocks::dropBlockAsItem(world, x, y, z, id, meta);
                            player.wearCharm(lucky, 1);
                            break;
                        }
                    }
                }
            }
        }
        rebuild.markBlock(x, y, z);
        almanacMap.invalidate();
        bottom.markDirty();
    };
    // Automatic movement test (sdmc:/3ds/nsss3ds/autotest.txt present): replays
    // tools/java/SimPlayer's input script and logs every tick, to compare with Java.
    int autotest = autotestOn ? 0 : -1;
    dbg("player loaded%s", autotest == 0 ? " (autotest)" : "");
    if (!save) {
        // built-in test world: something to build with
        const uint8_t hotbar[9] = {2, 3, 1, 4, 5, 17, 18, 12, 13};
        for (int i = 0; i < 9; i++) { inv.main[i].id = hotbar[i]; inv.main[i].count = 64; }
    }

    // ---- Minecraft.usePortal: the Teleporter finds (or builds) the portal on this side ----
    if (arrivingRespawn) { // preparePlayerToSpawn
        world.setWindow(world.info.spawnX >> 4, world.info.spawnZ >> 4);
        double feet = world.info.spawnY;
        int fx = world.info.spawnX, fz = world.info.spawnZ;
        while (feet < 255 && (world.blockAt(fx, int(feet), fz) != 0 || world.blockAt(fx, int(feet) + 1, fz) != 0)) feet += 1.0;
        player.place(fx + 0.5, feet + 1.62, fz + 0.5, player.rotationYaw, 0);
        cam.x = float(player.posX); cam.y = float(player.posY); cam.z = float(player.posZ);
    }
    if (arrivingByPortal && world.generates()) {
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        renderer.beginFrame();
        title.drawLoading(info.dimension == -1 ? "Entering the Nether" : "Leaving the Nether", "Building terrain", 0.0f);
        C3D_FrameEnd(0);
        world.setWindow(int(std::floor(player.posX)) >> 4, int(std::floor(player.posZ)) >> 4);
        u64 t0 = osGetTime();
        teleporter::place(world, player);
        while (world.updatingLighting()) {}
        dbg("teleporter: %.1f %.1f %.1f in %d ms", player.posX, player.posY, player.posZ, int(osGetTime() - t0));
        player.portalCooldown = 10; // field_9373_b: standing in the portal doesn't send you straight back
        cam.x = float(player.posX); cam.y = float(player.posY); cam.z = float(player.posZ);
    }
    // ---- Minecraft.func_6255_d: build the area around the player before play starts ----
    {
        int pcx = int(std::floor(player.posX)) >> 4, pcz = int(std::floor(player.posZ)) >> 4;
        const int R = LOAD_RADIUS + 1;
        const int total = (2 * R + 1) * (2 * R + 1);
        const char* heading = arrivingByPortal ? (info.dimension == -1 ? "Entering the Nether" : "Leaving the Nether")
                              : brandNew ? "Generating level" : "Loading level";
        u64 t0 = osGetTime();
        int done = 0;
        for (int ring = 0; ring <= R && aptMainLoop(); ring++)
            for (int dx = -ring; dx <= ring; dx++)
                for (int dz = -ring; dz <= ring; dz++) {
                    if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                    world.setWindow(pcx, pcz);
                    world.provideChunk(pcx + dx, pcz + dz);
                    while (world.updatingLighting()) {}
                    done++;
                    if (done % 3 == 0 || done == total) {
                        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
                        renderer.beginFrame();
                        title.drawLoading(heading, "Building terrain", float(done) / total);
                        C3D_FrameEnd(0);
                    }
                }
        world.justLoaded.clear();
        dbg("built %d chunks in %d ms", done, int(osGetTime() - t0));
    }
    JavaRandom deathRand{int64_t(osGetTime())};
    const bool nether = info.dimension == -1;
    float fogLightPrev = 1.0f, fogLight = 1.0f; // EntityRenderer q / r: fog darkens where it's dark
    double skyTemperature = 0.5;
    int cloudTicks = 0; // RenderGlobal.field_1435_x
    int skyTempX = INT_MIN, skyTempZ = INT_MIN;
    auto renderDistanceOption = [&]() { // Beta's far/normal/short/tiny (0..3) from the blocks drawn
        float o = std::log2(256.0f / renderer.renderDistance);
        return o < 0.0f ? 0.0f : o > 3.0f ? 3.0f : o;
    };
    world.skylightSubtracted = nether ? 0 : atmosphere::skylightSubtracted(atmosphere::celestialAngle(info.time, 1.0f, false));
    bool fiberOn = world.generates() && genFiber.start(world);
    dbg("gen fiber: %d", fiberOn);
    std::unordered_set<int64_t> shown; // chunk columns the renderer is drawing
    auto colKey = [](int cx, int cz) { return int64_t(cx) << 32 | uint32_t(cz); };
    MeshData scratch;
    Frustum frustum;
    FrameStats stats;
    u64 lastTick = osGetTime();
    const u64 playStart = lastTick;
    bool autotestDusk = false;
    float fpsTimer = 0, breakTimer = 0, placeTimer = 0, tickAcc = 0;
    int frames = 0;
    std::vector<axm::ModelVertex> animQuads;
    std::vector<BlockModels::Batch> animBatches;

    auto saveGame = [&]() {
        if (!world.canSave()) return;
        dbg("save: containers (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
        if (inv.containerKind != Inventory::NONE && inv.containerDirty) containers::save(world, inv);
        dbg("save: world (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
        world.saveAll();                                   // writes level.dat (creates Player if needed)
        dbg("save: player (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
        storePlayer(save, world, player, inv, almanacMap); // position, inventory, battle slots, waypoints
        dbg("save: world again (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
        world.saveAll();
        dbg("save: map (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
        almanacMap.save();
        dbg("save: done (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
    };
    int frameNo = 0;
    bool leftToTitle = false;
    bool portalTravel = false;
    while (aptMainLoop()) {
        frameNo++;
        static int traceUntil = 0;
        bool trace = frameNo <= 3 || frameNo == 60 || frameNo == 600 || frameNo <= traceUntil;
        if (trace) dbg("frame %d", frameNo);
#ifdef __3DS__
        if (frameNo % 100 == 0) // Old 3DS has 64 MB: watch it
            dbg("memory (frame %d): heap used %lu of %lu KB, linear free %lu KB", frameNo, (unsigned long)(mallinfo().uordblks / 1024),
                (unsigned long)(__ctru_heap_size / 1024), (unsigned long)(linearSpaceFree() / 1024));
#endif
        hidScanInput();
        u32 down = hidKeysDown(), held = hidKeysHeld();
        circlePosition cstick = {0, 0};
        if (irrstOk) { // Circle Pad Pro on an Old 3DS
            irrstScanInput();
            static u32 lastIr = 0;
            u32 ir = irrstKeysHeld();
            down |= ir & ~lastIr;
            held |= ir;
            lastIr = ir;
            irrstCstickRead(&cstick);
            if (!xboxLayout && (held & (KEY_ZL | KEY_ZR) || std::abs(cstick.dx) > 30 || std::abs(cstick.dy) > 30)) {
                xboxLayout = true;
                zButtons = true; // the Circle Pad Pro has ZL / ZR (and a second R) too
                dbg("Circle Pad Pro found: console layout");
            }
        } else {
            hidCstickRead(&cstick);
        }
        touchPosition touch;
        hidTouchRead(&touch);

        u64 now = osGetTime();
        float dt = (now - lastTick) / 1000.0f;
        lastTick = now;
        if (dt > 0.1f) dt = 0.1f;

        // ---- camera ----
        circlePosition pad;
        hidCircleRead(&pad);
        // ---- the game menu: START pauses (nothing moves while it's up) ----
        bool saveNow = false;
        if ((down & KEY_START) && !player.dead()) {
            paused = !paused;
            pauseSel = 0;
            down &= ~KEY_START;
            sound::playUi("random.click", 1.0f, 1.0f);
        }
        if (paused) {
            if (down & KEY_DUP) pauseSel = (pauseSel + 3) % 4;
            if (down & KEY_DDOWN) pauseSel = (pauseSel + 1) % 4;
            if (down & KEY_B) paused = false;
            if (down & KEY_A) {
                sound::playUi("random.click", 1.0f, 1.0f);
                if (pauseSel == 0) paused = false;
                else if (pauseSel == 1) saveNow = true;
                else if (pauseSel == 2) { paused = false; chatRequest = true; } // GuiChat
                else { leftToTitle = true; break; }
            }
            down = 0;
            held &= KEY_TOUCH;
            pad.dx = pad.dy = 0;
            cstick.dx = cstick.dy = 0;
        }
        if (mobs.introPlaying()) { // the boss intro: the camera isn't yours for a moment
            down &= KEY_START;
            held &= KEY_TOUCH;
            pad.dx = pad.dy = 0;
            cstick.dx = cstick.dy = 0;
            spellPicker = false;
        }
        if (spellPicker) { // GuiSpellPicker doesn't pause the game, but it takes the controls
            ItemStack& book = inv.held();
            int n = book.id == spells::BOOK ? int(spells::spellsIn(book).size()) : 0;
            if (n == 0 || player.dead()) spellPicker = false;
            else {
                if (down & KEY_DRIGHT) pickerCursor = (pickerCursor + 1) % n;
                if (down & KEY_DLEFT) pickerCursor = (pickerCursor + n - 1) % n;
                if ((down & KEY_DDOWN) && pickerCursor + 9 < n) pickerCursor += 9;
                if ((down & KEY_DUP) && pickerCursor >= 9) pickerCursor -= 9;
                if (down & KEY_A) {
                    spells::setSelected(book, pickerCursor);
                    sound::playUi("random.click", 1.0f, 1.0f);
                    spellPicker = false;
                    bottom.markDirty();
                }
                if (down & (KEY_B | KEY_START)) spellPicker = false;
            }
            down = 0;
            held &= KEY_TOUCH;
            pad.dx = pad.dy = 0;
            cstick.dx = cstick.dy = 0;
        }
        auto axis = [](int v, float range) { return std::fabs((float)v) > 15.0f ? v / range : 0.0f; };
        float lookX = axis(cstick.dx, 146.0f), lookY = axis(cstick.dy, 146.0f);
        bool jumpHeld, sneakHeld;
        if (xboxLayout) {
            jumpHeld = held & (KEY_A | KEY_DUP);
            sneakHeld = held & (KEY_B | KEY_DDOWN);
            if (zButtons && (down & KEY_R)) { inv.scrollHotbar(-1); bottom.markDirty(); }
            if (zButtons && (down & KEY_L)) { inv.scrollHotbar(1); bottom.markDirty(); }
            if (down & KEY_X) bottom.setTab(bottom.tab() == BottomScreen::Tab::Items ? BottomScreen::Tab::Map : BottomScreen::Tab::Items);
        } else {
            if (held & KEY_Y) lookX = -1.0f;
            if (held & KEY_A) lookX = 1.0f;
            if (held & KEY_X) lookY = 1.0f;
            if (held & KEY_B) lookY = -1.0f;
            jumpHeld = held & KEY_DUP;
            sneakHeld = held & KEY_DDOWN;
        }
        float moveY = jumpHeld ? 1.0f : sneakHeld ? -1.0f : 0.0f;
        // Looking turns the player (rotationYaw/Pitch in degrees, like the mouse in Beta); the
        // camera follows the player, never the other way round
        const float LOOK_DEG = 2.4f * 180.0f / PI; // degrees per second at full deflection
        if (gOptions.invertY) lookY = -lookY;
        player.rotationYaw += lookX * LOOK_DEG * gOptions.lookScale() * dt;
        player.rotationPitch -= lookY * LOOK_DEG * gOptions.lookScale() * dt;
        if (player.rotationPitch < -90.0f) player.rotationPitch = -90.0f;
        if (player.rotationPitch > 90.0f) player.rotationPitch = 90.0f;
        cam.yaw = player.rotationYaw * PI / 180.0f + PI;
        cam.pitch = player.rotationPitch * PI / 180.0f;
        if (flying) {
            cam.fast = true;
            cam.update(0, 0, axis(pad.dx, 156.0f), axis(pad.dy, 156.0f), moveY, dt);
            player.place(cam.x, cam.y, cam.z, player.rotationYaw, player.rotationPitch);
        }

        if ((down & KEY_SELECT) && !creative && inv.tutorial >= 0 && inv.tutorial < 99) { // skip the how-to-play hints
            inv.tutorial = 99;
            sound::playUi("random.click", 1.0f, 1.0f);
        }
        if ((down & KEY_SELECT) && creative) { // creative flight
            flying = !flying;
            player.motionX = player.motionY = player.motionZ = 0;
            player.fallDistance = 0;
            bottom.markDirty();
        }
        if (down & KEY_DLEFT) { inv.scrollHotbar(1); bottom.markDirty(); }
        if (down & KEY_DRIGHT) { inv.scrollHotbar(-1); bottom.markDirty(); }

        // ---- stream chunks around the player ----
        {
            int pcx = int(std::floor(cam.x)) >> 4, pcz = int(std::floor(cam.z)) >> 4;
            viewCx = pcx;
            viewCz = pcz;
            // one ring past what's drawn, so the drawn edge has the neighbours populate needs
            // tell the other core which never-saved chunks are coming up, nearest first
            if (terrainWorker.running() && (frameNo % 8) == 0) {
                std::vector<std::pair<int, int>> want;
                int R = LOAD_RADIUS + 2;
                for (int ring = 0; ring <= R && want.size() < 8; ring++)
                    for (int dx = -ring; dx <= ring && want.size() < 8; dx++)
                        for (int dz = -ring; dz <= ring && want.size() < 8; dz++) {
                            if (std::max(std::abs(dx), std::abs(dz)) != ring) continue;
                            int cx = pcx + dx, cz = pcz + dz;
                            if (world.isChunkLoaded(cx, cz) || world.chunkOnDisk(cx, cz)) continue;
                            want.emplace_back(cx, cz);
                        }
                terrainWorker.want(want);
            }
            u64 genT0 = svcGetSystemTick();
            float unloadMs = 0;
            std::vector<std::pair<int, int>> loaded;
            if (fiberOn) {
                // between chunks: move the window, drop far chunks, queue the next ones
                if (!genFiber.busy()) {
                    world.setWindow(pcx, pcz);
                    u64 u0 = svcGetSystemTick();
                    world.unloadFar(pcx, pcz, LOAD_RADIUS + 2);
                    unloadMs = float(svcGetSystemTick() - u0) * 1000.0f / SYSCLOCK_ARM11;
                    genFiber.request(world.missingNear(pcx, pcz, LOAD_RADIUS + 1, 2));
                }
                // adaptive: less when the frame rate sags, more when there's room
                static float genBudget = GEN_BUDGET_MS;
                if (stats.fps > 0 && stats.fps < 24) genBudget = std::max(4.0f, genBudget - 0.05f);
                else if (stats.fps >= 30) genBudget = std::min(12.0f, genBudget + 0.05f);
                genFiber.run(genBudget);
                loaded.swap(world.justLoaded);
                loaded.erase(std::remove_if(loaded.begin(), loaded.end(), [&](const std::pair<int, int>& p) { return !world.isChunkLoaded(p.first, p.second); }), loaded.end());
            } else {
                loaded = world.updateLoadedArea(pcx, pcz, LOAD_RADIUS + 1, 1);
            }
            static int genLogs = 0;
            if (!loaded.empty() && genLogs < 60) {
                genLogs++;
                dbg("chunks +%d (%d,%d) in %.1f ms (unload %.1f), light queue %d, fps %d", int(loaded.size()), loaded[0].first,
                    loaded[0].second, float(svcGetSystemTick() - genT0) * 1000.0f / SYSCLOCK_ARM11, unloadMs,
                    int(world.lightQueueSize()), stats.fps);
            }
            for (auto& c : loaded) {
                // a new neighbour changes the edge faces (and maybe populated into) the shown ones
                for (int d = 0; d < 4; d++) {
                    int nx = c.first + (d == 0) - (d == 1), nz = c.second + (d == 2) - (d == 3);
                    if (shown.count(colKey(nx, nz))) rebuild.markColumn(nx, nz);
                }
            }
            // columns entering / leaving the drawn area
            for (int dx = -LOAD_RADIUS; dx <= LOAD_RADIUS; dx++)
                for (int dz = -LOAD_RADIUS; dz <= LOAD_RADIUS; dz++) {
                    int cx = pcx + dx, cz = pcz + dz;
                    if (shown.count(colKey(cx, cz)) || !world.isChunkLoaded(cx, cz)) continue;
                    shown.insert(colKey(cx, cz));
                    rebuild.markColumn(cx, cz);
                }
            for (auto it = shown.begin(); it != shown.end();) {
                int cx = int(int32_t(*it >> 32)), cz = int(int32_t(uint32_t(*it)));
                if (std::abs(cx - pcx) <= LOAD_RADIUS && std::abs(cz - pcz) <= LOAD_RADIUS && world.isChunkLoaded(cx, cz)) {
                    ++it;
                    continue;
                }
                renderer.dropColumn(cx, cz);
                rebuild.dropColumn(cx, cz);
                it = shown.erase(it);
            }
        }

        if (trace) dbg(" streamed, columns queued %d", int(rebuild.items.size()));
        if (autotestQuit && osGetTime() - playStart > 25000) { // save and quit to the title, then back in
            dbg("autotest: save and quit (visit %d, heap %lu KB)", ++autotestVisits, (unsigned long)(mallinfo().uordblks / 1024));
            leftToTitle = true;
            break;
        }
        // autotest: jump to sunset after 40 s so the last screenshots show the sky at dusk
        if (autotestOn && !autotestDusk && osGetTime() - playStart > 40000) {
            autotestDusk = true;
            world.info.time = (world.info.time / 24000 + 1) * 24000 - 11500;
        }
        // autotest in the Nether: after 20 s, the Pocket crafting list at a crafting table on the bottom screen
        static bool autotestCraft = false;
        if (autotestNether && !autotestCraft && osGetTime() - playStart > 20000) {
            autotestCraft = true;
            gOptions.craftStyle = 1;
            const int give[][3] = {{17, 6, 0}, {4, 20, 0}, {280, 8, 0}, {265, 7, 0}, {264, 3, 0}, {263, 4, 0}, {35, 3, 0}};
            for (auto& g : give) {
                ItemStack st;
                st.id = uint16_t(g[0]);
                st.count = g[1];
                st.damage = int16_t(g[2]);
                inv.addStack(st);
            }
            inv.openTable();
            bottom.setTab(BottomScreen::Tab::Items);
            bottom.markDirty();
        }
        // ---- game ticks (20 per second) ----
        if (!paused) tickAcc += dt;
        // autosave every five minutes of play (when no chunk is half made)
        if (!paused && ++autosaveTicks >= 60 * 300 && !genFiber.busy()) saveNow = true;
        int ticks = 0;
        while (tickAcc >= 0.05f && ticks < 5) {
            tickAcc -= 0.05f;
            ticks++;
            world.info.time++;
            texfx::tick([&](int tile, const uint8_t* rgba) { renderer.updateTerrainTile(tile, rgba); });
            { // AxeolonDynamicLights.tick: what's glowing now
                std::vector<dynlight::Emitter> glow;
                int held = inv.held().empty() ? 0 : inv.held().id;
                int lvl = dynlight::itemLight(held);
                if (ItemStack* off = inv.offhand(); off && !off->empty()) lvl = std::max(lvl, dynlight::itemLight(off->id));
                bool fancy = gOptions.dynamicLights == dynlight::FANCY;
                if (fancy && player.fire > 0) lvl = 15;
                if (lvl > 0 && !player.dead())
                    glow.push_back({&player, player.posX, player.posY + double(0.12f) * 0.5, player.posZ, lvl}); // posY + getEyeHeight / 2
                if (fancy) {
                    for (const ItemEntity& e : itemEntities.all())
                        if (int l = dynlight::itemLight(e.item.id)) glow.push_back({&e, e.x, e.y, e.z, l});
                    mobs.forEachBurning([&](const void* k, double x, double y, double z) { glow.push_back({k, x, y, z, 15}); });
                }
                dynlight::tick(gOptions.dynamicLights, glow, worldDirty);
            }
            {
                texfx::ItemView iv;
                iv.inWorld = true;
                iv.px = player.posX;
                iv.pz = player.posZ;
                iv.yaw = player.rotationYaw;
                iv.spawnX = world.info.spawnX;
                iv.spawnZ = world.info.spawnZ;
                iv.celestial = atmosphere::celestialAngle(world.info.time, 1.0f, nether);
                iv.nether = nether;
                C3D_Tex* itemsTex = renderer.texture("romfs:/textures/gui/items.rgba");
                texfx::tickItems(iv, [&](int tile, const uint8_t* rgba) { renderer.updateTile(itemsTex, tile, rgba); });
            }
            ++cloudTicks;
            // World.tick: skylightSubtracted follows the sun (the shader relights every section)
            if (!nether) world.skylightSubtracted = atmosphere::skylightSubtracted(atmosphere::celestialAngle(world.info.time, 1.0f, false));
            { // EntityRenderer.updateRenderer: the fog's brightness follows the light where you are
                fogLightPrev = fogLight;
                int lx = int(std::floor(player.posX)), ly = int(std::floor(player.posY)), lz = int(std::floor(player.posZ));
                int lv = (ly >= 0 && ly < 128) ? world.getLightValue(lx, ly, lz) : 15;
                float lb = ChunkMesher::LIGHT_BRIGHTNESS[lv < 0 ? 0 : lv > 15 ? 15 : lv];
                float v2 = (3.0f - renderDistanceOption()) / 3.0f;
                fogLight += (lb * (1.0f - v2) + v2 - fogLight) * 0.1f;
                if (lx != skyTempX || lz != skyTempZ) {
                    skyTempX = lx;
                    skyTempZ = lz;
                    if (world.generates() && world.generator()) skyTemperature = world.generator()->manager->rawTemperature(lx, lz);
                }
            }
            // scheduled block updates: flowing water, falling sand... (not while a chunk is half
            // generated: a tick could reach into an unloaded chunk and start generating it)
            if (!genFiber.busy()) {
                world.tickUpdates(false);
                world.tickBlocks(player.posX, player.posY, player.posZ);
            }
            if (!player.dead() && !flying) Redstone::entityTouching(world, player.boundingBox, true); // pressure plates
            if (chest.open) { // walked away: the lid comes down
                float dx = cam.x - (chest.x + 0.5f), dz = cam.z - (chest.z + 0.5f);
                if (dx * dx + dz * dz > 36.0f) chest.open = false;
            }
            if (!flying && world.isChunkLoaded(int(std::floor(player.posX)) >> 4, int(std::floor(player.posZ)) >> 4)) {
                // EntityPlayerSP input: Circle Pad forward/back and sideways (left is +strafe)
                auto clamp1 = [](float v) { return v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v; };
                PlayerInput in;
                in.forward = clamp1(axis(pad.dy, 156.0f));
                in.strafe = clamp1(-axis(pad.dx, 156.0f));
                in.jump = jumpHeld;
                in.sneak = sneakHeld;
                if (autotest >= 0 && frameNo > 300 && autotest < 240) {
                    int t = autotest++;
                    in = PlayerInput();
                    if (t < 40) in.forward = 1;
                    else if (t < 70) { in.forward = 1; player.rotationYaw += 6.0f; }
                    else if (t < 100) { in.forward = 1; in.jump = true; }
                    else if (t < 140) { in.strafe = 1; in.sneak = true; }
                    else if (t < 200) { in.forward = 1; player.rotationYaw -= 3.0f; in.jump = (t % 15) < 3; }
                    else { in.forward = -1; in.strafe = -1; }
                    player.tick(world, in);
                    dbg("T %d %.17g %.17g %.17g %.17g %.17g %.17g %d", t, player.posX, player.posY, player.posZ,
                        player.motionX, player.motionY, player.motionZ, player.onGround ? 1 : 0);
                } else if (vehicles.riding()) { // EntityBoat: the rider's walking pushes it; sneak to get out
                    bool wet = Physics::isMaterialInBB(world, player.boundingBox.expand(0.0, double(-0.4f), 0.0), false);
                    vehicles.setRiderInput(in.strafe, in.forward, player.rotationYaw, wet);
                    if (in.sneak) vehicles.dismount(player);
                } else if (mobs.riding()) { // EntityAxeolonCreature: the rider steers; sneak to hop off
                    Mobs::RiderInput ri;
                    ri.forward = in.forward;
                    ri.strafe = in.strafe;
                    ri.yaw = player.rotationYaw;
                    ri.jump = in.jump;
                    mobs.setRiderInput(ri);
                    if (in.sneak) mobs.dismount(player);
                } else if (autotest < 0 || frameNo > 300) {
                    player.tick(world, in);
                }
            }
            ++hudCounter;
            hud.tickChat();
            { // SpellBookInput.tick: a quick press casts, holding opens the picker
                const ItemStack& h = inv.held();
                bool holdingBook = h.id == spells::BOOK && !h.empty();
                bool useDown = !spellPicker && !(held & KEY_TOUCH) && !player.dead() &&
                               (held & (zButtons ? KEY_ZL : (KEY_L | KEY_ZL))) && holdingBook;
                if (useDown) {
                    if (++bookHold == 8) {
                        if (spells::spellsIn(h).empty()) hud.addChat("The book is empty.");
                        else {
                            spellPicker = true;
                            pickerCursor = std::max(0, spells::selectedIndex(h));
                        }
                    }
                } else {
                    if (bookWasDown && bookHold > 0 && bookHold < 8 && holdingBook) {
                        std::string msg;
                        if (spells::castSelected(inv.held(), world, player, mobs, creative, !genFiber.busy(), msg))
                            earn(progress::SPARK);
                        hud.addChat(msg);
                        almanacMap.invalidate();
                        bottom.markDirty();
                    }
                    bookHold = 0;
                }
                bookWasDown = useDown;
            }
            firstPerson.tick(inv, player.blocking);
            particles.tick(world);
            if (signs.needsRefresh() || hudCounter % 20 == 0) signs.refresh(world, player.posX, player.posZ, LOAD_RADIUS);
            if (hudCounter % 20 == 0) refreshCrystals();
            particles.ambient(world, player.posX, player.posY, player.posZ);
            if (dig.active && !creative && dig.delay == 0) particles.blockHit(world, dig.x, dig.y, dig.z, dig.side);
            if (dig.active && !creative) player.swingItem(); // digging keeps the arm going
            if (player.dead() && !wasDead) {
                if (vehicles.riding()) vehicles.dismount(player);
                if (mobs.riding()) mobs.dismount(player);
                // EntityPlayer.onDeath -> InventoryPlayer.dropAllItems: everything scatters
                auto scatter = [&](ItemStack& st) {
                    if (st.empty()) return;
                    float sp = deathRand.nextFloat() * 0.5f;
                    float ang = deathRand.nextFloat() * PI * 2.0f;
                    itemEntities.spawnMoving(player.posX, player.posY - double(0.3f), player.posZ, st,
                                             double(-MathHelper::sin(ang) * sp), double(0.2f),
                                             double(MathHelper::cos(ang) * sp), 40);
                    st.clear();
                };
                for (ItemStack& st : inv.main) scatter(st);
                for (ItemStack& st : inv.armour) scatter(st);
                for (ItemStack& st : inv.charms) scatter(st);
                if (inv.containerKind != Inventory::NONE) { containers::close(world, inv); chest.open = false; }
                if (inv.tableOpen) inv.closeTable();
                if (inv.cutterOpen) inv.closeStonecutter();
                for (ItemStack& st : inv.craft) scatter(st);
                scatter(inv.cursor);
                bottom.markDirty();
                gameOverSel = 0;
                dig.active = false;
            }
            wasDead = player.dead();
            for (const Player::SoundEvent& e : player.sounds)
                sound::play(e.name, player.posX, player.posY - double(Player::YOFFSET), player.posZ, e.volume, e.pitch);
            player.sounds.clear();
            for (const std::string& msg : player.messages) { hud.addChat(msg); bottom.markDirty(); }
            player.messages.clear();
            sound::tick();
            BlockModels::tick(world, chest.open);
            mobs.bossTick(world, player, creative);
            player.difficulty = gOptions.difficulty;
            player.peaceful = gOptions.difficulty == 0;
            mobs.tick(world, player, itemEntities, !genFiber.busy(), player.peaceful);
            paintings.tick(world, itemEntities);
            vehicles.tick(world, player, itemEntities, &mobs);
            mobs.tickBobber(world, player, !inv.held().empty() && inv.held().id == 346);
            if (vehicles.riding()) { // updateRiderPosition: sat in the boat
                double sx, sy, sz;
                if (vehicles.riderSeat(sx, sy, sz)) {
                    player.prevPosX = player.posX;
                    player.prevPosY = player.posY;
                    player.prevPosZ = player.posZ;
                    player.setPosition(sx, sy, sz);
                    player.motionX = player.motionY = player.motionZ = 0;
                    player.fallDistance = 0;
                } else {
                    vehicles.dismount(player); // it's gone
                }
            }
            if (mobs.riding()) { // the rider sits on its back
                double sx, sy, sz;
                if (mobs.riderSeat(sx, sy, sz)) {
                    player.prevPosX = player.posX;
                    player.prevPosY = player.posY;
                    player.prevPosZ = player.posZ;
                    player.setPosition(sx, sy, sz);
                    player.motionX = player.motionY = player.motionZ = 0;
                    player.fallDistance = 0;
                    player.onGround = true;
                } else {
                    mobs.dismount(player); // it's gone
                }
            }
            if (mobs.pickedUpSomething) { mobs.pickedUpSomething = false; bottom.markDirty(); }
            for (const std::string& a : mobs.earned) earn(a);
            mobs.earned.clear();
            if (!creative && inv.tutorial < 99 && !player.dead()) { // the how-to-play hints, one step at a time
                if (inv.tutorial < 0) inv.tutorial = 0;
                if (tutShown != inv.tutorial) {
                    tutShown = inv.tutorial;
                    tutTicks = 0;
                    tutWalked = 0;
                    tutTurned = 0;
                    tutPrevX = player.posX;
                    tutPrevZ = player.posZ;
                    tutPrevYaw = player.rotationYaw;
                    tutBroke = tutPlaced = tutJumped = false;
                }
                ++tutTicks;
                double wdx = player.posX - tutPrevX, wdz = player.posZ - tutPrevZ;
                tutWalked += std::sqrt(wdx * wdx + wdz * wdz);
                tutPrevX = player.posX;
                tutPrevZ = player.posZ;
                tutTurned += std::fabs(player.rotationYaw - tutPrevYaw);
                tutPrevYaw = player.rotationYaw;
                if (!player.onGround && player.motionY > 0.3) tutJumped = true;
                bool hasWood = false;
                for (int i = 0; i < Inventory::MAIN; ++i) {
                    int id = inv.main[i].empty() ? 0 : inv.main[i].id;
                    if (id == 5 || id == 58 || id == 280) hasWood = true;
                }
                bool done = false;
                switch (inv.tutorial) {
                case 0: done = tutWalked > 6.0 && tutTicks > 60; break;
                case 1: done = tutTurned > 200.0f && tutTicks > 60; break;
                case 2: done = tutJumped && tutTicks > 40; break;
                case 3: done = tutBroke; break;
                case 4: done = bottom.tab() == BottomScreen::Tab::Items; break;
                case 5: done = hasWood; break;
                case 6: done = tutPlaced; break;
                default: done = tutTicks > 240; break; // the last hints just show for a while
                }
                if (done) {
                    sound::playUi("random.pop", 0.6f, 1.6f);
                    inv.tutorial = inv.tutorial >= 8 ? 99 : inv.tutorial + 1;
                }
            }
            { // AxeolonProgress.tick
                long t = long(world.info.time % 24000);
                if (t >= 13000 && t < 23000 && !player.dead()) inv.sawNight = true;
                else if (inv.sawNight && t < 1000) {
                    inv.sawNight = false;
                    earn(progress::NIGHT_WATCH);
                }
                if (hudCounter % 20 == 0) {
                    inv.markSeen(); // AxeolonProgress.tick: for the Almanac's recipes
                    bool iron[4] = {false, false, false, false};
                    for (int i = 0; i < Inventory::ARMOUR; ++i) {
                        int id = inv.armour[i].empty() ? 0 : inv.armour[i].id;
                        if (id >= 306 && id <= 309) iron[id - 306] = true;
                    }
                    if (iron[0] && iron[1] && iron[2] && iron[3]) earn(progress::READY_COLD);
                    // AxeolonLair.tick: down in the bear's cave
                    if (bosses::hasInt("lair.floor") && bosses::hasInt("lair.x")) {
                        double dx = player.posX - bosses::getInt("lair.x"), dz = player.posZ - bosses::getInt("lair.z");
                        int bx = int(std::floor(player.posX)), by = int(std::floor(player.posY)), bz = int(std::floor(player.posZ));
                        if (dx * dx + dz * dz < 40.0 * 40.0 && player.posY < bosses::getInt("lair.floor") + 14.0 &&
                            by >= 0 && by < 256 && !world.canBlockSeeTheSky(bx, by, bz))
                            earn(progress::SNOW);
                    }
                }
            }
            // SpawnerAnimals: monsters every tick, animals every 400 (World.tick)
            if (!genFiber.busy()) mobs.spawn(world, player, gOptions.difficulty > 0, world.info.time % 400 == 0);
            if (!genFiber.busy()) containers::tickFurnaces(world);
            if (inv.containerKind != Inventory::NONE) {
                if (inv.containerDirty) containers::save(world, inv);
                containers::refresh(world, inv);
                bottom.markDirty();
                const int* p = inv.containerPos[0];
                double dx = player.posX - (p[0] + 0.5), dy = player.posY - (p[1] + 0.5), dz = player.posZ - (p[2] + 0.5);
                bool gone = !containers::isContainer(world.getBlockId(p[0], p[1], p[2]));
                if (inv.cartId) { // canInteractWith: the cart still there, within 8
                    Vehicles::Cart* k = vehicles.cartById(inv.cartId);
                    gone = !k;
                    if (k) { dx = player.posX - k->x; dy = player.posY - k->y; dz = player.posZ - k->z; }
                }
                if (gone || dx * dx + dy * dy + dz * dz > 64.0 || bottom.tab() != BottomScreen::Tab::Items) {
                    containers::close(world, inv);
                    inv.returnCursor();
                    chest.open = false;
                }
            }
            itemEntities.spawnFromWorld(world);
            for (ItemStack& sp : inv.spill) // thrown out of a closed crafting screen, at the feet
                itemEntities.spawn(player.posX, player.posY - 1.62 + 0.3, player.posZ, sp, 40);
            inv.spill.clear();
            {
                bool picked = false;
                itemEntities.tick(world, player.boundingBox, inv, picked);
                if (picked) { // EntityItem.onCollideWithPlayer
                    bottom.markDirty();
                    sound::play("random.pop", player.posX, player.posY - 1.62, player.posZ, 0.2f,
                                ((world.rand.nextFloat() - world.rand.nextFloat()) * 0.7f + 1.0f) * 2.0f);
                }
            }
            // PlayerControllerSP.sendBlockRemoving: digging goes on every tick the button is held
            if (dig.active && !creative) {
                if (dig.delay > 0) {
                    --dig.delay;
                } else if (world.getBlockId(dig.x, dig.y, dig.z) != dig.id) {
                    dig.active = false;
                } else {
                    bool inWater = Physics::isMaterialInBB(world, player.boundingBox.expand(-0.001, -0.401, -0.001), false);
                    dig.damage += survival::blockStrength(dig.id, inv.held().empty() ? 0 : inv.held().id, inWater, player.onGround);
                    if (dig.hits % 4 == 0)
                        sound::play(BS[dig.id].step, dig.x + 0.5, dig.y + 0.5, dig.z + 0.5, (BS[dig.id].volume + 1.0f) / 8.0f,
                                    BS[dig.id].pitch * 0.5f);
                    ++dig.hits;
                    if (dig.damage >= 1.0f) {
                        breakBlock(dig.x, dig.y, dig.z);
                        dig.active = false;
                        dig.damage = 0;
                        dig.delay = 5; // blockHitDelay
                    }
                }
            }
            almanacMap.tick(int(std::floor(cam.x)), int(std::floor(cam.z)));
        }
        float partial = tickAcc / 0.05f;
        if (!flying) { // the camera sits at the eyes, between the last two ticks
            cam.x = float(player.prevPosX + (player.posX - player.prevPosX) * partial);
            cam.y = float(player.prevPosY + (player.posY - player.prevPosY) * partial);
            cam.z = float(player.prevPosZ + (player.posZ - player.prevPosZ) * partial);
        }
        if (!flying && gOptions.bobView) { // EntityRenderer.setupViewBobbing
            float d = player.distanceWalkedModified - player.prevDistanceWalked;
            d = player.distanceWalkedModified + d * partial;
            float b = player.prevCameraYaw + (player.cameraYaw - player.prevCameraYaw) * partial;
            float cp = player.prevCameraPitch + (player.cameraPitch - player.prevCameraPitch) * partial;
            cam.bobX = std::sin(d * PI) * b * 0.5f;
            cam.bobY = -std::fabs(std::cos(d * PI) * b);
            cam.bobRoll = std::sin(d * PI) * b * 3.0f * PI / 180.0f;
            cam.bobPitch = (std::fabs(std::cos(d * PI + 0.2f) * b) * 5.0f + cp) * PI / 180.0f;
        } else {
            cam.bobX = cam.bobY = cam.bobRoll = cam.bobPitch = 0.0f;
        }
        { // AxeolonCinematic.applyCamera: the boss intro flies the camera on its own
            double cx, cy, cz;
            float cyaw, cpitch, croll, cfov;
            float pyaw = player.rotationYaw, ppitch = player.rotationPitch;
            if (!flying && mobs.cinematic(partial, cam.x, cam.y, cam.z, pyaw, ppitch, cx, cy, cz, cyaw, cpitch, croll, cfov)) {
                cam.x = float(cx);
                cam.y = float(cy);
                cam.z = float(cz);
                cam.yaw = cyaw * PI / 180.0f + PI;
                cam.pitch = cpitch * PI / 180.0f;
                cam.bobX = cam.bobY = cam.bobPitch = 0.0f;
                cam.bobRoll = croll * PI / 180.0f;
                renderer.fovDeg = cfov;
            } else {
                renderer.fovDeg = Renderer::FOV_DEG;
            }
        }
        { // EntityRenderer.hurtCameraEffect
            float f = float(player.hurtTime) - partial;
            cam.deathRoll = 0;
            cam.hurtRoll = 0;
            if (player.dead()) {
                float d = float(player.deathTime) + partial;
                cam.deathRoll = (40.0f - 8000.0f / (d + 200.0f)) * PI / 180.0f;
            }
            if (f >= 0.0f && player.maxHurtTime > 0) {
                f /= float(player.maxHurtTime);
                f = std::sin(f * f * f * f * PI);
                cam.hurtYaw = player.attackedAtYaw * PI / 180.0f;
                cam.hurtRoll = f * 14.0f * PI / 180.0f;
            }
        }
        if (trace) dbg(" ticked");

        // ---- break / use / place (ignored while the stylus is down) ----
        bool touching = (held & KEY_TOUCH) || player.dead();
        bool breakHeld = !touching && (held & (zButtons ? KEY_ZR : (KEY_R | KEY_ZR)));
        bool placeHeld = !touching && (held & (zButtons ? KEY_ZL : (KEY_L | KEY_ZL)));
        breakTimer = breakHeld ? breakTimer - dt : 0;
        placeTimer = placeHeld ? placeTimer - dt : 0;
        if (inv.held().id == spells::BOOK) placeHeld = false; // SpellBookInput has it
        { // AxeolonBattleInput: in battle mode the off hand blocks (shield) or fights (sword, dagger)
            static float offCooldown = 0;
            offCooldown -= dt;
            bool useBtn = !touching && (held & (zButtons ? KEY_ZL : (KEY_L | KEY_ZL)));
            bool usePress = !touching && (down & (zButtons ? KEY_ZL : (KEY_L | KEY_ZL)));
            ItemStack* off = inv.offhand();
            int offId = off && !off->empty() ? off->id : 0;
            bool shield = (offId >= 1035 && offId <= 1038) || offId == 1042;
            bool weapon = offId == 267 || offId == 268 || offId == 272 || offId == 276 || offId == 283 ||
                          (offId >= 1015 && offId <= 1019); // swords and daggers fit the off hand
            player.blocking = inv.battleMode && shield && useBtn && !player.dead() && !spellPicker;
            if (inv.battleMode && (shield || weapon)) placeHeld = false;
            if (inv.battleMode && weapon && usePress && offCooldown <= 0 && !player.dead()) {
                float fx, fy, fz;
                aimDir(cam, renderer.fovDeg, fx, fy, fz);
                RayHit bh = world.rayTrace(cam.x, cam.y, cam.z, fx, fy, fz, 3.0f);
                double blockDist = 3.0, md;
                if (bh.hit) {
                    double bx = bh.x + 0.5 - cam.x, by = bh.y + 0.5 - cam.y, bz = bh.z + 0.5 - cam.z;
                    blockDist = std::sqrt(bx * bx + by * by + bz * bz) - 0.5;
                }
                int target = mobs.pick(cam.x, cam.y, cam.z, fx, fy, fz, 3.0, md);
                if (target >= 0 && md <= blockDist) {
                    mobs.attack(target, player, survival::entityDamage(offId), false, itemEntities);
                    int wear = creative ? 0 : survival::hitWear(offId);
                    if (wear > 0) {
                        off->damage = int16_t(off->damage + wear);
                        if (off->damage > survival::maxDamage(offId)) {
                            off->clear();
                            sound::play("random.break", player.posX, player.posY - 1.62, player.posZ, 0.8f, 0.8f + world.rand.nextFloat() * 0.4f);
                        }
                    }
                    bottom.markDirty();
                } else {
                    offCooldown = 0.5f; // swung at nothing: the hand is locked for 10 ticks
                }
                firstPerson.swingOffhand();
            }
        }
        if (placeHeld && placeTimer <= 0 && !player.dead() &&
            (inv.held().id == 261 || inv.held().id == 332 || inv.held().id == 344) && !inv.held().empty()) {
            // right-click repeats every 4 ticks while held (Minecraft.rightClickDelayTimer)
            if (itemuse::throwHeld(world, player, inv, creative, mobs)) bottom.markDirty();
            placeTimer = 0.2f;
            placeHeld = false;
        }
        if (placeHeld && placeTimer <= 0 && !player.dead() && inv.held().id == 346 && !inv.held().empty()) {
            // ItemFishingRod.onItemRightClick: cast, or reel in (the rod wears by what came back)
            if (mobs.bobber.active) {
                int wear = mobs.reelIn(player, itemEntities);
                ItemStack& rod = inv.held();
                if (wear > 0 && !creative) {
                    rod.damage = int16_t(rod.damage + wear);
                    if (rod.damage > survival::maxDamage(346)) {
                        rod.clear();
                        sound::play("random.break", player.posX, player.posY - 1.62, player.posZ, 0.8f, 0.8f + world.rand.nextFloat() * 0.4f);
                    }
                }
            } else {
                sound::play("random.bow", player.posX, player.posY - 1.62, player.posZ, 0.5f, 0.4f / (world.rand.nextFloat() * 0.4f + 0.8f));
                mobs.castBobber(player);
            }
            player.swingItem();
            bottom.markDirty();
            placeTimer = 0.3f;
            placeHeld = false;
        }
        if (placeHeld && placeTimer <= 0 && !player.dead() && inv.held().id == 333 && !inv.held().empty()) {
            // ItemBoat.onItemRightClick: a boat on the block (or still water) looked at
            if (vehicles.placeBoat(world, player, inv.held().damage)) {
                if (!creative) inv.consumeHeld();
                player.swingItem();
                bottom.markDirty();
            }
            placeTimer = 0.3f;
            placeHeld = false;
        }
        if (placeHeld && placeTimer <= 0 && inv.held().id == spells::PAGE && !inv.held().empty()) {
            bool used = false; // ItemSpellPage.onItemRightClick
            hud.addChat(spells::usePage(inv, inv.held(), used));
            if (used) sound::play("random.pop", player.posX, player.posY, player.posZ, 1.0f, 0.8f);
            placeTimer = 0.3f;
            placeHeld = false;
            bottom.markDirty();
        }
        if (placeHeld && placeTimer <= 0 && !player.dead()) { // Entity.interact: right-click on a mob
            float fx, fy, fz;
            aimDir(cam, renderer.fovDeg, fx, fy, fz);
            RayHit bh = world.rayTrace(cam.x, cam.y, cam.z, fx, fy, fz, 3.0f);
            double blockDist = 3.0, md;
            if (bh.hit) {
                double bx = bh.x + 0.5 - cam.x, by = bh.y + 0.5 - cam.y, bz = bh.z + 0.5 - cam.z;
                blockDist = std::sqrt(bx * bx + by * by + bz * bz) - 0.5;
            }
            int t = mobs.pick(cam.x, cam.y, cam.z, fx, fy, fz, 3.0, md);
            double bd, cd;
            int boat = vehicles.pick(cam.x, cam.y, cam.z, fx, fy, fz, 3.0, bd);
            int cart = vehicles.pickCart(cam.x, cam.y, cam.z, fx, fy, fz, 3.0, cd);
            if (cart >= 0 && (boat < 0 || cd < bd)) boat = -1;
            std::string chat;
            if (cart >= 0 && cd <= blockDist && (t < 0 || cd < md) && boat < 0) { // EntityMinecart.interact
                if (mobs.riding()) mobs.dismount(player);
                int open = vehicles.useCart(cart, player, inv, creative);
                if (open) { // displayGUIChest
                    if (inv.containerKind != Inventory::NONE) containers::close(world, inv);
                    if (Vehicles::Cart* k = vehicles.cartById(open)) containers::openCart(inv, open, k->items);
                    bottom.setTab(BottomScreen::Tab::Items);
                }
                placeTimer = 0.3f;
                placeHeld = false;
                bottom.markDirty();
            } else if (boat >= 0 && bd <= blockDist && (t < 0 || bd < md)) { // EntityBoat.interact: climb in
                if (mobs.riding()) mobs.dismount(player);
                vehicles.use(boat, player);
                placeTimer = 0.3f;
                placeHeld = false;
            } else if (t >= 0 && md <= blockDist && mobs.interact(t, player, inv, player.isSneaking(), creative, chat)) {
                hud.addChat(chat);
                placeTimer = 0.3f;
                placeHeld = false;
                bottom.markDirty();
            }
        }
        if (!breakHeld) { dig.active = false; dig.damage = 0; }
        // EntityRenderer.getMouseOver for mobs: the closest one in reach, nearer than any block
        bool breakPressed = !touching && (down & (zButtons ? KEY_ZR : (KEY_R | KEY_ZR)));
        bool mobInFront = false;
        if (breakPressed && !player.dead()) player.swingItem(); // Minecraft.clickMouse(0)
        if (breakHeld && !player.dead()) {
            float fx, fy, fz;
            aimDir(cam, renderer.fovDeg, fx, fy, fz);
            int heldId = inv.held().empty() ? 0 : inv.held().id;
            double reach = survival::isSpear(heldId) ? 5.0 : 3.0;
            RayHit bh = world.rayTrace(cam.x, cam.y, cam.z, fx, fy, fz, float(reach));
            double blockDist = reach;
            if (bh.hit) {
                double bx = bh.x + 0.5 - cam.x, by = bh.y + 0.5 - cam.y, bz = bh.z + 0.5 - cam.z;
                blockDist = std::sqrt(bx * bx + by * by + bz * bz) - 0.5;
            }
            double md;
            int target = mobs.pick(cam.x, cam.y, cam.z, fx, fy, fz, reach, md);
            mobInFront = target >= 0 && md <= blockDist;
            { // EntityBoat.attackEntityFrom: knock it about; enough and it's an item again
                double bd, cd;
                int boat = vehicles.pick(cam.x, cam.y, cam.z, fx, fy, fz, reach, bd);
                int cart = vehicles.pickCart(cam.x, cam.y, cam.z, fx, fy, fz, reach, cd);
                if (cart >= 0 && cd <= blockDist && (!mobInFront || cd < md) && (boat < 0 || cd < bd)) {
                    boat = -1;
                    mobInFront = true;
                    target = -1;
                    if (breakPressed) {
                        vehicles.hitCart(cart, survival::entityDamage(heldId), itemEntities);
                        player.swingItem();
                    }
                }
                if (boat >= 0 && bd <= blockDist && (!mobInFront || bd < md)) {
                    mobInFront = true;
                    target = -1;
                    if (breakPressed) {
                        vehicles.hit(boat, survival::entityDamage(heldId), itemEntities);
                        player.swingItem();
                    }
                }
            }
            { // EntityFireball.attackEntityFrom: a punch sends a ghast's fireball back where you look
                double fd;
                if (breakPressed && mobs.punchFireball(cam.x, cam.y, cam.z, fx, fy, fz, mobInFront ? md : blockDist, fd)) {
                    player.swingItem();
                    mobInFront = true;
                    target = -1;
                }
            }
            double pd;
            int art = paintings.pick(cam.x, cam.y, cam.z, fx, fy, fz, reach, pd);
            if (art >= 0 && pd <= blockDist && (!mobInFront || pd < md)) { // a painting is in the way
                mobInFront = true; // (no digging through it)
                target = -1;
                if (breakPressed) paintings.knockDown(world, art, itemEntities);
            }
            if (mobInFront && breakPressed && target >= 0) {
                // attackTargetEntityWithCurrentItem: damage, then the weapon wears
                mobs.attack(target, player, survival::entityDamage(heldId), survival::isMace(heldId), itemEntities);
                player.swingItem();
                ItemStack& h = inv.held();
                int wear = creative ? 0 : survival::hitWear(heldId);
                if (wear > 0) {
                    h.damage = int16_t(h.damage + wear);
                    if (h.damage > survival::maxDamage(heldId)) {
                        h.clear();
                        sound::play("random.break", player.posX, player.posY - 1.62, player.posZ, 0.8f, 0.8f + world.rand.nextFloat() * 0.4f);
                    }
                    bottom.markDirty();
                }
            }
        }
        if (mobInFront) { dig.active = false; dig.damage = 0; }
        if ((breakHeld && !mobInFront && (breakTimer <= 0 || !creative)) || (placeHeld && placeTimer <= 0)) {
            float fx, fy, fz;
            aimDir(cam, renderer.fovDeg, fx, fy, fz);
            RayHit hit = world.rayTrace(cam.x, cam.y, cam.z, fx, fy, fz, REACH);
            bool locked = hit.hit && !creative && mobs.arenaLocked(hit.x, hit.z); // AxeolonBossFight.isLocked
            if (hit.hit && !locked && breakHeld && !mobInFront && (breakTimer <= 0 || !creative)) {
                if (creative) { // PlayerControllerCreative: gone at once, nothing drops
                    breakTimer = REPEAT;
                    breakBlock(hit.x, hit.y, hit.z);
                } else if (!dig.active || dig.x != hit.x || dig.y != hit.y || dig.z != hit.z) {
                    // PlayerControllerSP.clickBlock: start digging (instantly if strong enough)
                    dig = Dig{};
                    dig.x = hit.x; dig.y = hit.y; dig.z = hit.z;
                    Redstone::blockClicked(world, hit.x, hit.y, hit.z); // Block.onBlockClicked: note blocks play
                    if (world.getBlockId(hit.x, hit.y, hit.z) == 92) eatCake(hit.x, hit.y, hit.z); // ... and a punch eats cake
                    dig.side = itemuse::sideOf(hit);
                    dig.id = world.getBlockId(hit.x, hit.y, hit.z);
                    dig.active = dig.id != 0;
                    if (dig.active && survival::blockStrength(dig.id, inv.held().empty() ? 0 : inv.held().id, false, player.onGround) >= 1.0f) {
                        breakBlock(hit.x, hit.y, hit.z);
                        dig.active = false;
                    }
                }
            }
            if (!breakHeld || !hit.hit || hit.x != dig.x || hit.y != dig.y || hit.z != dig.z) {
                if (!breakHeld) { dig.active = false; dig.damage = 0; }
            }
            if (!hit.hit && placeHeld && placeTimer <= 0 && !player.dead()) {
                // Item.onItemRightClick in the air: food, buckets
                if (itemuse::useItem(world, player, inv, creative, cam.x, cam.y, cam.z, fx, fy, fz)) {
                    placeTimer = 0.3f;
                    almanacMap.invalidate();
                    bottom.markDirty();
                }
            }
            if (hit.hit && !locked && placeHeld && placeTimer <= 0 && !player.dead()) {
                placeTimer = REPEAT;
                int useId = world.getBlockId(hit.x, hit.y, hit.z);
                if (containers::isContainer(useId) && !player.isSneaking()) {
                    if (containers::open(world, inv, hit.x, hit.y, hit.z)) {
                        if (useId == 54 || useId == 142) { // the lid opens while it's open
                            BlockModels::openContainer(world, hit.x, hit.y, hit.z);
                            chest = {hit.x, hit.y, hit.z, true};
                        }
                        bottom.setTab(BottomScreen::Tab::Items);
                        bottom.markDirty();
                    }
                    placeTimer = 0.3f;
                } else if (useId == 110 && !player.isSneaking()) { // BlockStonecutter.blockActivated
                    inv.openStonecutter();
                    bottom.setTab(BottomScreen::Tab::Items);
                    bottom.markDirty();
                    placeTimer = 0.3f;
                } else if (useId == 58 && !player.isSneaking()) { // BlockWorkbench
                    inv.openTable();
                    bottom.setTab(BottomScreen::Tab::Items);
                    bottom.markDirty();
                    placeTimer = 0.3f;
                } else if (useId == 84 && world.getBlockMetadata(hit.x, hit.y, hit.z) > 0) { // BlockJukeBox.blockActivated
                    ejectRecord(hit.x, hit.y, hit.z);
                    placeTimer = 0.3f;
                } else if (useId == 84 && (inv.held().id == 2256 || inv.held().id == 2257) && !inv.held().empty()) {
                    // ItemRecord.onItemUse: on it goes
                    int disc = inv.held().id;
                    world.setBlockMetadataWithNotify(hit.x, hit.y, hit.z, disc - 2256 + 1);
                    const char* name = disc == 2256 ? "13" : "cat";
                    sound::playRecord(name, hit.x, hit.y, hit.z);
                    mobs.jukebox(hit.x, hit.y, hit.z, true);
                    hud.setRecordPlaying(std::string("C418 - ") + name);
                    if (!creative) inv.consumeHeld();
                    player.swingItem();
                    bottom.markDirty();
                    placeTimer = 0.3f;
                } else if (useId == 92) { // BlockCake.blockActivated
                    eatCake(hit.x, hit.y, hit.z);
                    placeTimer = 0.3f;
                } else if (useBlock(world, hit.x, hit.y, hit.z, inv.held().empty())) {
                    placeTimer = 0.3f;
                } else {
                    // where on the block the crosshair is (modern top/bottom stair placement)
                    float hitY = 0.5f;
                    if (hit.nx != 0 || hit.nz != 0) {
                        float t = hit.nx != 0 ? (float(hit.x + (hit.nx > 0 ? 1 : 0)) - cam.x) / fx
                                              : (float(hit.z + (hit.nz > 0 ? 1 : 0)) - cam.z) / fz;
                        hitY = cam.y + fy * t - float(hit.y);
                    }
                    bool used = false;
                    int heldNow = inv.held().empty() ? 0 : inv.held().id;
                    if (heldNow == 1039) { // the Spawn charm
                        used = placeCrystal(hit.x, hit.y, hit.z, itemuse::sideOf(hit));
                    } else if ((heldNow == 328 || heldNow == 342 || heldNow == 343) && useId == 66) { // ItemMinecart.onItemUse
                        used = vehicles.placeCart(world, hit.x, hit.y, hit.z, heldNow == 328 ? 0 : heldNow == 342 ? 1 : 2);
                        if (used && !creative) inv.consumeHeld();
                    } else if (inv.held().id == 321 && !inv.held().empty()) { // ItemPainting
                        bool hung = false;
                        used = paintings.place(world, hit.x, hit.y, hit.z, itemuse::sideOf(hit), hung);
                        if (hung && !creative) inv.consumeHeld();
                    } else {
                        used = itemuse::useOnBlock(world, player, inv, hit, hitY, creative);
                    }
                    if (used) tutPlaced = true;
                    if (!used) used = itemuse::useItem(world, player, inv, creative, cam.x, cam.y, cam.z, fx, fy, fz);
                    if (used) {
                        player.swingItem();
                        almanacMap.invalidate();
                        bottom.markDirty();
                    }
                }
            }
        }

        if (player.dead()) { // GuiGameOver: the buttons work after 20 ticks
            if (down & (KEY_DUP | KEY_DDOWN)) gameOverSel ^= 1;
            if ((down & KEY_TOUCH) && touch.py > 120) gameOverSel = touch.py > 168 ? 1 : 0;
            if (player.deathTime >= 20 && (down & (KEY_A | KEY_TOUCH))) {
                if (gameOverSel == 1) { leftToTitle = true; break; }
                if (nether && world.canSave()) { // Minecraft.respawn: out of the Nether first
                    player.resetAfterRespawn();
                    respawnHome = true;
                    portalTravel = true;
                    break;
                }
                // Minecraft.respawn: a new player at the world spawn (preparePlayerToSpawn)
                world.setWindow(world.info.spawnX >> 4, world.info.spawnZ >> 4);
                double feet = world.info.spawnY;
                int fx = world.info.spawnX, fz = world.info.spawnZ;
                while (feet < 255 && (world.blockAt(fx, int(feet), fz) != 0 || world.blockAt(fx, int(feet) + 1, fz) != 0)) feet += 1.0;
                player.resetAfterRespawn();
                player.place(fx + 0.5, feet + 1.62, fz + 0.5, player.rotationYaw, 0);
                cam.x = float(player.posX); cam.y = float(player.posY); cam.z = float(player.posZ);
                wasDead = false;
                bottom.markDirty();
            }
            down &= ~(KEY_A | KEY_B | KEY_X | KEY_Y);
        }
        if (itemuse::signEdit) { // GuiEditSign: the system keyboard, four lines of fifteen
            itemuse::signEdit = false;
            std::string lines[4];
            SwkbdState kb;
            swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 63);
            swkbdSetHintText(&kb, "Edit sign message");
            swkbdSetFeatures(&kb, SWKBD_MULTILINE);
            char buf[72] = {0};
            if (swkbdInputText(&kb, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
                int line = 0;
                for (const char* c = buf; *c && line < 4; ++c) {
                    if (*c == '\n') { ++line; continue; }
                    unsigned char ch = (unsigned char)*c;
                    if (ch >= 32 && ch < 127 && lines[line].size() < 15) lines[line] += char(ch);
                }
            }
            if (nbt::Tag* te = world.tileEntityAt(itemuse::signX, itemuse::signY, itemuse::signZ))
                for (int i = 0; i < 4; ++i) te->setString(("Text" + std::to_string(i + 1)).c_str(), lines[i]);
            signs.markDirty();
            lastTick = osGetTime();
        }
        if (chatRequest) { // GuiChat (T in the mod): the system keyboard, then CommandHandler
            chatRequest = false;
            hud.chatOpen = true;
            SwkbdState kb;
            swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, 100);
            swkbdSetHintText(&kb, "Chat, or /help for commands");
            char buf[128] = {0};
            if (swkbdInputText(&kb, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
                std::string line;
                for (const char* ch = buf; *ch; ++ch)
                    if ((unsigned char)*ch >= 32 && (unsigned char)*ch < 127) line += *ch;
                bool wasCreative = creative;
                commands::Context cc{world, player, inv, itemEntities, mobs, creative, flying, saveNow,
                                     [&](const std::string& t) { hud.addChat(t); }};
                commands::handle(cc, line);
                if (creative != wasCreative) bottom.markDirty();
            }
            hud.chatOpen = false;
            lastTick = osGetTime();
        }
        if (saveNow && !genFiber.busy()) {
            savingFrames = 45;
            // draw the indicator first so it's up while the card is written
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            renderer.prepare2D();
            C2D_SceneBegin(renderer.topTarget(0));
            hud.drawSaving(0);
            C3D_FrameEnd(0);
            u64 t0 = osGetTime();
            saveGame();
            autosaveTicks = 0;
            dbg("saved in %d ms", int(osGetTime() - t0));
            lastTick = osGetTime(); // the save's time isn't game time
        }
        bottom.handleInput(down, held, touch);
        if ((down & KEY_TOUCH) && (bottom.tab() == BottomScreen::Tab::Map || bottom.tab() == BottomScreen::Tab::Almanac))
            earn(progress::NEW_PAGE); // GuiAlmanac opened
        if (bottom.quitRequested()) { leftToTitle = true; break; }
        if (player.travelNow && !player.dead() && world.canSave()) { portalTravel = true; break; }

        // ---- lighting: Minecraft.run calls World.func_6465_g once a frame (up to 500 updates) ----
        {
            u64 t0 = svcGetSystemTick();
            // Java does up to 500 per frame at 60+ fps; here: as many as fit in a few ms
            while (world.updatingLighting() && svcGetSystemTick() - t0 < SYSCLOCK_ARM11 / 250) {}
            stats.lightMs = float(svcGetSystemTick() - t0) * 1000.0f / SYSCLOCK_ARM11;
        }

        // ---- section rebuilds: as many as fit in ~5 ms ----
        {
            u64 start = svcGetSystemTick();
            const u64 budget = SYSCLOCK_ARM11 / 200; // 5 ms
            int cx, sy, cz;
            while (svcGetSystemTick() - start < budget && rebuild.pop(cx, sy, cz)) {
                if (!world.isChunkLoaded(cx, cz) || !shown.count(colKey(cx, cz))) continue;
                ChunkMesher::buildSection(world, cx, sy, cz, scratch);
                renderer.setSectionMesh(cx, sy, cz, scratch);
            }
        }

        if (trace) dbg(" meshed, %d left", int(rebuild.items.size()));
        // ---- stats ----
        frames++;
        fpsTimer += dt;
        if (fpsTimer >= 1.0f) { stats.fps = frames; frames = 0; fpsTimer = 0; }
        stats.sections = renderer.lastDrawnSections();
        stats.triangles = renderer.lastDrawnTriangles();
        stats.gpuKB = (unsigned)(renderer.linearBytesUsed() / 1024);
        bottom.update(dt, stats); // may upload the map texture (outside the frame)

        { // EntityRenderer.updateFogColor + setupFog
            using namespace atmosphere;
            float angle = celestialAngle(world.info.time, partial, nether);
            float opt = renderDistanceOption();
            float k = 1.0f - std::pow(1.0f / (4.0f - opt), 0.25f);
            RGB sky = skyColour(angle, skyTemperature), fog = fogColour(angle, nether);
            fog.r += (sky.r - fog.r) * k;
            fog.g += (sky.g - fog.g) * k;
            fog.b += (sky.b - fog.b) * k;
            float far = renderer.renderDistance, start = far * 0.25f, end = far;
            if (nether) start = 0.0f;
            if (player.isEyeIn(world, false)) { fog = {0.02f, 0.02f, 0.2f}; start = 0.0f; end = 24.0f; }
            else if (player.isEyeIn(world, true)) { fog = {0.6f, 0.1f, 0.0f}; start = 0.0f; end = 1.5f; }
            float lit = fogLightPrev + (fogLight - fogLightPrev) * partial;
            fog.r *= lit;
            fog.g *= lit;
            fog.b *= lit;
            renderer.setAtmosphere(world.skylightSubtracted, nether ? 0.1f : 0.05f, pack(fog), start, end);
            Renderer::SkyState& st = renderer.sky;
            st.enabled = !nether;
            st.angle = angle;
            st.sky[0] = sky.r; st.sky[1] = sky.g; st.sky[2] = sky.b;
            st.sunrise = atmosphere::sunrise(angle, st.sunriseColour);
            st.stars = starBrightness(angle);
            RGB cl = cloudColour(angle);
            st.cloud[0] = cl.r; st.cloud[1] = cl.g; st.cloud[2] = cl.b;
            double cu = double(cam.x) + double((float(cloudTicks) + partial) * 0.03f), cv = double(cam.z);
            st.cloudU = float(cu - std::floor(cu / 2048.0) * 2048.0);
            st.cloudV = float(cv - std::floor(cv / 2048.0) * 2048.0);
            st.cloudY = 140.33f - cam.y;
        }
        sound::setListener(cam.x, cam.y, cam.z, player.rotationYaw);
        sound::update();
        // ---- draw ----
        C3D_Mtx view;
        cam.viewMatrix(view);
        const float viewAspect = 400.0f / 240.0f;
        frustum.build(cam.x, cam.y, cam.z, cam.yaw, cam.pitch, C3D_AngleFromDegrees(renderer.fovDeg), viewAspect);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        renderer.beginFrame();
        animQuads.clear();
        animBatches.clear();
        BlockModels::render(world, partial, animQuads, animBatches);
        { // TileEntitySpawnCrystalRenderer: the crystal floats over its pedestal, glowing while it's your spawn
            static axm::Model crystalModel;
            static bool crystalTried = false;
            if (!crystalTried) { crystalTried = true; crystalModel.load("romfs:/axeolon/block/spawn_crystal.axm"); }
            const RigTexture* ctex = BlockModels::rigTexture("spawn_crystal");
            for (const Crystal& c : crystals) {
                double dx = c.x + 0.5 - cam.x, dz = c.z + 0.5 - cam.z;
                if (!ctex || dx * dx + dz * dz > 48.0 * 48.0) continue;
                bool active = crystalActive(c.x, c.y, c.z);
                float t = float(world.info.time % 100000) + partial;
                auto it = crystalSetAt.find(crystalKey(c.x, c.y, c.z));
                float since = it == crystalSetAt.end() ? 99.0f : (t - float(it->second % 100000)) / 20.0f;
                const char* anim = active ? (since >= 0.0f && since < 1.5f ? "activate" : "active") : "dormant";
                size_t start = animQuads.size();
                crystalModel.emitRig(animQuads, anim, active && since < 1.5f ? since : t / 20.0f, 0.0f, 0.0f,
                                     axm::Model::HALF_ALL, c.x, c.y + 1.2, c.z, nullptr, nullptr);
                float b = BlockModels::lightBrightness(world, c.x, c.y + 1, c.z);
                const float cx = c.x + 0.5f, cy = c.y + 1.2f, cz = c.z + 0.5f;
                for (size_t i = start; i < animQuads.size(); ++i) { // 0.9 the size, about its foot
                    axm::ModelVertex& v = animQuads[i];
                    v.x = cx + (v.x - cx) * 0.9f;
                    v.y = cy + (v.y - cy) * 0.9f;
                    v.z = cz + (v.z - cz) * 0.9f;
                    v.shade = active ? 1.0f : 0.6f * b;
                }
                animBatches.push_back({ctex, start, animQuads.size() - start});
            }
        }
        // 3D slider: 0 draws one picture; up draws one per eye (twice the GPU work)
        float slider = osGet3DSliderState();
        int eyes = slider > 0.0f ? 2 : 1;
        for (int eye = 0; eye < eyes; eye++) {
        renderer.setEye(eye, slider / 4.0f);
        renderer.renderWorld(view, frustum, cam.x, cam.y, cam.z);
        if (trace) dbg(" world drawn: %d sections %d tris", renderer.lastDrawnSections(), renderer.lastDrawnTriangles());
        renderer.drawAtlasQuads(animQuads, animBatches);
        mobs.render(world, renderer, partial, cam.x, cam.y, cam.z, renderer.renderDistance);
        itemEntities.render(world, renderer, partial, cam.yaw, cam.pitch);
        mobs.renderBolts(renderer, partial, (cam.yaw - PI) * 180.0f / PI, cam.pitch * 180.0f / PI);
        mobs.renderTnt(world, renderer, partial);
        mobs.renderFalling(world, renderer, partial);
        paintings.render(world, renderer);
        const float viewYaw = (cam.yaw - PI) * 180.0f / PI, viewPitch = cam.pitch * 180.0f / PI; // billboards face the camera
        mobs.renderThrown(renderer, partial, viewYaw, viewPitch);
        mobs.renderFireballs(renderer, partial, viewYaw, viewPitch);
        vehicles.render(world, renderer, partial);
        mobs.renderBobber(renderer, player, partial, viewYaw, viewPitch);
        mobs.renderIcicles(renderer, partial);
        particles.render(world, renderer, partial, viewYaw, viewPitch);
        signs.render(world, renderer, hud.textFont(), cam.x, cam.y, cam.z);
        if (dig.active && dig.damage > 0) drawCracks(renderer, dig.x, dig.y, dig.z, int(dig.damage * 10.0f));
        if (trace) dbg(" mobs drawn (%d)", int(mobs.count()));
        if ((!flying || !creative) && !mobs.cinematicActive()) {
            float hb = BlockModels::lightBrightness(world, int(std::floor(player.posX)), int(std::floor(player.posY)),
                                                    int(std::floor(player.posZ)));
            firstPerson.draw(renderer, player, cam, partial, hb);
        }
        renderer.prepare2D();
        C2D_SceneBegin(renderer.topTarget(eye));
        bool cine = mobs.cinematicActive(); // AxeolonCinematic.renderBars hides the HUD
        if (!player.dead() && !cine) {
            float eb = BlockModels::lightBrightness(world, int(std::floor(player.posX)), int(std::floor(player.posY)),
                                                    int(std::floor(player.posZ)));
            hud.drawOverlays(eb, player.isInsideOfWater(world), player.fire > 0 && !creative, player.rotationYaw,
                             player.rotationPitch, renderer);
            hud.drawPortal(player.prevTimeInPortal + (player.timeInPortal - player.prevTimeInPortal) * partial, renderer);
            drawCrosshair(); // Pocket Edition has none: you aim with your finger
        }
        { // the how-to-play hint
            bool want = false /* NSSS (Alpha): no How to Play hints */ && !creative && inv.tutorial >= 0 && inv.tutorial < 99 && !cine && !paused && !spellPicker && !player.dead();
            tutFade += ((want ? 1.0f : 0.0f) - tutFade) * 0.15f;
            if (tutFade > 0.01f && inv.tutorial >= 0 && inv.tutorial < 99) {
                const char* look = xboxLayout ? (isNew3DS ? "the C-Stick" : "the Circle Pad Pro's stick") : "A, B, X and Y";
                const char* jump = xboxLayout ? "A" : "Up on the D-Pad";
                const char* brk = zButtons ? "ZR" : "R";
                const char* use = zButtons ? "ZL" : "L";
                char buf[200];
                switch (inv.tutorial) {
                case 0: snprintf(buf, sizeof buf, "Use the Circle Pad to walk around."); break;
                case 1: snprintf(buf, sizeof buf, "Look around with %s.", look); break;
                case 2: snprintf(buf, sizeof buf, "Press %s to jump.", jump); break;
                case 3: snprintf(buf, sizeof buf, "Hold %s to mine a block. Punch a tree to collect wood.", brk); break;
                case 4: snprintf(buf, sizeof buf, "Your inventory is on the touch screen: tap Items%s.", xboxLayout ? " or press X" : ""); break;
                case 5: snprintf(buf, sizeof buf, "Drag wood into the 2x2 crafting grid to make planks, then take them from the result slot."); break;
                case 6: snprintf(buf, sizeof buf, "Press %s to place blocks, and to open doors and chests.", use); break;
                case 7: snprintf(buf, sizeof buf, "Monsters come out at night. Build a shelter and light it with torches before dark."); break;
                default: snprintf(buf, sizeof buf, "The Almanac on the touch screen maps where you've been. Tap the map to drop a waypoint."); break;
                }
                char title[48];
                snprintf(title, sizeof title, "How to Play  %d/9   SELECT: skip", std::min(inv.tutorial, 8) + 1);
                hud.drawHint(title, buf, tutFade);
            }
        }
        if (cine) {
            hud.drawCinemaBars(mobs.cinemaBars());
        } else {
            hud.draw(player, player.isInsideOfWater(world), hudCounter, !creative);
            hud.drawChat();
            hud.drawRecordPlaying(partial);
        }
        // (NSSS is Alpha: no boss bars, spell picker or achievement toasts)
        if (player.dead()) hud.drawGameOver(gameOverSel, player.deathTime >= 20, 0);
        if (paused) hud.drawPause(pauseSel);
        if (savingFrames > 0) hud.drawSaving(frameNo);
        }
        bottom.draw();
        C3D_FrameEnd(0);
        if (savingFrames > 0) --savingFrames;
        if (trace) dbg(" frame end");
    }

    // ---- save and quit ----
    sound::playRecord("", 0, 0, 0); // a record doesn't follow you out of the world
    dbg("leaving: fiber (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
    genFiber.stop(); // finish the chunk in progress
    dbg("leaving: fiber stopped (heap %lu KB)", (unsigned long)(mallinfo().uordblks / 1024));
    if (portalTravel) {
        // Minecraft.usePortal: the other dimension, 8 times nearer (or further) on the ground plan
        player.travelNow = false;
        if (mobs.riding()) mobs.dismount(player);
        if (vehicles.riding()) vehicles.dismount(player);
        int to = nether ? 0 : -1;
        double f = to == -1 ? 1.0 / 8.0 : 8.0;
        if (respawnHome) f = 1.0; // placed at the spawn on arrival
        player.place(player.posX * f, player.posY, player.posZ * f, player.rotationYaw, player.rotationPitch);
        world.info.dimension = to;
        portalWorld = worldName;
        leftToTitle = true;
        // the trip's own loading screen, while this side saves
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        renderer.beginFrame();
        title.drawLoading(to == -1 ? "Entering the Nether" : respawnHome ? "Respawning" : "Leaving the Nether", "Saving", 0.0f);
        C3D_FrameEnd(0);
    }
    saveGame();
    dbg("saved, %s (heap used %lu KB)", leftToTitle ? "back to the title" : "closing", (unsigned long)(mallinfo().uordblks / 1024));

    bottom.shutdown();
    renderer.clearWorld();
    rebuild = RebuildQueue();
    BlockModels::clear();
    gParticles = nullptr;
    Blocks::onTntIgnited = nullptr;
    Redstone::anythingOn = nullptr;
    world.onBlockAnim = nullptr;
    world.onDoorSwing = nullptr;
    world.onParticle = nullptr;
    world.onLaunch = nullptr;
    world.onDispense = nullptr;
    world.onFallingBlock = nullptr;
    progress::clearToasts();
    paintings.clear();
    chest.open = false;
    delete save;
    if (!leftToTitle) appRunning = false;
    } // world session
    } // title <-> world

    title.shutdown();
    renderer.shutdown();
    sound::shutdown();
    C2D_Fini();
    C3D_Fini();
    romfsExit();
    if (irrstOk) irrstExit();
    gfxExit();
    return 0;
}
