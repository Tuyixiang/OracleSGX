#ifndef _E_WOLFSSL_H_
#define _E_WOLFSSL_H_

#include <wolfssl/ssl.h>
#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/types.h>

// 全局仅有一个，仅在初始化时创建
extern WOLFSSL_CTX *global_ctx;

#endif // _E_WOLFSSL_H_