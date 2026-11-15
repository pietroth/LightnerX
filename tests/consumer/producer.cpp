#include <lightnerx/communication/message.hpp>
#include <lightnerx/communication/event.hpp>
#include <lightnerx/network/message_batch.hpp>
#include <lightnerx/network/target.hpp>
#include <lightnerx/network/network_manager.hpp>

const void* producerStateAddress() {
    return &lightnerx::network::g_bNetworkInitialized;
}

bool queueFromProducer() {
    namespace network = lightnerx::network;
    namespace communication = lightnerx::communication;
    return bool(network::queueMessage(communication::event2OutboundMessage(
        communication::Event(8, 12), {}, network::Target::single(network::k_invalidConnection))));
}
