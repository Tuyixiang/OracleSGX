#ifndef _SHARED_STATUSCODE_H_
#define _SHARED_STATUSCODE_H_

#include <string>

#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

struct StatusCode {
  enum Code : int {
    Success,
    Blocking,
    Uninitialized,
    NoAvailableWorker,
    WolfsslError,
    Unknown,
  } code;

  const std::string message() {
    switch (code) {
    case Success:
      return GREEN "Success." RESET;
    case Blocking:
      return YELLOW "IO operation blocking." RESET;
    case Uninitialized:
      return RED "Something uninitialized." RESET;
    case NoAvailableWorker:
      return RED
          "All workers occupied. Cannot create new SSL connection." RESET;
    case WolfsslError:
      return RED "WolfSSL error." RESET;
    case Unknown:
      return RED "WTF?" RESET;
    }
  }

  StatusCode() : code(Unknown) {}
  StatusCode(int code) : code(static_cast<Code>(code)) {}

  operator int() const { return static_cast<int>(code); }
};

#endif // _SHARED_STATUSCODE_H_