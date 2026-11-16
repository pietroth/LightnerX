#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/event.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/network_manager.hpp>
#include <lightnerx/communication/response.hpp>
#include <array>
#include <lightnerx/network/server.hpp>

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
