#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "scheduler.h"
#include "thread.h"

using threading::Scheduler;

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

    std::printf("\nAll tests passed.\n");
}
