#include "Oracle.h"
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
std::map<int, Executor>::iterator Oracle::remove_job(
    const std::map<int, Executor>::iterator &it) {
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
  executors.emplace(std::piecewise_construct, std::make_tuple(id),
                    std::tie(ctx, id, address, request));
}

// 遍历并执行所有任务
void Oracle::work() {
  for (auto it = executors.begin(); it != executors.cend();) {
    try {
      if (it->second.work()) {
        // 该任务完成，将其释放
        LOG(GREEN "Executor completed, freeing executor %d" RESET, it->first);
        it->second.close();
        ctx.poll();
        ctx.restart();
        it = remove_job(it);
      } else {
        // 继续执行后续任务
        ++it;
      }
    } catch (const StatusCode &status) {
      // 出现错误，将其终止
      ERROR("Executor %d terminated with message: %s", it->first,
            status.message());
      it->second.close();
      ctx.poll();
      ctx.restart();
      it = remove_job(it);
    }
  }
}

void Oracle::test_run(const std::string &address, const std::string &request) {
  for (int i = 0; i < 256; i += 1) {
    new_job(address, request);
  }
  while (!executors.empty()) {
    if (executors.size() < 256) {
      new_job(address, request);
    }
    work();
    ctx.poll();
    ctx.restart();
  }
}
