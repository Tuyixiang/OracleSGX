#include "WolfSSL.h"
#include "../Enclave_t.h"
#include "CA.h"

#include "sgx_trts.h"

WOLFSSL_CTX *global_ctx = nullptr;

// 初始化系统
void e_init() {
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
}
