// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Iterators over the C89 intrusive list (common/intrusive_list.h), for the C++
// code that walks lists with range-for and std algorithms: the shader IR, the
// structurizer, the CFG and the sleep queues. Every change to a list is one
// of the C functions; this only adds iterators and the hook's constructor and
// destructor (a hook starts unlinked and takes itself out when its element
// dies, so a list never points at a dead element).

#include <cstddef>
#include <iterator>
#include <type_traits>

#include "common/intrusive_list.h"

namespace Common {

struct ListHook : list_hook_t {
    ListHook() noexcept {
        list_hook_init(this);
    }
    // A copied element starts out of any list
    ListHook(const ListHook&) noexcept : ListHook() {}
    ListHook& operator=(const ListHook&) noexcept {
        return *this;
    }
    ~ListHook() {
        list_hook_unlink(this);
    }
    bool is_linked() const noexcept {
        return list_hook_is_linked(this) != 0;
    }
    void unlink() noexcept {
        list_hook_unlink(this);
    }
};

template <typename T>
class IntrusiveList {
    template <bool Const>
    class Iter {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<Const, const T*, T*>;
        using reference = std::conditional_t<Const, const T&, T&>;

        Iter() noexcept = default;
        // Implicit, so an element pointer can be used where an iterator is wanted
        Iter(const list_hook_t* h) noexcept : hook{const_cast<list_hook_t*>(h)} {}
        template <bool C = Const, typename = std::enable_if_t<C>>
        Iter(const Iter<false>& o) noexcept : hook{o.Hook()} {}

        reference operator*() const noexcept {
            return *operator->();
        }
        pointer operator->() const noexcept {
            return static_cast<pointer>(
                static_cast<std::conditional_t<Const, const ListHook*, ListHook*>>(hook));
        }
        Iter& operator++() noexcept {
            hook = hook->next;
            return *this;
        }
        Iter operator++(int) noexcept {
            Iter t = *this;
            hook = hook->next;
            return t;
        }
        Iter& operator--() noexcept {
            hook = hook->prev;
            return *this;
        }
        Iter operator--(int) noexcept {
            Iter t = *this;
            hook = hook->prev;
            return t;
        }
        friend bool operator==(const Iter& a, const Iter& b) noexcept {
            return a.hook == b.hook;
        }
        list_hook_t* Hook() const noexcept {
            return hook;
        }

    private:
        list_hook_t* hook = nullptr;
    };

public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = Iter<false>;
    using const_iterator = Iter<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    IntrusiveList() noexcept {
        list_hook_init(&root);
    }
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList(IntrusiveList&& o) noexcept {
        list_hook_init(&root);
        list_take_all(&root, &o.root);
    }
    IntrusiveList& operator=(IntrusiveList&& o) noexcept {
        if (this != &o) {
            list_clear(&root);
            list_take_all(&root, &o.root);
        }
        return *this;
    }
    // The elements are left unlinked, not destroyed
    ~IntrusiveList() {
        list_clear(&root);
    }

    iterator begin() noexcept {
        return iterator{root.next};
    }
    iterator end() noexcept {
        return iterator{&root};
    }
    const_iterator begin() const noexcept {
        return const_iterator{root.next};
    }
    const_iterator end() const noexcept {
        return const_iterator{&root};
    }
    const_iterator cbegin() const noexcept {
        return begin();
    }
    const_iterator cend() const noexcept {
        return end();
    }
    reverse_iterator rbegin() noexcept {
        return reverse_iterator{end()};
    }
    reverse_iterator rend() noexcept {
        return reverse_iterator{begin()};
    }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator{end()};
    }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator{begin()};
    }
    const_reverse_iterator crbegin() const noexcept {
        return rbegin();
    }
    const_reverse_iterator crend() const noexcept {
        return rend();
    }

    bool empty() const noexcept {
        return !list_hook_is_linked(&root);
    }
    // Walks the list: elements can leave on their own, so no count is kept
    size_type size() const noexcept {
        return list_count(&root);
    }

    T& front() noexcept {
        return *begin();
    }
    const T& front() const noexcept {
        return *begin();
    }
    T& back() noexcept {
        return *iterator{root.prev};
    }
    const T& back() const noexcept {
        return *const_iterator{root.prev};
    }

    iterator insert(const_iterator pos, T& v) noexcept {
        list_hook_t* h = static_cast<ListHook*>(&v);
        list_hook_link_before(h, pos.Hook());
        return iterator{h};
    }
    void push_back(T& v) noexcept {
        insert(end(), v);
    }
    void push_front(T& v) noexcept {
        insert(begin(), v);
    }
    void pop_front() noexcept {
        list_hook_unlink(root.next);
    }
    void pop_back() noexcept {
        list_hook_unlink(root.prev);
    }

    // Takes the element out; it is not destroyed
    iterator erase(const_iterator pos) noexcept {
        list_hook_t* next = pos.Hook()->next;
        list_hook_unlink(pos.Hook());
        return iterator{next};
    }
    iterator erase(const_iterator first, const_iterator last) noexcept {
        while (first != last) {
            first = erase(first);
        }
        return iterator{last.Hook()};
    }
    void clear() noexcept {
        list_clear(&root);
    }

    // Moves [first, last) of @other (or of this list) in front of @pos
    void splice(const_iterator pos, IntrusiveList&, const_iterator first,
                const_iterator last) noexcept {
        list_splice_before(pos.Hook(), first.Hook(), last.Hook());
    }
    void splice(const_iterator pos, IntrusiveList& other) noexcept {
        list_splice_before(pos.Hook(), other.root.next, &other.root);
    }

    static iterator s_iterator_to(T& v) noexcept {
        return iterator{static_cast<ListHook*>(&v)};
    }
    static const_iterator s_iterator_to(const T& v) noexcept {
        return const_iterator{static_cast<const ListHook*>(&v)};
    }
    iterator iterator_to(T& v) noexcept {
        return s_iterator_to(v);
    }
    const_iterator iterator_to(const T& v) const noexcept {
        return s_iterator_to(v);
    }

private:
    list_hook_t root;
};

} // namespace Common
