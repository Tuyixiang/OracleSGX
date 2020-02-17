// 用于访问 IAS
#ifndef _A_SSLCLIENT_H_
#define _A_SSLCLIENT_H_

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
using namespace boost::asio;

class SSLClient {
 public:
  typedef std::function<void(const std::string&)> Callback;

 protected:
  int id;
  io_context& ctx;
  ip::tcp::resolver resolver;
  ssl::stream<ip::tcp::socket> stream;
  std::string hostname;
  std::string request;
  Callback callback;
  bool* destroy_flag = nullptr;

 public:
  SSLClient(int id, const std::string& hostname, const std::string& request,
            io_context& ctx, Callback&& cb);

  void run();

  void close();

  ~SSLClient();
};

#endif  // _A_SSLCLIENT_H_