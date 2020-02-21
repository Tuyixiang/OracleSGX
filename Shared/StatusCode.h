#ifndef _SHARED_STATUSCODE_H_
#define _SHARED_STATUSCODE_H_

#include <string>
#include "Shared/Logging.h"

#define RED "\033[37;41m"
#define GREEN "\033[37;42m"
#define YELLOW "\033[37;43m"
#define RESET "\033[0m"

struct StatusCode {
  enum Code : int {
    Success,
    Blocking,
    Uninitialized,
    NoAvailableWorker,
    ResponseTooLarge,
    ParserError,
    LibraryError,
    Timeout,
    Unknown,
  } code;

  const char *message() const {
    switch (code) {
      case Success:
        return "Success.";
      case Blocking:
        return "IO operation blocking.";
      case Uninitialized:
        return RED "Something uninitialized." RESET;
      case NoAvailableWorker:
        return RED
            "All workers occupied. Cannot create new SSL connection." RESET;
      case ResponseTooLarge:
        return RED "Requested page exceeds size limit." RESET;
      case ParserError:
        return RED "Failed to parse HTTP response." RESET;
      case LibraryError:
        return RED "3rd-party library error." RESET;
      case Timeout:
        return RED "Timeout." RESET;
      case Unknown:
        return RED "WTF?" RESET;
      default: { UNREACHABLE(); }
    }
  }

  bool is_error() const {
    switch (code) {
      case Success:
      case Blocking:
        return false;
      case Uninitialized:
      case NoAvailableWorker:
      case ResponseTooLarge:
      case ParserError:
      case LibraryError:
      case Timeout:
      case Unknown:
        return true;
      default: { UNREACHABLE(); }
    }
  }

  bool is_fatal() const { return code == Unknown; }

  StatusCode() : code(Unknown) {}
  StatusCode(int code) : code(static_cast<Code>(code)) {}
  StatusCode(Code code) : code(code) {}

  operator int() const { return static_cast<int>(code); }
};

#endif  // _SHARED_STATUSCODE_H_