#include <cstdarg>
#include <cstdio>

const char *format(const char *fmt, ...) {
  static char buffer[BUFSIZ] = {0};
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buffer, BUFSIZ, fmt, ap);
  va_end(ap);
  return buffer;
}