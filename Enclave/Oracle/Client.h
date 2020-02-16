#ifndef _E_WORKER_H_
#define _E_WORKER_H_

#include "Enclave/Enclave.h"
#include "Enclave/Enclave_t.h"
#include "Shared/Config.h"
#include "Shared/Logging.h"
#include "Shared/StatusCode.h"
#include "Shared/deps/http_parser.h"
#include "WolfSSL.h"
#include "sgx_trts.h"
#include <map>
#include <wolfssl/wolfio.h>

class Client {
public:
  enum State {
    Connecting,
    Writing,
    Reading,
    Complete,
  };

protected:
  // 当前状态
  State state = Connecting;
  // Session
  WOLFSSL *const ssl;
  // 需要发送的消息
  const std::string request_message;
  // 已经写完的长度
  int written_size = 0;
  // 解析回复
  http_parser parser;
  http_parser_settings parser_settings;
  // 回复
  std::string response;
  // 当 http_parser 调用完成回调时，设置为 true，停止读取
  bool response_complete = false;

  // 给定 WolfSSL 对于某一操作的返回值，
  // 返回 StatusCode::Success, StatusCode::LibraryError 或 StatusCode::Blocking
  StatusCode parse_wolfssl_status(int ret) const;

  // 发起握手连接，非阻塞，需要重复调用直到返回 StatusCode::Success
  StatusCode connect() const;

  // 发送内容，非阻塞，需要重复调用直到返回 StatusCode::Success
  StatusCode write();

  // 接收内容，非阻塞，需要重复调用直到收到足够数据
  // 重复从 socket 中进行读取，直到 socket 被阻塞
  StatusCode read();

  // 根据错误代码，打印 WolfSSL 的错误信息
  void print_wolfssl_error(int err) const;

  // 初始化 http_parser，在消息结束时置 response_complete = true
  void init_parser();

public:
  Client(const std::string &request_message, int socket);
  Client(std::string &&request_message, int socket);

  // 禁止 copy 和 move
  Client(const Client &) = delete;
  Client(Client &&) = delete;
  Client operator=(const Client &) = delete;
  Client operator=(Client &&) = delete;

  // 对应 socket 准备好时调用，继续进行一步操作，当 IO 再次等待时返回
  // 仅当全部流程处理完时返回 StatusCode::Success，否则返回 StatusCode::Blocking
  // 或错误代码
  StatusCode work();
  const std::string get_response() const;

  // 释放 ssl 对象
  ~Client();

  friend int recv_callback(WOLFSSL *, char *buffer, int size, void *ctx);
  friend int send_callback(WOLFSSL *, char *buffer, int size, void *ctx);
};

#endif // _E_WORKER_H_