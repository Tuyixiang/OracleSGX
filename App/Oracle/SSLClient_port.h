// Boost::beast（App）和 WolfSSL 会有冲突
#ifndef _A_SSLCLIENT_PORT_H_
#define _A_SSLCLIENT_PORT_H_
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <functional>
#include <string>

class SSLClient;

boost::shared_ptr<SSLClient> create_SSLClient(
    int id, const std::string& hostname, const std::string& request,
    boost::asio::io_context& context,
    std::function<void(const std::string&)>&& callback);

void close_SSLClient(boost::shared_ptr<SSLClient> client);

#endif  // _A_SSLCLIENT_PORT_H_