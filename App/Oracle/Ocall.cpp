#include <sys/socket.h>
#include "../Enclave_u.h"
#include <sys/time.h>
#include <time.h>

size_t o_recv(int socket, void *buff, size_t size, int flags) {
  return recv(socket, buff, size, flags | MSG_DONTWAIT);
}

size_t o_send(int socket, const void *buff, size_t size, int flags) {
  return send(socket, buff, size, flags | MSG_DONTWAIT);
}

static double current_time() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return (double)(1000000 * tv.tv_sec + tv.tv_usec) / 1000000.0;
}

void o_current_time(double *time) {
  if (!time)
    return;
  *time = current_time();
  return;
}

void o_low_res_time(int *time) {
  struct timeval tv;
  if (!time)
    return;
  *time = tv.tv_sec;
  return;
}

time_t o_time(time_t *timer) { return time(timer); }
