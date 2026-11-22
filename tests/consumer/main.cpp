#include <steam/steamnetworkingsockets.h>

import std;
import lightnerx.network.server;
import lightnerx.communication.response;
import lightnerx.network.client;
import lightnerx.network.transport;
import lightnerx.network.message_batch;
import lightnerx.network.connection_manager;
import lightnerx.network.network_manager;

static_assert(lightnerx::network::k_invalidConnection == k_HSteamNetConnection_Invalid);
static_assert(!std::is_copy_constructible_v<lightnerx::network::Client>);

bool producerNetworkInitialized();
std::array<const void*, 8> producerConstantAddresses();
bool queueFromProducer();

int main() {
    lightnerx::network::Client client;
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;
    const std::array<const void*, 8> constants{
        &communication::APPROVED_RESPONSE, &communication::DENIED_RESPONSE,
        &communication::PARTIALLY_APPROVED_RESPONSE, &communication::FAILURE_RESPONSE,
        &network::k_invalidConnection, &network::k_invalidSocket,
        &network::k_invalidPollGroup, &network::invalidConnection
    };
    if (producerConstantAddresses() != constants) return 8;
    if (client.getServerHConn() != network::k_invalidConnection) return 1;
    if (producerNetworkInitialized() || network::isNetworkInitialized()) return 2;
    if (!network::networkInit()) return 3;
    if (!producerNetworkInitialized() || !network::isNetworkInitialized()) return 9;
    if (!queueFromProducer()) return 4;
    network::ConnectionManager connections(0, 0);
    // The invalid destination proves the other translation unit used this queue.
    const auto flushed = network::flushAll(connections);
    if (flushed || flushed.error() != std::errc::not_connected) return 5;
    if (!network::flushAll(connections)) return 6;
    network::networkKill();
    return producerNetworkInitialized() || network::isNetworkInitialized() ? 7 : 0;
}
