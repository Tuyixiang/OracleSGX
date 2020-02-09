// 向目标地址请求网页
#include "WolfSSL.h"
#include <map>

class Client {
public:
  enum State {
    Initialized,
    Connecting,
    Connected,
    Writing,
    Reading,
    Idle,
  };

protected:
  // 当前状态
  State state = Initialized;
  // Session
  WOLFSSL *const ssl;

  // 发起握手连接，非阻塞，需要重复调用直到返回 SSL_SUCCESS
  int connect() { return wolfSSL_connect(ssl); }

  // 发送内容，非阻塞，需要重复调用直到返回 SSL_SUCCESS
  int write(const char *buff, int size) {
    return wolfSSL_write(ssl, buff, size);
  }

  // 接收内容，非阻塞，需要重复调用直到收到足够数据
  int read(const char *buff, int size) {
    return wolfSSL_write(ssl, buff, size);
  }

public:
  Client(const char *request_message, int socket)
      : ssl(wolfSSL_new(global_ctx)) {}

  // 释放 ssl 对象
  ~Client() { wolfSSL_free(ssl); }
};

// Client 连接通过 id 进行查找
std::map<int, Client *> clients;

// 创建一个新的 SSL 连接，返回连接的 id
int e_new_ssl(const char *request_message, int socket) {}