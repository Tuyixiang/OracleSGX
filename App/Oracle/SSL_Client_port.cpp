#include "SSLClient.h"
#include "SSLClient_port.h"
#include "Shared/Logging.h"

SSLClient* create_SSLClient(
    int id, const std::string& hostname, const std::string& request,
    boost::asio::io_context& context,
    std::function<void(const std::string&)>&& callback) {
  LOG("create_SSLClient %d", id);
  SSLClient* client =
      new SSLClient(id, hostname, request, context, std::move(callback));
  client->run();
  return client;
}

void free_SSLClient(SSLClient* client) { delete client; }

void close_SSLClient(SSLClient* client) {
  if (client) {
    client->close();
  }
}