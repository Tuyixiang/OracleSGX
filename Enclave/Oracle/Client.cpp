// 向目标地址请求网页
#include "Client.h"
#include "WolfSSL.h"
#include <wolfssl/wolfcrypt/sha512.h>
#include <wolfssl/wolfio.h>
#include <map>
#include "Enclave/Enclave_t.h"
#include "Enclave/deps/cJSON.h"
#include "sgx_trts.h"
#include "sgx_uae_service.h"

// 给定 WolfSSL 对于某一操作的返回值，
// 返回 StatusCode::Success, StatusCode::LibraryError 或 StatusCode::Blocking
StatusCode Client::parse_wolfssl_status(int ret) const {
  if (ret == SSL_SUCCESS) {
    return StatusCode::Success;
  } else {
    // WolfSSL 应当返回等待 IO 的错误代码
    auto err = wolfSSL_get_error(ssl, ret);
    switch (err) {
      case SSL_ERROR_WANT_READ:
      case SSL_ERROR_WANT_WRITE:
        return StatusCode::Blocking;
      default:
        print_wolfssl_error(err);
        return StatusCode::LibraryError;
    }
  }
  UNREACHABLE();
}

// 发起握手连接，非阻塞，需要重复调用直到返回 StatusCode::Success
StatusCode Client::connect() const {
  return parse_wolfssl_status(wolfSSL_connect(ssl));
}

// 发送内容，非阻塞，需要重复调用直到返回 StatusCode::Success
StatusCode Client::write() {
  auto request_size = static_cast<int>(request.size());
  auto ret = wolfSSL_write(ssl, request.c_str() + written_size,
                           request_size - written_size);
  if (ret > 0) {
    // 更新已发送长度
    written_size += ret;
    if (written_size >= request_size) {
      return StatusCode::Success;
    } else {
      return StatusCode::Blocking;
    }
  } else {
    return parse_wolfssl_status(ret);
  }
}

// 接收内容，非阻塞，需要重复调用直到收到足够数据
// 重复从 socket 中进行读取，直到 socket 被阻塞
StatusCode Client::read() {
  static char buffer[SOCKET_READ_SIZE];
  auto old_size = response.size();
  auto ret = wolfSSL_read(ssl, buffer, SOCKET_READ_SIZE);
  // 重复读直到 socket 没有准备好的数据
  while (ret > 0) {
    response.append(buffer, ret);
    // 拒绝过大的网页响应
    if (response.size() > MAX_RESPONSE_SIZE) {
      return StatusCode::ResponseTooLarge;
    }
    ret = wolfSSL_read(ssl, buffer, SOCKET_READ_SIZE);
  }
  // 本次 read() 没有读取到任何数据，则直接返回
  if (response.size() == old_size) {
    return parse_wolfssl_status(ret);
  }
  // 解析网页
  auto parse_size =
      http_parser_execute(&parser, &parser_settings, response.data() + old_size,
                          response.size() - old_size);
  // 检查解析错误
  if (parse_size < response.size() - old_size) {
    return StatusCode::ParserError;
  }
  // 检查是否解析完成
  if (response_complete) {
    return StatusCode::Success;
  } else {
    return parse_wolfssl_status(ret);
  }
}

// 根据错误代码，打印 WolfSSL 的错误信息
void Client::print_wolfssl_error(int err) const {
  static char buffer[89] = "WOLFSSL: ";
  wolfSSL_ERR_error_string((unsigned long)err, buffer + 9);
  ERROR(buffer);
}

// 初始化 http_parser，在消息结束时置 response_complete = true
void Client::init_parser() {
  parser_settings.on_message_complete = [&](auto) {
    response_complete = true;
    return 0;
  };
  http_parser_init(&parser, HTTP_RESPONSE);
}

// 打包一个用于生成 quote 的数据，包含所有必要信息
std::string Client::wrap() const {
  auto obj = cJSON_CreateObject();
  // todo: time
  cJSON_AddStringToObject(obj, "request", request.c_str());
  cJSON_AddStringToObject(obj, "response", response.c_str());
  auto json_dump = cJSON_Print(obj);
  std::string str = json_dump;
  cJSON_free(json_dump);
  cJSON_Delete(obj);
  return str;
}

Client::Client(const std::string &request, int id)
    : id(id), ssl(wolfSSL_new(global_ctx)), request(request) {
  wolfSSL_set_fd(ssl, id);
  init_parser();
}

Client::Client(std::string &&request, int id)
    : id(id), ssl(wolfSSL_new(global_ctx)), request(std::move(request)) {
  wolfSSL_set_fd(ssl, id);
  init_parser();
}

// 对应 socket 准备好时调用，继续进行一步操作，当 IO 再次等待时返回
// 仅当全部流程处理完时返回 StatusCode::Success，否则返回 StatusCode::Blocking
// 或错误代码
StatusCode Client::work() {
  while (true) {
    switch (state) {
      case Connecting: {
        // 连接中，检查返回状态是否为 Success
        switch (connect()) {
          case StatusCode::Success: {
            // 成功后则转 Writing 继续执行
            LOG("Connected");
            state = Writing;
            continue;
          }
          case StatusCode::Blocking: {
            LOG("Connecting");
            return StatusCode::Blocking;
          }
          case StatusCode::LibraryError: {
            LOG("Connection failed");
            return StatusCode::LibraryError;
          }
          default:
            UNREACHABLE();
        }
        UNREACHABLE();
      }
      case Writing: {
        // 正在发送请求，检查返回状态是否为 Success
        switch (write()) {
          case StatusCode::Success: {
            // 已经发送完成
            LOG("Request Sent");
            state = Reading;
            continue;
          }
          case StatusCode::Blocking: {
            LOG("%d/%lu bytes written", written_size, request.size());
            return StatusCode::Blocking;
          }
          case StatusCode::LibraryError: {
            LOG("Writing failed");
            return StatusCode::LibraryError;
          }
          default:
            UNREACHABLE();
        }
      }
      case Reading: {
        // 正在读取并处理响应，仅当读取完时返回 Success
        switch (read()) {
          case StatusCode::Success: {
            // 响应接收完成
            LOG(GREEN "Response received" RESET);
            state = Quoting;
            continue;
          }
          case StatusCode::Blocking: {
            LOG("%lu bytes received", response.size());
            return StatusCode::Blocking;
          }
          case StatusCode::LibraryError: {
            LOG("Reading failed");
            return StatusCode::LibraryError;
          }
          case StatusCode::ParserError: {
            LOG("Parsing failed for message:\n%s", response.c_str());
            return StatusCode::ParserError;
          }
          default: { UNREACHABLE(); }
        }
      }
      case Quoting: {
        // 获取需要 hash 的全部信息
        LOG("Getting JSON dump");
        response = wrap();
        // hash
        LOG("Hashing");
        Sha512 sha512;
        unsigned char sha_sum[64];
        static_assert(sizeof(sgx_report_data_t) == 64);
        wc_InitSha512(&sha512);
        wc_Sha512Update(&sha512, (unsigned char *)response.data(), response.size());
        wc_Sha512Final(&sha512, sha_sum);
        // 生成 report
        LOG("Creating Report");
        sgx_create_report(&target_info, (sgx_report_data_t *)sha_sum, &report);
        state = Complete;
        return StatusCode::Success;
      }
      case Complete: {
        UNREACHABLE();
      }
      default: { UNIMPLEMENTED(); }
    }
  }
}

// 释放 ssl 对象
Client::~Client() {
  LOG("Free SSL object")
  wolfSSL_free(ssl);
}