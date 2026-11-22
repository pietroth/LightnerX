import std;
import lightnerx.communication.message;
import lightnerx.communication.event;
import lightnerx.network.message_batch;
import lightnerx.network.target;
import lightnerx.network.network_manager;
import lightnerx.communication.response;
import lightnerx.network.server;
import lightnerx.network.connection_manager;

namespace {
struct ReserveMessageBatcher {
    ReserveMessageBatcher() { lightnerx::network::initMessagesBatcher(1); }
};

// Reserve from a consumer translation unit before main().
const ReserveMessageBatcher reserveMessageBatcher;
}

std::array<const void*, 8> producerConstantAddresses() {
    namespace communication = lightnerx::communication;
    return {&communication::APPROVED_RESPONSE, &communication::DENIED_RESPONSE,
            &communication::PARTIALLY_APPROVED_RESPONSE, &communication::FAILURE_RESPONSE,
            &lightnerx::network::k_invalidConnection, &lightnerx::network::k_invalidSocket,
            &lightnerx::network::k_invalidPollGroup, &lightnerx::network::invalidConnection};
}

bool producerNetworkInitialized() {
    return lightnerx::network::isNetworkInitialized();
}

bool queueFromProducer() {
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;
    auto destination = network::Target::group({network::k_invalidConnection});
    auto copy = destination;
    auto moved = std::move(copy);
    copy = destination;
    destination = std::move(moved);
    if (!copy.isGroup() || destination.connection != copy.connection) return false;
    auto message = communication::event2OutboundMessage(communication::Event(8, 12), {}, destination);
    return bool(network::queueMessage(std::move(message)));
}
