/*
 * Copyright (c) 2022 Fred Chen
 *
 * Created on Sun Oct 23 2022
 * Author: Fred Chen
 */
#pragma once

#include <immintrin.h>
#include <openssl/evp.h>

#include <array>
#include <iomanip>
#include <map>
#include <ostream>
#include <type_traits>

#include "hashes/b3/blake3.h"
#include "hashes/xxhash64.h"
#include "micros.hpp"

namespace FRED {

/**
 * @brief a generic hash digest, it's basically a byte array.
 *
 * @tparam HashEngine is a functor/function object that generates the hash
 *         digest from a input.
 * @tparam nbytes is the number of bytes of the final digest.
 */
template <typename HashEngine, int nbytes = 20>
class HashKey {
public:
    using HashKeyType = HashKey<HashEngine, nbytes>;
    enum { length = nbytes };

private:
    std::array<uint8_t, nbytes> mDigest;  // the digest
public:
    HashKey() : mDigest({}){};

    HashKey(const unsigned char* input, const size_t len) : mDigest({}) {
        HashEngine()(input, len, (uint8_t*)&mDigest, nbytes);
    }
    template <class T, size_t size>
    HashKey(const std::array<T, size>& input) : mDigest({}) {
        HashEngine()((const unsigned char*)input, input.size(),
                     (uint8_t*)&mDigest);
    }

    /// assignable
    HashKey(const HashKeyType& other) = default;
    HashKeyType& operator=(const HashKeyType& other) = default;

    /// movable
    HashKey(HashKeyType&& other) = default;
    HashKeyType& operator=(HashKeyType&& other) = default;

    /// getters
    std::array<uint8_t, nbytes>& digest() { return mDigest; }
    int size() { return nbytes; }

    /// operators
    friend bool operator==(const HashKeyType& lhs, const HashKeyType& rhs) {
        for (int i = 0; i < nbytes; i++) {
            if (lhs.mDigest[i] != rhs.mDigest[i]) {
                return false;
            }
        }
        return true;
    }
    friend bool operator!=(const HashKeyType& lhs, const HashKeyType& rhs) {
        return !(rhs == lhs);
    }
    friend bool operator<(const HashKeyType& lhs, const HashKeyType& rhs) {
        // treat mDigest as a big-endian number (MSB at lower address, MSB
        // first, start with MSB)
        for (int i = 0; i < nbytes; i++) {
            if (lhs.mDigest[i] < rhs.mDigest[i]) {
                return true;
            }
        }
        return false;
    }
    friend bool operator>(const HashKeyType& lhs, const HashKeyType& rhs) {
        return rhs < lhs;
    }
    friend bool operator<=(const HashKeyType& lhs, const HashKeyType& rhs) {
        return !(rhs < lhs);
    }
    friend bool operator>=(const HashKeyType& lhs, const HashKeyType& rhs) {
        return !(lhs < rhs);
    }
    uint8_t& operator[](int index) { return mDigest[index]; }

    // output
    friend std::ostream& operator<<(std::ostream& out, HashKeyType& key) {
        for (const auto& c : key.digest()) {
            out << std::setfill('0') << std::setw(2) << std::right << std::hex
                << (int)c;
        }
        out << std::endl;
        return out;
    }

    // iterator
    typename std::array<uint8_t, nbytes>::iterator begin() {
        return mDigest.begin();
    }
    typename std::array<uint8_t, nbytes>::iterator end() {
        return mDigest.end();
    }

    // information
    const char* engine_name() const { return HashEngine().engine_name(); }
};

/**
 * @brief DummyHashEngine is a common interface of all hash engines,
 *        it also provides a 'fake' hash function which simply read from input.
 *        DummyHashEngine can be used to test 'pure memory read' performance.
 *        And pure memory read speed can be compared with real hash functions
 *        like SHA1 etc.
 *
 */
class DummyHashEngine {
private:
    static constexpr char _engine_name[] = "DummyHashEngine";

public:
    // no copy, no assign, no move
    DummyHashEngine(const DummyHashEngine& other) = delete;
    DummyHashEngine(const DummyHashEngine&& other) = delete;
    DummyHashEngine& operator=(const DummyHashEngine&) = delete;
    DummyHashEngine& operator=(const DummyHashEngine&&) = delete;

    DummyHashEngine() = default;

    void operator()(const unsigned char* input, size_t len, uint8_t* digest,
                    unsigned int digest_len) {
        UNUSED(digest);
        UNUSED(digest_len);

        // data = input[i];
#if defined(__x86_64__)
        __m256i v;  // for faster avx2 instructions
        for (size_t i = 0; i < len / 32; i++) {
            v = _mm256_load_si256(&((const __m256i*)input)[i]);
        }
#else
        __uint128_t v;
        for (size_t i = 0; i < len / 16; i++) {
            v = ((__uint128_t*)input)[i];
        }
#endif
        UNUSED(v);
    }

    const char* engine_name() const { return _engine_name; }
    friend std::ostream& operator<<(std::ostream& o,
                                    const DummyHashEngine& engine) {
        o << engine.engine_name();
        return o;
    }
};
static_assert(sizeof(DummyHashEngine) == 1);

class Blake3Engine : public DummyHashEngine {
private:
    static constexpr const char _engine_name[] = "Blake3Engine";

public:
    void operator()(const unsigned char* input, size_t len, uint8_t* digest,
                    unsigned int digest_len) {
        // Initialize the hasher.
        static thread_local blake3_hasher hasher;
        blake3_hasher_init(&hasher);

        // Update the hasher with data
        // for (size_t i = 0; i < len; i += update_len) {
        //     blake3_hasher_update(&hasher, input + i,
        //                          i + update_len >= len ? len - i :
        //                          update_len);
        // }
        blake3_hasher_update(&hasher, input, len);

        // Finalize the hash.
        blake3_hasher_finalize(&hasher, digest, digest_len);
    }
    const char* engine_name() const { return _engine_name; }
};
static_assert(sizeof(Blake3Engine) == 1);

/**
 * @brief the generic hash engine for sha algorithms.
 * the sha_type could be one of:
 *   0: use sha1 as the hash function
 *   1: use sha224 as the hash function
 *   2: use sha256 as the hash function
 *   3: use sha384 as the hash function
 *   4: use sha512 as the hash function
 *   5: use sha3_224 as the hash function, safer and slower
 *   6: use sha3_256 as the hash function, safer and slower
 *   7: use sha3_384 as the hash function, safer and slower
 *   8: use sha3_512 as the hash function, safer and slower
 *
 * @tparam sha_type is the algorithm chooser. eg. 0 for sha1 and 1 for sha224.
 *
 */
template <int sha_type = 2>
class SHAEngine : DummyHashEngine {
public:
    enum {
        SHA1 = 0,
        SHA224 = 1,
        SHA256 = 2,
        SHA384 = 3,
        SHA512 = 4,
        SHA3_224 = 5,
        SHA3_256 = 6,
        SHA3_384 = 7,
        SHA3_512 = 8
    };

private:
    typedef const EVP_MD* (*hash_algorithm)(void);
    static constexpr const hash_algorithm _algorithm_table[] = {
        EVP_sha1,     EVP_sha224,   EVP_sha256,   EVP_sha384,   EVP_sha512,
        EVP_sha3_224, EVP_sha3_256, EVP_sha3_384, EVP_sha3_512,
    };
    static constexpr const char* _engine_names[] = {
        "SHA1Engine",     "SHA224Engine",   "SHA256Engine",
        "SHA384Engine",   "SHA512Engine",   "SHA3_224Engine",
        "SHA3_256Engine", "SHA3_384Engine", "SHA3_512Engine",
    };

public:
    void operator()(const unsigned char* input, size_t len, uint8_t* digest,
                    unsigned int digest_len) {
        static const size_t sha_size = EVP_MD_size(
            _algorithm_table[sha_type]());  // the size of algorithm result
        static thread_local EVP_MD_CTX* mdctx = EVP_MD_CTX_create();
        static thread_local uint8_t* _digest = (uint8_t*)OPENSSL_malloc(
            sha_size);  // temp result if user provided digest isn't long enough
                        // to hold the hash result

        ASSUME(mdctx, "failed to allocate EVP_MD_CTX.");

        // Initialize the hasher.
        EVP_DigestInit_ex(mdctx, _algorithm_table[sha_type](), NULL);

        // Update the hasher with data
        // for (size_t i = 0; i < len; i += update_len) {
        //     EVP_DigestUpdate(mdctx, input + i,
        //                      i + update_len >= len ? len - i : update_len);
        // }

        EVP_DigestUpdate(mdctx, input, len);

        // Finalize the hash.
        if (sha_size <= digest_len) {
            EVP_DigestFinal_ex(mdctx, digest, NULL);
        } else {
            EVP_DigestFinal_ex(mdctx, _digest, NULL);
            memcpy(digest, _digest, digest_len);
        }

        // dont free the hash because the context is a shared hash engine
        // EVP_MD_CTX_destroy(mdctx);
    }
    const char* engine_name() const { return _engine_names[sha_type]; }
};
static_assert(sizeof(SHAEngine<>) == 1);

/**
 * @brief xxhash is a fast, non-cryptographic hash
 *        the output length shouldn't exceed 64bits(8bytes).
 *
 */
class XXhash64Engine : public DummyHashEngine {
private:
    static constexpr const char _engine_name[] = "XXhash64Engine";

public:
    void operator()(const unsigned char* input, size_t len, uint8_t* digest,
                    unsigned int digest_len) {
        // Initialize the hasher.
        XXHash64 myhash(0);

        // Update the hasher with data
        // for (size_t i = 0; i < len; i += update_len) {
        //     myhash.add(input + i, i + update_len >= len ? len - i :
        //     update_len);
        // }
        myhash.add(input, len);

        // Finalize the hash.
        if (digest_len >= sizeof(uint64_t)) {
            *((uint64_t*)digest) = myhash.hash();
        } else {
            uint64_t result = myhash.hash();
            memcpy(digest, &result, digest_len);
        }
    }
    const char* engine_name() const { return _engine_name; }
};
static_assert(sizeof(XXhash64Engine) == 1);

///
/// the HashKey class is trivially copyable
/// meaning you can:
/// {
///     HashKey key;
///     HashKey Key1;
///
///     memcpy(&key, &key1);  // you can simply memcopy this HashKey object
///     key = key1;           // to achieve the same result as assignment
/// }
///
static_assert(std::is_trivially_copyable<HashKey<DummyHashEngine>>::value,
              "HashKey is not trivially copyable.");

typedef HashKey<DummyHashEngine, 20> DummyHashKey;
typedef HashKey<SHAEngine<SHAEngine<>::SHA1>, 20> SHA1Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA224>, 28> SHA224Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA256>, 32> SHA256Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA384>, 48> SHA384Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA512>, 64> SHA512Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA3_224>, 26> SHA3_224Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA3_256>, 32> SHA3_256Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA3_384>, 48> SHA3_384Key;
typedef HashKey<SHAEngine<SHAEngine<>::SHA3_512>, 64> SHA3_512Key;
typedef HashKey<Blake3Engine, 32> Blake3_256Key;
typedef HashKey<XXhash64Engine, 64> XXhash64Key;

}  // namespace FRED