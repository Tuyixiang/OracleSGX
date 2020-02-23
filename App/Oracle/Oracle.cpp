#include "Oracle.h"
#include <boost/thread.hpp>
#include <chrono>
#include "App/Enclave_u.h"
#include "Shared/Config.h"
#include "Shared/StatusCode.h"

using namespace boost::asio;
using namespace std::chrono;

const std::string request =
    "GET / HTTP/1.1\r\n"
    "Host: www.baidu.com\r\n"
    // "Upgrade-Insecure-Requests: 1\r\n"
    // "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_2) "
    // "AppleWebKit/537.36 (KHTML, like Gecko) Chrome/80.0.3987.100 "
    // "Safari/537.36\r\n"
    // "Accept: "
    // "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,image/"
    // "apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9\r\n"
    "Accept-Encoding: identity\r\n\r\n";

void test() { Oracle::global().test_run("www.baidu.com", request); }

// 获取唯一的 id
int Oracle::get_random_id() {
  static std::random_device rd;
  static std::mt19937 mt(rd());
  // 超出数量则抛出错误
  if (executors.size() >= MAX_WORKER) {
    throw StatusCode(StatusCode::NoAvailableWorker);
  }
  // 获取唯一随机非负整数 id
  int new_id;
  do {
    new_id = abs(static_cast<int>(mt()));
  } while (executors.find(new_id) != executors.cend());
  return new_id;
}

// 从 map 中和 Enclave 中移除一个任务
std::map<int, boost::shared_ptr<Executor>>::iterator Oracle::remove_job(
    const std::map<int, boost::shared_ptr<Executor>>::iterator &it) {
  if (it == executors.cend()) {
    return it;
  }
  // 在 Enclave 中移除
  e_remove_ssl(global_eid, it->first);
  // 移除并返回 iterator
  return executors.erase(it);
}

// 创建一个新的任务
void Oracle::new_job(const std::string &address, const std::string &request) {
  auto id = get_random_id();
  auto shared_p =
      boost::shared_ptr<Executor>(new Executor(ctx, id, address, request));
  executors.emplace(id, shared_p);
  pending.push_back(std::move(shared_p));
}

// 某个异步 IO 操作完成，需要执行 Executor::work
void Oracle::need_work(boost::shared_ptr<Executor> p_executor) {
  boost::lock_guard lock(mutex);
  pending.push_back(std::move(p_executor));
}

// 遍历并执行所有任务
void Oracle::work() {
  // 取出所有等待进行的 work 任务
  decltype(pending) recorded_pending;
  {
    boost::lock_guard lock(mutex);
    pending.swap(recorded_pending);
  }
  // 处理这些任务
  for (auto it = recorded_pending.begin(); it != recorded_pending.cend();
       it++) {
    auto &executor = **it;
    try {
      if (executor.work()) {
        // 该任务完成，将其释放
        LOG(GREEN "Executor %d completed, freeing" RESET, executors.size(), executor.id);
        remove_job(executor.id);
      } else {
        if (!executor.blocking) {
          need_work(*it);
        }
      }
    } catch (const StatusCode &status) {
      // 出现错误，将其终止
      ERROR("Executor %d terminated with message: %s", executor.id,
            status.message());
      remove_job(executor.id);
    }
  }
}

void Oracle::test_run(const std::string &address, const std::string &request) {
  // 建立多个线程执行不需要 Enclave 参与的步骤，即 io_context::run()
  auto run = [&]() {
    while (true) {
      ctx.run();
      ctx.restart();
    }
  };
  boost::thread pool[] = {
      boost::thread(run), boost::thread(run), boost::thread(run),
      boost::thread(run), boost::thread(run),
  };
  while (true) {
    if (executors.size() < 512) {
      new_job(address, request);
    }
    work();
  }
}
