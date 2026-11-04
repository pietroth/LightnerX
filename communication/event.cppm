module;

#include <cstdint>

export module communication.event;

import communication.base_protocol;
import communication.mbt;
import network.client;

export namespace communication {
    using namespace communication;
    using namespace network;

    #pragma pack(push, 1)
    struct Event {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes

        Event(uint16_t size, uint16_t typeId) : protocol(size), mbt(typeId) {}
    };
    #pragma pack(pop)
}
