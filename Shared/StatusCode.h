#ifndef _SHARED_STATUSCODE_H_
#define _SHARED_STATUSCODE_H_

#include <string>
#include "Shared/Logging.h"

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
        return "Something uninitialized.";
      case NoAvailableWorker:
        return "All workers occupied. Cannot create new SSL connection.";
      case ResponseTooLarge:
        return "Requested page exceeds size limit.";
      case ParserError:
        return "Failed to parse HTTP response.";
      case LibraryError:
        return "3rd-party library error.";
      case Timeout:
        return "Timeout.";
      case Unknown:
        return "WTF?";
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