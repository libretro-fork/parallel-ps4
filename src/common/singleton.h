// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Common {

template <class T>
class Singleton {
public:
    static T* Instance() {
        if (!m_instance) {
            // Made once, kept for the process's life
            m_instance = new T();
        }
        return m_instance;
    }

    /// The instance if there is one yet, never making it: for a crash report
    static T* Peek() {
        return m_instance;
    }

protected:
    Singleton();
    ~Singleton();

private:
    static inline T* m_instance{};
};

} // namespace Common
