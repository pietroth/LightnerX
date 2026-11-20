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

const void* producerStateAddress() {
    return &lightnerx::network::g_bNetworkInitialized;
}

bool queueFromProducer() {
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;
    return bool(network::queueMessage(communication::event2OutboundMessage(
        communication::Event(8, 12), {}, network::Target::single(network::k_invalidConnection))));
}
