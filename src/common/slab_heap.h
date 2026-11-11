// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <retro_atomic.h>
#include "common/assert.h"
#include "common/types.h"

namespace Common {

class SlabHeapImpl {
public:
    struct Node {
        Node* next{};
    };

public:
    constexpr SlabHeapImpl() = default;

    void Initialize() {
        ASSERT(Pointer(static_cast<u64>(retro_atomic_load_acquire_64(&m_head))) == nullptr);
    }

    Node* GetHead() const {
        return Pointer(static_cast<u64>(retro_atomic_load_acquire_64(&m_head)));
    }

    /// Pops the free list: a compare-exchange on its head, which carries a tag that every
    /// change bumps, so a head popped and pushed back meanwhile (ABA) fails the exchange. The
    /// tag sits in the pointer's top 16 bits, which user addresses leave clear on the 48-bit
    /// x86-64 and arm64 address spaces this runs on. A node read while another thread takes it
    /// is still slab memory, never unmapped, and its stale next is thrown away by the failed
    /// exchange.
    void* Allocate() {
        u64 head = static_cast<u64>(retro_atomic_load_acquire_64(&m_head));
        for (;;) {
            Node* node = Pointer(head);
            if (node == nullptr) {
                return nullptr;
            }
            const u64 next = Pack(node->next, Tag(head) + 1);
            if ((retro_atomic_cas_64(&m_head, head, next) ||
                 ((head = static_cast<u64>(retro_atomic_load_acquire_64(&m_head))), false))) {
                return node;
            }
        }
    }

    void Free(void* obj) {
        Node* node = static_cast<Node*>(obj);
        u64 head = static_cast<u64>(retro_atomic_load_relaxed_64(&m_head));
        do {
            node->next = Pointer(head);
        } while (!(retro_atomic_cas_64(&m_head, head, Pack(node, Tag(head) + 1)) ||
                   ((head = static_cast<u64>(retro_atomic_load_acquire_64(&m_head))), false)));
    }

private:
    static constexpr u64 PointerMask = (u64{1} << 48) - 1;

    static Node* Pointer(u64 word) {
        return reinterpret_cast<Node*>(static_cast<uintptr_t>(word & PointerMask));
    }
    static u64 Tag(u64 word) {
        return word >> 48;
    }
    static u64 Pack(Node* node, u64 tag) {
        const u64 address = reinterpret_cast<uintptr_t>(node);
        ASSERT_MSG((address & ~PointerMask) == 0, "Slab address beyond 48 bits");
        return address | ((tag & 0xFFFF) << 48);
    }

    retro_atomic_64_t m_head{};
};

class SlabHeapBase : protected SlabHeapImpl {
private:
    size_t m_obj_size{};
    uintptr_t m_peak{};
    uintptr_t m_start{};
    uintptr_t m_end{};

public:
    constexpr SlabHeapBase() = default;

    bool Contains(uintptr_t address) const {
        return m_start <= address && address < m_end;
    }

    void Initialize(size_t obj_size, void* memory, size_t memory_size) {
        // Ensure we don't initialize a slab using null memory.
        ASSERT(memory != nullptr);

        // Set our object size.
        m_obj_size = obj_size;

        // Initialize the base allocator.
        SlabHeapImpl::Initialize();

        // Set our tracking variables.
        const size_t num_obj = (memory_size / obj_size);
        m_start = reinterpret_cast<uintptr_t>(memory);
        m_end = m_start + num_obj * obj_size;
        m_peak = m_start;

        // Free the objects.
        u8* cur = reinterpret_cast<u8*>(m_end);

        for (size_t i = 0; i < num_obj; i++) {
            cur -= obj_size;
            SlabHeapImpl::Free(cur);
        }
    }

    size_t GetSlabHeapSize() const {
        return (m_end - m_start) / this->GetObjectSize();
    }

    size_t GetObjectSize() const {
        return m_obj_size;
    }

    void* Allocate() {
        void* obj = SlabHeapImpl::Allocate();
        return obj;
    }

    void Free(void* obj) {
        // Don't allow freeing an object that wasn't allocated from this heap.
        const bool contained = this->Contains(reinterpret_cast<uintptr_t>(obj));
        ASSERT(contained);
        SlabHeapImpl::Free(obj);
    }

    size_t GetObjectIndex(const void* obj) const {
        return (reinterpret_cast<uintptr_t>(obj) - m_start) / this->GetObjectSize();
    }

    size_t GetPeakIndex() const {
        return this->GetObjectIndex(reinterpret_cast<const void*>(m_peak));
    }

    uintptr_t GetSlabHeapAddress() const {
        return m_start;
    }

    size_t GetNumRemaining() const {
        // Only calculate the number of remaining objects under debug configuration.
        return 0;
    }
};

template <typename T>
class SlabHeap final : public SlabHeapBase {
private:
    using BaseHeap = SlabHeapBase;

public:
    constexpr SlabHeap() = default;

    void Initialize(void* memory, size_t memory_size) {
        BaseHeap::Initialize(sizeof(T), memory, memory_size);
    }

    T* Allocate() {
        T* obj = static_cast<T*>(BaseHeap::Allocate());

        if (obj != nullptr) [[likely]] {
            std::construct_at(obj);
        }
        return obj;
    }

    void Free(T* obj) {
        std::destroy_at(obj);
        BaseHeap::Free(obj);
    }

    size_t GetObjectIndex(const T* obj) const {
        return BaseHeap::GetObjectIndex(obj);
    }
};

} // namespace Common
