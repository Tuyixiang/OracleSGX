#include "SSLClient.h"
#include "SSLClient_port.h"
#include "Shared/Logging.h"

SSLClient* create_SSLClient(
    const std::string& hostname, const std::string& request,
    boost::asio::io_context& context,
    std::function<void(const std::string&)>&& callback) {
  LOG("run_SSLClient");
  SSLClient* client =
      new SSLClient(hostname, request, context, std::move(callback));
  client->run();
  return client;
}

void free_SSLClient(SSLClient* client) { delete client; }