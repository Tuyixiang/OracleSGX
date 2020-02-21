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
#include <map>

#include "Enclave.h"
#include "Enclave_t.h" /* print_string */
#include "Shared/Config.h"
#include "Shared/Logging.h"

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

// WolfSSL 读取 socket 会频繁调用
// 为了减少 ocall 次数，每次 ocall 尽可能多读取数据
struct RecvBuffer {
  char data[SOCKET_READ_SIZE];
  int recv_size;
  int used_size;
  RecvBuffer() : recv_size(0), used_size(0) {}
  int valid_size() const { return recv_size - used_size; }
  const char *valid_data() const { return data + used_size; }
};
std::map<int, RecvBuffer> buffers;

extern "C" size_t recv(int socket, void *buff, size_t size, int flags) {
  (void)flags;
  auto it = buffers.find(socket);
  if (it == buffers.cend()) {
    // 没有缓存，进行 ocall
    int recv_ret;
    char *buffer_in;
    o_recv(&recv_ret, socket, &buffer_in, SOCKET_READ_SIZE, &errno);
    if (recv_ret > 0) {
      // 成功收到数据
      ASSERT(recv_ret <= SOCKET_READ_SIZE);
      if (recv_ret > (int)size) {
        // 收到的大小大于请求的大小
        // 将多余部分存下来
        RecvBuffer recv_buffer;
        memcpy(recv_buffer.data, buffer_in + size, recv_ret - size);
        recv_buffer.recv_size = recv_ret - (int)size;
        buffers.emplace(socket, std::move(recv_buffer));
        // 返回需要的部分
        INFO("recv() get %lu bytes (%s, %lu)", size,
             abstract({buffer_in, size}).c_str(), size);
        memcpy(buff, buffer_in, size);
        return size;
      } else {
        // ocall 收到的大小小于等于请求的大小
        // 返回目前收到的数据，等待再次调用
        INFO("recv() get %d bytes (%s, %d)", recv_ret,
             abstract({buffer_in, recv_ret}).c_str(), recv_ret);
        memcpy(buff, buffer_in, (size_t)recv_ret);
        return (size_t)recv_ret;
      }
    } else {
      // 没有收到数据
      return (size_t)recv_ret;
    }
  } else {
    // 有已经缓存的数据
    auto &recv_buffer = it->second;
    if (recv_buffer.valid_size() > (int)size) {
      // 缓存数据充足
      INFO("recv() get %lu bytes from cached (%s, %lu)", size,
           abstract({recv_buffer.valid_data(), size}).c_str(), size);
      memcpy(buff, recv_buffer.valid_data(), size);
      recv_buffer.used_size += (int)size;
      return size;
    } else {
      // 返回已缓存的数据，释放缓存，等待下一次调用
      auto valid_size = recv_buffer.valid_size();
      INFO("recv() get %d bytes from cached (%s, %d)", valid_size,
           abstract({recv_buffer.valid_data(), size}).c_str(), valid_size);
      memcpy(buff, recv_buffer.valid_data(), valid_size);
      buffers.erase(it);
      return valid_size;
    }
  }
  UNREACHABLE();
}

extern "C" size_t send(int socket, const void *buff, size_t size, int flags) {
  (void)flags;
  int send_ret;
  o_send(&send_ret, socket, (const char *)buff, (int)size, &errno);
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