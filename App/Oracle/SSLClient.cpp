#include "SSLClient.h"
#include <boost/asio/spawn.hpp>
#include <boost/beast/http.hpp>
#include "Shared/Logging.h"
#include "Shared/deps/http_parser.h"
using namespace boost::asio;
using namespace boost::beast;

ssl::context ssl_ctx(ssl::context::method::sslv23_client);
bool initialized = false;

void initialize_context() {
  if (!initialized) {
    initialized = true;
    ssl_ctx.load_verify_file("App/Oracle/ca_certs.pem");
  }
}

SSLClient::SSLClient(const std::string& hostname, const std::string& request,
                     io_context& ctx, Callback&& cb)
    : ctx(ctx),
      resolver(ctx),
      stream(ctx, ssl_ctx),
      hostname(hostname),
      request(request),
      callback(std::move(cb)) {
  initialize_context();
  stream.set_verify_mode(ssl::verify_peer);
  stream.set_verify_callback(ssl::rfc2818_verification(hostname));
}

void SSLClient::run() {
  // 使用 coroutine 连接异步操作
  LOG("Running SSLClient");
  spawn(ctx, [&](auto yield) {
    try {
      // 错误检查
      boost::system::error_code error;
      auto check = [&](const std::string& label) {
        if (error) {
          throw "Failed to "s + label + ": "s + error.message();
        } else {
          LOG("Successfully %s", label.c_str());
        }
      };
      // 1. 解析域名
      auto endpoints = resolver.async_resolve(hostname, "https", yield[error]);
      check("resolve IAS hostname");
      // 2. 建立 TCP 连接
      async_connect(stream.lowest_layer(), endpoints, yield[error]);
      check("connect to IAS service");
      // 3. 建立 SSL 握手连接
      stream.async_handshake(ssl::stream_base::client, yield[error]);
      check("establish SSL connection to IAS service");
      // 4. 发送请求
      async_write(stream, const_buffer(request.data(), request.size()),
                  yield[error]);
      check("send IAS request");
      // 5. 接收回复
      streambuf buffer;
      http::response<http::string_body> response;
      http::async_read(stream, buffer, response, yield[error]);
      check("retrieve IAS response");
      callback(response.body());
    } catch (const std::string& error_message) {
      ERROR(error_message.c_str());
      callback("");
    }
  });
}