#ifndef _A_EXECUTOR_H_
#define _A_EXECUTOR_H_

#include <atomic>
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <chrono>
#include <string>
#include "App/App.h"
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/EnclaveResult.h"
#include "Shared/StatusCode.h"

using namespace boost::asio;
using namespace std::chrono;

class SSLClient;

class Executor : public boost::enable_shared_from_this<Executor> {
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
  // 主机地址
  const std::string hostname;
  // 解析得到的一系列 IP 地址
  ip::tcp::resolver resolver;
  ip::tcp::resolver::results_type endpoints;
  // 访问 IAS 所用
  boost::shared_ptr<SSLClient> ssl_client;
  // 开始时间
  const time_point<steady_clock> start_time;

  // 在 Enclave 中创建对应的对象
  static void init_enclave_ssl(const std::string& hostname,
                               const std::string& request, int id);

  // 解析域名之后的回调
  static void resolve_callback(boost::shared_ptr<Executor> p_executor,
                               const boost::system::error_code& ec,
                               ip::tcp::resolver::results_type endpoints);

  // SSL 连接（握手）之后的回调
  static void connect_callback(boost::shared_ptr<Executor> p_executor,
                               const boost::system::error_code& ec,
                               const ip::tcp::endpoint& endpoint);

  // 启动 IAS 的请求
  static void ias_request(boost::shared_ptr<Executor> p_executor,
                          sgx_report_t report);

 public:
  // 使用 SSLClient 进行 IAS 确认之后的回调
  static void ias_callback(boost::shared_ptr<Executor> p_executor,
                           const std::string& response);

  // 在 map<int, Executor> 中的 key，也是 Enclave 的 o_recv 和 o_send 查找的标识
  const int id;
  // 需要使用这个 context 来进行许多操作
  io_context& ctx;
  // o_recv 和 o_send 需要通过 Executor 来找到对应的 socket
  ip::tcp::socket socket;
  // 调用 o_recv 或 o_send 出现阻塞时置 true，然后通过 async_wait 置 false
  // 遍历所有 Executor 时会略过正在阻塞的
  std::atomic<bool> blocking = false;
  // Enclave 返回结果
  static EnclaveResult enclave_result;

  Executor(io_context& ctx, int id, const std::string& hostname,
           const std::string& request);

  // 异步回调发现错误，调用此函数置错误标记，下次 work 时返回错误
  void async_error();

  // 执行工作，返回是否完成，错误则抛出
  bool work();

  // 关闭所有 socket，在释放前必须 close() 且 context 执行完回调
  void close();

  ~Executor();
};

#endif  // _A_EXECUTOR_H_