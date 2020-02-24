#ifndef _A_IAS_H_
#define _A_IAS_H_

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <tuple>
#include "Shared/Config.h"

using namespace boost::asio;
using namespace boost::beast;
class Executor;

class IAS {
 protected:
  IAS();

  int index;

  static io_context ctx;
  ip::tcp::resolver::results_type endpoints;
  ssl::stream<tcp_stream> stream;

  bool tcp_connected = false;
  bool ssl_connected = false;

  // 等待进行 IAS 的任务
  static std::list<std::tuple<boost::shared_ptr<Executor>, std::string>> tasks;
  static boost::mutex task_mutex;
  // IAS 池，每个 IAS 对应一个 SSL 连接
  static IAS ias_pool[IAS_POOL_SIZE];
  static std::list<int> idle_ias;

  // 初始化全局变量和 IAS 池
  static void initialize_context();
  // 执行的主流程
  void async_process(yield_context yield);
  // 激活某个 IAS
  static void spawn_ias(int index);

 public:
  // 发送 IAS，会调用 Executor 中的回调函数
  static void send_ias(boost::shared_ptr<Executor> p_executor,
                       const std::string& request);
};

#endif  // _A_IAS_H_