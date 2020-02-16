#ifndef _A_ORACLE_H_
#define _A_ORACLE_H_

#include "App/App.h"
#include "App/Enclave_u.h"
#include "Executor.h"
#include "Shared/Config.h"
#include "Shared/Logging.h"
#include "sgx_urts.h"
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <errno.h>
#include <iostream>
#include <random>

using namespace boost::asio;

void test();

class Oracle {

protected:
  int get_random_id() {
    static std::random_device rd;
    static std::mt19937 mt(rd());
    // 超出数量则抛出错误
    if (executors.size() >= MAX_WORKER) {
      throw StatusCode::NoAvailableWorker;
    }
    // 获取唯一随机非负整数 id
    int new_id;
    do {
      new_id = abs(static_cast<int>(mt()));
    } while (executors.find(new_id) != executors.cend());
    return new_id;
  }

  Oracle() = default;

  auto remove_job(const std::map<int, Executor>::iterator &it) {
    if (it == executors.cend()) {
      return it;
    }
    // 在 Enclave 中移除
    e_remove_ssl(global_eid, it->first);
    // 移除并返回 iterator
    return executors.erase(it);
  }

  void remove_job(int id) { remove_job(executors.find(id)); }

public:
  io_context ctx;
  std::map<int, Executor> executors;

  static Oracle &global() {
    static Oracle oracle;
    return oracle;
  }

  void new_job(const std::string &address, const std::string &request) {
    auto id = get_random_id();
    executors.emplace(std::piecewise_construct, std::make_tuple(id),
                      std::tie(ctx, id, address, request));
  }

  // 遍历并执行所有任务
  void work() {
    for (auto it = executors.begin(); it != executors.cend();) {
      try {
        if (it->second.work()) {
          // 该任务完成，将其释放
          it = executors.erase(it);
        } else {
          // 继续执行后续任务
          ++it;
        }
      } catch (const StatusCode &status) {
        // 出现错误，将其终止
        ERROR("Executor %d terminated with message: %s", it->first,
              status.message());
        it = remove_job(it);
      }
    }
  }

  void test_run(const std::string &address, const std::string &request) {
    new_job(address, request);
    while (!executors.empty()) {
      work();
      ctx.poll();
      usleep(1000);
    }
  }
};

#endif // _A_ORACLE_H_