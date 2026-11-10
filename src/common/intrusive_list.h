// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// A doubly linked list threaded through its elements, in the shape of the C
// lists kernels use: each element carries a ListHook (two pointers) as a base,
// and the list is a ring through one more hook it owns. Nothing is allocated;
// inserting or erasing relinks four pointers. It replaces
// boost::intrusive::list and list_base_hook.
//
// A hook that is not in a list points at itself. One that is destroyed while
// still linked takes itself out (Boost's auto_unlink), so a list never points
// at a dead element. Because elements can leave on their own, size() counts
// unless CountSize is set for a list whose elements only leave through it.

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace Common {

struct ListHook {
    ListHook* prev;
    ListHook* next;

    ListHook() noexcept : prev{this}, next{this} {}
    // A copied element starts out of any list
    ListHook(const ListHook&) noexcept : ListHook() {}
    ListHook& operator=(const ListHook&) noexcept {
        return *this;
    }
    ~ListHook() {
        unlink();
    }

    bool is_linked() const noexcept {
        return next != this;
    }
    // Takes this element out of whatever list holds it
    void unlink() noexcept {
        prev->next = next;
        next->prev = prev;
        prev = next = this;
    }

    // Links this (unlinked) hook in before @pos
    void LinkBefore(ListHook* pos) noexcept {
        prev = pos->prev;
        next = pos;
        pos->prev->next = this;
        pos->prev = this;
    }
};

template <typename T, bool CountSize = false>
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
        Iter(const ListHook* h) noexcept : hook{const_cast<ListHook*>(h)} {}
        // iterator -> const_iterator
        template <bool C = Const, typename = std::enable_if_t<C>>
        Iter(const Iter<false>& o) noexcept : hook{o.Hook()} {}

        reference operator*() const noexcept {
            return static_cast<reference>(*hook);
        }
        pointer operator->() const noexcept {
            return static_cast<pointer>(hook);
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

        ListHook* Hook() const noexcept {
            return hook;
        }

    private:
        ListHook* hook = nullptr;
    };

public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = Iter<false>;
    using const_iterator = Iter<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    IntrusiveList() noexcept = default;
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;
    IntrusiveList(IntrusiveList&& o) noexcept {
        TakeFrom(o);
    }
    IntrusiveList& operator=(IntrusiveList&& o) noexcept {
        if (this != &o) {
            clear();
            TakeFrom(o);
        }
        return *this;
    }
    // The elements are left unlinked, not destroyed
    ~IntrusiveList() {
        clear();
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
        return root.next == &root;
    }
    size_type size() const noexcept {
        if constexpr (CountSize) {
            return count;
        } else {
            size_type n = 0;
            for (const ListHook* h = root.next; h != &root; h = h->next) {
                ++n;
            }
            return n;
        }
    }

    T& front() noexcept {
        return static_cast<T&>(*root.next);
    }
    const T& front() const noexcept {
        return static_cast<const T&>(*root.next);
    }
    T& back() noexcept {
        return static_cast<T&>(*root.prev);
    }
    const T& back() const noexcept {
        return static_cast<const T&>(*root.prev);
    }

    iterator insert(const_iterator pos, T& v) noexcept {
        static_assert(std::is_base_of_v<ListHook, T>, "elements derive from Common::ListHook");
        ListHook* h = &v;
        h->LinkBefore(pos.Hook());
        Added(1);
        return iterator{h};
    }
    void push_back(T& v) noexcept {
        insert(end(), v);
    }
    void push_front(T& v) noexcept {
        insert(begin(), v);
    }
    void pop_front() noexcept {
        erase(begin());
    }
    void pop_back() noexcept {
        erase(iterator{root.prev});
    }

    // Takes the element out; it is not destroyed
    iterator erase(const_iterator pos) noexcept {
        ListHook* h = pos.Hook();
        ListHook* next = h->next;
        h->unlink();
        Added(-1);
        return iterator{next};
    }
    iterator erase(const_iterator first, const_iterator last) noexcept {
        while (first != last) {
            first = erase(first);
        }
        return iterator{last.Hook()};
    }
    void clear() noexcept {
        while (!empty()) {
            root.next->unlink();
        }
        count = 0;
    }

    // Moves [first, last) of @other in front of @pos
    void splice(const_iterator pos, IntrusiveList& other, const_iterator first,
                const_iterator last) noexcept {
        if (first == last) {
            return;
        }
        if constexpr (CountSize) {
            if (&other != this) {
                const size_type n = static_cast<size_type>(std::distance(first, last));
                other.count -= n;
                count += n;
            }
        }
        ListHook* f = first.Hook();
        ListHook* l = last.Hook()->prev; // the last one moved
        ListHook* p = pos.Hook();
        // out of the source
        f->prev->next = last.Hook();
        last.Hook()->prev = f->prev;
        // into place before pos
        f->prev = p->prev;
        l->next = p;
        p->prev->next = f;
        p->prev = l;
    }
    void splice(const_iterator pos, IntrusiveList& other) noexcept {
        splice(pos, other, other.begin(), other.end());
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
    void Added(int n) noexcept {
        if constexpr (CountSize) {
            count = static_cast<size_type>(static_cast<std::ptrdiff_t>(count) + n);
        }
    }

    // Moves @o's ring onto this list's root
    void TakeFrom(IntrusiveList& o) noexcept {
        if (o.empty()) {
            return;
        }
        root.next = o.root.next;
        root.prev = o.root.prev;
        root.next->prev = &root;
        root.prev->next = &root;
        o.root.next = o.root.prev = &o.root;
        count = o.count;
        o.count = 0;
    }

    ListHook root;
    size_type count = 0;
};

} // namespace Common
