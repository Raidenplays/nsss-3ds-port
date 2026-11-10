#pragma once
// Runs ChunkGenerator::provideChunk (NSSS's Alpha terrain and caves) on the 3DS's
// second CPU core for chunks the player is about to need, so the main thread only has to
// populate them. Its own ChunkGenerator, so nothing is shared with the game thread except the
// request list and the finished chunks (both behind a lock).
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct BiomeOverride;

class TerrainWorker {
public:
    bool start(int64_t seed); // false: run without it
    void stop();
    ~TerrainWorker() { stop(); }
    // Chunks wanted next, nearest first (replaces the previous list)
    void want(const std::vector<std::pair<int, int>>& chunks);
    // Takes a finished chunk if it's ready (65536 block ids)
    bool take(int cx, int cz, uint8_t* out);
    void prioritize(int cx, int cz); // wanted right now: to the front of the list
    bool running() const { return impl != nullptr; }

private:
    struct Impl;
    Impl* impl = nullptr;
};
