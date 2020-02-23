#include "Executor.h"
#include <boost/bind.hpp>
#include <iostream>
#include "App/deps/base64.h"
#include "SSLClient_port.h"
#include "Shared/deps/http_parser.h"
#include "sgx_uae_service.h"
#include "Oracle.h"

using namespace boost::asio;

EnclaveResult Executor::enclave_result;

// 在 Enclave 中创建对应的对象
void Executor::init_enclave_ssl(const std::string& hostname,
                                const std::string& request, int id) {
  int status;
  e_new_ssl(global_eid, &status, id, hostname.data(), hostname.size(),
            request.data(), request.size());
  ASSERT(status == StatusCode::Success);
}

// 解析域名之后的回调
void Executor::resolve_callback(boost::shared_ptr<Executor> p_executor,
                                const boost::system::error_code& ec,
                                ip::tcp::resolver::results_type endpoints) {
  auto& executor = *p_executor;
  if (ec) {
    // 解析错误
    LOG("Executor %d failed to resolve: %s", executor.id, ec.message().c_str());
    executor.async_error();
  } else {
    // 解析完成后，进行连接
    LOG("Executor %d resolved", executor.id);
    executor.state = Connect;
    executor.endpoints = std::move(endpoints);
  }
  executor.blocking = false;
  Oracle::global().need_work(std::move(p_executor));
}

// SSL 连接（握手）之后的回调
void Executor::connect_callback(boost::shared_ptr<Executor> p_executor,
                                const boost::system::error_code& ec,
                                const ip::tcp::endpoint& endpoint) {
  auto& executor = *p_executor;
  LOG("Executor %d connected to endpoint %s", executor.id,
      endpoint.address().to_string().c_str());
  if (ec) {
    // 连接失败
    LOG("Executor %d failed to connect: %s", executor.id, ec.message().c_str());
    executor.async_error();
  } else {
    // 连接成功
    LOG("Executor %d connected", executor.id);
    executor.state = Process;
    // 设置非阻塞
    executor.socket.non_blocking(true);
  }
  executor.blocking = false;
  Oracle::global().need_work(std::move(p_executor));
}

// 使用 SSLClient 进行 IAS 确认之后的回调
void Executor::ias_callback(boost::shared_ptr<Executor> p_executor,
                            const std::string& response) {
  auto& executor = *p_executor;
  if (response.empty()) {
    // 出现错误
    executor.async_error();
    return;
  } else {
    LOG("IAS done %d", executor.id);
    executor.state = Finished;
    executor.error_code = StatusCode::Success;
  }
  executor.blocking = false;
  Oracle::global().need_work(std::move(p_executor));
}

Executor::Executor(io_context& ctx, int id, const std::string& hostname,
                   const std::string& request)
    : hostname(hostname),
      resolver(ctx),
      start_time(steady_clock::now()),
      id(id),
      ctx(ctx),
      socket(ctx) {
  init_enclave_ssl(hostname, request, id);
}

void Executor::async_error() {
  state = Finished;
  error_code = StatusCode::LibraryError;
}

// 执行工作，返回是否完成，错误则抛出
bool Executor::work() {
  if (blocking) {
    // 正在进行异步操作，尚未结束
    if (steady_clock::now() - start_time > TASK_TIMEOUT) {
      // 超时时，关闭连接
      throw StatusCode(StatusCode::Timeout);
    }
    return false;
  }
  while (true) {
    switch (state) {
      case Resolve: {
        // 解析域名
        blocking = true;
        resolver.async_resolve(
            hostname, "https",
            boost::bind(resolve_callback, shared_from_this(),
                        placeholders::error, placeholders::results));
        return false;
      }
      case Connect: {
        // 进行连接（尝试所有 endpoints）
        blocking = true;
        async_connect(socket, endpoints,
                      boost::bind(connect_callback, shared_from_this(),
                                  placeholders::error, placeholders::endpoint));
        return false;
      }
      case Process: {
        // 由 Enclave 进行处理
        int status;
        e_work(global_eid, &status, id, &enclave_result);
        if (StatusCode(status).is_error()) {
          throw StatusCode(status);
        }
        if (status == StatusCode::Success) {
          // 处理完成
          LOG("Executor %d processing done", id);
          socket.close();
          state = Attest;
          // 执行下一步
          continue;
        } else if (status == StatusCode::Blocking) {
          // 阻塞中
          return false;
        }
        UNREACHABLE();
      }
      case Attest: {
        static const char spid[] =
            "\xDE\xD9\x6A\x2B\x18\x11\x28\xD8\x0F\x4F\x4F\xE9\x09\x1B\xCE\xBF";
        static_assert(sizeof(sgx_spid_t) < sizeof(spid));
        // todo: 更新 sigRL
        static const unsigned char* sigrl = nullptr;
        static const unsigned sigrl_size = 0;
        // 获取 quote 大小
        unsigned quote_size;
        sgx_calc_quote_size(sigrl, sigrl_size, &quote_size);
        // 获得 quote
        auto quote = (sgx_quote_t*)malloc(quote_size);
        sgx_get_quote(&enclave_result.report, SGX_LINKABLE_SIGNATURE,
                      (const sgx_spid_t*)spid, nullptr, nullptr, 0, nullptr,
                      quote, quote_size);
        // Base64 编码
        auto b64_quote = base64_encode((unsigned char*)quote, quote_size);
        free(quote);
        // 生成使用 IAS API 的请求
        auto hostname = "api.trustedservices.intel.com"s;
        auto request_body = "{\"isvEnclaveQuote\":\""s + b64_quote + "\"}"s;
        auto request =
            "POST /sgx/dev/attestation/v3/report HTTP/1.1\r\n"
            "Content-Type: application/json\r\n"
            "Host: api.trustedservices.intel.com\r\n"
            "Ocp-Apim-Subscription-Key: 141e8ac09b50434ea6b35198cf635090\r\n"
            "Content-Length: "s +
            std::to_string(request_body.size()) + "\r\n\r\n"s + request_body +
            "\r\n\r\n"s;
        // 连接 IAS 服务
        blocking = true;
        ssl_client =
            create_SSLClient(id, hostname, request, ctx,
                             boost::bind(ias_callback, shared_from_this(), _1));
        return false;
      }
      case Finished: {
        if (error_code.is_error()) {
          throw error_code;
        } else {
          return true;
        }
      }
      default: { UNREACHABLE(); }
    }
  }
  UNREACHABLE();
}

Executor::~Executor() { LOG("Removing executor %d", id); }

void Executor::close() {
  socket.close();
  close_SSLClient(ssl_client);
}