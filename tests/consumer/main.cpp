#include <steam/steamnetworkingsockets.h>

#include <type_traits>
#include <array>
#include <lightnerx/network/server.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/transport.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/connection_manager.hpp>

#ifndef TCB_SPAN_TERMINATE_ON_CONTRACT_VIOLATION
#error "The LightnerX target must propagate the span contract policy to consumers"
#endif

static_assert(lightnerx::network::k_invalidConnection == k_HSteamNetConnection_Invalid);
static_assert(!std::is_copy_constructible_v<lightnerx::network::Client>);

const void* producerStateAddress();
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
    if (producerStateAddress() != &network::g_bNetworkInitialized) return 2;
    if (!network::networkInit()) return 3;
    if (!queueFromProducer()) return 4;
    network::ConnectionManager connections(0, 0);
    // The invalid destination proves the other translation unit used this queue.
    const auto flushed = network::flushAll(connections);
    if (flushed || flushed.error() != std::errc::not_connected) return 5;
    if (!network::flushAll(connections)) return 6;
    network::networkKill();
    return network::isNetworkInitialized() ? 7 : 0;
}
