#include "Executor.h"
#include <iostream>
#include "App/deps/base64.h"
#include "SSLClient_port.h"
#include "Shared/deps/http_parser.h"
#include "sgx_uae_service.h"

using namespace boost::asio;

EnclaveResult Executor::enclave_result;

// 在 Enclave 中创建对应的对象
void Executor::init_enclave_ssl(const std::string& request, int id) {
  int status;
  e_new_ssl(global_eid, &status, id, request.data(), (int)request.size());
  ASSERT(status == StatusCode::Success);
}

Executor::Executor(io_context& ctx, int id, std::string address,
                   const std::string& request)
    : address(std::move(address)),
      id(id),
      resolver(ctx),
      ctx(ctx),
      socket(ctx) {
  init_enclave_ssl(request, id);
  // 设置 non_blocking 需要在 socket 连接后进行，因此在回调中进行
  // socket.non_blocking(true);
}

void Executor::async_error() {
  state = Error;
  error_code = StatusCode::LibraryError;
}

// 执行工作，返回是否完成，错误则抛出
bool Executor::work() {
  if (blocking) {
    // 正在进行异步操作，尚未结束
    return false;
  }
  while (true) {
    switch (state) {
      case Resolve: {
        // 解析域名
        blocking = true;
        resolver.async_resolve(address, "https",
                               [&](auto& error, auto results) {
                                 blocking = false;
                                 if (error) {
                                   // 解析错误
                                   LOG("Executor %d failed to resolve: %s", id,
                                       error.message().c_str());
                                   async_error();
                                 } else {
                                   // 解析完成后，进行连接
                                   LOG("Executor %d resolved", id);
                                   state = Connect;
                                   endpoints = results;
                                   // 执行下一步
                                   work();
                                 }
                               });
        return false;
      }
      case Connect: {
        // 进行连接（尝试所有 endpoints）
        blocking = true;
        async_connect(socket, endpoints, [&](auto& error, auto) {
          blocking = false;
          if (error) {
            // 连接失败
            LOG("Executor %d failed to connect: %s", id,
                error.message().c_str());
            async_error();
          } else {
            // 连接成功
            LOG("Executor %d connected", id);
            state = Process;
            // 设置非阻塞
            socket.non_blocking(true);
            // 执行下一步
            work();
          }
        });
        return false;
      }
      case Process: {
        // 由 Enclave 进行处理
        int status;
        e_work(global_eid, &status, id, &enclave_result);
        if (StatusCode(status).is_error()) {
          throw status;
        }
        if (status == StatusCode::Success) {
          // 处理完成
          LOG("Executor %d processing done", id);
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
                      (sgx_spid_t*)spid, nullptr, nullptr, 0, nullptr, quote,
                      quote_size);
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
        ssl_client = create_SSLClient(
            hostname, request, ctx, [&](const std::string& response_body) {
              blocking = false;
              if (response_body.empty()) {
                // 出现错误
                async_error();
                return;
              }
              LOG("Response: %s", response_body.c_str());
              state = Error;
              error_code = StatusCode::Success;
            });
        return false;
      }
      case Error: {
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

Executor::~Executor() {
  free_SSLClient(ssl_client);
}