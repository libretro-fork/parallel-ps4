// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include <crypto/crypto.h>
#include <crypto/kdf.h>

#include "core/net/p2p_crypto.h"

namespace Core::Net::P2P {

constexpr size_t IvSeedSize = 4;
constexpr size_t SignatureSize = 4;

bool Crypt(std::vector<u8>& payload, const P2PKey& key, const std::array<u8, 4>& iv_seed,
           bool encrypt) {
    std::array<u8, 16> iv;
    for (size_t i = 0; i < iv.size(); ++i) {
        iv[i] = iv_seed[i & 3];
    }
    aes_ctx aes;
    if (aes_init(&aes, key.value.data(), key.value.size()) != 0) {
        return false;
    }
    unsigned num = 0;
    if (encrypt) {
        aes_cfb128_encrypt(&aes, iv.data(), &num, payload.data(), payload.data(), payload.size());
    } else {
        aes_cfb128_decrypt(&aes, iv.data(), &num, payload.data(), payload.data(), payload.size());
    }
    crypto_memzero(&aes, sizeof(aes));
    return true;
}

std::array<u8, SignatureSize> Sign(std::span<const u8> body, const P2PKey& key) {
    std::array<u8, 20> digest{};
    hmac_sha1(key.value.data(), key.value.size(), body.data(), body.size(), digest.data());
    std::array<u8, SignatureSize> signature{};
    std::copy_n(digest.begin(), signature.size(), signature.begin());
    return signature;
}

std::array<u8, 4> DeriveCommunicationId(std::span<const u8, 16> value) {
    std::array<u8, 20> digest{};
    const u8 empty = 0;
    hmac_sha1(value.data(), value.size(), &empty, 0, digest.data());
    std::array<u8, 4> id{};
    std::copy_n(digest.begin(), id.size(), id.begin());
    return id;
}

bool ProtectPayload(std::span<const u8> data, bool encrypt, bool sign, const P2PKey& key,
                    std::vector<u8>& output) {
    std::vector<u8> body(data.begin(), data.end());
    std::array<u8, IvSeedSize> iv{};
    if (encrypt &&
        (crypto_random_bytes(iv.data(), iv.size()) != 0 || !Crypt(body, key, iv, true))) {
        return false;
    }
    output.clear();
    output.reserve(body.size() + (encrypt ? IvSeedSize : 0) + (sign ? SignatureSize : 0));
    if (encrypt) {
        output.insert(output.end(), iv.begin(), iv.end());
    }
    if (sign) {
        const auto signature = Sign(body, key); // signs ciphertext
        output.insert(output.end(), signature.begin(), signature.end());
    }
    output.insert(output.end(), body.begin(), body.end());
    return true;
}

bool UnprotectPayload(std::span<const u8> data, bool encrypt, bool sign, const P2PKey& key,
                      std::vector<u8>& output) {
    const size_t prefix = (encrypt ? IvSeedSize : 0) + (sign ? SignatureSize : 0);
    if (data.size() < prefix) {
        return false;
    }
    std::array<u8, IvSeedSize> iv{};
    std::array<u8, SignatureSize> signature{};
    size_t offset = 0;
    if (encrypt) {
        std::copy_n(data.begin(), iv.size(), iv.begin());
        offset += iv.size();
    }
    if (sign) {
        std::copy_n(data.begin() + offset, signature.size(), signature.begin());
        offset += signature.size();
    }
    output.assign(data.begin() + offset, data.end());
    if (sign) {
        const auto expected = Sign(output, key);
        if (!crypto_memeq_ct(signature.data(), expected.data(), signature.size())) {
            return false;
        }
    }
    return !encrypt || Crypt(output, key, iv, false);
}

} // namespace Core::Net::P2P
