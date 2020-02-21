#ifndef _SHARED_CONFIG_H_
#define _SHARED_CONFIG_H_

// 最多同时维持连接数量
const int MAX_WORKER = 1024;
// socket 单次读取长度
const int SOCKET_READ_SIZE = 8192;
// 如果网页响应超过此长度则会丢弃
const int MAX_RESPONSE_SIZE = 1 << 20;  // 1MB
// 在 App 中用一个单独的（线程不应冲突）buffer 保存 Enclave 返回的数据
const int APP_RESPONSE_BUFFER_SIZE = 1 << 23;  // 8MB
// 单个完整任务超时时限
#define TASK_TIMEOUT 30s

#endif  // _SHARED_CONFIG_H_