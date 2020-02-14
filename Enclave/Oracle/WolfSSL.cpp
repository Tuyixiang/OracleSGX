#include "WolfSSL.h"
#include "CA.h"
#include "Enclave/Enclave.h"
#include "Enclave/Enclave_t.h"
#include "Shared/Logging.h"
#include "Shared/StatusCode.h"
#include "Worker.h"

#include "sgx_trts.h"

WOLFSSL_CTX *global_ctx = nullptr;

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
  // 保存 ctx
  global_ctx = ctx;
  LOG("Context initialized");
  return StatusCode::Success;
}

// Worker 连接通过 id 进行查找
std::map<int, Worker> workers;

// 创建一个新的 SSL 连接，返回连接的 id
int e_new_ssl(int *p_id, const char *request_message, int request_size,
              int socket) {
  // 检验 ctx 已经初始化
  if (global_ctx == nullptr) {
    return StatusCode::Uninitialized;
  }
  // 检查连接是否达到上限
  if (workers.size() >= MAX_WORKER) {
    return StatusCode::NoAvailableWorker;
  }
  // 获取一个未使用的 id
  int &id = *p_id;
  do {
    sgx_read_rand((unsigned char *)&id, sizeof(int));
    id = abs(id);
  } while (workers.find(id) != workers.cend());
  // 创建 client
  workers.emplace(
      std::piecewise_construct, std::make_tuple(id),
      std::make_tuple(std::string(request_message, request_size), socket));
  LOG("Created SSL with id %d", id);
  return StatusCode::Success;
}

// 根据 id 令指定的 SSL 连接进行工作
// 如果处理完成，则将网页写入 p_response 指向的空间（至少 1MB）
int e_work(int id, char *p_response, int *p_response_size) {
  // 根据 id 找到指定的 worker
  auto iter = workers.find(id);
  if (iter == workers.cend()) {
    ERROR("No worker with id %d", id);
    return StatusCode::Unknown;
  }
  auto &worker = iter->second;
  // 执行操作
  auto status = worker.work();
  switch (status) {
  case StatusCode::Success: {
    // 执行完成
    const auto &response = worker.get_response();
    memcpy(p_response, response.data(), response.size());
    *p_response_size = (int)response.size();
    // 释放空间
    LOG("Work finished, freeing worker %d", id);
    workers.erase(iter);
    return StatusCode::Success;
  }
  case StatusCode::Blocking: {
    // 正在等待 IO
    return StatusCode::Blocking;
  }
  default: {
    // 出现错误
    ASSERT(status.is_error());
    // 释放空间
    LOG("Work failed, freeing worker %d", id);
    workers.erase(iter);
    return status;
  }
  }
  UNREACHABLE();
}