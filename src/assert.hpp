#pragma once

#include <source_location>
#include <print>
#include <exception>

#define __assert_1(expr) \
  do { \
    const auto loc = std::source_location::current(); \
    if (!(expr)) { \
      std::println(stderr, "{}:{}:{}: assertion failed: ({}) in function '{}'", loc.file_name(), loc.line(), loc.column(), #expr, loc.function_name()); \
      std::terminate(); \
    } \
  } while (0)

#define __assert_2(expr, msg) \
  do { \
    const auto loc = std::source_location::current(); \
    if (!(expr)) { \
      std::println(stderr, "{}:{}:{}: assertion failed: ({}) in '{}'", loc.file_name(), loc.line(), loc.column(), #expr, loc.function_name()); \
      std::println(stderr, "message: {}", msg); \
      std::terminate(); \
    } \
  } while (0)

#define __assert_3(expr, msg, terminate_fn) \
  do { \
    const auto loc = std::source_location::current(); \
    if (!(expr)) { \
      std::println(stderr, "{}:{}:{}: assertion failed: ({}) in '{}'", loc.file_name(), loc.line(), loc.column(), #expr, loc.function_name()); \
      std::println(stderr, "message: {}", msg); \
      std::terminate(terminate_fn); \
    } \
  } while (0)

#define __assert_choose(_1, _2, _3, NAME, ...) NAME
#define __assert(...) __assert_choose(__VA_ARGS__, __assert_3, __assert_2, __assert_1)(__VA_ARGS__)
