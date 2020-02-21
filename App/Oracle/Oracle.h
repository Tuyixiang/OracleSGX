#ifndef _A_ORACLE_H_
#define _A_ORACLE_H_

#include <errno.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <random>
#include "App/App.h"
#include "App/Enclave_u.h"
#include "Executor.h"
#include "Shared/Config.h"
#include "Shared/Logging.h"
#include "sgx_urts.h"

using namespace boost::asio;

void test();

class Oracle {
 protected:
  // 单件
  Oracle() = default;

  // 获取唯一的 id
  int get_random_id();

  // 从 map 中和 Enclave 中移除一个任务
  void remove_job(int id) { remove_job(executors.find(id)); }
  std::map<int, boost::shared_ptr<Executor>>::iterator remove_job(
      const std::map<int, boost::shared_ptr<Executor>>::iterator &it);

 public:
  io_context ctx;
  std::map<int, boost::shared_ptr<Executor>> executors;

  // 获取全局的对象
  static Oracle &global() {
    static Oracle oracle;
    return oracle;
  }

  // 创建一个新的任务
  void new_job(const std::string &address, const std::string &request);

  // 遍历并执行所有任务
  void work();

  void test_run(const std::string &address, const std::string &request);
};

#endif  // _A_ORACLE_H_