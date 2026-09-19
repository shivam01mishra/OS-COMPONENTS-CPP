#pragma once
#include <functional>
#include <memory>
#include <vector>
#include "thread.h"

namespace threading {

// Cooperative round-robin dispatcher: repeatedly cycles through every
// not-yet-terminated thread in spawn order, giving each one a turn
// until it calls yield() (or returns). This formalizes what the earlier
// demo faked by hand-calling resume() on two threads directly.
//
// Note there's no "quantum" here like the CPU scheduler module had --
// a cooperative thread's "slice" is simply "however long until it next
// calls yield()", decided by the thread itself, not imposed by a timer.
class Scheduler {
public:
    // Registers a new thread to run once run() is called, and returns a
    // reference to it -- e.g. so another thread can later join() on it.
    // Threads are heap-allocated (via unique_ptr) so their addresses
    // stay stable as more are spawned -- a vector of Thread by value
    // would relocate existing threads on reallocation, invalidating
    // exactly the kind of reference this function hands out. Real
    // kernels heap-allocate task structs for the same reason.
    Thread& spawn(std::function<void()> entry);

    // Runs every spawned thread to completion, round-robin. Must be
    // called after all spawn() calls -- this simple scheduler doesn't
    // support adding threads mid-run.
    void run();

private:
    ucontext_t scheduler_ctx_{};
    std::vector<std::unique_ptr<Thread>> threads_;
};

} // namespace threading
