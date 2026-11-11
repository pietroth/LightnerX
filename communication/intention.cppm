module;

export module lightnerx.communication.intention;

import std;

import lightnerx.communication.base_protocol;
import lightnerx.communication.mbt;

export namespace lightnerx::communication {
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
