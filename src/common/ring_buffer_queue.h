// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/types.h"

/// The last @N items pushed, oldest first: a full queue drops its oldest for a new one
template <class T, u32 N>
class RingBufferQueue {
public:
    void Push(const T& item) {
        const u32 index = (m_begin + m_size) % N;
        m_storage[index] = item;
        if (m_size < N) {
            m_size += 1;
        } else {
            m_begin = (m_begin + 1) % N;
        }
    }

    /// Moves the oldest item into *@out; false when there is none
    bool Pop(T* out) {
        if (m_size == 0) {
            return false;
        }
        const u32 index = m_begin;
        m_begin = (m_begin + 1) % N;
        m_size -= 1;
        *out = static_cast<T&&>(m_storage[index]);
        return true;
    }

    /// The oldest item, or null
    const T* Peek() const {
        return m_size == 0 ? nullptr : &m_storage[m_begin];
    }

    void Clear() {
        m_begin = 0;
        m_size = 0;
    }

private:
    u32 m_begin = 0;
    u32 m_size = 0;
    T m_storage[N]{};
};
