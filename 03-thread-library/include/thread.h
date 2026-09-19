#pragma once
#include <ucontext.h>
#include <cstddef>
#include <functional>
#include <vector>

namespace threading {

enum class ThreadState { Ready, Running, Terminated };

// 64 KB is generous for our tiny demo functions. A real thread gets a
// much larger virtual stack that's only lazily backed by physical pages
// as it's touched -- we don't simulate that, this buffer is fully
// resident the moment the Thread is constructed.
constexpr std::size_t kStackSize = 64 * 1024;

// A "thread" here is nothing more than a dedicated stack plus a saved
// CPU context (registers, stack pointer, program counter) pointing
// somewhere on that stack. That pair is the entire concept -- there's
// no kernel object, no scheduling entity beyond this struct.
struct Thread {
    ucontext_t context{};
    std::vector<char> stack;
    std::function<void()> entry;
    ThreadState state = ThreadState::Ready;

    // `on_finish` is the context control transfers to if `entry` ever
    // returns normally, via ucontext_t's uc_link mechanism -- without
    // it, returning from `entry` would terminate the whole process
    // (see thread.cpp).
    Thread(std::function<void()> entry, ucontext_t* on_finish);
};

// Switches the CPU onto `t`, saving the caller's own state into `*from`
// first. If `t` hasn't run yet, execution begins at the top of its
// entry function; if it's been yielded before, execution resumes
// exactly where it left off, because ucontext_t's saved program
// counter and stack pointer make "exactly where it left off" a literal
// fact, not an approximation.
void resume(Thread& t, ucontext_t* from);

// Called from *inside* a running thread's own entry function: suspends
// it and switches back to whichever context last called resume() on it.
// This is the cooperative half of the model -- nothing preempts a
// thread, it must call yield() itself to give up the CPU.
void yield();

// Called from inside a running thread's own entry function to terminate
// it immediately -- skipping any code after this call, even code in
// functions further down the call stack. Mirrors pthread_exit(): like
// the real thing, this does NOT unwind the C++ call stack, so any local
// objects with non-trivial destructors on that stack never get
// destroyed. A normal return from entry() doesn't have this problem.
//
// Not marked [[noreturn]]: that would be accurate only as long as the
// scheduler never resumes a Terminated thread, and asserting it via the
// attribute would turn a violation of that invariant into undefined
// behavior instead of a normal (debuggable) bug.
void exit();

// Called from inside one thread to block until `target` terminates.
// There's no real blocking here -- no OS wait queue, no removal from
// any ready list -- it's just a loop that yields and rechecks, relying
// on the scheduler to keep giving `target` turns in the meantime. A
// real join() suspends the caller without spending any CPU time on it;
// this one busy-waits, spending one scheduler turn per check.
void join(const Thread& target);

} // namespace threading
