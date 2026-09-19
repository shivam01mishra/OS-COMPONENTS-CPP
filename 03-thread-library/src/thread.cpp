#include "thread.h"

namespace threading {

namespace {

// makecontext() can only forward plain `int` arguments to the function
// it starts -- a 32-bit-era wart in an API that predates 64-bit
// pointers fitting in one register. It can't hand `trampoline` a
// std::function or even a raw Thread* directly. Rather than split a
// 64-bit pointer across two int arguments (the traditional portable
// workaround), we hand off through this global instead.
//
// This is safe ONLY because our model is cooperative with a single
// carrier thread: at most one Thread is ever "being started for the
// first time" at any instant. A preemptive or multi-threaded scheduler
// could not get away with this.
Thread* g_starting = nullptr;

// Tracks whichever Thread is currently executing, so yield() (called
// from inside that thread's own entry function, with no argument) knows
// whose context to save. Same single-carrier-thread assumption as above.
Thread* g_current = nullptr;

// Where yield() should switch back to: whichever context most recently
// called resume().
ucontext_t* g_caller = nullptr;

void trampoline() {
    Thread* self = g_starting;
    self->entry();
    self->state = ThreadState::Terminated;
    // Falling off the end here resumes *context.uc_link (set to
    // `on_finish` below) instead of terminating the process -- that's
    // ucontext_t's built-in handling for "the thread function returned".
}

} // namespace

Thread::Thread(std::function<void()> entry_fn, ucontext_t* on_finish)
    : stack(kStackSize), entry(std::move(entry_fn)) {
    getcontext(&context);
    context.uc_stack.ss_sp   = stack.data();
    context.uc_stack.ss_size = stack.size();
    context.uc_link          = on_finish;
    makecontext(&context, reinterpret_cast<void (*)()>(trampoline), 0);
}

void resume(Thread& t, ucontext_t* from) {
    if (t.state == ThreadState::Ready) g_starting = &t;
    g_current = &t;
    g_caller  = from;
    t.state   = ThreadState::Running;
    swapcontext(from, &t.context);
}

void yield() {
    g_current->state = ThreadState::Ready;
    swapcontext(&g_current->context, g_caller);
}

void exit() {
    g_current->state = ThreadState::Terminated;
    swapcontext(&g_current->context, g_caller);
    // Unreachable under correct use: the scheduler skips Terminated
    // threads, so nothing will ever swap back into this context again.
}

void join(const Thread& target) {
    while (target.state != ThreadState::Terminated) {
        yield();
    }
}

} // namespace threading
