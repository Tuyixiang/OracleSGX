#include "WolfSSL.h"
#include <map>
#include "CA.h"
#include "Client.h"
#include "Enclave/Enclave.h"
#include "Enclave/Enclave_t.h"
#include "Shared/EnclaveResult.h"
#include "Shared/Logging.h"
#include "Shared/StatusCode.h"

#include "sgx_trts.h"

WOLFSSL_CTX *global_ctx = nullptr;
sgx_target_info_t target_info;

// 初始化系统
int e_init(const void *p_target_info) {
  // 记录 target_info
  ASSERT(sgx_is_outside_enclave(p_target_info, sizeof(sgx_target_info_t)));
  memcpy(&target_info, p_target_info, sizeof(sgx_target_info_t));
  // 初始化 WolfSSL
  // wolfSSL_Debugging_ON();
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

// Client 连接通过 socket 进行查找
std::map<int, Client> workers;

// 创建一个新的 SSL 连接，返回连接的 id
// App 内需要确保 socket 是唯一的
int e_new_ssl(int socket_id, const char *hostname, size_t hostname_size,
              const char *request, size_t request_size) {
  ASSERT(sgx_is_outside_enclave(hostname, hostname_size));
  ASSERT(sgx_is_outside_enclave(request, request_size));
  // 检验 ctx 已经初始化
  if (global_ctx == nullptr) {
    return StatusCode::Uninitialized;
  }
  // 检查连接是否达到上限
  if (workers.size() >= MAX_WORKER) {
    return StatusCode::NoAvailableWorker;
  }
  // socket_id 必须是唯一的
  if (workers.find(socket_id) != workers.cend()) {
    ERROR("socket_id not unique");
    return StatusCode::Unknown;
  }
  // 创建 client
  workers.emplace(
      std::piecewise_construct, std::make_tuple(socket_id),
      std::make_tuple(std::string(hostname, hostname_size),
                      std::string(request, request_size), socket_id));
  LOG("Created SSL with id %d", socket_id);
  return StatusCode::Success;
}

// 移除指定的 SSL 连接
void e_remove_ssl(int id) {
  if (workers.erase(id)) {
    LOG("Removed worker %d", id);
  }
}

// 根据 id 令指定的 SSL 连接进行工作
// 如果处理完成，则将网页写入 p_result 指向的空间
int e_work(int id, void *p_result) {
  // 检查指针范围
  ASSERT(sgx_is_outside_enclave(p_result, sizeof(EnclaveResult)));
  auto &result = *(EnclaveResult *)p_result;
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
      // 写入回复
      auto &response = worker.get_response();
      memcpy(result.data, response.data(), response.size());
      result.data_size = (int)response.size();
      // 写入 report
      result.report = worker.get_report();
      // 释放空间
      LOG("Client %d finished, freeing", id);
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
      LOG("Client %d failed with error '%s', freeing", id, status.message());
      workers.erase(iter);
      return status;
    }
  }
  UNREACHABLE();
}