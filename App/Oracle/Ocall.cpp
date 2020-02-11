#include "../Enclave_u.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>

size_t o_recv(int socket, void *buff, size_t size, int flags) {
  return recv(socket, buff, size, (flags | MSG_DONTWAIT) & (~MSG_WAITALL));
}

size_t o_send(int socket, const void *buff, size_t size, int flags) {
  return send(socket, buff, size, flags | MSG_DONTWAIT);
}

void o_gettimeofday(long *tv_sec, long *tv_usec) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  *tv_sec = tv.tv_sec;
  *tv_usec = tv.tv_usec;
}

time_t o_time(time_t *timer) { return time(timer); }