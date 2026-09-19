# OS Components in C++

A hands-on learning project for building core operating system internals from scratch using modern C++.

---

## Goal

Understand how operating systems work by implementing their key components yourself — no black boxes. Each module is self-contained, buildable, and heavily commented to explain the *why*, not just the *what*.

---

## Project Structure

```
os-components-cpp/
├── README.md
├── CMakeLists.txt          # Top-level build config
│
├── 01-memory-manager/      # Memory allocation & management
├── 02-process-scheduler/   # CPU scheduling algorithms
├── 03-thread-library/      # User-space threading (fibers/coroutines)
├── 04-ipc/                 # Inter-process communication (pipes, semaphores, queues)
├── 05-virtual-memory/      # Paging, page tables, TLB simulation
├── 06-file-system/         # A simple file system (FAT or ext2-like)
├── 07-shell/               # A minimal Unix-like shell
├── 08-bootloader/          # (Advanced) x86 bootloader in C++ + ASM
└── docs/                   # Notes, diagrams, references
```

---

## Modules

### 1. Memory Manager
Build a heap allocator from scratch.
- `malloc` / `free` equivalents using a free-list or buddy system
- Fragmentation handling
- Memory pool allocator
- **Concepts:** heap layout, pointer arithmetic, alignment, sbrk/mmap

### 2. Process Scheduler
Simulate how an OS decides which process runs next.
- First-Come-First-Served (FCFS)
- Shortest Job First (SJF)
- Round Robin with configurable quantum
- Priority scheduling with aging (to prevent starvation)
- **Concepts:** PCB (Process Control Block), context switching, scheduling queues

### 3. Thread Library
Implement cooperative user-space threads without using `pthreads`.
- Context save/restore using `ucontext_t` or `setjmp/longjmp`
- Thread create, yield, join, exit
- Simple mutex (spinlock and sleep-based)
- **Concepts:** stack frames, execution context, synchronization primitives

### 4. Inter-Process Communication (IPC)
Implement classic IPC mechanisms.
- Named and unnamed pipes
- Message queues (FIFO)
- Shared memory segments
- Semaphores (counting and binary)
- **Concepts:** producer-consumer problem, deadlock, race conditions

### 5. Virtual Memory Simulator
Simulate paging and address translation.
- Page table walk (single-level and multi-level)
- TLB (Translation Lookaside Buffer) simulation
- Page replacement algorithms: FIFO, LRU, Optimal, Clock
- **Concepts:** virtual vs physical addresses, page faults, working set

### 6. File System
Build a simple file system stored in a flat binary file.
- Superblock, inode table, data blocks
- Create, read, write, delete files and directories
- Basic journaling for crash recovery
- **Concepts:** inodes, block allocation, directory entries, VFS layer

### 7. Shell
Write a minimal Unix-like command interpreter.
- Command parsing and tokenization
- `fork` + `exec` to launch processes
- Pipes (`cmd1 | cmd2`), redirection (`>`, `<`, `>>`)
- Background jobs (`&`), signal handling (`Ctrl+C`, `Ctrl+Z`)
- **Concepts:** process creation, file descriptors, signals, job control

### 8. Bootloader (Advanced)
Write a tiny x86 bootloader that boots into a C++ kernel.
- 16-bit real mode → 32-bit protected mode switch
- Load and jump to a C++ kernel entry point
- Print text to VGA framebuffer
- **Concepts:** x86 boot process, GDT, segmentation, linker scripts

---

## Prerequisites

| Area | What you need |
|------|--------------|
| C++ | Comfortable with C++17 (pointers, templates, RAII) |
| C | Basic C knowledge (useful for syscalls, low-level APIs) |
| OS Theory | Optional but helpful — read alongside an OS textbook |
| Tools | `g++`/`clang++`, `cmake`, `make`, `gdb` |

---

## Build & Run

Each module has its own `CMakeLists.txt`. To build a single module:

```bash
cd 02-process-scheduler
mkdir build && cd build
cmake ..
make
./scheduler
```

To build everything from the root:

```bash
mkdir build && cd build
cmake ..
make
```

---

## Learning Path (Recommended Order)

1. **Memory Manager** — fundamentals of pointers and manual memory
2. **Process Scheduler** — understand the CPU time-sharing illusion
3. **Thread Library** — see how concurrency is actually implemented
4. **IPC** — learn how isolated processes communicate
5. **Virtual Memory** — understand why every process has its "own" RAM
6. **File System** — persist data beyond process lifetime
7. **Shell** — tie it all together with a real user-facing interface
8. **Bootloader** — go all the way to bare metal (optional, advanced)

---

## Recommended Reading

- **Operating Systems: Three Easy Pieces** — Arpaci-Dusseau (free at ostep.org)
- **Modern Operating Systems** — Tanenbaum
- **The Linux Programming Interface** — Kerrisk (for Linux syscall details)
- **Computer Systems: A Programmer's Perspective (CS:APP)** — Bryant & O'Hallaron

---

## C++ Concepts You Will Practice

- Raw pointers and manual memory management
- Templates and generic data structures
- RAII and smart pointers (where appropriate)
- `std::atomic` and memory ordering
- Low-level system calls via POSIX APIs
- Bit manipulation and bitfields
- Linker scripts and binary layout (bootloader module)

---

## License

MIT License — see [LICENSE](LICENSE) for details.

Feel free to use, modify, fork, and learn from this project.
