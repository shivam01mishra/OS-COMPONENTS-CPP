# Memory Manager

## What problem does it solve?

A program needs to request and release memory at runtime, in amounts and
lifetimes it can't know at compile time. Something has to track which
regions of a raw block of memory are in use and which are free, and
hand out chunks that satisfy alignment and size requirements without
overlapping.

## How real OSes handle it

Linux's `malloc` (via glibc) is layered: user code calls `malloc`, which
is serviced by an allocator (ptmalloc2/dlmalloc-derived) using per-thread
arenas, small-object bins, and a fast-path free list, falling back to
`brk`/`sbrk` to grow the heap or `mmap` for large allocations. The kernel
itself never sees individual `malloc` calls — it only sees the much
rarer `brk`/`mmap` syscalls when the allocator needs more virtual
memory from the OS. This module doesn't attempt any of that layering —
no arenas, no per-size bins, no syscalls — just the single core idea:
track free/used regions of one buffer.

## Our simplified design

A single flat buffer (`char heap[4096]` in the demo) is carved up with
an explicit free-list allocator: a linked list of variable-size blocks,
each starting with a header, threaded through the buffer itself (no
separate metadata array). `allocate` does a first-fit linear scan;
`deallocate` marks a block free and immediately coalesces adjacent free
blocks in one pass.

The free-list logic is wrapped in an `mm::Allocator` class rather than
free functions mutating a hidden global — same algorithm, but state
(the free list) has clear ownership, and an instance could in principle
manage more than one independent heap.

## Data structures

```cpp
class Allocator {
    struct BlockHeader {
        std::size_t  size;  // usable bytes, NOT counting this header
        bool         free;
        BlockHeader* next;
    };
    BlockHeader* head_ = nullptr;
    ...
};
```
The header sits immediately before the memory handed back to the
caller, so `ptr - sizeof(BlockHeader)` recovers it in O(1) — the same
trick real allocators use to avoid a side table.

## Implementation decisions

- **First-fit, not best-fit**: simplest correct policy; best-fit would
  reduce fragmentation slightly but adds complexity disproportionate to
  the learning value here.
- **Split only when the leftover is worth it** (`>= kHeaderSize + kAlign`):
  splitting a block that's barely bigger than the request would waste
  an entire header on a near-useless sliver.
- **Coalesce on every deallocate**, not lazily/on a separate compaction
  pass: keeps the free list compact with no extra bookkeeping (no
  "dirty" flag, no periodic sweep needed).
- **Alignment via `alignof(std::max_align_t)`**: matches what a real
  `malloc` guarantees (safe for any built-in type), computed once as a
  constant rather than parameterized per-call.
- **Raw pointer arithmetic confined to three named helpers**
  (`header_before`, `payload_after`, `header_at_offset`) instead of
  inline `reinterpret_cast` chains scattered through `allocate`/
  `deallocate`/`reallocate`. The underlying arithmetic is unavoidable —
  it's the actual mechanism a header-before-payload allocator needs —
  but naming it means the higher-level methods read close to
  pseudocode instead of being dominated by casts.
- **State wrapped in a class, not a file-local global** (`g_head` in
  the original version): makes ownership explicit and the API easier
  to reason about, without changing the free-list algorithm itself.

## Important bugs

- **Header/`.cpp` namespace mismatch** in the pre-refactor version:
  `allocator.h` declared everything in `namespace mm`, but
  `allocator.cpp` defined it all in `namespace memory_manager` (the
  closing brace comment even incorrectly claimed `// namespace mm`).
  This module had never actually been built in this project's history
  up to that point — the mismatch would have caused a **linker error**
  (`undefined reference to mm::heap_init` etc.) the first time anyone
  tried to compile it, since the declared symbols and defined symbols
  lived in different namespaces. Caught by re-reading the source
  carefully before treating "it looks complete" as "it works," and
  confirmed by actually compiling and running the demo (all tests
  pass) after the fix.

## What I learned

- Why a header must live *before* the returned pointer, not in a
  separate table: it makes `deallocate(ptr)` O(1) with pure pointer
  arithmetic, at the cost of every allocation carrying a fixed
  overhead.
- Internal fragmentation (wasted space *inside* an allocated block,
  from alignment/rounding) vs. external fragmentation (free space
  split into pieces too small individually to satisfy a request) are
  different problems with different mitigations — this allocator only
  addresses the second, via coalescing.
- Coalescing eagerly (every deallocate) trades a bit of `deallocate()`
  cost for a free list that's always in its simplest possible state —
  no separate defragmentation step is ever needed.
- A module "looking done" (compiles in isolation as a header, has a
  full-featured `main.cpp`) is not the same as it being verified —
  this one had a real linker-breaking bug that only surfaced by
  actually building it.

## Limitations

- Single heap per `Allocator` instance is supported by the class, but
  not thread-safe (no locking around `head_`) — concurrent use from
  multiple threads would race.
- No fragmentation statistics or memory-pool support — deliberately
  out of scope; the goal here was understanding one free-list allocator
  well, not reproducing every feature a real allocator has.
- `assert(!blk->free && "double-free")` catches double-free only in
  debug builds; a release build (`NDEBUG`) would silently corrupt the
  free list on a double-free.
- First-fit degrades to O(n) per `allocate` call as the free list
  grows — fine for a 4 KB demo heap, not for a real allocator at scale.
