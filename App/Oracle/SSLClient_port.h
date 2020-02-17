// Boost::beast（App）和 WolfSSL 会有冲突
#ifndef _A_SSLCLIENT_PORT_H_
#define _A_SSLCLIENT_PORT_H_
#include <boost/asio.hpp>
#include <functional>
#include <string>

class SSLClient;

SSLClient* create_SSLClient(int id, const std::string& hostname,
                            const std::string& request,
                            boost::asio::io_context& context,
                            std::function<void(const std::string&)>&& callback);

void free_SSLClient(SSLClient* client);

void close_SSLClient(SSLClient* client);

#endif  // _A_SSLCLIENT_PORT_H_