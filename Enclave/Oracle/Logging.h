#ifndef _E_LOGGING_H_
#define _E_LOGGING_H_

#include <string>
#include <tuple>
inline std::string operator""s(const char *str, std::size_t) { return str; }

// 匹配后缀，符合的文件来源允许打印
const std::string allowed_files[] = {".cpp", ".h"};

const char *format(const char *fmt, ...);

#define LOG(str, ...)                                                          \
  do {                                                                         \
    auto file_str = std::string(__FILE__);                                     \
    for (auto match : allowed_files) {                                         \
      if (file_str.compare(file_str.size() - match.size(), match.size(),       \
                           match) == 0) {                                      \
        auto print_str = format(str, ##__VA_ARGS__);                           \
        printf("%s:%d: %s\n", __FILE__, __LINE__, print_str);                  \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
  } while (0);

// 任何来源的 Error 都会被打印
#define ERROR(str, ...)                                                        \
  do {                                                                         \
    auto print_str = format(str, ##__VA_ARGS__);                               \
    printf("\033[37;41mERROR!\033[0m %s\n", print_str);                        \
  } while (0);

#define ASSERT(condition)                                                      \
  do {                                                                         \
    if (!(condition)) {                                                        \
      ERROR("Assertion failed at %s:%d: '%s'", __FILE__, __LINE__,             \
            #condition);                                                       \
    }                                                                          \
  } while (0);

#define UNREACHABLE()                                                          \
  do {                                                                         \
    ERROR("Unreachable at %s:%d", __FILE__, __LINE__);                         \
    abort();                                                                   \
  } while (0);

#define UNIMPLEMENTED()                                                        \
  do {                                                                         \
    ERROR("Unimplemented at %s:%d", __FILE__, __LINE__);                       \
    abort();                                                                   \
  } while (0);

#endif // _E_LOGGING_H_