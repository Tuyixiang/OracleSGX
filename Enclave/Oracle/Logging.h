#ifndef _E_LOGGING_H_
#define _E_LOGGING_H_

#include <string>
inline std::string operator""s(const char *str, std::size_t) { return str; }

// 匹配后缀，符合的文件来源允许打印
const std::string allowed_files[] = {".cpp"s, ".h"s};

#define LOG(str)                                                               \
  do {                                                                         \
    auto file_str = std::string(__FILE__);                                     \
    for (auto match : allowed_files) {                                         \
      if (file_str.compare(file_str.size() - match.size(), match.size(),       \
                           match) == 0) {                                      \
        printf("%s:%d: %s\n", __FILE__, __LINE__, std::string(str).c_str());   \
        break;                                                                 \
      }                                                                        \
    }                                                                          \
  } while (0);

// 任何来源的 Error 都会被打印
#define ERROR(str)                                                             \
  do {                                                                         \
    printf("\033[37;41mERROR!\033[0m %s\n", str);                              \
  } while (0);

#define UNREACHABLE()                                                          \
  do {                                                                         \
    ERROR(("Unreachable at "s + __FILE__ + ":" + std::to_string(__LINE__))     \
              .c_str());                                                       \
    abort();                                                                   \
  } while (0);

#define UNIMPLEMENTED()                                                        \
  do {                                                                         \
    ERROR(("Unimplemented at "s + __FILE__ + ":" + std::to_string(__LINE__))   \
              .c_str());                                                       \
    abort();                                                                   \
  } while (0);

#endif // _E_LOGGING_H_