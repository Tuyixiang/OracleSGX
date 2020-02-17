#ifndef _A_EXECUTOR_H_
#define _A_EXECUTOR_H_

#include <atomic>
#include <boost/asio.hpp>
#include <string>
#include "App/App.h"
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/EnclaveResult.h"
#include "Shared/StatusCode.h"

using namespace boost::asio;

class SSLClient;

class Executor {
 protected:
  enum State {
    // 解析域名
    Resolve,
    // 建立 socket 的 TCP 连接
    Connect,
    // 在 Enclave 中执行操作（获取目标网页、生成 Report）
    Process,
    // Intel SGX Attestation（生成 quote 并请求 IAS）
    Attest,
    // 出现错误，或执行完毕，在回调中设置，由 error_code 区分
    Finished,
  } state = Resolve;
  // 如果异步操作出现错误，回调中设置 code，work() 时返回
  StatusCode error_code;
  // 所连接的网址
  const std::string address;
  // 在 map<int, Executor> 中的 key，也是 Enclave 的 o_recv 和 o_send 查找的标识
  const int id;
  // 解析得到的一系列 IP 地址
  ip::tcp::resolver resolver;
  ip::tcp::resolver::results_type endpoints;
  // 访问 IAS 所用
  SSLClient* ssl_client = nullptr;

  // 在 Enclave 中创建对应的对象
  static void init_enclave_ssl(const std::string& request, int id);

 public:
  // 需要使用这个 context 来进行许多操作
  io_context& ctx;
  // o_recv 和 o_send 需要通过 Executor 来找到对应的 socket
  ip::tcp::socket socket;
  // 调用 o_recv 或 o_send 出现阻塞时置 true，然后通过 async_wait 置 false
  // 遍历所有 Executor 时会略过正在阻塞的
  std::atomic<bool> blocking = false;
  // Enclave 返回结果
  static EnclaveResult enclave_result;

  Executor(io_context& ctx, int id, std::string address,
           const std::string& request);

  // 异步回调发现错误，调用此函数置错误标记，下次 work 时返回错误
  void async_error();

  // 执行工作，返回是否完成，错误则抛出
  bool work();

  ~Executor();
};

#endif  // _A_EXECUTOR_H_