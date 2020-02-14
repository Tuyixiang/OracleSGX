#include "Oracle.h"
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/StatusCode.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <errno.h>
#include <iostream>
#include <memory>

using namespace boost::asio;

struct Response {
  char *p;
  int size;

  static const int capacity = MAX_RESPONSE_SIZE;

  Response() : p(new char[capacity]), size(0) {}
  Response(Response &&other) : p(other.p), size(other.size) {
    other.p = nullptr;
    other.size = 0;
  }
  Response(const Response &other) : Response() {
    memcpy(p, other.p, other.size);
  }
  Response &operator=(Response &&other) {
    delete[] p;
    new (this) Response(std::move(other));
    return *this;
  }
  Response &operator=(const Response &other) {
    delete[] p;
    new (this) Response(other);
    return *this;
  }
  ~Response() { delete[] p; }
};

const std::string request =
    "GET / HTTP/1.1\n"
    "Host: baidu.com\n"
    "Upgrade-Insecure-Requests: 1\n"
    "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_2) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.100 "
    "Safari/537.36\n"
    "Accept: "
    "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/"
    "apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\n"
    "Accept-Encoding: gzip, deflate\n";

std::map<int, ip::tcp::socket> sockets;

void test_send(sgx_enclave_id_t eid) {
  int status;
  e_init(eid, &status);
  int ssl_id;

  auto ctx = io_service();
  auto resolver = ip::tcp::resolver(ctx);
  auto query = ip::tcp::resolver::query("www.baidu.com", "https");
  auto endpoints = resolver.resolve(query);
  sockets.emplace(0, ctx);
  auto &socket = sockets.at(0);
  connect(socket, endpoints);
  socket.non_blocking(true);
  e_new_ssl(eid, &status, &ssl_id, request.data(), (int)request.size(), 0);
  Response response;
  e_work(eid, &status, ssl_id, response.p, &response.size);
}

// 需要符合 Linux socket IO 的接口，按照标准设置 errno
int o_recv(int socket_id, char **p_buffer, int *p_size, int *p_errno) {
  static char buffer[SOCKET_READ_SIZE];
  // 找到对应的 socket
  auto iter = sockets.find(socket_id);
  if (iter == sockets.cend()) {
    ERROR("No socket with id %d", socket_id);
    *p_errno = ENOTSOCK;
    return -1;
  }
  auto &socket = iter->second;
  boost::system::error_code error;
  // 调用非阻塞 IO
  auto recv_size =
      socket.receive(mutable_buffer(buffer, SOCKET_READ_SIZE), 0, error);
  if (!error) {
    // 成功读取数据，则返回数据
    LOG("o_recv() get %lu bytes (%s)", recv_size,
        abstract({buffer, recv_size}).c_str());
    *p_buffer = buffer;
    *p_size = (int)recv_size;
    return recv_size;
  } else if (error == error::would_block) {
    // 数据等待中
    LOG("o_recv() blocking");
    *p_errno = EWOULDBLOCK;
    return -1;
  } else {
    // 出现错误
    ERROR("o_recv() failed with message: %s", error.message());
    *p_errno = EBADF;
    return -1;
  }
  UNREACHABLE();
}

int o_send(int socket_id, const char *buffer, int size, int *p_errno) {
  // 找到对应的 socket
  auto iter = sockets.find(socket_id);
  if (iter == sockets.cend()) {
    ERROR("No socket with id %d", socket_id);
    *p_errno = ENOTSOCK;
    return -1;
  }
  auto &socket = iter->second;
  boost::system::error_code error;
  // 调用非阻塞 IO
  auto send_size = socket.send(const_buffer(buffer, size), 0, error);
  if (!error) {
    // 发送成功
    LOG("o_send() send %lu bytes (%s)", send_size,
        abstract({buffer, send_size}).c_str());
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