# Process Scheduler

## What problem does it solve?

With more runnable processes than CPU cores, something has to decide
which process runs next, for how long, and in what order — the
mechanism that creates the illusion of many programs running "at
once" on limited hardware.

## How real OSes handle it

Linux's default scheduler (CFS, Completely Fair Scheduler) doesn't use
any of FCFS/SJF/RR/Priority directly. It tracks each runnable task's
`vruntime` (virtual runtime, roughly "how much CPU time it's had,
weighted by priority/`nice` value") in a red-black tree and always
picks the task with the smallest `vruntime` — conceptually closer to
our priority-with-aging scheduler than to Round Robin, though the data
structure and the trigger for switching (timer interrupts, not a
library call) are entirely different. Real scheduling also deals with
I/O-blocked processes, multiple CPUs, and cache affinity, none of
which this module models.

## Our simplified design

Four scheduling policies operating on the same in-memory process
model, no real OS processes involved — a discrete-event simulation of
scheduling *decisions*, not of execution itself:

- **FCFS** — sort by arrival time once, run each to completion in order.
- **SJF** (non-preemptive) — at each dispatch point, scan arrived-but-
  not-run processes and pick the smallest burst time.
- **Round Robin** — preemptive; a real FIFO ready queue, each process
  capped at a fixed time quantum per turn.
- **Priority + aging** — like SJF's scan-and-pick shape, but selecting
  on `priority - waited/aging_interval` instead of burst time, so a
  process's effective priority improves the longer it waits.

## Data structures

```cpp
enum class ProcessState { New, Ready, Running, Terminated };

struct Process {
    int pid, arrival_time, burst_time, remaining_time, priority;
    ProcessState state;
};

struct Metrics {
    int pid, completion_time, turnaround_time, waiting_time;
};
```
`Metrics` is deliberately separate from `Process` — completion/
turnaround/waiting time are *derived* from a simulation run, not
intrinsic properties of a process, so they can never go stale the way
a cached field could.

## Implementation decisions

- **FCFS presorts; SJF and Priority scan-and-pick per dispatch.** FCFS's
  order is knowable upfront; SJF/Priority's isn't, because eligibility
  depends on the simulated clock as it advances — you can't presort by
  burst time or priority without also accounting for arrival time.
- **SJF/Priority are O(n²)**, not a min-heap fed by an arrival queue
  (O(n log n)). At the process counts this simulator will ever run
  (tens, for hand-verifiable demos), the nested scan is far more
  obviously correct and easier to trace than coordinating two data
  structures — the heap version would only be worth building if this
  needed to scale to thousands of processes.
- **Round Robin admits newly-arrived processes to the ready queue
  *before* re-queueing a just-preempted process.** Getting this
  backwards would let the preempted process unfairly cut in front of
  processes that had genuinely been waiting longer — a correctness bug
  that no compiler or type system would catch, only a carefully chosen
  test case exposes it.
- **Aging formula uses integer division** (`waited / aging_interval`),
  extracted into a single lambda in `priority_scheduling` so the two
  comparison call sites can't silently drift out of sync with each
  other.

## Important bugs

- Initial project scaffolding had a typo (`ShortTermSceduler.cpp`) and
  a two-level long-term/short-term scheduler design that was abandoned
  in favor of the single-queue model actually implemented — caught and
  corrected before any scheduling logic was written, by explicitly
  confirming scope up front instead of building around a name that
  had already been typed.
- No implementation bugs surfaced after the fact: every algorithm was
  checked against a hand-worked Gantt-chart trace *before* being
  trusted, and each was compiled and run to confirm the program's
  actual output matched the hand computation exactly.

## What I learned

- "Waiting time" and "turnaround time" are derived quantities
  (`turnaround = completion - arrival`, `waiting = turnaround - burst`)
  — computing them from timestamps instead of tracking running counters
  avoids an entire class of update-forgot-to-happen bugs.
- Fairness and average-case efficiency are different, sometimes
  competing goals: in the aging demo, average waiting time was
  *identical* with and without aging (2.67) — aging only redistributed
  which process paid the waiting cost, it didn't reduce the total.
- A scheduling algorithm's correctness is easy to get subtly wrong
  (see the Round Robin ordering point above) in ways that "the code
  compiles and runs" will never catch — only a deliberately chosen
  example that exercises the edge case will.

## Limitations

- No I/O-blocked state is modeled — real processes alternate between
  CPU bursts and I/O waits; this simulator treats every process as one
  uninterrupted CPU burst.
- No multi-core, no cache affinity, no priority inversion handling.
- Round Robin's quantum has no associated context-switch cost — a real
  OS pays real overhead per switch, which is exactly why quantum can't
  be made arbitrarily small; this simulator doesn't penalize that choice.
- Aging uses a simple linear formula, not the more elaborate schemes
  real schedulers use (e.g. CFS's weighted vruntime).
