/*
 * Copyright (C) 2011-2019 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdarg.h>
#include <stdio.h> /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */

/*
 * printf:
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
extern "C" void printf(const char *fmt, ...) {
  static char buf[BUFSIZ] = {'\0'};
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZ, fmt, ap);
  va_end(ap);
  ocall_print_string(buf);
}

extern "C" size_t recv(int socket, void *buff, size_t size, int flags) {
  int recv_ret;
  char *buffer_in;
  int size_in;
  o_recv(&recv_ret, socket, &buffer_in, &size_in, &errno);
  memcpy(buff, buffer_in, size_in);
  return (size_t)recv_ret;
}

extern "C" size_t send(int socket, const void *buff, size_t size, int flags) {
  int send_ret;
  o_send(&send_ret, socket, (const char *)buff, size, &errno);
  return (size_t)send_ret;
}

long tv_sec;
long tv_usec;

extern "C" double current_time(void) {
  o_gettimeofday(&tv_sec, &tv_usec);
  return (double)(1000000 * tv_sec + tv_usec) / 1000000.0;
}

extern "C" int LowResTimer(void) { return (int)tv_sec; }

extern "C" time_t XTIME(time_t *timer) {
  time_t time;
  o_time(&time, timer);
  return time;
}