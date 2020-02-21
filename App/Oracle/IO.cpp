// send 和 recv 的 ocall
#include "Oracle.h"

// 需要符合 Linux socket IO 的接口，按照标准设置 errno
int o_recv(int socket_id, char **p_buffer, int size, int *p_errno) {
  static char buffer[SOCKET_READ_SIZE];
  // 找到对应的 socket
  auto iter = Oracle::global().executors.find(socket_id);
  if (iter == Oracle::global().executors.cend()) {
    ERROR("No socket with id %d", socket_id);
    *p_errno = ENOTSOCK;
    return -1;
  }
  auto &executor = iter->second;
  auto &socket = executor.socket;
  // 调用非阻塞 IO
  boost::system::error_code error;
  auto recv_size = socket.receive(
      mutable_buffer(buffer, std::min(SOCKET_READ_SIZE, size)), 0, error);
  if (!error) {
    // 成功读取数据，则返回数据
    INFO("o_recv() get %lu bytes (%s, %d)", recv_size,
         abstract({buffer, recv_size}).c_str(), size);
    *p_buffer = buffer;
    ASSERT(recv_size <= sizeof(buffer));
    return (int)recv_size;
  } else if (error == error::would_block) {
    // 数据等待中
    INFO("o_recv() blocking");
    *p_errno = EWOULDBLOCK;
    executor.blocking = true;
    socket.async_wait(ip::tcp::socket::wait_read, [&](auto &error) {
      INFO("o_recv() block release");
      if (error == error::operation_aborted) {
        // 已经取消，此时 Executor 可能已经被释放
        return;
      }
      executor.blocking = false;
      if (error) {
        ERROR("async_wait on receive failed: %s", error.message().c_str());
        executor.async_error();
      }
    });
    return -1;
  } else {
    // 出现错误
    ERROR("o_recv() failed with message: %s", error.message().c_str());
    *p_errno = EBADF;
    return -1;
  }
  UNREACHABLE();
}

int o_send(int socket_id, const char *buffer, int size, int *p_errno) {
  // 找到对应的 socket
  auto iter = Oracle::global().executors.find(socket_id);
  if (iter == Oracle::global().executors.cend()) {
    ERROR("No socket with id %d", socket_id);
    *p_errno = ENOTSOCK;
    return -1;
  }
  auto &executor = iter->second;
  auto &socket = executor.socket;
  boost::system::error_code error;
  // 调用非阻塞 IO
  auto send_size = socket.send(const_buffer(buffer, size), 0, error);
  if (!error) {
    // 发送成功
    ASSERT(send_size > 0 and (int) send_size <= size);
    INFO("o_send() send %lu bytes (%s, %d)", send_size,
         abstract({buffer, send_size}).c_str(), size);
    return (int)send_size;
  } else if (error == error::would_block) {
    // 数据等待中
    INFO("o_send() blocking");
    *p_errno = EWOULDBLOCK;
    executor.blocking = true;
    socket.async_wait(ip::tcp::socket::wait_write, [&](auto &error) {
      INFO("o_send() block release");
      if (error == error::operation_aborted) {
        // 已经取消，此时 Executor 可能已经被释放
        return;
      }
      executor.blocking = false;
      if (error) {
        ERROR("async_wait on send failed: %s", error.message().c_str());
        executor.async_error();
      }
    });
    return -1;
  } else {
    // 出现错误
    ERROR("o_send() failed with message: %s", error.message());
    *p_errno = EBADF;
    return -1;
  }
  UNREACHABLE();
}