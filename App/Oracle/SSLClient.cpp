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

SSLClient::SSLClient(int id, const std::string& hostname,
                     const std::string& request, io_context& ctx, Callback&& cb)
    : id(id),
      ctx(ctx),
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
  LOG("Running SSLClient %d", id);
  // 需要捕获 id 数值用于打印 debug 信息
  // 因为在取消错误回调时，this 可能已被销毁
  auto capture_id = id;
  spawn(ctx, [this, capture_id](auto yield) {
    try {
      // 当 SSLClient 对象被释放时，置 flag 为 false，此后的异步操作将直接终止
      bool destroy_flag = false;
      this->destroy_flag = &destroy_flag;
      // 检查错误的代码，供后面复用
      boost::system::error_code error;
      auto check = [&](const char* msg) {
        if (destroy_flag or error == error::operation_aborted) {
          ERROR("SSLClient %d canceled before %s", capture_id, msg);
          // 对象已经被终止，不执行回调
          throw false;
        } else if (error) {
          ERROR("SSLClient %d %s failed: %s", capture_id, msg,
                error.message().c_str());
          throw true;
        } else {
          LOG("SSLClient %d done %s", capture_id, msg);
        }
      };
      // 1. 解析域名
      auto endpoints = resolver.async_resolve(hostname, "https", yield[error]);
      check("resolving IAS hostname");
      // 2. 建立 TCP 连接
      async_connect(stream.lowest_layer(), endpoints, yield[error]);
      check("connecting to IAS service");
      // 3. 建立 SSL 握手连接
      stream.async_handshake(ssl::stream_base::client, yield[error]);
      check("establishing SSL connection with IAS service");
      // 4. 发送请求
      async_write(stream, const_buffer(request.data(), request.size()),
                  yield[error]);
      check("sending IAS request");
      // 5. 接收回复
      streambuf buffer;
      http::response<http::string_body> response;
      http::async_read(stream, buffer, response, yield[error]);
      check("retrieving IAS response");
      callback(response.body());
      this->destroy_flag = nullptr;
    } catch (bool execute_callback) {
      if (execute_callback) {
        // callback("");
        destroy_flag = nullptr;
      }
    }
  });
}

void SSLClient::close() {
  LOG("SSLClient %d closed", id);
  stream.lowest_layer().close();
  boost::system::error_code dont_care;
  stream.shutdown(dont_care);
  if (destroy_flag) {
    *destroy_flag = true;
  }
}

SSLClient::~SSLClient() {
  LOG("Removing SSLClient %d", id);
  if (destroy_flag) {
    *destroy_flag = true;
  }
}