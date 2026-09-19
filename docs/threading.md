# Thread Library

## What problem does it solve?

A single flow of execution can only do one thing at a time. Threads
let a program interleave multiple independent flows of execution
within one process, sharing the same address space — the foundation
concurrency (and eventually IPC/synchronization) is built on.

## How real OSes handle it

Real OS threads (pthreads on Linux) are scheduled preemptively by the
kernel: a timer interrupt can suspend a thread at any instruction,
save its full register state, and hand the CPU to another thread or
process, without either thread's cooperation. The kernel also handles
per-thread stacks, thread-local storage, and signal delivery. Our
model implements none of the preemption: threads only ever switch at a
point they explicitly choose (`yield()`), and everything runs on a
single underlying OS thread (a single "carrier thread") — there is no
real parallelism here, just interleaving.

## Our simplified design

```text
Thread            -- a stack + a saved ucontext_t
resume(t, from)   -- switch the CPU onto t, saving caller state into from
yield()           -- called from inside a thread, switch back to caller
exit()            -- called from inside a thread, terminate it permanently
join(target)      -- block the caller until target terminates
Scheduler         -- round-robin dispatcher over a set of spawned threads
```

Built with POSIX `ucontext_t` (`getcontext`/`makecontext`/`swapcontext`)
rather than `setjmp`/`longjmp`, because hijacking `longjmp`'s saved
stack pointer to resume on a *different* stack is undefined behavior
in standard C — `ucontext_t` is the API actually designed for this.

## Data structures

```cpp
struct Thread {
    ucontext_t context;
    std::vector<char> stack;      // 64 KB, dedicated to this thread
    std::function<void()> entry;
    ThreadState state;            // Ready, Running, Terminated
};
```
A "thread" is nothing more than this pair (stack + saved context) —
there's no kernel object, no scheduling entity beyond this struct.

## Implementation decisions

- **Global handoff (`g_starting`, `g_current`, `g_caller`) instead of
  passing a pointer through `makecontext`.** `makecontext` can only
  forward plain `int` arguments (a 32-bit-era API wart), so it can't
  hand a `std::function` or even a raw `Thread*` to the trampoline
  function directly. The globals are safe *only* because this is a
  cooperative, single-carrier-thread model: exactly one thread is ever
  "being started" or "currently running" at any instant. This would
  break instantly under real preemption or multiple carrier threads.
- **`uc_link` wired to a scheduler-owned context.** Without it, a
  thread function returning normally would call `exit()` on the whole
  process — `ucontext_t`'s built-in "the function returned" handling
  needed somewhere to go.
- **`Scheduler` stores threads as `std::vector<std::unique_ptr<Thread>>`,
  not `std::vector<Thread>`.** Storing by value would let spawning a
  new thread reallocate the vector and relocate every existing
  `Thread`, corrupting a context that's mid-execution. Heap allocation
  keeps addresses stable regardless of how many more threads are
  spawned later — the same reason real kernels heap-allocate task
  structs.
- **No quantum in the thread scheduler**, unlike the CPU scheduler's
  Round Robin. A cooperative thread's "slice" is simply "until it next
  calls `yield()`," decided by the thread itself — there's no timer
  imposing a fixed slice length, which is the actual definition of
  cooperative vs. preemptive multitasking.
- **`exit()` is *not* marked `[[noreturn]]`**, even though it never
  returns under correct use. That attribute would turn a future
  scheduler bug (resuming an already-`Terminated` thread) into full
  undefined behavior instead of a normal, debuggable failure.
- **`join()` busy-waits** (`while (!terminated) yield();`) rather than
  truly blocking. A real `pthread_join()` removes the caller from the
  ready queue entirely and costs nothing while waiting; this
  implementation spends one scheduler turn per check. Called out
  explicitly as a simplification, not hidden as if it were equivalent.

## Important bugs

- **MSYS2/mingw64 `g++` has no `<ucontext.h>`** — it targets native
  Windows, which doesn't have this POSIX API at all. This module (and
  everything IPC/Shell-related after it) has to be built through the
  real WSL toolchain instead.
- **`wsl.exe` silently re-wraps commands through a second shell unless
  `--exec` is passed**, which was dropping shell variables between
  `;`-separated statements in a multi-line script (`Y=42; echo $Y`
  printed nothing). Diagnosed by tracing execution with `set -x` and
  noticing `Y` was already empty by the second statement — pointing to
  the command being split across two separate shell invocations rather
  than run as one script.

## What I learned

- A "thread" is a genuinely minimal concept at its core: one stack,
  one saved register/PC/SP snapshot. Everything else (scheduling
  policy, synchronization) is built on top of that pair, not part of
  what a thread *is*.
- `resume()`/`yield()` are the same primitive (`swapcontext`) used in
  both directions — the entire "cooperative multitasking" mechanism is
  one `swapcontext` call, called from two different roles.
- `exit()` turned out to differ from `yield()` by exactly one line
  (`Terminated` instead of `Ready`) — a good sign the abstraction was
  factored at the right level, since the "permanent" and "temporary"
  suspend cases share almost everything.
- Environment assumptions are easy to get wrong invisibly: the fact
  that a Windows-native compiler was silently being used for a
  POSIX-dependent module could have produced confusing build failures
  much later (in IPC/Shell) if it hadn't been checked before writing
  any `ucontext_t` code.

## Limitations

- Cooperative only — a thread that never calls `yield()` (e.g. an
  infinite loop with no yield point) starves every other thread
  forever; there's no preemption to save you from that.
- Single carrier thread — no real parallelism, and the global-handoff
  trick in `thread.cpp` would need to be replaced with thread-local
  storage or an explicit parameter if that ever changed.
- No stack-overflow protection (no guard page) — overflowing a 64 KB
  thread stack silently corrupts adjacent memory rather than crashing
  cleanly.
- `join()` busy-waits instead of truly blocking (see above).
- `exit()` does not run C++ destructors for objects on the terminated
  thread's stack, matching a real limitation of `pthread_exit()` in C++.
