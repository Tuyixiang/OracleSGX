// 向目标地址请求网页
#include "Enclave/Enclave.h"
#include "Enclave/Enclave_t.h"
#include "Logging.h"
#include "Shared/Config.h"
#include "Shared/StatusCode.h"
#include "WolfSSL.h"
#include "sgx_trts.h"
#include <map>

class Client {
public:
  enum State {
    Connecting,
    Writing,
    Reading,
    Idle,
  };

protected:
  // 当前状态
  State state = Connecting;
  // Session
  WOLFSSL *const ssl;
  // 需要发送的消息
  const char *const request_message;
  // 消息长度
  const int request_size;
  // 已经写完的长度
  int written_size = 0;

  // 给定 WolfSSL 对于某一操作的返回值，
  // 返回 StatusCode::Success, StatusCode::WolfsslError 或 StatusCode::Blocking
  StatusCode parse_wolfssl_status(int ret) const {
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
        return StatusCode::WolfsslError;
      }
    }
  }

  // 发起握手连接，非阻塞，需要重复调用直到返回 StatusCode::Success
  StatusCode connect() const {
    return parse_wolfssl_status(wolfSSL_connect(ssl));
  }

  // 发送内容，非阻塞，需要重复调用直到返回 StatusCode::Success
  // 返回状态以及已经写入的字符数
  std::tuple<StatusCode, int> write(const char *buff, int size) const {
    auto ret = wolfSSL_write(ssl, buff, size);
    if (ret > 0) {
      return std::make_tuple(StatusCode::Success, ret);
    } else {
      return std::make_tuple(parse_wolfssl_status(ret), 0);
    }
  }

  // 接收内容，非阻塞，需要重复调用直到收到足够数据
  // 返回状态以及已经读取的字符数
  std::tuple<StatusCode, int> read(char *buff, int size) const {
    auto ret = wolfSSL_read(ssl, buff, size);
    if (ret > 0) {
      return std::make_tuple(StatusCode::Success, ret);
    } else {
      return std::make_tuple(parse_wolfssl_status(ret), 0);
    }
  }

  void print_wolfssl_error(int err) const {
    static char buffer[89] = "WOLFSSL: ";
    wolfSSL_ERR_error_string((unsigned long)err, buffer + 9);
    ERROR(buffer);
  }

public:
  Client(const char *request_message, int socket)
      : ssl(wolfSSL_new(global_ctx)), request_message(request_message),
        request_size(strlen(request_message)) {}

  // 禁止 copy 和 move
  Client(const Client &) = delete;
  Client(Client &&) = delete;

  // 对应 socket 准备好时调用，继续进行一步操作，当 IO 再次等待时返回
  // 仅当全部流程处理完时返回 StatusCode::Success，否则返回 StatusCode::Blocking 或错误代码
  StatusCode work() {
    while (true) {
      switch (state) {
      case Connecting: {
        // 连接中，检查返回状态是否为 Success
        switch (parse_wolfssl_status(connect())) {
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
          case StatusCode::WolfsslError: {
            LOG("Connection failed");
            return StatusCode::WolfsslError;
          }
          default:
            UNREACHABLE();
        }
        UNREACHABLE();
      }
      case Writing: {
        // 正在发送请求，检查返回状态是否为 Success
        // 从已经发完的部分后面继续发送
        auto [status, size] = write(request_message + written_size, request_size - written_size);
        switch (status) {
          case StatusCode::Success: {
            written_size += size;
            LOG(std::to_string(written_size) + "/" + std::to_string(request_size) + " bytes written"s);
            if (written_size >= request_size) {
              // 发送完成，转 Reading 继续执行
              state = Reading;
              continue;
            } else {
              return StatusCode::Blocking;
            }
            UNREACHABLE();
          }
          case StatusCode::Blocking: {
            LOG("Writing");
            return StatusCode::Blocking;
          }
          case StatusCode::WolfsslError: {
            LOG("Writing failed");
            return StatusCode::WolfsslError;
          }
          default:
            UNREACHABLE();
        }
      }
      case Reading: {
        // 正在读取并处理响应，仅当读取完时返回 Success
        auto [status, size] = write(request_message + written_size, request_size - written_size);
        switch (status) {
          case StatusCode::Success: {
            written_size += size;
            LOG(std::to_string(written_size) + "/" + std::to_string(request_size) + " bytes written"s);
            if (written_size >= request_size) {
              // 发送完成，转 Reading 继续执行
              state = Reading;
              continue;
            } else {
              return StatusCode::Blocking;
            }
            UNREACHABLE();
          }
          case StatusCode::Blocking: {
            LOG("Writing");
            return StatusCode::Blocking;
          }
          case StatusCode::WolfsslError: {
            LOG("Writing failed");
            return StatusCode::WolfsslError;
          }
          default:
            UNREACHABLE();
        }
      }
      default: {
        UNIMPLEMENTED();
      }
      }
    }
  }

  // 释放 ssl 对象
  ~Client() {
    LOG("Free SSL object")
    wolfSSL_free(ssl);
  }
};

// Client 连接通过 id 进行查找
std::map<int, Client> clients;

// 创建一个新的 SSL 连接，返回连接的 id
int e_new_ssl(int *p_id, const char *request_message, int socket) {
  // 检查连接是否达到上限
  if (clients.size() >= MAX_WORKER) {
    return StatusCode::NoAvailableWorker;
  }
  // 获取一个未使用的 id
  int &id = *p_id;
  do {
    sgx_read_rand((unsigned char *)&id, sizeof(int));
    id = abs(id);
  } while (clients.find(id) != clients.cend());
  // 创建 client
  clients.emplace(std::piecewise_construct, std::make_tuple(id),
                  std::make_tuple(request_message, socket));
  LOG("Created SSL with id " + std::to_string(id));
  return StatusCode::Success;
}
