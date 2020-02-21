#include "SSLClient.h"
#include "SSLClient_port.h"
#include "Shared/Logging.h"

boost::shared_ptr<SSLClient> create_SSLClient(
    int id, const std::string& hostname, const std::string& request,
    boost::asio::io_context& context,
    std::function<void(const std::string&)>&& callback) {
  LOG("create_SSLClient %d", id);
  boost::shared_ptr<SSLClient> p_client(new SSLClient(id, hostname, request, context, std::move(callback)));
  p_client->run();
  return p_client;
}

void close_SSLClient(boost::shared_ptr<SSLClient> p_client) {
  if (p_client) {
    p_client->close();
  }
}