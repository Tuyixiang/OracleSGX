#include "Executor.h"

using namespace boost::asio;

// 在 Enclave 中创建对应的对象
void Executor::init_enclave_ssl(const std::string& request, int id) {
  int status;
  e_new_ssl(global_eid, &status, id, request.data(), (int)request.size());
  ASSERT(status == StatusCode::Success);
}

Executor::Executor(io_context& ctx, int id, std::string address,
                   const std::string& request)
    : address(std::move(address)),
      id(id),
      ctx(ctx),
      socket(ctx),
      resolver(ctx) {
  init_enclave_ssl(request, id);
  // 设置 non_blocking 需要在 socket 连接后进行，因此在回调中进行
  // socket.non_blocking(true);
}

// 执行工作，返回是否完成，错误则抛出
bool Executor::work() {
  if (blocking) {
    // 正在进行异步操作，尚未结束
    return false;
  }
  while (true) {
    switch (state) {
      case Resolve: {
        // 解析域名
        LOG("Executor %d resolving address '%s'", id, address.c_str());
        blocking = true;
        resolver.async_resolve(address, "https",
                               [&](auto& error, auto results) {
                                 blocking = false;
                                 if (error) {
                                   // 解析错误
                                   LOG("Executor %d failed to resolve: %s", id,
                                       error.message().c_str());
                                   state = Error;
                                   error_code = StatusCode::LibraryError;
                                 } else {
                                   // 解析完成后，进行连接
                                   LOG("Executor %d resolved", id);
                                   state = Connect;
                                   endpoints = results;
                                   // 执行下一步
                                   work();
                                 }
                               });
        return false;
      }
      case Connect: {
        // 进行连接（尝试所有 endpoints）
        LOG("Executor %d connecting", id);
        blocking = true;
        async_connect(socket, endpoints, [&](auto& error, auto) {
          blocking = false;
          if (error) {
            // 连接失败
            LOG("Executor %d failed to connect: %s", id,
                error.message().c_str());
            state = Error;
            error_code = StatusCode::LibraryError;
          } else {
            // 连接成功
            LOG("Executor %d connected", id);
            state = Process;
            // 设置非阻塞
            socket.non_blocking(true);
            // 执行下一步
            work();
          }
        });
        return false;
      }
      case Process: {
        // 由 Enclave 进行处理
        LOG("Executor %d processing", id);
        int status;
        e_work(global_eid, &status, id, response, &response_size);
        if (StatusCode(status).is_error()) {
          throw status;
        }
        if (status == StatusCode::Success) {
          // 处理完成
          LOG("Executor %d processing done", id);
          state = Attest;
          // 执行下一步
          continue;
        } else if (status == StatusCode::Blocking) {
          // 阻塞中
          return false;
        }
        UNREACHABLE();
      }
      case Attest: {
        // 先跳过
        LOG("Executor %d complete (%d bytes):\n%s", id, response_size,
            response);
        return true;
      }
      case Error: {
        ASSERT(error_code.is_error());
        throw error_code;
      }
      default: { UNREACHABLE(); }
    }
  }
  UNREACHABLE();
}
