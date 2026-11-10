#include "TerrainWorker.h"
#ifdef __3DS__
#include <3ds.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include "ChunkGenerator.h"

struct TerrainWorker::Impl {
    std::unique_ptr<ChunkGenerator> gen;
    Thread thread = nullptr;
    LightLock lock;
    volatile bool quit = false;
    std::vector<std::pair<int, int>> wanted;
    struct Done { int cx, cz; std::vector<uint8_t> blocks; };
    std::vector<Done> done; // a few finished chunks, oldest first
    int busyX = 0, busyZ = 0;
    bool busy = false;
    static const size_t KEEP = 6;

    bool haveLocked(int cx, int cz) const {
        for (auto& d : done)
            if (d.cx == cx && d.cz == cz) return true;
        return busy && busyX == cx && busyZ == cz;
    }
    void run() {
        std::vector<uint8_t> buf(65536);
        while (!quit) {
            int cx = 0, cz = 0;
            bool got = false;
            LightLock_Lock(&lock);
            for (auto& w : wanted)
                if (!haveLocked(w.first, w.second)) { cx = w.first; cz = w.second; got = true; break; }
            if (got) { busy = true; busyX = cx; busyZ = cz; }
            LightLock_Unlock(&lock);
            if (!got) { svcSleepThread(4000000LL); continue; }
            gen->provideChunk(cx, cz, buf.data());
            LightLock_Lock(&lock);
            busy = false;
            // only keep it if it's still wanted
            if (std::find(wanted.begin(), wanted.end(), std::make_pair(cx, cz)) != wanted.end()) {
                if (done.size() >= KEEP) done.erase(done.begin());
                done.push_back(Done{cx, cz, buf});
            }
            LightLock_Unlock(&lock);
        }
    }
    static void entry(void* p) { static_cast<Impl*>(p)->run(); }
};

bool TerrainWorker::start(int64_t seed, const std::string& worldDir, const std::vector<BiomeOverride>& overrides, int dimension) {
    stop();
    // Old 3DS: let this app use most of core 1 (the system keeps the rest)
    APT_SetAppCpuTimeLimit(80);
    Impl* i = new Impl();
    LightLock_Init(&i->lock);
    i->gen.reset(new ChunkGenerator(seed, worldDir, dimension));
    i->gen->manager->overrides = overrides;
    i->thread = threadCreate(Impl::entry, i, 256 * 1024, 0x3F, 1, false);
    if (!i->thread) {
        delete i;
        return false;
    }
    impl = i;
    return true;
}

void TerrainWorker::stop() {
    if (!impl) return;
    impl->quit = true;
    threadJoin(impl->thread, U64_MAX);
    threadFree(impl->thread);
    delete impl;
    impl = nullptr;
}

void TerrainWorker::want(const std::vector<std::pair<int, int>>& chunks) {
    if (!impl) return;
    LightLock_Lock(&impl->lock);
    impl->wanted = chunks;
    LightLock_Unlock(&impl->lock);
}

void TerrainWorker::prioritize(int cx, int cz) {
    if (!impl) return;
    LightLock_Lock(&impl->lock);
    auto& w = impl->wanted;
    auto it = std::find(w.begin(), w.end(), std::make_pair(cx, cz));
    if (it != w.end()) w.erase(it);
    w.insert(w.begin(), std::make_pair(cx, cz));
    LightLock_Unlock(&impl->lock);
}

bool TerrainWorker::take(int cx, int cz, uint8_t* out) {
    if (!impl) return false;
    bool ok = false;
    LightLock_Lock(&impl->lock);
    for (size_t k = 0; k < impl->done.size(); ++k)
        if (impl->done[k].cx == cx && impl->done[k].cz == cz) {
            std::memcpy(out, impl->done[k].blocks.data(), 65536);
            impl->done.erase(impl->done.begin() + k);
            ok = true;
            break;
        }
    LightLock_Unlock(&impl->lock);
    return ok;
}
#else
struct TerrainWorker::Impl {};
bool TerrainWorker::start(int64_t, const std::string&, const std::vector<BiomeOverride>&, int) { return false; }
void TerrainWorker::stop() {}
void TerrainWorker::want(const std::vector<std::pair<int, int>>&) {}
bool TerrainWorker::take(int, int, uint8_t*) { return false; }
void TerrainWorker::prioritize(int, int) {}
#endif
