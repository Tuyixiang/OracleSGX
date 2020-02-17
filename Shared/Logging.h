#ifndef _E_LOGGING_H_
#define _E_LOGGING_H_

#include <string>
#include <tuple>
#ifdef SGX_IN_ENCLAVE
#include "Enclave/Enclave.h" // printf
#endif
#include "deps/sha256.h"
inline std::string operator""s(const char *str, std::size_t) { return str; }

// 匹配后缀，符合的文件来源允许打印
const std::string allowed_files[] = {};//{".cpp", ".h"};

// #define INFO_ON

const char *format(const char *fmt, ...);

inline const std::string abstract(const std::string &str) {
  return {sha256(str).data(), 16};
}

#define LOG(str, ...)                                                    \
  do {                                                                   \
    auto file_str = std::string(__FILE__);                               \
    for (auto match : allowed_files) {                                   \
      if (file_str.compare(file_str.size() - match.size(), match.size(), \
                           match) == 0) {                                \
        auto print_str = format(str, ##__VA_ARGS__);                     \
        printf("%s:%d: %s\n", __FILE__, __LINE__, print_str);            \
        break;                                                           \
      }                                                                  \
    }                                                                    \
  } while (0);

#ifdef INFO_ON
#define INFO(str, ...)                                                       \
  do {                                                                       \
    auto file_str = std::string(__FILE__);                                   \
    for (auto match : allowed_files) {                                       \
      if (file_str.compare(file_str.size() - match.size(), match.size(),     \
                           match) == 0) {                                    \
        auto print_str = format(str, ##__VA_ARGS__);                         \
        printf("\033[2m%s:%d: %s\033[22m\n", __FILE__, __LINE__, print_str); \
        break;                                                               \
      }                                                                      \
    }                                                                        \
  } while (0);
#else
#define INFO(str, ...)
#endif

// 任何来源的 Error 都会被打印
#define ERROR(str, ...)                                                     \
  do {                                                                      \
    auto print_str = format(str, ##__VA_ARGS__);                            \
    printf("\033[37;41m%s:%d: %s\033[0m\n", __FILE__, __LINE__, print_str); \
  } while (0);

#define ASSERT(condition)                                          \
  do {                                                             \
    if (!(condition)) {                                            \
      ERROR("Assertion failed at %s:%d: '%s'", __FILE__, __LINE__, \
            #condition);                                           \
    }                                                              \
  } while (0);

#define UNREACHABLE()                                  \
  do {                                                 \
    ERROR("Unreachable at %s:%d", __FILE__, __LINE__); \
    abort();                                           \
  } while (0);

#define UNIMPLEMENTED()                                  \
  do {                                                   \
    ERROR("Unimplemented at %s:%d", __FILE__, __LINE__); \
    abort();                                             \
  } while (0);

#endif  // _E_LOGGING_H_