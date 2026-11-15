#include <steam/steamnetworkingsockets.h>

#include <type_traits>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/transport.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/connection_manager.hpp>

static_assert(lightnerx::network::k_invalidConnection == k_HSteamNetConnection_Invalid);
static_assert(!std::is_copy_constructible_v<lightnerx::network::Client>);

const void* producerStateAddress();
bool queueFromProducer();

int main() {
    lightnerx::network::Client client;
    namespace network = lightnerx::network;
    if (client.getServerHConn() != network::k_invalidConnection) return 1;
    if (producerStateAddress() != &network::g_bNetworkInitialized) return 2;
    if (!network::networkInit()) return 3;
    network::initMessagesBatcher(1);
    if (!queueFromProducer()) return 4;
    network::ConnectionManager connections(0, 0);
    // The invalid destination proves the other translation unit used this queue.
    const auto flushed = network::flushAll(connections);
    if (flushed || flushed.error() != std::errc::not_connected) return 5;
    if (!network::flushAll(connections)) return 6;
    network::networkKill();
    return network::isNetworkInitialized() ? 7 : 0;
}
