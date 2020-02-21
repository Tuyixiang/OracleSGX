#include "SSLClient.h"
#include <boost/asio/spawn.hpp>
#include <boost/beast/http.hpp>
#include <boost/bind.hpp>
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

void SSLClient::async_process(boost::shared_ptr<SSLClient> p_client,
                              yield_context yield) {
  auto& client = *p_client;
  try {
    // 检查错误的代码，供后面复用
    boost::system::error_code error;
    auto check = [&](const char* msg) {
      if (error == boost::asio::error::operation_aborted) {
        ERROR("SSLClient %d canceled before %s", client.id, msg);
        // 对象已经被终止，不执行回调
        throw false;
      } else if (error) {
        ERROR("SSLClient %d %s failed: %s", client.id, msg,
              error.message().c_str());
        throw true;
      } else {
        LOG("SSLClient %d done %s", client.id, msg);
      }
    };
    // 1. 解析域名
    auto endpoints =
        client.resolver.async_resolve(client.hostname, "https", yield[error]);
    check("resolving IAS hostname");
    // 2. 建立 TCP 连接
    async_connect(client.stream.lowest_layer(), endpoints, yield[error]);
    check("connecting to IAS service");
    // 3. 建立 SSL 握手连接
    client.stream.async_handshake(ssl::stream_base::client, yield[error]);
    check("establishing SSL connection with IAS service");
    // 4. 发送请求
    async_write(client.stream,
                const_buffer(client.request.data(), client.request.size()),
                yield[error]);
    check("sending IAS request");
    // 5. 接收回复
    streambuf buffer;
    http::response<http::string_body> response;
    http::async_read(client.stream, buffer, response, yield[error]);
    check("retrieving IAS response");
    client.callback(response.body());
  } catch (bool execute_callback) {
    if (execute_callback) {
      client.callback("");
    }
  }
}

void SSLClient::run() {
  // 使用 coroutine 连接异步操作
  LOG("Running SSLClient %d", id);
  // 需要捕获 id 数值用于打印 debug 信息
  // 因为在取消错误回调时，this 可能已被销毁
  auto capture_id = id;
  spawn(ctx, boost::bind(async_process, shared_from_this(), _1));
}

void SSLClient::close() {
  LOG("SSLClient %d closed", id);
  stream.lowest_layer().close();
  boost::system::error_code dont_care;
  stream.shutdown(dont_care);
}

SSLClient::~SSLClient() { LOG("Removing SSLClient %d", id); }