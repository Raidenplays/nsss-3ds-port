#pragma once
// Chunk loading / generation as a coroutine. Populating one chunk on an Old 3DS takes longer
// than a frame, and Beta does it in one go, so it runs on a second thread on the same core that
// takes turns with the game loop: the game hands it a few milliseconds each frame
// (run()), and the generation code gives control back at World's yield points once its time is
// up. Only one of the two runs at any moment, so the world needs no locks - but while a chunk
// is half done the game must not unload chunks or load any itself (busy()).
#include <cstdint>
#include <utility>
#include <vector>

class World;

class GenFiber {
public:
    bool start(World& w); // false: not on a 3DS (host builds)
    void stop();
    ~GenFiber() { stop(); }
    bool busy() const { return working; } // a chunk is in progress
    // Next chunks to provide (nearest first); ignored while busy
    void request(const std::vector<std::pair<int, int>>& chunks);
    // Lets generation run for up to budgetMs (returns early when it's out of work)
    void run(float budgetMs);
    void maybeYield(); // World::onYieldPoint
    bool onFiber() const;
    void yieldNow();   // from the fiber: give the rest of this frame back

private:
    struct Impl;
    Impl* impl = nullptr;
    bool working = false;
};
