// 用于访问 IAS
#ifndef _A_SSLCLIENT_H_
#define _A_SSLCLIENT_H_

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/enable_shared_from_this.hpp>

using namespace boost::asio;

class SSLClient : public boost::enable_shared_from_this<SSLClient> {
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

  static void async_process(boost::shared_ptr<SSLClient> p_client, yield_context yield);

 public:
  SSLClient(int id, const std::string& hostname, const std::string& request,
            io_context& ctx, Callback&& cb);

  void run();

  void close();

  ~SSLClient();
};

#endif  // _A_SSLCLIENT_H_