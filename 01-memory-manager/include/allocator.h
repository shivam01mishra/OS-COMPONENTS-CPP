#pragma once
#include <cstddef>

namespace mm {

// A heap allocator over a single pre-allocated buffer, using a
// singly-linked free list threaded through the buffer itself (no
// separate metadata array). This is our stand-in for malloc/free:
// same contract (raw void* in, raw void* out, caller tracks size),
// implemented with a first-fit search and eager coalescing.
class Allocator {
public:
    // Carves `memory[0..size)` into one big free block. Must be called
    // before any allocate()/deallocate() call, and `memory` must
    // outlive the Allocator.
    void init(void* memory, std::size_t size);

    // Returns a pointer to at least `size` usable, aligned bytes, or
    // nullptr if no free block is large enough (out of memory) or
    // size == 0.
    void* allocate(std::size_t size);

    // Returns a block obtained from allocate() back to the free list.
    // Passing nullptr is a no-op, matching free()'s contract.
    void deallocate(void* ptr);

    // Grows/shrinks a block, preserving its contents up to the smaller
    // of the old and new sizes. nullptr in behaves like allocate();
    // new_size == 0 behaves like deallocate() and returns nullptr.
    void* reallocate(void* ptr, std::size_t new_size);

    // Prints every block in the free list with its address, size, and
    // free/used state -- for inspecting the heap's layout while learning
    // or debugging, not part of the malloc/free contract itself.
    void dump() const;

private:
    struct BlockHeader {
        std::size_t  size;  // usable bytes, NOT counting this header
        bool         free;
        BlockHeader* next;
    };

    static constexpr std::size_t kAlign      = alignof(std::max_align_t);
    static constexpr std::size_t kHeaderSize = sizeof(BlockHeader);

    static std::size_t align_up(std::size_t n);

    // The three places raw pointer arithmetic happens, named so the
    // rest of the class reads in terms of "the header before this
    // payload" / "the payload after this header" instead of inline
    // reinterpret_cast chains.
    static BlockHeader* header_before(void* payload);
    static void*         payload_after(BlockHeader* header);
    static BlockHeader*  header_at_offset(BlockHeader* header, std::size_t bytes);

    BlockHeader* head_ = nullptr;
};

} // namespace mm
