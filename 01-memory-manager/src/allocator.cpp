#include "allocator.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace mm {

std::size_t Allocator::align_up(std::size_t n) {
    return (n + kAlign - 1) & ~(kAlign - 1);
}

Allocator::BlockHeader* Allocator::header_before(void* payload) {
    return reinterpret_cast<BlockHeader*>(static_cast<char*>(payload) - kHeaderSize);
}

void* Allocator::payload_after(BlockHeader* header) {
    return reinterpret_cast<char*>(header) + kHeaderSize;
}

Allocator::BlockHeader* Allocator::header_at_offset(BlockHeader* header, std::size_t bytes) {
    return reinterpret_cast<BlockHeader*>(reinterpret_cast<char*>(header) + bytes);
}

void Allocator::init(void* memory, std::size_t size) {
    assert(size > kHeaderSize);
    head_       = static_cast<BlockHeader*>(memory);
    head_->size = size - kHeaderSize;
    head_->free = true;
    head_->next = nullptr;
}

void* Allocator::allocate(std::size_t size) {
    if (size == 0) return nullptr;
    size = align_up(size);

    for (BlockHeader* blk = head_; blk; blk = blk->next) {
        if (!blk->free || blk->size < size) continue;

        // Split only when the leftover can hold a header + at least one
        // aligned word. Splitting a block that's barely large enough
        // just wastes a header on a near-useless sliver.
        if (blk->size >= size + kHeaderSize + kAlign) {
            BlockHeader* tail = header_at_offset(blk, kHeaderSize + size);
            tail->size = blk->size - size - kHeaderSize;
            tail->free = true;
            tail->next = blk->next;
            blk->size  = size;
            blk->next  = tail;
        }

        blk->free = false;
        return payload_after(blk);
    }

    return nullptr; // out of memory
}

void Allocator::deallocate(void* ptr) {
    if (!ptr) return;

    BlockHeader* blk = header_before(ptr);
    assert(!blk->free && "double-free");
    blk->free = true;

    // Single forward pass: merge any two adjacent free blocks. Running
    // this after every deallocate keeps the list compact without a
    // separate compaction step.
    for (BlockHeader* cur = head_; cur && cur->next; ) {
        if (cur->free && cur->next->free) {
            cur->size += kHeaderSize + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void* Allocator::reallocate(void* ptr, std::size_t new_size) {
    if (!ptr)      return allocate(new_size);
    if (!new_size) { deallocate(ptr); return nullptr; }

    BlockHeader* blk = header_before(ptr);
    if (blk->size >= align_up(new_size)) return ptr;

    void* dst = allocate(new_size);
    if (!dst) return nullptr;
    std::memcpy(dst, ptr, blk->size);
    deallocate(ptr);
    return dst;
}

void Allocator::dump() const {
    std::size_t i = 0;
    for (BlockHeader* blk = head_; blk; blk = blk->next, ++i)
        std::printf("  [%2zu]  addr=%p  size=%-6zu  %s\n",
                    i, static_cast<const void*>(blk), blk->size,
                    blk->free ? "FREE" : "USED");
}

} // namespace mm
