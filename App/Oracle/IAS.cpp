#include "IAS.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/thread.hpp>
#include <boost/thread/lock_guard.hpp>
#include "Executor_port.h"
#include "IAS_port.h"
#include "Oracle_port.h"
#include "Shared/Logging.h"

using namespace boost::beast;

const std::string ias_hostname = "api.trustedservices.intel.com";

io_context IAS::ctx;

ssl::context ssl_ctx(ssl::context::method::sslv23_client);

// 初始化全局变量和 IAS 池
void IAS::initialize_context() {
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    // 加载 CA 根证书
    ssl_ctx.load_verify_file("App/Oracle/ca_certs.pem");
    // 启动线程令 ctx 工作
    auto executor = []() {
      while (true) {
        ctx.run();
        ctx.restart();
      }
    };
    boost::thread t(executor);
    // 让池中 IAS 开始工作
    for (int i = 0; i < IAS_POOL_SIZE; i += 1) {
      ias_pool[i].index = i;
      spawn_ias(i);
    }
  }
}

// 等待进行 IAS 的任务
std::list<std::tuple<boost::shared_ptr<Executor>, std::string>> IAS::tasks;
boost::mutex IAS::task_mutex;
// IAS 池，每个 IAS 对应一个 SSL 连接
IAS IAS::ias_pool[IAS_POOL_SIZE];
std::list<int> IAS::idle_ias;

// 执行的主流程
void IAS::async_process(yield_context yield) {
  LOG("IAS %d started", index);
  // 解析域名
  if (endpoints.empty()) {
    ip::tcp::resolver resolver(ctx);
    do {
      try {
        endpoints = resolver.async_resolve(ias_hostname, "https", yield);
      } catch (const boost::system::system_error& e) {
        ERROR("IAS %d resolving hostname failed: %s", index, e.what());
        continue;
      }
    } while (false);
    LOG("IAS %d hostname resolved", index);
  }
  // 循环：握手，然后不断执行任务，如果中断则从握手重来
  while (true) {
    // 建立连接
    do {
      try {
        // TCP
        if (!tcp_connected) {
          get_lowest_layer(stream).expires_after(TASK_TIMEOUT);
          get_lowest_layer(stream).async_connect(endpoints, yield);
          tcp_connected = true;
        }
        // SSL
        if (!ssl_connected) {
          stream.async_handshake(ssl::stream_base::client, yield);
          ssl_connected = true;
        }
      } catch (const boost::system::system_error& e) {
        ERROR("IAS %d handshake failed: %s", index, e.what());
        tcp_connected = ssl_connected = false;
        get_lowest_layer(stream).close();
        stream.async_shutdown(yield);
        continue;
      }
    } while (false);
    LOG("IAS %d connected", index);
    // 执行任务
    while (true) {
      boost::shared_ptr<Executor> p_executor;
      std::string request;
      try {
        // 取出一个任务
        {
          boost::lock_guard lock(task_mutex);
          if (tasks.empty()) {
            // 无任务执行，退出
            LOG("IAS %d exit", index);
            // 将自己标注为空闲
            idle_ias.push_back(index);
            return;
          }
          LOG("IAS %d processing task", index);
          auto& task = tasks.front();
          p_executor = std::get<0>(task);
          request = std::get<1>(task);
          tasks.pop_front();
        }
        // 发送请求
        get_lowest_layer(stream).expires_after(TASK_TIMEOUT);
        async_write(stream, const_buffer(request.data(), request.size()),
                    yield);
        // 接收回复
        streambuf buffer;
        http::response<http::string_body> response;
        http::async_read(stream, buffer, response, yield);
        INFO("IAS %d response: %s", index, response.body().c_str());
        executor_ias_callback(p_executor, response.body());
      } catch (const boost::system::system_error& e) {
        ERROR("IAS %d task failed: %s", index, e.what());
        // 出现错误，将任务重新放回队列
        if (p_executor) {
          send_ias(p_executor, request);
        }
        get_lowest_layer(stream).close();
        tcp_connected = ssl_connected = false;
        break;
      }
    }
  }
  UNREACHABLE();
}

void IAS::spawn_ias(int index) {
  spawn(ctx, [index](auto yield) { ias_pool[index].async_process(yield); });
}

IAS::IAS() : stream(ctx, ssl_ctx) {}

// 发送 IAS，会调用 Executor 中的回调函数
void IAS::send_ias(boost::shared_ptr<Executor> p_executor,
                   const std::string& request) {
  initialize_context();
  // 将任务加入队列
  INFO("IAS task added");
  // 如果有空闲 IAS，则将其激活

  // 加锁找空闲 IAS
  task_mutex.lock();
  tasks.push_back(std::make_tuple(p_executor, request));
  if (!idle_ias.empty()) {
    int index = idle_ias.front();
    idle_ias.pop_front();
    task_mutex.unlock();
    spawn_ias(index);
  } else {
    task_mutex.unlock();
  }
}

void send_ias(boost::shared_ptr<Executor> p_executor,
              const std::string& request) {
  IAS::send_ias(p_executor, request);
}
