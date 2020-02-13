#include "WolfSSL.h"
#include "CA.h"
#include "Enclave/Enclave.h"
#include "Enclave/Enclave_t.h"
#include "Shared/Logging.h"
#include "Shared/StatusCode.h"
#include "Worker.h"

#include "sgx_trts.h"

WOLFSSL_CTX *global_ctx = nullptr;

int recv_callback(WOLFSSL *, char *buffer, int size, void *ctx) {
  auto &worker = *reinterpret_cast<Worker *>(ctx);
  if (worker.recv_status > 0) {
    // 有内容可以读取
    auto buffered_size = worker.recv_status;
    LOG("Receive: %d bytes (%s)", buffered_size, abstract(worker.recv_buffer).c_str());
    ASSERT(size >= buffered_size);
    memcpy(buffer, worker.recv_buffer.data(), buffered_size);
    // 清除已缓存数据
    worker.recv_status = WOLFSSL_CBIO_ERR_WANT_READ;
    // 返回长度
    return buffered_size;
  } else if (worker.recv_status == WOLFSSL_CBIO_ERR_WANT_READ) {
    // IO 阻塞，返回此信号
    return WOLFSSL_CBIO_ERR_WANT_READ;
  } else {
    // IO 异常，返回错误
    LOG("IO error with recv_status=%d", worker.recv_status);
    return WOLFSSL_CBIO_ERR_GENERAL;
  }
  UNREACHABLE();
}

int send_callback(WOLFSSL *, char *buffer, int size, void *ctx) {
  auto &worker = *reinterpret_cast<Worker *>(ctx);
  LOG("Send: %d bytes (%s). Previous send_status=%d", size,
      abstract({buffer, size}).c_str(), worker.send_status);
  if (worker.send_status > 0) {
    // 之前的内容发送成功
    auto sent_size = worker.send_status;
    ASSERT(std::string(buffer, sent_size) ==
           std::string(worker.send_buffer, sent_size));
    // 清除已缓存数据
    worker.send_status = WOLFSSL_CBIO_ERR_WANT_WRITE;
    // 返回长度
    return sent_size;
  } else if (worker.send_status == WOLFSSL_CBIO_ERR_WANT_WRITE) {
    // IO 阻塞，返回此信号，同时写入 buffer 等待真正的 IO 操作
    worker.send_buffer = {buffer, size};
    return WOLFSSL_CBIO_ERR_WANT_WRITE;
  } else {
    // IO 异常，返回错误
    LOG("IO error with send_status=%d", worker.send_status);
    return WOLFSSL_CBIO_ERR_GENERAL;
  }
  UNREACHABLE();
}

// 初始化系统
int e_init() {
  // 初始化 WolfSSL
  wolfSSL_Init();
  // 创建 ctx
  auto method = wolfSSLv23_client_method();
  auto ctx = wolfSSL_CTX_new(method);
  // 加载 CA 证书
  wolfSSL_CTX_load_verify_buffer(ctx, (const unsigned char *)ca_certs_raw,
                                 strlen(ca_certs_raw), SSL_FILETYPE_PEM);
  // 重载 IO 操作
  wolfSSL_SetIORecv(ctx, recv_callback);
  wolfSSL_SetIOSend(ctx, send_callback);
  // 保存 ctx
  global_ctx = ctx;
  LOG("Context initialized");
  return StatusCode::Success;
}