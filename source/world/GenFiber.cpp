#include "GenFiber.h"
#ifdef __3DS__
#include <3ds.h>
#include "World.h"

struct GenFiber::Impl {
    World* world = nullptr;
    Thread thread = nullptr;
    LightEvent toFiber, toGame;
    volatile bool quit = false;
    std::vector<std::pair<int, int>> todo;
    u64 deadline = 0;
    u32 fiberId = 0;
    GenFiber* owner = nullptr;

    void handBack() { // fiber -> game, and wait for the next turn
        LightEvent_Signal(&toGame);
        LightEvent_Wait(&toFiber);
    }
    void loop() {
        svcGetThreadId(&fiberId, CUR_THREAD_HANDLE);
        LightEvent_Wait(&toFiber);
        while (!quit) {
            while (!todo.empty() && !quit) {
                std::pair<int, int> c = todo.front();
                todo.erase(todo.begin());
                if (!world->getChunk(c.first, c.second)) world->provideChunk(c.first, c.second);
            }
            owner->working = false;
            handBack();
        }
        LightEvent_Signal(&toGame);
    }
    static void entry(void* p) { static_cast<Impl*>(p)->loop(); }
};

bool GenFiber::start(World& w) {
    stop();
    Impl* i = new Impl();
    i->world = &w;
    i->owner = this;
    LightEvent_Init(&i->toFiber, RESET_ONESHOT);
    LightEvent_Init(&i->toGame, RESET_ONESHOT);
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    // same core as the game, lower priority: it only runs while the game waits for it
    i->thread = threadCreate(Impl::entry, i, 512 * 1024, prio + 1, -2, false);
    if (!i->thread) {
        delete i;
        return false;
    }
    impl = i;
    w.onYieldPoint = [this] { maybeYield(); };
    return true;
}

void GenFiber::stop() {
    if (!impl) return;
    // finish whatever is half done, so the world is left whole
    while (working) run(1000.0f);
    impl->world->onYieldPoint = nullptr;
    impl->quit = true;
    LightEvent_Signal(&impl->toFiber);
    threadJoin(impl->thread, U64_MAX);
    threadFree(impl->thread);
    delete impl;
    impl = nullptr;
}

void GenFiber::request(const std::vector<std::pair<int, int>>& chunks) {
    if (!impl || working || chunks.empty()) return;
    impl->todo = chunks;
    working = true;
}

void GenFiber::run(float budgetMs) {
    if (!impl || !working) return;
    impl->deadline = svcGetSystemTick() + u64(budgetMs * (SYSCLOCK_ARM11 / 1000.0f));
    LightEvent_Signal(&impl->toFiber);
    LightEvent_Wait(&impl->toGame);
}

bool GenFiber::onFiber() const {
    if (!impl) return false;
    u32 id = 0;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    return id == impl->fiberId;
}

void GenFiber::yieldNow() {
    if (onFiber()) impl->handBack();
}

void GenFiber::maybeYield() {
    if (!impl) return;
    u32 id = 0;
    svcGetThreadId(&id, CUR_THREAD_HANDLE);
    if (id != impl->fiberId) return; // the game thread ran some world code itself
    if (svcGetSystemTick() < impl->deadline) return;
    impl->handBack();
}
#else
// Everywhere else (Android, host tools): the same turn-taking with std::thread
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "World.h"

namespace {
using Clock = std::chrono::steady_clock;
}

struct GenFiber::Impl {
    World* world = nullptr;
    std::thread thread;
    std::mutex m;
    std::condition_variable cv;
    int turn = 0; // 0: the game runs, 1: the fiber runs
    bool quit = false;
    std::vector<std::pair<int, int>> todo;
    Clock::time_point deadline;
    std::thread::id fiberId;
    GenFiber* owner = nullptr;

    void give(int to) { // hand the turn over and wait for it to come back
        std::unique_lock<std::mutex> l(m);
        turn = to;
        cv.notify_all();
        cv.wait(l, [&] { return turn == 1 - to || (to == 0 && quit); });
    }
    void handBack() { give(0); }
    void loop() {
        {
            std::unique_lock<std::mutex> l(m);
            cv.wait(l, [&] { return turn == 1; });
        }
        while (!quit) {
            while (!todo.empty() && !quit) {
                std::pair<int, int> c = todo.front();
                todo.erase(todo.begin());
                if (!world->getChunk(c.first, c.second)) world->provideChunk(c.first, c.second);
            }
            owner->working = false;
            handBack();
        }
        std::lock_guard<std::mutex> l(m);
        turn = 0;
        cv.notify_all();
    }
};

bool GenFiber::start(World& w) {
    stop();
    Impl* i = new Impl();
    i->world = &w;
    i->owner = this;
    i->thread = std::thread([i] { i->loop(); });
    i->fiberId = i->thread.get_id();
    impl = i;
    w.onYieldPoint = [this] { maybeYield(); };
    return true;
}

void GenFiber::stop() {
    if (!impl) return;
    while (working) run(1000.0f);
    impl->world->onYieldPoint = nullptr;
    {
        std::lock_guard<std::mutex> l(impl->m);
        impl->quit = true;
        impl->turn = 1;
        impl->cv.notify_all();
    }
    impl->thread.join();
    delete impl;
    impl = nullptr;
}

void GenFiber::request(const std::vector<std::pair<int, int>>& chunks) {
    if (!impl || working || chunks.empty()) return;
    impl->todo = chunks;
    working = true;
}

void GenFiber::run(float budgetMs) {
    if (!impl || !working) return;
    impl->deadline = Clock::now() + std::chrono::microseconds(int64_t(budgetMs * 1000.0f));
    impl->give(1);
}

bool GenFiber::onFiber() const { return impl && std::this_thread::get_id() == impl->fiberId; }

void GenFiber::yieldNow() {
    if (onFiber()) impl->handBack();
}

void GenFiber::maybeYield() {
    if (!onFiber() || Clock::now() < impl->deadline) return;
    impl->handBack();
}
#endif
