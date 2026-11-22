#include <type_traits>
#include <array>
#include <memory>
#include <utility>
#include <lightnerx/network/server.hpp>
#include <lightnerx/communication/response.hpp>
#include <lightnerx/network/client.hpp>
#include <lightnerx/network/transport.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/connection_manager.hpp>
#include <lightnerx/network/network_manager.hpp>

static_assert(lightnerx::network::k_invalidConnection == 0);
static_assert(!std::is_copy_constructible_v<lightnerx::network::Client>);

bool producerNetworkInitialized();
std::array<const void*, 8> producerConstantAddresses();
bool queueFromProducer();

int main() {
    using lightnerx::Result;
    const Result<int, int> success = 7;
    const auto failure = Result<int, int>::failure(42);
    if (!success || success.value() != 7 || failure || failure.error() != 42) return 9;

    Result<std::unique_ptr<int>> movable = std::make_unique<int>(17);
    auto value = std::move(movable).value();
    if (!value || *value != 17) return 10;

    const auto customError = Result<void, std::unique_ptr<int>>::failure(std::make_unique<int>(23));
    if (customError || *customError.error() != 23) return 11;

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
    if (!producerNetworkInitialized() || !network::isNetworkInitialized()) return 12;
    if (!queueFromProducer()) return 4;
    network::ConnectionManager connections(0, 0);
    // The invalid destination proves the other translation unit used this queue.
    const auto flushed = network::flushAll(connections);
    if (flushed || flushed.error() != lightnerx::ErrorCode::NotConnected) return 5;
    if (!network::flushAll(connections)) return 6;
    network::networkKill();
    return producerNetworkInitialized() || network::isNetworkInitialized() ? 7 : 0;
}
