//  SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
//  SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

template <size_t N, typename C = char>
struct StringLiteral {
    static constexpr size_t len = N;

    constexpr StringLiteral(const C (&str)[N]) {
        for (size_t i = 0; i < N; ++i) {
            value[i] = str[i];
        }
    }

    C value[N]{};
};
