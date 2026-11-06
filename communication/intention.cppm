module;

export module communication.intention;

import std;

import communication.base_protocol;
import communication.mbt;

export namespace communication {
    #pragma pack(push, 1)
    struct Intention {
        BaseProtocol protocol; // 2 bytes
        MBT mbt; // 2 bytes;
        std::uint16_t correlationId;

        Intention(std::uint16_t size, std::uint16_t type, std::uint16_t correlationId)
            : protocol(size), mbt(type), correlationId(correlationId) {}
    };
    #pragma pack(pop)
}
