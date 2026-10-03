module;

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

        Event(BaseProtocol protocol, MBT mbt) : protocol(protocol), mbt(mbt) {}
    };
    #pragma pack(pop)
}
