#include "Oracle.h"
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/StatusCode.h"

using namespace boost::asio;

const std::string request =
    "GET / HTTP/1.1\n"
    "Host: www.baidu.com\n"
    "Upgrade-Insecure-Requests: 1\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_2) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.100 "
    "Safari/537.36\n"
    "Accept: "
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/"
    "apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\n"
    "Accept-Encoding: identity\n\n";

void test() {
  Oracle::global().test_run("www.baidu.com", request);
}

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
  auto &socket = iter->second.socket;
  // 调用非阻塞 IO
  boost::system::error_code error;
  auto recv_size = socket.receive(
      mutable_buffer(buffer, std::min(SOCKET_READ_SIZE, size)), 0, error);
  if (!error) {
    // 成功读取数据，则返回数据
    LOG("o_recv() get %lu bytes (%s, %d)", recv_size,
        abstract({buffer, recv_size}).c_str(), size);
    *p_buffer = buffer;
    return recv_size;
  } else if (error == error::would_block) {
    // 数据等待中
    LOG("o_recv() blocking");
    *p_errno = EWOULDBLOCK;
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
  auto iter =  Oracle::global().executors.find(socket_id);
  if (iter ==  Oracle::global().executors.cend()) {
    ERROR("No socket with id %d", socket_id);
    *p_errno = ENOTSOCK;
    return -1;
  }
  auto &socket = iter->second.socket;
  boost::system::error_code error;
  // 调用非阻塞 IO
  auto send_size = socket.send(const_buffer(buffer, size), 0, error);
  if (!error) {
    // 发送成功
    LOG("o_send() send %lu bytes (%s, %d)", send_size,
        abstract({buffer, send_size}).c_str(), size);
    return send_size;
  } else if (error == error::would_block) {
    // 数据等待中
    LOG("o_send() blocking");
    *p_errno = EWOULDBLOCK;
    return -1;
  } else {
    // 出现错误
    ERROR("o_send() failed with message: %s", error.message());
    *p_errno = EBADF;
    return -1;
  }
  UNREACHABLE();
}