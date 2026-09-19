#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "scheduler.h"
#include "thread.h"

using threading::Scheduler;
using threading::Thread;

static std::vector<std::string> trace; // records what actually ran, in order

static void log(const std::string& msg) {
    std::printf("%s\n", msg.c_str());
    trace.push_back(msg);
}

int main() {
    Scheduler scheduler;

    // Different step counts, deliberately: it's what actually exercises
    // round-robin behavior -- threads finishing at different times, and
    // the scheduler correctly skipping terminated ones instead of
    // resuming them again.
    scheduler.spawn([]{
        for (int i = 1; i <= 2; ++i) {
            log("[Thread A] step " + std::to_string(i));
            threading::yield();
        }
        log("[Thread A] done");
    });

    scheduler.spawn([]{
        for (int i = 1; i <= 3; ++i) {
            log("[Thread B] step " + std::to_string(i));
            threading::yield();
        }
        log("[Thread B] done");
    });

    scheduler.spawn([]{
        for (int i = 1; i <= 1; ++i) {
            log("[Thread C] step " + std::to_string(i));
            threading::yield();
        }
        log("[Thread C] done");
    });

    scheduler.run();
    log("[main] all threads finished");

    // Expected round-robin trace (spawn order A, B, C; skip terminated):
    //   round 1: A1 B1 C1
    //   round 2: A2 B2 (C finishes -> "C done")
    //   round 3: (A finishes -> "A done") B3
    //   round 4: (B finishes -> "B done")
    std::vector<std::string> expected = {
        "[Thread A] step 1", "[Thread B] step 1", "[Thread C] step 1",
        "[Thread A] step 2", "[Thread B] step 2", "[Thread C] done",
        "[Thread A] done",   "[Thread B] step 3",
        "[Thread B] done",
        "[main] all threads finished",
    };
    assert(trace == expected);

    // join() + exit(): Worker exits early via threading::exit(), so the
    // line after it must never run. Waiter joins on Worker before doing
    // anything else, so "worker finished" must print only after Worker
    // actually terminates -- regardless of round-robin timing.
    Scheduler scheduler2;
    trace.clear();

    Thread& worker = scheduler2.spawn([]{
        log("[Worker] step 1");
        threading::yield();
        log("[Worker] step 2");
        threading::exit();
        log("[Worker] UNREACHABLE");
    });

    scheduler2.spawn([&worker]{
        log("[Waiter] waiting for worker...");
        threading::join(worker);
        log("[Waiter] worker finished, resuming");
    });

    scheduler2.run();
    log("[main] join/exit demo finished");

    std::vector<std::string> expected2 = {
        "[Worker] step 1",
        "[Waiter] waiting for worker...",
        "[Worker] step 2",
        "[Waiter] worker finished, resuming",
        "[main] join/exit demo finished",
    };
    assert(trace == expected2);

    std::printf("\nAll tests passed.\n");
}
