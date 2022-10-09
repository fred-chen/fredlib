#pragma once

#include <cstring>
#include <iostream>

namespace FRED {

#if defined(UNDER_TEST)
#define TEST_ONLY(expr) expr
#define NOT_FOR_TEST(expr)
#else
#define TEST_ONLY(expr)
#define NOT_FOR_TEST(expr) expr
#endif

#define UNUSED(expr)  \
    do {              \
        (void)(expr); \
    } while (0);

#if defined(__x86_64__)
#define ALWAYS_BREAK() __asm__("int $3")
#else
#define ALWAYS_BREAK() ::abort()
#endif

inline static void __assertFunction(const char *message, const char *file,
                                    int line) {
    std::cerr << "ASSERT:" << file << "(" << line << ") " << message << " "
              << errno << "(" << std::strerror(errno) << ") ";
    ALWAYS_BREAK();
}

#define ASSUME(must_be_true_predicate, msg) \
    ((must_be_true_predicate) ? (void)0     \
                              : __assertFunction(msg, __FILE__, __LINE__))

}  // namespace FRED