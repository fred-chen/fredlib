#if defined(UNDER_TEST)
#define TEST_ONLY(expr) expr
#define NOT_FOR_TEST(expr)
#else
#define TEST_ONLY(expr)
#define NOT_FOR_TEST(expr) expr
#endif
