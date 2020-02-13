#include "Oracle.h"
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/StatusCode.h"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>

using namespace boost::asio;

struct Response {
  char *p;
  int size;

  Response() : p(new char[MAX_RESPONSE_SIZE]), size(0) {}
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

void test_send(sgx_enclave_id_t eid) {
  int status;
  e_init(eid, &status);
  int ssl_id;

  auto ctx = io_service();
  auto resolver = ip::tcp::resolver(ctx);
  auto query = ip::tcp::resolver::query("www.baidu.com", "https");
  auto endpoints = resolver.resolve(query);
  auto socket = ip::tcp::socket(ctx);
  connect(socket, endpoints);
  for (auto it = endpoints; it != ip::tcp::resolver::iterator(); it++) {
    std::cout << it->endpoint() << std::endl;
  }

  e_new_ssl(eid, &status, &ssl_id, request.data(), (int)request.size(), 0);

  Response response;
  e_work(eid, &status, ssl_id, response.p, &response.size);
}