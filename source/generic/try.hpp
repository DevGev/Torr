#pragma once

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <cstdlib>

/* NOTE: possibly update to try operator (P0779R0) in the future.
 * https://www.reddit.com/r/cpp/comments/198rf37/easier_expected_handling_in_gccclang_with_try_and */

/* auto value_or_error = myfunc();
 * if (!value_or_error.has_value())
 *      return std::unexpected(value_or_error.error());
 *
 * becomes:
 *
 * auto value = TRY(myfunc());
 * */

/* TRY() depends on a non-standard C++ extension, specifically
 * on statement expressions [1]. This is known to be implemented
 * by at least clang and gcc.
 * [1] https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html
 * */

#if __clang__ || __GNUC__
#define TRY(failable) ({ \
    auto result = (failable); \
    if (!result) return std::unexpected { result.error() }; \
    *result; })
#else
#error "unsupported compiler: TRY macro only supported for GCC and Clang"
#endif

#define MUST(failable) [](const auto &x) { \
    if (!x){ \
        std::println(stderr, __FILE__ ":{} [must] {}", __LINE__, x.error()); \
        std::abort(); \
    } \
    return *x; }(failable)
