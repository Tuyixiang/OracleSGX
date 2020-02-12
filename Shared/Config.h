// 最多同时维持连接数量
const int MAX_WORKER = 64;
// socket 单次读取长度
const int SOCKET_READ_SIZE = 8192;
// 如果网页响应超过此长度则会丢弃
const int MAX_RESPONSE_SIZE = 1048576;  // 1MB