#define UNDER_TEST

#include "hash.hpp"

#include <sys/mman.h>

#include <future>
#include <set>

#include "catch.hpp"
#include "threading.hpp"
#include "time.hpp"

using namespace FRED;

TEST_CASE("dummy", "[hash]") {
    const int nbytes = 31;
    HashKey<DummyHashEngine, nbytes> key;

    // digest lenth is geiven in template constant
    REQUIRE(key.digest().size() == nbytes);

    // HashKey object is a thin array
    REQUIRE(sizeof(key) == nbytes);

    // initialized with 0
    for (auto& c : key.digest()) {
        REQUIRE(c == 0);
    }

    // trivial copyable
    int i = 1;
    for (auto& c : key.digest()) {
        c = i++;
    }
    void* tmp = malloc(sizeof(HashKey<DummyHashEngine, nbytes>));
    memcpy(tmp, &key, sizeof(key));

    REQUIRE(*((HashKey<DummyHashEngine, nbytes>*)tmp) == key);

    // comparison operators
    HashKey<DummyHashEngine, nbytes> key1;
    memcpy(&key1, &key, sizeof(key));

    REQUIRE(key1 == key);

    key[sizeof(key) - 1] = 0;  // this will set the lsb = 0
                               // and key < key1
    REQUIRE(key < key1);
    REQUIRE(key1 != key);
    REQUIRE(key1 > key);
    REQUIRE(key1 >= key);
    REQUIRE((key1 <= key) == false);

    // copyable
    key = key1;
    REQUIRE(key == key1);

    HashKey<DummyHashEngine, nbytes> key2(key);
    REQUIRE(key == key2);

    // movable
    HashKey<DummyHashEngine, nbytes> key3{
        HashKey<DummyHashEngine, nbytes>(key)};
    REQUIRE(key == key3);

    HashKey<DummyHashEngine, nbytes> key4;
    key4 = HashKey<DummyHashEngine, nbytes>(key);
    REQUIRE(key == key4);

    free(tmp);
}

/**
 * @brief check hash collision with given length of the digest.
 *        obviously shorter digest causes more collisions.
 *        but the number of collisions with same length is the key.
 *        algorithms with fewer collsions is considered better/safer.
 *
 * @param nbytes
 * @return int
 */
template <class HashKeyType>
int collision_check() {
    union {
        uint64_t input;
        char input_c[sizeof(uint64_t)];
    };

    // run hash calculation for maximum 1,000,000 times and check the collisions
    size_t iteration = HashKeyType::length * 8 > 63
                           ? 1000000ul
                           : 0x1ull << (HashKeyType::length * 8 - 8);
    iteration = std::min(iteration, 1000000ul);
    std::set<HashKeyType> m;

    auto start = timer_start();
    for (input = 0; input < iteration; input++) {
        HashKeyType key((const unsigned char*)input_c, sizeof(uint64_t));
        m.insert(key);
        // std::cout << key << std::endl;
    }
    auto dur = ms_elapsed_since(start);
    std::cout << HashKeyType().engine_name() << ": " << dur
              << "ms for collision_check." << std::endl;

    // std::cout << iteration << " iterations."
    //           << "key length: " << HashKeyType::length
    //           << " 1ull << (HashKeyType::length * 8 - 8)=" << std::dec
    //           << (0x1ull << (HashKeyType::length * 8 - 8)) << std::endl;
    return iteration - m.size();
}

template <class HashKeyType>
size_t performance_check() {
    GenericThreadPool pool;

    // allocate a big buffer
    unsigned char* p = nullptr;
    size_t input_size = 2ull * GB;
    ::posix_memalign((void**)&p, 64, input_size);
    ::mlock2(p, input_size, 0);    // lock the huge input in memory
    ::memset(p, 255, input_size);  // touch each byte of the input

    auto key1 = HashKeyType(p, input_size);

    // hash this big buffer for n times
    int n = 40;
    FanInPoint fip(n);
    auto f = [p, &key1, input_size](FanInPoint& fip) {
        REQUIRE(HashKeyType(p, input_size) == key1);
        fip.done();
    };
    auto start = timer_start();
    for (int i = 0; i < n; i++) {
        pool.push_function(f, std::ref(fip));
    }
    fip.wait();

    auto dur = ms_elapsed_since(start);
    std::cout << HashKeyType().engine_name() << ": " << dur
              << "ms for performance_check. " << std::fixed
              << std::setprecision(3)
              << double(n * input_size / GB * 1000) / dur << "GB/s"
              << std::endl;
    return dur;
}

template <class HashKeyType>
bool basic_check() {
    const char* data1 = "abcde";
    const char* data2 = "abcdef";

    auto start = timer_start();

    HashKeyType key1((const unsigned char*)data1, strlen(data1) + 1);
    HashKeyType key2((const unsigned char*)data2, strlen(data2) + 1);

    // same input, same output
    for (int i = 0; i < 1000; i++) {
        REQUIRE(HashKeyType((const unsigned char*)data1, strlen(data1) + 1) ==
                key1);
    }

    // thread safety, same input in different threads
    // should return the same hash value
    std::future<HashKeyType> futs[100];
    for (int i = 0; i < 100; i++) {
        futs[i] = std::async(std::launch::async, [data1]() -> HashKeyType {
            return HashKeyType((const unsigned char*)data1, strlen(data1) + 1);
        });
    }
    for (auto& fut : futs) {
        REQUIRE(fut.get() == key1);
    }

    // different input, different output
    REQUIRE(key1 != key2);

    // big buffer check
    unsigned char* p = nullptr;
    size_t input_size = 100 * MB;
    ::posix_memalign((void**)&p, 64, input_size);
    ::mlock2(p, input_size, 0);
    ::memset(p, 255, input_size);

    auto key3 = HashKeyType(p, input_size);
    auto key4 = HashKeyType(p, input_size);
    REQUIRE(key3 == key4);
    p[0] ^= 0x1;  // one bit change of input,
                  // would cause dramatic changes in the result
    auto key5 = HashKeyType(p, input_size);
    REQUIRE(key3 != key5);

    // hash this big buffer for n times expecting the same result
    int iterations = 20;
    for (int i = 0; i < iterations; i++) {
        REQUIRE(HashKeyType(p, input_size) == key5);
    }

    auto dur = ms_elapsed_since(start);
    std::cout << HashKeyType().engine_name() << ": " << dur
              << "ms for basic_check." << std::endl;
    return true;
}

TEST_CASE("engines", "[hash]") {
    performance_check<DummyHashKey>();

    REQUIRE(basic_check<SHA1Key>());
    REQUIRE(collision_check<SHA1Key>() == 0);
    performance_check<SHA1Key>();

    REQUIRE(basic_check<SHA224Key>());
    REQUIRE(collision_check<SHA224Key>() == 0);
    performance_check<SHA224Key>();

    REQUIRE(basic_check<SHA256Key>());
    REQUIRE(collision_check<SHA256Key>() == 0);
    performance_check<SHA256Key>();

    REQUIRE(basic_check<SHA384Key>());
    REQUIRE(collision_check<SHA384Key>() == 0);
    performance_check<SHA384Key>();

    REQUIRE(basic_check<SHA512Key>());
    REQUIRE(collision_check<SHA512Key>() == 0);
    performance_check<SHA512Key>();

    REQUIRE(basic_check<SHA3_224Key>());
    REQUIRE(collision_check<SHA3_224Key>() == 0);
    performance_check<SHA3_224Key>();

    REQUIRE(basic_check<SHA3_256Key>());
    REQUIRE(collision_check<SHA3_256Key>() == 0);
    performance_check<SHA3_256Key>();

    REQUIRE(basic_check<SHA3_384Key>());
    REQUIRE(collision_check<SHA3_384Key>() == 0);
    performance_check<SHA3_384Key>();

    REQUIRE(basic_check<SHA3_512Key>());
    REQUIRE(collision_check<SHA3_512Key>() == 0);
    performance_check<SHA3_512Key>();

    REQUIRE(basic_check<Blake3_256Key>());
    REQUIRE(collision_check<Blake3_256Key>() == 0);
    performance_check<Blake3_256Key>();

    REQUIRE(basic_check<XXhash64Key>());
    REQUIRE(collision_check<XXhash64Key>() == 0);
    performance_check<XXhash64Key>();
}
