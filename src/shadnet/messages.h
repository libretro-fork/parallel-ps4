// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// C++ conveniences over the C89 shadNet messages (shadnet_msg.h, pb_wire.h).
//
// Strings and bytes are borrowed: Bytes() points a field at the caller's
// data, which must outlive the encode, and a decoded field points into the
// buffer it was decoded from. Repeated fields decode into a Scratch the
// caller keeps for as long as it reads the message.

#include <cstring>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "common/types.h"
#include "shadnet/shadnet_msg.h"

namespace ShadNet {

template <typename T>
struct MessageDesc;

#define SHADNET_DESC_TRAIT(name)                                                                   \
    template <>                                                                                    \
    struct MessageDesc<shadnet_##name> {                                                           \
        static const pb_msg_desc_t& Get() {                                                        \
            return shadnet_##name##_desc;                                                          \
        }                                                                                          \
    };
SHADNET_MESSAGES(SHADNET_DESC_TRAIT)
#undef SHADNET_DESC_TRAIT

inline pb_bytes_t Bytes(std::string_view s) {
    return pb_bytes_t{reinterpret_cast<const uint8_t*>(s.data()), s.size()};
}
inline pb_bytes_t Bytes(const void* data, std::size_t size) {
    return pb_bytes_t{static_cast<const uint8_t*>(data), size};
}
// Deleted so a temporary string can't be borrowed past its life
pb_bytes_t Bytes(std::string&&) = delete;

inline std::string_view View(const pb_bytes_t& b) {
    return std::string_view{reinterpret_cast<const char*>(b.data), b.size};
}
inline std::string Str(const pb_bytes_t& b) {
    return std::string{View(b)};
}

// The elements of a repeated field
template <typename A>
inline auto Items(const A& a) {
    return std::span{a.items, a.count};
}

// Points a repeated field at the caller's elements
template <typename A, typename T>
inline void SetItems(A& a, T* items, std::size_t count) {
    a.items = items;
    a.count = count;
}
template <typename A, typename T>
inline void SetItems(A& a, std::vector<T>& v) {
    a.items = v.data();
    a.count = v.size();
}

// Holds what a request being built points at: the elements of its repeated
// fields and strings made just for it. Keep it alive until the request is
// encoded.
class Builder {
public:
    // Appends a zeroed element to the repeated field @a and returns it. Earlier
    // elements may move; the returned pointer is good until the next Add to @a.
    template <typename A>
    auto* Add(A& a) {
        using T = std::remove_pointer_t<decltype(a.items)>;
        auto& v = Storage<T>(&a, a.items, a.count);
        v.emplace_back();
        std::memset(&v.back(), 0, sizeof(T));
        a.items = v.data();
        a.count = v.size();
        return &v.back();
    }

    // Appends a scalar, or a string kept by the builder, to the repeated field @a
    template <typename A, typename V>
    void Push(A& a, V&& value) {
        using T = std::remove_pointer_t<decltype(a.items)>;
        if constexpr (std::is_same_v<T, pb_bytes_t>) {
            *Add(a) = Keep(std::forward<V>(value));
        } else {
            *Add(a) = static_cast<T>(value);
        }
    }

    // A copy of @s that lives as long as the builder
    pb_bytes_t Keep(std::string_view s) {
        return Bytes(strings.emplace_back(s));
    }

private:
    struct Holder {
        virtual ~Holder() = default;
        const void* owner = nullptr;
        const void* type = nullptr;
    };
    template <typename T>
    static const void* TypeTag() {
        static const char tag = 0;
        return &tag;
    }
    template <typename T>
    struct VecHolder : Holder {
        std::vector<T> v;
    };

    // The storage behind the field at @owner. A field inside an element that has
    // since moved is found again by where its items are.
    template <typename T>
    std::vector<T>& Storage(const void* owner, const T* items, std::size_t count) {
        for (auto& h : holders) {
            if (h->type != TypeTag<T>()) {
                continue;
            }
            auto& v = static_cast<VecHolder<T>*>(h.get())->v;
            if (h->owner == owner || (count && v.data() == items)) {
                h->owner = owner;
                return v;
            }
        }
        auto h = std::make_unique<VecHolder<T>>();
        h->owner = owner;
        h->type = TypeTag<T>();
        auto& v = h->v;
        v.assign(items, items + count);
        holders.push_back(std::move(h));
        return v;
    }

    std::vector<std::unique_ptr<Holder>> holders;
    std::deque<std::string> strings;
};

template <typename T>
inline std::size_t EncodedSize(const T& msg) {
    return pb_encoded_size(&MessageDesc<T>::Get(), &msg);
}

// Appends @msg to @out
template <typename T, typename Byte>
inline void EncodeAppend(const T& msg, std::vector<Byte>& out) {
    static_assert(sizeof(Byte) == 1);
    const std::size_t at = out.size();
    out.resize(at + EncodedSize(msg));
    pb_encode(&MessageDesc<T>::Get(), &msg, reinterpret_cast<uint8_t*>(out.data() + at));
}

template <typename T>
inline std::string Encode(const T& msg) {
    std::string out(EncodedSize(msg), '\0');
    pb_encode(&MessageDesc<T>::Get(), &msg, reinterpret_cast<uint8_t*>(out.data()));
    return out;
}

// Room for a decoded message's arrays; reusable from one decode to the next.
using Scratch = std::vector<u64>;

template <typename T>
inline bool Decode(T& msg, const void* data, std::size_t size, Scratch& scratch) {
    const auto& desc = MessageDesc<T>::Get();
    const auto* bytes = static_cast<const uint8_t*>(data);
    const std::size_t need = pb_decode_scratch(&desc, bytes, size);
    if (need == static_cast<std::size_t>(-1)) {
        std::memset(&msg, 0, sizeof(msg));
        return false;
    }
    if (scratch.size() * sizeof(u64) < need) {
        scratch.resize((need + sizeof(u64) - 1) / sizeof(u64));
    }
    return pb_decode(&desc, &msg, bytes, size, scratch.data(), scratch.size() * sizeof(u64));
}

template <typename T>
inline bool Decode(T& msg, std::string_view blob, Scratch& scratch) {
    return Decode(msg, blob.data(), blob.size(), scratch);
}
// Deleted: the message would point into a temporary
template <typename T>
bool Decode(T& msg, std::string&& blob, Scratch& scratch) = delete;

} // namespace ShadNet
